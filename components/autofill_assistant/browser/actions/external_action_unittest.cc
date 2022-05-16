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

    ActionProto action_proto;
    *action_proto.mutable_external_action() = proto_;
    ExternalAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ExternalActionProto proto_;
};

TEST_F(ExternalActionTest, Success) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(RunOnceCallback<2>(
          ExternalActionDelegate::ActionResult{.success = true}));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, ExternalFailure) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(RunOnceCallback<2>(
          ExternalActionDelegate::ActionResult{.success = false}));
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              UNKNOWN_ACTION_STATUS))));
  Run();
}

TEST_F(ExternalActionTest, FailsIfProtoExtensionInfoNotSet) {
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, FailsIfDelegateDoesNotSupportExternalActions) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, SupportsExternalActions())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, ExternalActionWithInterrupts) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce([](const ExternalActionProto& external_action,
                   base::OnceCallback<void()> start_dom_checks_callback,
                   base::OnceCallback<void(ExternalActionDelegate::ActionResult
                                               result)> end_action_callback) {
        std::move(start_dom_checks_callback).Run();
        std::move(end_action_callback).Run({.success = true});
      });
  EXPECT_CALL(mock_action_delegate_, WaitForDom);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, ExternalActionWithoutInterrupts) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(false);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce([](const ExternalActionProto& external_action,
                   base::OnceCallback<void()> start_dom_checks_callback,
                   base::OnceCallback<void(ExternalActionDelegate::ActionResult
                                               result)> end_action_callback) {
        std::move(start_dom_checks_callback).Run();
        std::move(end_action_callback).Run({.success = true});
      });
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, DoesNotStartWaitForDomIfDomChecksAreNotRequested) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce([](const ExternalActionProto& external_action,
                   base::OnceCallback<void()> start_dom_checks_callback,
                   base::OnceCallback<void(ExternalActionDelegate::ActionResult
                                               result)> end_action_callback) {
        // We call the |end_action_callback| without calling
        // |start_dom_checks_callback|.
        std::move(end_action_callback).Run({.success = true});
      });
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
