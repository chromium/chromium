// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_document_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

class WaitForDocumentActionTest : public testing::Test {
 public:
  WaitForDocumentActionTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, OnWaitForDocumentReadyState(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus(), DOCUMENT_COMPLETE));
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
  // task_env_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_env_;

  MockActionDelegate mock_action_delegate_;
  WaitForDocumentProto proto_;
  ProcessedActionProto processed_action_;
  std::unique_ptr<WaitForDocumentAction> action_;
};

TEST_F(WaitForDocumentActionTest, CheckOnceComplete) {
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
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
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
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
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
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
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
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
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
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
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_LOADING));
  EXPECT_CALL(mock_action_delegate_,
              OnWaitForDocumentReadyState(_, DOCUMENT_INTERACTIVE, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), DOCUMENT_INTERACTIVE));
  proto_.set_timeout_ms(1000);
  Run();
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().end_ready_state());
}

TEST_F(WaitForDocumentActionTest, WaitForDocumentInteractiveTimesOut) {
  // The first time the document is reported as loading.
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_LOADING));

  // The document doesn't become complete right away.
  base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
      captured_callback;
  EXPECT_CALL(mock_action_delegate_,
              OnWaitForDocumentReadyState(_, DOCUMENT_COMPLETE, _))
      .WillOnce(Invoke(
          [&captured_callback](
              const Selector& frame, DocumentReadyState min_ready_state,
              base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>&
                  callback) { captured_callback = std::move(callback); }));

  proto_.set_timeout_ms(1000);
  proto_.set_min_ready_state(DOCUMENT_COMPLETE);
  Run();

  // 1s afterwards, the document has become interactive, but not complete. The
  // action times out and reports that.
  EXPECT_CALL(mock_action_delegate_, OnGetDocumentReadyState(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_INTERACTIVE));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(TIMED_OUT, processed_action_.status());
  EXPECT_EQ(DOCUMENT_LOADING,
            processed_action_.wait_for_document_result().start_ready_state());
  EXPECT_EQ(DOCUMENT_INTERACTIVE,
            processed_action_.wait_for_document_result().end_ready_state());

  // This callback should be ignored. It's too late. This should not crash.
  std::move(captured_callback).Run(OkClientStatus(), DOCUMENT_COMPLETE);
}

TEST_F(WaitForDocumentActionTest, CheckDocumentInFrame) {
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(Selector({"#frame"}), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));

  EXPECT_CALL(mock_action_delegate_,
              OnGetDocumentReadyState(Selector({"#frame"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), DOCUMENT_COMPLETE));

  proto_.set_timeout_ms(0);
  proto_.mutable_frame()->add_selectors("#frame");
  Run();
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
}

TEST_F(WaitForDocumentActionTest, CheckFrameElementNotFound) {
  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
      .WillRepeatedly(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED)));

  proto_.set_timeout_ms(0);
  proto_.mutable_frame()->add_selectors("#frame");
  Run();
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, processed_action_.status());
}

}  // namespace
}  // namespace autofill_assistant
