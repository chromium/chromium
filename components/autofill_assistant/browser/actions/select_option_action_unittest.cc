// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/select_option_action.h"

#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class SelectOptionActionTest : public testing::Test {
 public:
  SelectOptionActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));

    proto_.set_option_comparison_attribute(SelectOptionProto::VALUE);
  }

 protected:
  void Run() {
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));

    ActionProto action_proto;
    *action_proto.mutable_select_option() = proto_;
    SelectOptionAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  SelectOptionProto proto_;
  UserData user_data_;
  UserModel user_model_;
};

TEST_F(SelectOptionActionTest, NoValueToSelectFails) {
  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(SelectOptionActionTest, EmptyTextFilterValueFails) {
  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_text_filter_value()->set_re2("");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(SelectOptionActionTest, EmptyAutofillValueFails) {
  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_autofill_regexp_value();
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(SelectOptionActionTest, EmptySelectorFails) {
  proto_.mutable_text_filter_value()->set_re2("option");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(SelectOptionActionTest, MissingOptionComparisonAttributeFails) {
  Selector selector({"select"});
  *proto_.mutable_element() = selector.proto;
  proto_.set_option_comparison_attribute(SelectOptionProto::NOT_SET);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(SelectOptionActionTest, CheckExpectedCallChain) {
  InSequence sequence;

  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_text_filter_value()->set_re2("option");
  proto_.set_strict(true);

  Selector expected_selector = selector;
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_web_controller_,
              SelectOption("option", false, SelectOptionProto::VALUE, true,
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<5>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(SelectOptionActionTest, RequestDataFromUnknownProfile) {
  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  auto* value = proto_.mutable_autofill_regexp_value();
  value->mutable_profile()->set_identifier("none");
  value->mutable_value_expression_re2()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_text("value");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  Run();
}

TEST_F(SelectOptionActionTest, RequestUnknownDataFromProfile) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  // Middle name is expected to be empty.
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");

  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  auto* value = proto_.mutable_autofill_regexp_value();
  value->mutable_profile()->set_identifier("contact");
  value->mutable_value_expression_re2()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_key(static_cast<int>(autofill::ServerFieldType::NAME_MIDDLE));

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              AUTOFILL_INFO_NOT_AVAILABLE))));
  Run();
}

TEST_F(SelectOptionActionTest, SelectOptionFromProfileValue) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe", "", "", "", "",
                                 "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  InSequence sequence;

  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  auto* value = proto_.mutable_autofill_regexp_value();
  value->mutable_profile()->set_identifier("contact");
  value->mutable_value_expression_re2()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_key(static_cast<int>(autofill::ServerFieldType::NAME_FIRST));

  Selector expected_selector = selector;
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(mock_web_controller_,
              SelectOption("John", false, SelectOptionProto::VALUE, false,
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, expected_selector)),
                           _))
      .WillOnce(RunOnceCallback<5>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(SelectOptionActionTest, SelectRegularExpressionValue) {
  InSequence sequence;

  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_text_filter_value()->set_re2("^option$");
  proto_.mutable_text_filter_value()->set_case_sensitive(true);

  Selector expected_selector = selector;
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_web_controller_,
              SelectOption("^option$", true, SelectOptionProto::VALUE, false,
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<5>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(SelectOptionActionTest, EscapeRegularExpressionAutofillValue) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe", "", "", "", "",
                                 "", "", "", "", "+41791234567");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  InSequence sequence;

  Selector selector({"#select"});
  *proto_.mutable_element() = selector.proto;
  auto* value = proto_.mutable_autofill_regexp_value();
  value->mutable_profile()->set_identifier("contact");
  *value->mutable_value_expression_re2()->mutable_value_expression() =
      test_util::ValueExpressionBuilder()
          .addChunk("^")
          .addChunk(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER)
          .addChunk("$")
          .toProto();
  value->mutable_value_expression_re2()->set_case_sensitive(true);

  Selector expected_selector = selector;
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(
      mock_web_controller_,
      SelectOption("^\\+41791234567$", true, SelectOptionProto::VALUE, false,
                   EqualsElement(test_util::MockFindElement(
                       mock_action_delegate_, expected_selector)),
                   _))
      .WillOnce(RunOnceCallback<5>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
