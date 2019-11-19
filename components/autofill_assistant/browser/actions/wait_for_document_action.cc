// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_document_action.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

WaitForDocumentAction::WaitForDocumentAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_wait_for_document());
}

WaitForDocumentAction::~WaitForDocumentAction() {}

void WaitForDocumentAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  Selector selector(proto_.wait_for_document().frame());
  if (selector.empty()) {
    // No element to wait for.
    OnShortWaitForElement(ClientStatus(ACTION_APPLIED));
    return;
  }
  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&WaitForDocumentAction::OnShortWaitForElement,
                               weak_ptr_factory_.GetWeakPtr()));
}

void WaitForDocumentAction::OnShortWaitForElement(
    const ClientStatus& element_status) {
  if (!element_status.ok()) {
    SendResult(element_status, DOCUMENT_UNKNOWN_READY_STATE);
    return;
  }
  delegate_->GetDocumentReadyState(
      Selector(proto_.wait_for_document().frame()),
      base::BindOnce(&WaitForDocumentAction::OnGetStartState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WaitForDocumentAction::OnGetStartState(const ClientStatus& status,
                                            DocumentReadyState start_state) {
  if (!status.ok()) {
    SendResult(status, DOCUMENT_UNKNOWN_READY_STATE);
    return;
  }

  processed_action_proto_->mutable_wait_for_document_result()
      ->set_start_ready_state(start_state);

  if (start_state >= proto_.wait_for_document().min_ready_state()) {
    SendResult(OkClientStatus(), start_state);
    return;
  }

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      proto_.wait_for_document().timeout_ms());
  if (timeout.is_zero()) {
    SendResult(ClientStatus(TIMED_OUT), start_state);
    return;
  }

  timer_.Start(FROM_HERE, timeout,
               base::BindOnce(&WaitForDocumentAction::OnTimeout,
                              weak_ptr_factory_.GetWeakPtr()));
  delegate_->WaitForDocumentReadyState(
      Selector(proto_.wait_for_document().frame()),
      proto_.wait_for_document().min_ready_state(),
      base::BindOnce(&WaitForDocumentAction::OnWaitForStartState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WaitForDocumentAction::OnWaitForStartState(
    const ClientStatus& status,
    DocumentReadyState current_state) {
  SendResult(status, current_state);
}

void WaitForDocumentAction::OnTimeout() {
  // We've already returned successfully.
  if (!callback_)
    return;

  delegate_->GetDocumentReadyState(
      Selector(proto_.wait_for_document().frame()),
      base::BindOnce(&WaitForDocumentAction::OnTimeoutInState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WaitForDocumentAction::OnTimeoutInState(const ClientStatus& status,
                                             DocumentReadyState end_state) {
  DVLOG_IF(1, !status.ok())
      << __func__ << ": cannot report end_state because of " << status;
  SendResult(ClientStatus(TIMED_OUT), end_state);
}

void WaitForDocumentAction::SendResult(const ClientStatus& status,
                                       DocumentReadyState end_state) {
  // Only send a result for the first of OnTimeout or OnSuccess that finishes.
  if (!callback_)
    return;

  processed_action_proto_->mutable_wait_for_document_result()
      ->set_end_ready_state(end_state);
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
