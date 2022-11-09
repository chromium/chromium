// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/wait_for_document_operation.h"

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

WaitForDocumentOperation::WaitForDocumentOperation(
    ScriptExecutorDelegate* script_executor_delegate,
    base::TimeDelta max_wait_time,
    DocumentReadyState min_ready_state,
    const ElementFinderResult& optional_frame_element,
    WaitForDocumentOperation::Callback callback)
    : script_executor_delegate_(script_executor_delegate),
      max_wait_time_(max_wait_time),
      min_ready_state_(min_ready_state),
      optional_frame_element_(optional_frame_element),
      callback_(std::move(callback)) {}

WaitForDocumentOperation::~WaitForDocumentOperation() = default;

void WaitForDocumentOperation::Run() {
  timer_.Start(
      FROM_HERE, max_wait_time_,
      base::BindOnce(&WaitForDocumentOperation::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
  script_executor_delegate_->GetWebController()->WaitForDocumentReadyState(
      *optional_frame_element_, min_ready_state_,
      base::BindOnce(&WaitForDocumentOperation::OnWaitForState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WaitForDocumentOperation::OnTimeout(base::TimeTicks wait_time_start) {
  if (!callback_) {
    // Callback already ran successfully.
    return;
  }

  ClientStatus timed_out(TIMED_OUT);
  timed_out.mutable_details()
      ->mutable_web_controller_error_info()
      ->set_failed_web_action(
          WebControllerErrorInfoProto::WAIT_FOR_DOCUMENT_READY_STATE);
  std::move(callback_).Run(timed_out, DOCUMENT_UNKNOWN_READY_STATE,
                           base::TimeTicks::Now() - wait_time_start);
}

void WaitForDocumentOperation::OnWaitForState(const ClientStatus& status,
                                              DocumentReadyState current_state,
                                              base::TimeDelta wait_time) {
  if (!callback_) {
    // Callback already ran through timeout.
    return;
  }

  std::move(callback_).Run(status, current_state, wait_time);
}

}  // namespace autofill_assistant
