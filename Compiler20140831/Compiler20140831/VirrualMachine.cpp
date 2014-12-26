#include "VirtualMachine.h"
#include "parser.h"
#include "SymbolTable.h"
using namespace vm;
/*
*@time 2014-12-24
*@author wanghesai
*All rights reserved
*/
VirtualMachine::VirtualMachine()
{
	programCounter = 0;
	reg_flag = false;
	//it = generator->IRCodeFile.begin();
	//this->labelScan();
}
VirtualMachine::VirtualMachine(compiler::IRCodeGen *generator)
{
	programCounter = 0;
	reg_flag = false;
	this->generator = generator;
	it = generator->IRCodeFile.begin();
	this->labelScan();
}
VirtualMachine::VirtualMachine(compiler::IRCodeGen *generator, 
							   swd::SymbolTable *symTable)
{
	programCounter = 0;
	reg_flag = false;
	this->generator = generator;
	this->symTable = symTable;
	currentTable = symTable;
	it = generator->IRCodeFile.begin();
	this->labelScan();
}

void VirtualMachine::move()
{
	it++;
	programCounter=it - generator->IRCodeFile.begin();//��Զ������itͬ��
}

void VirtualMachine::initRuntimeInfo()
{
	rtInfo.lastScope = "";
	rtInfo.currentScope = "global";
	rtInfo.vStackItems = vStack.size();
	//rtInfo.varStackItems
	rtInfo.constStackItems=constStack.size();
	rtInfo.paramNum=0;
}

