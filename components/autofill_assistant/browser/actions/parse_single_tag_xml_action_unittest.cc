// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/parse_single_tag_xml_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

const char kDefaultXmlInput[] = "xml_input";
const char kDefaultInputClientMemoryKey[] = "input_client_memory_key";
const char kPersonIdFieldKey[] = "id";
const char kPersonIdFieldOutputClientMemoryKey[] = "person_id";
const char kPersonIdValue[] = "1234";
const char kPersonNameFieldKey[] = "name";
const char kPersonNameFieldOutputClientMemoryKey[] = "person_name";
const char kPersonNameValue[] = "XYZ";

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Return;

class ParseSingleTagXmlActionTest : public testing::Test {
 public:
  ParseSingleTagXmlActionTest() = default;

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_parse_single_tag_xml() = proto_;
    ParseSingleTagXmlAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Adds a field containing key and output client memory to the
  // ParseFlatXmlAction proto.
  void AddFieldToProto(const std::string& key,
                       const std::string& output_client_memory_key) {
    auto* field = proto_.add_fields();
    field->set_key(key);
    field->set_output_client_memory_key(output_client_memory_key);
  }

  // Adds default input client memory key to proto and updates |UserModel| with
  // the given XML.
  void AddXmlToClientMemoryAndUpdateProto(const std::string& xml_string) {
    proto_.set_input_client_memory_key(kDefaultInputClientMemoryKey);
    user_model_.SetValue(kDefaultInputClientMemoryKey,
                         SimpleValue(std::string(xml_string),
                                     /* is_client_side_only= */ true));
  }

  UserModel user_model_;
  UserData user_data_;
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ParseSingleTagXmlProto proto_;
};

TEST_F(ParseSingleTagXmlActionTest,
       FailsWhenFieldOutputClientMemoryKeyIsEmpty) {
  AddXmlToClientMemoryAndUpdateProto(kDefaultXmlInput);
  AddFieldToProto(kPersonIdFieldKey, "");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

TEST_F(ParseSingleTagXmlActionTest, FailsWhenFieldKeyIsEmpty) {
  AddXmlToClientMemoryAndUpdateProto(kDefaultXmlInput);
  AddFieldToProto("", kPersonIdFieldOutputClientMemoryKey);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

TEST_F(ParseSingleTagXmlActionTest, FailsWhenXmlIsEmpty) {
  AddXmlToClientMemoryAndUpdateProto("");
  AddFieldToProto(kPersonIdFieldKey, kPersonIdFieldOutputClientMemoryKey);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  Run();
}

TEST_F(ParseSingleTagXmlActionTest, SuccessfullyExtractsXmlValues) {
  std::vector<std::string> output_values;
  AddXmlToClientMemoryAndUpdateProto(kDefaultXmlInput);
  AddFieldToProto(kPersonIdFieldKey, kPersonIdFieldOutputClientMemoryKey);
  output_values.push_back(kPersonIdValue);
  AddFieldToProto(kPersonNameFieldKey, kPersonNameFieldOutputClientMemoryKey);
  output_values.push_back(kPersonNameValue);

  InSequence seq;
  EXPECT_CALL(mock_action_delegate_, IsXmlSigned)
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_action_delegate_, ExtractValuesFromSingleTagXml)
      .Times(1)
      .WillOnce(Return(output_values));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();

  EXPECT_EQ(*user_model_.GetValue(kPersonIdFieldOutputClientMemoryKey),
            SimpleValue(std::string(kPersonIdValue),
                        /* is_client_side_only= */ true));
  EXPECT_EQ(*user_model_.GetValue(kPersonNameFieldOutputClientMemoryKey),
            SimpleValue(std::string(kPersonNameValue),
                        /* is_client_side_only= */ true));
}

TEST_F(ParseSingleTagXmlActionTest, FailsWhenAllValuesAreNotExtracted) {
  std::vector<std::string> output_values;
  AddXmlToClientMemoryAndUpdateProto(kDefaultXmlInput);
  AddFieldToProto(kPersonIdFieldKey, kPersonIdFieldOutputClientMemoryKey);
  output_values.push_back(kPersonIdValue);
  AddFieldToProto(kPersonNameFieldKey, kPersonNameFieldOutputClientMemoryKey);

  InSequence seq;
  EXPECT_CALL(mock_action_delegate_, IsXmlSigned)
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_action_delegate_, ExtractValuesFromSingleTagXml)
      .Times(1)
      .WillOnce(Return(output_values));
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              XML_PARSE_INCORRECT_DATA))));

  Run();
}

TEST_F(ParseSingleTagXmlActionTest, FailsWhenXmlIsSigned) {
  AddXmlToClientMemoryAndUpdateProto("1234567890");
  AddFieldToProto(kPersonIdFieldKey, kPersonIdFieldOutputClientMemoryKey);

  InSequence seq;
  EXPECT_CALL(mock_action_delegate_, IsXmlSigned)
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              XML_PARSE_SIGNED_DATA))));

  Run();
}

}  // namespace
}  // namespace autofill_assistant
