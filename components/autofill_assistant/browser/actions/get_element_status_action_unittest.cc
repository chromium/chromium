// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_element_status_action.h"

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
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

const char kValue[] = "Some Value";

class GetElementStatusActionTest : public testing::Test {
 public:
  GetElementStatusActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    test_util::MockFindAnyElement(mock_action_delegate_);
    ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus(), kValue));

    proto_.set_value_source(GetElementStatusProto::VALUE);
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_get_element_status() = proto_;
    GetElementStatusAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  GetElementStatusProto proto_;
  UserData user_data_;
};

TEST_F(GetElementStatusActionTest, EmptySelectorFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionFailsForNonExistentElement) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      kValue);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(TIMED_OUT),
                                   base::TimeDelta::FromSeconds(0)));

  EXPECT_CALL(callback_,
              Run(Pointee(Property(&ProcessedActionProto::status, TIMED_OUT))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionReportsAllVariations) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      kValue);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::reports,
                             SizeIs(4))))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionFailsForMismatchIfRequired) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "other");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ELEMENT_MISMATCH),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             false)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForMismatchIfAllowed) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "other");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             false)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForNoExpectation) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "other");
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::get_element_status_result,
                   AllOf(Property(&GetElementStatusProto::Result::not_empty,
                                  true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveFullMatch) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      kValue);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, selector);
  EXPECT_CALL(mock_web_controller_,
              GetStringAttribute(ElementsAre("value"),
                                 EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), kValue));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveContains) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "me Va");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_contains(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveStartsWith) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "Some");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_starts_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveEndsWith) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "Value");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_ends_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseInsensitiveFullMatch) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "sOmE vAlUe");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(false);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForFullMatchWithoutSpaces) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      "S o m eV a l u e");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_remove_space(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, EmptyTextForEmptyValueIsSuccess) {
  ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(OkClientStatus(), std::string()));

  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      std::string());
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(
                  Property(&GetElementStatusProto::Result::not_empty, false),
                  Property(&GetElementStatusProto::Result::match_success, true),
                  Property(&GetElementStatusProto::Result::expected_empty_match,
                           true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, InnerTextLookupSuccess) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value(
      kValue);
  proto_.set_value_source(GetElementStatusProto::INNER_TEXT);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, selector);
  EXPECT_CALL(mock_web_controller_,
              GetStringAttribute(ElementsAre("innerText"),
                                 EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), kValue));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, MatchingValueWithRegexpCaseSensitive) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("Valu.");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_ends_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, MatchingValueWithRegexpCaseInsensitive) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("vAlU.");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(false);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_ends_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionFailsForRegexMismatchIfRequired) {
  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("none");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ELEMENT_MISMATCH),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             false)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, EmptyRegexpForEmptyValueIsSuccess) {
  ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(OkClientStatus(), std::string()));

  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("^$");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(
                  Property(&GetElementStatusProto::Result::not_empty, false),
                  Property(&GetElementStatusProto::Result::match_success, true),
                  Property(&GetElementStatusProto::Result::expected_empty_match,
                           true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, BlankTextWithRemovingSpacesIsExpectedEmpty) {
  ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(OkClientStatus(), "   "));

  Selector selector({"#element"});
  *proto_.mutable_element() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_value("   ");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_remove_space(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(
                  // The field is not empty (it is blank), but the match is
                  // still a success and expects to be empty given the
                  // configuration.
                  Property(&GetElementStatusProto::Result::not_empty, true),
                  Property(&GetElementStatusProto::Result::match_success, true),
                  Property(&GetElementStatusProto::Result::expected_empty_match,
                           true)))))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