void VirtualMachine::labelScan()
{
	vector<shared_ptr<IRCode>>::iterator iter; 
	int pos=0;
	for (iter = this->generator->IRCodeFile.begin(); 
		 iter != this->generator->IRCodeFile.end(); ++iter)
	{
		pos++;
		if ((*iter)->_opType == OperationType::LABEL || (*iter)->_opType == OperationType::FUNC)
		{
			labelPos.insert(std::pair<string, int>((*iter)->_op1, pos));
		}
	}
}
/*
MOV,ADD,SUB,MUL,DIV,*^
CMP,*
JMP,JMPF,JE,JMPT,*
STORE,LOAD,*
PUSH,POP,*
EQ, UNEQ,*
LT,GT,^
LE,GE,^
AND,OR,^
LABEL,FUNC,PARAM,RET,CALL,*
IConst,FConst*
*/
void VirtualMachine::run()
{
	static bool isJmpOrCall=false;
	
#define GENE_CONSTITEM(TAG_TYPE)                                  \
    if ((*it)->Count == 3)                                        \
	{                                                             \
		StackItem sitem = { (*it)->_op1, (*it)->_op2, TAG_TYPE }; \
		constStack.push_back(sitem);                              \
	}else if ((*it)->Count == 2)                                  \
	{                                                             \
		StackItem sitem = { (*it)->_op1, "", TAG_TYPE };          \
		constStack.push_back(sitem);                              \
	}                                                             

	switch ((*it)->_opType)
	{
	case OperationType::IConst:
	{
		GENE_CONSTITEM(Tag::INT);
		break;
	}
	case OperationType::FConst:
	{
		GENE_CONSTITEM(Tag::FLOAT);
		break;
	}
	case OperationType::PUSH:
	{
		StackItem sitem = { "", (*it)->_op1, Tag::INT };
		vStack.push_back(sitem);
		break;
	}
	case OperationType::POP:
	{
		vStack.pop_back();
		break;
	}
	case OperationType::STORE:
	{
		StackItem aitem = vStack.back();
		vStack.pop_back();
		varStack.insert(std::pair<string, string>((*it)->_op1, aitem.value));
		break;
	}
	case OperationType::LOAD:
	{
		string varValue = varStack[(*it)->_op1];
		Node *varNode=currentTable->lookup((*it)->_op1);//ȷ����������
		Tag t = Tag::INT;
		if (varNode != NULL)
		{
			t = varNode->value.tag;
		}
		StackItem sitem = { (*it)->_op1, varValue, t };
		vStack.push_back(sitem);
		break;
	}
	case OperationType::ADD:
	{
		double result = 0;
		Tag resultType;
		if ((*it)->Count == 1)
		{
			//������������������Ǳ�������δ���.�˴��ü����ű�֮��Ҫ
			StackItem item1 = vStack.back();
			vStack.pop_back();
			StackItem item2 = vStack.back();
			vStack.pop_back();

			//�˴����ô���׺��ָ��ĺô�
			if (item1.type == Tag::INT && item2.type == Tag::INT)
			{
				resultType = Tag::INT;
			}
			else
			{
				resultType = Tag::FLOAT;
			}
			result = atof(item1.value.c_str()) + atof(item2.value.c_str());
			if (resultType == Tag::INT)
			{
				StackItem sitem = { item1.value+item2.value+"_result", to_string((int)result), Tag::INT };
				vStack.push_back(sitem);
			}
			else
			{
				StackItem sitem = { item1.value + item2.value + "_result", to_string(result), Tag::FLOAT };
				vStack.push_back(sitem);
			}
		}
		else if ((*it)->Count == 3)//һ����˵���������ʽ������forѭ�� add a 1
		{
			if (varStack.find((*it)->_op1) != varStack.end())
			{
				string val=varStack[(*it)->_op1];
				varStack[(*it)->_op1] = to_string(atoi(val.c_str()) + atoi((*it)->_op2.c_str()));
			}
		}
		break;
		
	}
	case OperationType::JMP:
	{
		isJmpOrCall = true;
		if (labelPos.find((*it)->_op1) != labelPos.end())
		{
			it = generator->IRCodeFile.begin() + labelPos[(*it)->_op1];
		}
		break;
	}
	case OperationType::JMPT:
	{
		isJmpOrCall = true;
		if (reg_flag && labelPos.find((*it)->_op1) != labelPos.end())
		{
			it = generator->IRCodeFile.begin() + labelPos[(*it)->_op1];
		}
		break;
	}
	case OperationType::JE:
	{
		isJmpOrCall = true;
		StackItem item1 = vStack.back();
		vStack.pop_back();
		StackItem item2 = vStack.back();
		vStack.pop_back();
		if (atof(item1.value.c_str()) - atof(item2.value.c_str()) == 0)//���ʱ����־λΪ��
		{
			reg_flag = true;
		}
		if (reg_flag && labelPos.find((*it)->_op1) != labelPos.end())
		{
			it = generator->IRCodeFile.begin() + labelPos[(*it)->_op1];
		}
		break;
	}
	case OperationType::JMPF:
	{
		isJmpOrCall = true;
		if (!reg_flag && labelPos.find((*it)->_op1) != labelPos.end())
		{
			it = generator->IRCodeFile.begin() + labelPos[(*it)->_op1];
		}
		break;
	}
	case OperationType::EQ:
	{
		StackItem item1 = vStack.back();
		vStack.pop_back();
		StackItem item2 = vStack.back();
		vStack.pop_back();
		if (item1.value == item2.value)
		{
			reg_flag = true;
		}
		break;
	}
	case OperationType::UNEQ:
	{
		StackItem item1 = vStack.back();
		vStack.pop_back();
		StackItem item2 = vStack.back();
		vStack.pop_back();
		if (item1.value != item2.value)
		{
			reg_flag = true;
		}
		break;
	}
	case OperationType::CMP:
	{
		StackItem item1 = vStack.back();
		vStack.pop_back();
		StackItem item2 = vStack.back();
		vStack.pop_back();
		if (atof(item1.value.c_str()) - atof(item2.value.c_str()) == 0)//���ʱ����־λΪ��
		{
			reg_flag = true;
		}
		break;
	}
	case OperationType::LABEL:
	{
		break;//nothing to do with label instruction
	}
	case OperationType::FUNC:
	{
		break;//nothing to do with FUNC instruction
	}
	case OperationType::PARAM:
	{
		//��Ȼ��Ҫ������ջ��TMD����ʱ��Ҫpopջ�е�ʵ�Σ�ʵ�δ�ʱ�����ǵ���
		StackItem item = vStack.back();
		vStack.pop_back();
		currentTable = currentTable->findInnerTable(rtInfo.currentScope);
		//Tag t = currentTable->lookupInScope((*it)->_op1)->value.tag;
		rtInfo.paramNum++;//�м���param����ʼ��ʱʵ���Ѿ����뵽��stack�ϣ�����retʱ��Ҫ��ʵ��Ҳɾ�����˴�����ȷ��ʵ�θ���
		varStack.insert(std::pair<string, string>((*it)->_op1, item.value));
		//distance ��iterator������
		rtInfo.varStackItems.push_back(distance(varStack.find((*it)->_op1),varStack.begin()));
		break;
	}
	case OperationType::RET:
	{
		isJmpOrCall = true;//retҲ����תָ��
		if (labelPos.find(rtInfo.currentScope+"_call") != labelPos.end())
		{
			it = generator->IRCodeFile.begin() + labelPos[(*it)->_op1];
		}
		rtInfo.lastScope = rtInfo.currentScope;
		rtInfo.currentScope = rtInfo.lastScope;
		int vStackClear= vStack.size() - rtInfo.vStackItems-rtInfo.paramNum;
		
		int constStackClear = constStack.size() - rtInfo.constStackItems;
		while (vStackClear>0 ||constStackClear>0)//clear stack
		{
			vStack.pop_back();
			//varStack.erase
			constStack.pop_back();
			vStackClear--;
		}
		while (rtInfo.varStackItems.size() > 0)//ɾ���¼ӵľֲ�����
		{
			int item = rtInfo.varStackItems.back();
			//map�Ƿ����Ե�������iterator����ֱ�ӼӼ�
			map<string,string>::iterator iter= varStack.begin();
			advance(iter, item);//ǰ��iterator
			varStack.erase(iter);
		}
		rtInfo.vStackItems=vStack.size();//�ָ�ջ֡
		currentTable = currentTable->outer;//�ָ����ű�
		break;
	}
	case OperationType::CALL:
	{
		isJmpOrCall = true;
		if (labelPos.find((*it)->_op1) != labelPos.end())
		{
			it = generator->IRCodeFile.begin() + labelPos[(*it)->_op1];
			labelPos.insert(std::pair<string, int>((*it)->_op1 + "_call", programCounter+1));
			rtInfo.lastScope = rtInfo.currentScope;
			rtInfo.currentScope = (*it)->_op1;
			rtInfo.vStackItems = vStack.size();
		}
		break;
	}
	}
	if (!isJmpOrCall)
	{
		move();
	}
	else
	{
		programCounter = it - generator->IRCodeFile.begin();
	}
}