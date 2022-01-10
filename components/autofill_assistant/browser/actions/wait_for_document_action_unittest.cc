// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_document_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;

class WaitForDocumentActionTest : public testing::Test {
 public:
  WaitForDocumentActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_action_delegate_, WaitForDocumentReadyState(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(), DOCUMENT_COMPLETE,
                                          base::Seconds(0)));
  }

  // Runs the action defined in |proto_| and reports the result to
  // |processed_action_|.
  //
  // Once it has run, the result of the action is available in
  // |processed_action_|. Before the action has run, |processed_action_| status
  // is UNKNOWN_ACTION_STATUS.
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_wait_for_document() = proto_;
    action_ = std::make_unique<WaitForDocumentAction>(&mock_action_delegate_,
                                                      action_proto);
    action_->ProcessAction(base::BindOnce(base::BindLambdaForTesting(
        [&](std::unique_ptr<ProcessedActionProto> result) {
          LOG(ERROR) << "Got Processed action Result";
          processed_action_ = *result;
        })));
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  WaitForDocumentProto proto_;
  ProcessedActionProto processed_action_;

 private:
  std::unique_ptr<WaitForDocumentAction> action_;
};

TEST_F(WaitForDocumentActionTest, CheckOnceComplete) {
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_COMPLETE));
  proto_.set_timeout_ms(0);
  Run();
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(DOCUMENT_COMPLETE,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_COMPLETE,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, CheckOnceInteractive) {
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_INTERACTIVE));
  proto_.set_timeout_ms(0);
  Run();
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, CheckOnceLoading) {
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_LOADING));
  proto_.set_timeout_ms(0);
  Run();
  EXPECT_EQ(TIMED_OUT, processed_action_.status());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, CheckOnceRejectInteractive) {
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_INTERACTIVE));
  proto_.set_timeout_ms(0);
  proto_.set_min_ready_state(DOCUMENT_COMPLETE);
  Run();
  EXPECT_EQ(TIMED_OUT, processed_action_.status());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, CheckOnceAcceptLoading) {
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_LOADING));
  proto_.set_timeout_ms(0);
  proto_.set_min_ready_state(DOCUMENT_LOADING);
  Run();
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, WaitForDocumentInteractive) {
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_LOADING));
  EXPECT_CALL(mock_action_delegate_,
              WaitForDocumentReadyState(_, DOCUMENT_INTERACTIVE, _, _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), DOCUMENT_INTERACTIVE,
                                   base::Seconds(0)));
  proto_.set_timeout_ms(1000);
  Run();
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, WaitForDocumentInteractiveTimesOut) {
  InSequence sequence;

  // The first time the document is reported as loading.
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_LOADING));
  EXPECT_CALL(mock_action_delegate_,
              WaitForDocumentReadyState(_, DOCUMENT_COMPLETE, _, _))
      .WillOnce(RunOnceCallback<3>(ClientStatus(TIMED_OUT),
                                   DOCUMENT_UNKNOWN_READY_STATE,
                                   base::Seconds(0)));
  // The second time the document is reported interactive.
  EXPECT_CALL(mock_web_controller_, GetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_INTERACTIVE));

  proto_.set_timeout_ms(1000);
  proto_.set_min_ready_state(DOCUMENT_COMPLETE);
  Run();
  EXPECT_EQ(TIMED_OUT, processed_action_.status());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, CheckDocumentInFrame) {
  Selector expected_frame_selector({"#frame"});
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_frame_selector, _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(mock_web_controller_,
              GetDocumentReadyState(
                  EqualsElement(test_util::MockFindElement(
                      mock_action_delegate_, expected_frame_selector)),
                  _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_COMPLETE));

  proto_.set_timeout_ms(0);
  *proto_.mutable_frame() = ToSelectorProto("#frame");
  Run();
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
}

TEST_F(WaitForDocumentActionTest, CheckFrameElementNotFound) {
  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
      .WillRepeatedly(RunOnceCallback<1>(
          ClientStatus(ELEMENT_RESOLUTION_FAILED), base::Seconds(0)));

  proto_.set_timeout_ms(0);
  *proto_.mutable_frame() = ToSelectorProto("#frame");
  Run();
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, processed_action_.status());
}

}  // namespace
}  // namespace autofill_assistant
