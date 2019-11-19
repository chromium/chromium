// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

UploadDomAction::UploadDomAction(ActionDelegate* delegate,
                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_upload_dom());
}

UploadDomAction::~UploadDomAction() {}

void UploadDomAction::InternalProcessAction(ProcessActionCallback callback) {
  Selector selector = Selector(proto_.upload_dom().tree_root());
  if (selector.empty()) {
    DVLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    return;
  }
  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&UploadDomAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback), selector));
}

void UploadDomAction::OnWaitForElement(ProcessActionCallback callback,
                                       const Selector& selector,
                                       const ClientStatus& element_status) {
  if (!element_status.ok()) {
    UpdateProcessedAction(element_status.proto_status());
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate_->GetOuterHtml(
      selector,
      base::BindOnce(&UploadDomAction::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UploadDomAction::OnGetOuterHtml(ProcessActionCallback callback,
                                     const ClientStatus& status,
                                     const std::string& outer_html) {
  if (!status.ok()) {
    UpdateProcessedAction(status);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  processed_action_proto_->set_html_source(outer_html);
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
