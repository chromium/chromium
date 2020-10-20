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
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;

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
  expected_selector.MustBeVisible();
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(TIMED_OUT)));
  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_)).Times(0);

  EXPECT_CALL(callback_,
              Run(Pointee(Property(&ProcessedActionProto::status, TIMED_OUT))));
  Run();
}

TEST_F(ShowCastActionTest, CheckExpectedCallChain) {
  InSequence sequence;

  Selector selector({"#focus"});
  *proto_.mutable_element_to_present() = selector.proto;

  Selector expected_selector = selector;
  expected_selector.MustBeVisible();
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_action_delegate_, WaitForDocumentToBecomeInteractive(
                                         EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_,
              ScrollToElementPosition(expected_selector, _,
                                      EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowCastActionTest, SetsTitleIfSpecified) {
  ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
  test_util::MockFindAnyElement(mock_action_delegate_);
  ON_CALL(mock_action_delegate_, WaitForDocumentToBecomeInteractive(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
  ON_CALL(mock_action_delegate_, ScrollToElementPosition(_, _, _, _))
      .WillByDefault(RunOnceCallback<3>(OkClientStatus()));

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
