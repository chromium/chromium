// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/click_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
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
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class ClickActionTest : public testing::Test {
 public:
  ClickActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    ON_CALL(mock_action_delegate_, WaitUntilDocumentIsInReadyState(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    ON_CALL(mock_web_controller_, ScrollIntoView(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_web_controller_, WaitUntilElementIsStable(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    ON_CALL(mock_web_controller_, CheckOnTop(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_web_controller_, ClickOrTapElement(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_web_controller_, JsClickElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_click() = proto_;
    ClickAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Return an error status with details as created by
  // WebController::CheckOnTop.
  ClientStatus NotOnTopStatus() {
    ClientStatus not_on_top(ELEMENT_NOT_ON_TOP);
    not_on_top.mutable_details()
        ->mutable_web_controller_error_info()
        ->set_failed_web_action(WebControllerErrorInfoProto::ON_TOP);
    return not_on_top;
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ClickProto proto_;
};

TEST_F(ClickActionTest, EmptySelectorFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(ClickActionTest, CheckExpectedCallChain) {
  Selector expected_selector({"#click"});
  *proto_.mutable_element_to_click() = expected_selector.proto;
  proto_.set_click_type(ClickType::CLICK);

  ElementFinder::Result expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  InSequence seq;
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(mock_web_controller_,
              ScrollIntoView(true, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_web_controller_,
      WaitUntilElementIsStable(_, _, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(
      mock_web_controller_,
      ClickOrTapElement(ClickType::CLICK, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();
}

TEST_F(ClickActionTest, JavaScriptClickSkipsWaitForElementStable) {
  Selector expected_selector({"#click"});
  *proto_.mutable_element_to_click() = expected_selector.proto;
  proto_.set_click_type(ClickType::JAVASCRIPT);

  ElementFinder::Result expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(mock_web_controller_, WaitUntilElementIsStable(_, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_web_controller_, CheckOnTop(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_,
              JsClickElement(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ClickActionTest, SkipCheckOnTop) {
  Selector expected_selector({"#click"});
  *proto_.mutable_element_to_click() = expected_selector.proto;
  proto_.set_click_type(ClickType::TAP);
  proto_.set_on_top(SKIP_STEP);

  ElementFinder::Result expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(mock_web_controller_, CheckOnTop(_, _)).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ClickActionTest, RequireCheckOnTop) {
  Selector expected_selector({"#click"});
  *proto_.mutable_element_to_click() = expected_selector.proto;
  proto_.set_click_type(ClickType::TAP);
  proto_.set_on_top(REQUIRE_STEP_SUCCESS);

  ElementFinder::Result expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  InSequence seq;
  EXPECT_CALL(
      mock_web_controller_,
      WaitUntilElementIsStable(_, _, EqualsElement(expected_element), _));
  EXPECT_CALL(mock_web_controller_,
              CheckOnTop(EqualsElement(expected_element), _));
  EXPECT_CALL(
      mock_web_controller_,
      ClickOrTapElement(ClickType::TAP, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ClickActionTest, OptionalCheckOnTop) {
  Selector expected_selector({"#click"});
  *proto_.mutable_element_to_click() = expected_selector.proto;
  proto_.set_click_type(ClickType::TAP);
  proto_.set_on_top(REPORT_STEP_RESULT);

  ElementFinder::Result expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  InSequence seq;
  EXPECT_CALL(
      mock_web_controller_,
      WaitUntilElementIsStable(_, _, EqualsElement(expected_element), _));
  EXPECT_CALL(mock_web_controller_,
              CheckOnTop(EqualsElement(expected_element), _));
  EXPECT_CALL(
      mock_web_controller_,
      ClickOrTapElement(ClickType::TAP, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ClickActionTest, RequiredCheckOnTopFails) {
  Selector expected_selector({"#click"});
  *proto_.mutable_element_to_click() = expected_selector.proto;
  proto_.set_click_type(ClickType::TAP);
  proto_.set_on_top(REQUIRE_STEP_SUCCESS);

  ElementFinder::Result expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  // CheckOnTop fails.
  EXPECT_CALL(mock_web_controller_,
              CheckOnTop(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(NotOnTopStatus()));

  // The action must not tap.
  EXPECT_CALL(mock_web_controller_, ClickOrTapElement(_, _, _)).Times(0);

  ProcessedActionProto result;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&result));
  Run();

  EXPECT_EQ(ELEMENT_NOT_ON_TOP, result.status());
  EXPECT_EQ(
      WebControllerErrorInfoProto::ON_TOP,
      result.status_details().web_controller_error_info().failed_web_action());
}

TEST_F(ClickActionTest, OptionalCheckOnTopFails) {
  Selector expected_selector({"#click"});
  *proto_.mutable_element_to_click() = expected_selector.proto;
  proto_.set_click_type(ClickType::TAP);
  proto_.set_on_top(REPORT_STEP_RESULT);

  ElementFinder::Result expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  // CheckOnTop fails.
  EXPECT_CALL(mock_web_controller_,
              CheckOnTop(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(NotOnTopStatus()));

  // The action must tap anyway.
  EXPECT_CALL(
      mock_web_controller_,
      ClickOrTapElement(ClickType::TAP, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  ProcessedActionProto result;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&result));
  Run();

  EXPECT_EQ(ACTION_APPLIED, result.status());
  EXPECT_EQ(
      WebControllerErrorInfoProto::ON_TOP,
      result.status_details().web_controller_error_info().failed_web_action());
  EXPECT_EQ(ELEMENT_NOT_ON_TOP, result.status_details().original_status());
}

}  // namespace
}  // namespace autofill_assistant
