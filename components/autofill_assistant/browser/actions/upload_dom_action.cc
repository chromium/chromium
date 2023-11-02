// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

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
  delegate_->ShortWaitForElementWithSlowWarning(
      selector,
      base::BindOnce(
          &UploadDomAction::OnWaitForElementTimed,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&UploadDomAction::OnWaitForElement,
                         weak_ptr_factory_.GetWeakPtr(), selector,
                         proto_.upload_dom().can_match_multiple_elements(),
                         proto_.upload_dom().include_all_inner_text())));
}

void UploadDomAction::OnWaitForElement(const Selector& selector,
                                       bool can_match_multiple_elements,
                                       bool include_all_inner_text,
                                       const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  if (can_match_multiple_elements) {
    delegate_->FindAllElements(
        selector,
        base::BindOnce(
            &element_action_util::TakeElementAndGetProperty<
                const std::vector<std::string>&>,
            base::BindOnce(&WebController::GetOuterHtmls,
                           delegate_->GetWebController()->GetWeakPtr(),
                           include_all_inner_text),
            std::vector<std::string>(),
            base::BindOnce(&UploadDomAction::OnGetOuterHtmls,
                           weak_ptr_factory_.GetWeakPtr())));
    return;
  }

  delegate_->FindElement(
      selector,
      base::BindOnce(
          &element_action_util::TakeElementAndGetProperty<const std::string&>,
          base::BindOnce(&WebController::GetOuterHtml,
                         delegate_->GetWebController()->GetWeakPtr(),
                         include_all_inner_text),
          std::string(),
          base::BindOnce(&UploadDomAction::OnGetOuterHtml,
                         weak_ptr_factory_.GetWeakPtr())));
}

void UploadDomAction::OnGetOuterHtml(const ClientStatus& status,
                                     const std::string& outer_html) {
  if (status.ok()) {
    processed_action_proto_->mutable_upload_dom_result()->add_outer_htmls(
        outer_html);
  }
  EndAction(status);
}

void UploadDomAction::OnGetOuterHtmls(
    const ClientStatus& status,
    const std::vector<std::string>& outer_htmls) {
  if (status.ok()) {
    auto* result = processed_action_proto_->mutable_upload_dom_result();
    for (const auto& outer_html : outer_htmls) {
      result->add_outer_htmls(outer_html);
    }
  }

  EndAction(status);
}

void UploadDomAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
