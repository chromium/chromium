// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

UploadDomAction::UploadDomAction(ActionDelegate* delegate,
                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_upload_dom());
}

UploadDomAction::~UploadDomAction() {}

void UploadDomAction::InternalProcessAction(ProcessActionCallback callback) {
  process_action_callback_ = std::move(callback);

  Selector selector = Selector(proto_.upload_dom().tree_root());
  if (selector.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }
  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&UploadDomAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(), selector));
}

void UploadDomAction::OnWaitForElement(const Selector& selector,
                                       const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  action_delegate_util::FindElementAndGetProperty(
      delegate_, selector,
      base::BindOnce(&ActionDelegate::GetOuterHtml, delegate_->GetWeakPtr()),
      base::BindOnce(&UploadDomAction::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UploadDomAction::OnGetOuterHtml(const ClientStatus& status,
                                     const std::string& outer_html) {
  if (status.ok()) {
    processed_action_proto_->set_html_source(outer_html);
  }
  EndAction(status);
}

void UploadDomAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
