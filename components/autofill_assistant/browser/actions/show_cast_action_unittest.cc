// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_cast_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SaveArgPointee;

class ShowCastActionTest : public testing::Test {
 public:
  ShowCastActionTest() {}

  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_show_cast() = proto_;
    ShowCastAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
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
      .WillOnce(RunOnceCallback<1>(ClientStatus(TIMED_OUT),
                                   base::TimeDelta::FromSeconds(0)));
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
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(1)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(2)));
  EXPECT_CALL(mock_action_delegate_,
              ScrollToElementPosition(expected_selector, _, Eq(nullptr),
                                      EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));
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
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(1)));

  auto expected_container = test_util::MockFindElement(
      mock_action_delegate_, expected_container_selector);
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(2)));
  EXPECT_CALL(
      mock_action_delegate_,
      ScrollToElementPosition(expected_selector, _,
                              Pointee(EqualsElement(expected_container)),
                              EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));
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
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(
      mock_action_delegate_,
      WaitUntilElementIsStable(_, _, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(1)));
  EXPECT_CALL(mock_action_delegate_,
              ScrollToElementPosition(expected_selector, _, _,
                                      EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.status(), ACTION_APPLIED);
  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 1000);
}

TEST_F(ShowCastActionTest, SetsTitleIfSpecified) {
  ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(),
                                        base::TimeDelta::FromSeconds(0)));
  test_util::MockFindAnyElement(mock_action_delegate_);
  ON_CALL(mock_action_delegate_, WaitUntilDocumentIsInReadyState(_, _, _, _))
      .WillByDefault(RunOnceCallback<3>(OkClientStatus(),
                                        base::TimeDelta::FromSeconds(0)));
  ON_CALL(mock_action_delegate_, ScrollToElementPosition(_, _, _, _, _))
      .WillByDefault(RunOnceCallback<4>(OkClientStatus()));

  Selector selector({"#focus"});
  *proto_.mutable_element_to_present() = selector.proto;
  proto_.set_title("Title");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage("Title"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
