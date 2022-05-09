// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/external_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Property;
using ::testing::Return;

class ExternalActionTest : public testing::Test {
 protected:
  void Run() {
    ON_CALL(mock_action_delegate_, SupportsExternalActions)
        .WillByDefault(Return(true));

    CreateAction().ProcessAction(callback_.Get());
  }

  ExternalAction CreateAction() {
    ActionProto action_proto;
    *action_proto.mutable_external_action() = proto_;
    return ExternalAction(&mock_action_delegate_, action_proto);
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ExternalActionProto proto_;
};

TEST_F(ExternalActionTest, Success) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          RunOnceCallback<1>(ExternalActionDelegate::ActionResult({true})));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, ExternalFailure) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          RunOnceCallback<1>(ExternalActionDelegate::ActionResult({false})));
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              UNKNOWN_ACTION_STATUS))));
  Run();
}

TEST_F(ExternalActionTest, RequiresInfo) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, Unsupported) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, SupportsExternalActions())
      .WillOnce(Return(false));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, AllowsInterrupts) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);
  base::OnceCallback<void(ExternalActionDelegate::ActionResult result)>
      external_action_callback;

  EXPECT_CALL(mock_action_delegate_, SupportsExternalActions)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [&external_action_callback](
              const ExternalActionProto& external_action,
              base::OnceCallback<void(
                  ExternalActionDelegate::ActionResult result)> callback) {
            // We save the callback and call it later to simulate the external
            // action not finishing right away (e.g. it's waiting for user input
            // or a DOM condition).
            external_action_callback = std::move(callback);
          });
  EXPECT_CALL(mock_action_delegate_, WaitForDom);

  // Explicitly creating the action here instead of calling |Run| to have it
  // survive long enough for the callback to be called.
  ExternalAction action = CreateAction();
  action.ProcessAction(callback_.Get());

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  std::move(external_action_callback).Run({true});
}

TEST_F(ExternalActionTest,
       DoesNotWaitForDomIfExternalActionReturnsImmediately) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          RunOnceCallback<1>(ExternalActionDelegate::ActionResult({true})));
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
