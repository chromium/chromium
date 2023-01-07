// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_cast_action.h"

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
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArgPointee;

class ShowCastActionTest : public testing::Test {
 public:
  ShowCastActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_show_cast() = proto_;
    ShowCastAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowCastProto proto_;
};

TEST_F(ShowCastActionTest, EmptySelectorFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(ShowCastActionTest, ActionFailsForNonExistentElement) {
  InSequence sequence;

  Selector selector({"#focus"});
  *proto_.mutable_element_to_present() = selector.proto;

  Selector expected_selector = selector;
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(TIMED_OUT), base::Seconds(0)));
  EXPECT_CALL(mock_action_delegate_, StoreScrolledToElement(_)).Times(0);
  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_)).Times(0);

  EXPECT_CALL(callback_,
              Run(Pointee(Property(&ProcessedActionProto::status, TIMED_OUT))));
  Run();
}

TEST_F(ShowCastActionTest, CheckExpectedCallChain) {
  InSequence sequence;

  Selector expected_selector({"#focus"});
  *proto_.mutable_element_to_present() = expected_selector.proto;

  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(1)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), base::Seconds(2)));
  EXPECT_CALL(mock_action_delegate_,
              StoreScrolledToElement(EqualsElement(expected_element)));
  EXPECT_CALL(mock_web_controller_,
              ScrollToElementPosition(Eq(nullptr), _,
                                      EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.status(), ACTION_APPLIED);
  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 3000);
}

TEST_F(ShowCastActionTest, ScrollContainerIfSpecified) {
  InSequence sequence;

  Selector expected_selector({"#focus"});
  *proto_.mutable_element_to_present() = expected_selector.proto;
  Selector expected_container_selector({"#scrollable"});
  *proto_.mutable_container() = expected_container_selector.proto;

  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(1)));

  auto expected_container = test_util::MockFindElement(
      mock_action_delegate_, expected_container_selector);
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), base::Seconds(2)));
  EXPECT_CALL(mock_action_delegate_,
              StoreScrolledToElement(EqualsElement(expected_element)));
  EXPECT_CALL(
      mock_web_controller_,
      ScrollToElementPosition(Pointee(EqualsElement(expected_container)), _,
                              EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.status(), ACTION_APPLIED);
  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 3000);
}

TEST_F(ShowCastActionTest, WaitsForStableElementIfSpecified) {
  InSequence sequence;

  Selector expected_selector({"#focus"});
  *proto_.mutable_element_to_present() = expected_selector.proto;
  proto_.set_wait_for_stable_element(REQUIRE_STEP_SUCCESS);

  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(mock_web_controller_,
              ScrollIntoViewIfNeeded(true, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_web_controller_,
      WaitUntilElementIsStable(_, _, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), base::Seconds(1)));
  EXPECT_CALL(mock_action_delegate_,
              StoreScrolledToElement(EqualsElement(expected_element)));
  EXPECT_CALL(mock_web_controller_,
              ScrollToElementPosition(_, _, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.status(), ACTION_APPLIED);
  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 1000);
}

}  // namespace
}  // namespace autofill_assistant
