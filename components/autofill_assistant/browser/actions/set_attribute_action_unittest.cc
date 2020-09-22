// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_attribute_action.h"

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

class SetAttributeActionTest : public testing::Test {
 public:
  SetAttributeActionTest() {}

  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_set_attribute() = proto_;
    SetAttributeAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  SetAttributeProto proto_;
};

TEST_F(SetAttributeActionTest, EmptySelectorFails) {
  proto_.add_attribute("value");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(SetAttributeActionTest, CheckExpectedCallChain) {
  InSequence sequence;

  Selector selector({"#input"});
  *proto_.mutable_element() = selector.proto;
  proto_.add_attribute("value");
  proto_.set_value("Hello World");

  Selector expected_selector = selector;
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  std::vector<std::string> expected_attributes = {"value"};
  EXPECT_CALL(mock_action_delegate_,
              SetAttribute(expected_attributes, "Hello World",
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
