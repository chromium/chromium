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
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;

class ClickActionTest : public testing::Test {
 public:
  ClickActionTest() {}

  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_click() = proto_;
    ClickAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
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
  InSequence sequence;

  Selector selector({"#click"});
  *proto_.mutable_element_to_click() = selector.proto;
  proto_.set_click_type(ClickType::CLICK);

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
              ScrollIntoView(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::CLICK, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
