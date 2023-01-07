// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/wait_for_document_operation.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Property;

class WaitForDocumentOperationTest : public testing::Test {
 public:
  WaitForDocumentOperationTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    fake_script_executor_delegate_.SetWebController(&mock_web_controller_);

    wait_for_document_operation_ = std::make_unique<WaitForDocumentOperation>(
        &fake_script_executor_delegate_, base::Seconds(1), DOCUMENT_COMPLETE,
        ElementFinderResult(), mock_callback_.Get());
  }

 protected:
  // task_env_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_env_;

  FakeScriptExecutorDelegate fake_script_executor_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<WaitForDocumentOperation::Callback> mock_callback_;
  std::unique_ptr<WaitForDocumentOperation> wait_for_document_operation_;
};

TEST_F(WaitForDocumentOperationTest, ReportsSuccess) {
  EXPECT_CALL(mock_web_controller_,
              WaitForDocumentReadyState(_, DOCUMENT_COMPLETE, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), DOCUMENT_COMPLETE,
                                   base::Seconds(0)));
  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _));

  wait_for_document_operation_->Run();
}

TEST_F(WaitForDocumentOperationTest, ReportsFailure) {
  EXPECT_CALL(mock_web_controller_,
              WaitForDocumentReadyState(_, DOCUMENT_COMPLETE, _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(TIMED_OUT),
                                   DOCUMENT_UNKNOWN_READY_STATE,
                                   base::Seconds(0)));
  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, TIMED_OUT), _, _));

  wait_for_document_operation_->Run();
}

TEST_F(WaitForDocumentOperationTest, TimesOutAfterWaiting) {
  // Capture the call without answering it.
  WaitForDocumentOperation::Callback captured_callback;
  EXPECT_CALL(mock_web_controller_,
              WaitForDocumentReadyState(_, DOCUMENT_COMPLETE, _))
      .WillOnce(Invoke([&captured_callback](
                           const ElementFinderResult& optional_frame_element,
                           DocumentReadyState min_ready_state,
                           WaitForDocumentOperation::Callback callback) {
        captured_callback = std::move(callback);
      }));
  EXPECT_CALL(mock_callback_, Run(_, _, _)).Times(0);
  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, TIMED_OUT), _, _));

  wait_for_document_operation_->Run();
  task_env_.FastForwardBy(base::Seconds(2));

  // This callback should be ignored, it's too late. This should not report a
  // success or crash.
  std::move(captured_callback)
      .Run(OkClientStatus(), DOCUMENT_COMPLETE, base::Seconds(2));
}

TEST_F(WaitForDocumentOperationTest, TimeoutIsIgnoredAfterSuccess) {
  EXPECT_CALL(mock_web_controller_,
              WaitForDocumentReadyState(_, DOCUMENT_COMPLETE, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), DOCUMENT_COMPLETE,
                                   base::Seconds(0)));
  EXPECT_CALL(mock_callback_, Run(_, _, _)).Times(0);
  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _));

  wait_for_document_operation_->Run();

  // Moving forward in time causes the timer to expire. This should not report
  // a failure or crash.
  task_env_.FastForwardBy(base::Seconds(2));
}

}  // namespace
}  // namespace autofill_assistant
