// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

UploadDomAction::UploadDomAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_upload_dom());
}

UploadDomAction::~UploadDomAction() {}

void UploadDomAction::InternalProcessAction(ActionDelegate* delegate,
                                            ProcessActionCallback callback) {
  DCHECK_GT(proto_.upload_dom().tree_root().selectors_size(), 0);
  delegate->WaitForElement(
      ExtractSelectors(proto_.upload_dom().tree_root().selectors()),
      base::BindOnce(&UploadDomAction::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void UploadDomAction::OnWaitForElement(ActionDelegate* delegate,
                                       ProcessActionCallback callback,
                                       bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate->GetOuterHtml(
      ExtractSelectors(proto_.upload_dom().tree_root().selectors()),
      base::BindOnce(&UploadDomAction::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UploadDomAction::OnGetOuterHtml(ProcessActionCallback callback,
                                     bool successful,
                                     const std::string& outer_html) {
  if (!successful) {
    UpdateProcessedAction(OTHER_ACTION_STATUS);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  processed_action_proto_->set_html_source(outer_html);
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
