// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_handler_proxy.h"

#include "components/autofill/core/browser/autofill_provider.h"

namespace autofill {

using base::TimeTicks;

AutofillHandlerProxy::AutofillHandlerProxy(
    AutofillDriver* driver,
    AutofillClient* client,
    AutofillProvider* provider,
    AutofillHandler::AutofillDownloadManagerState enable_download_manager)
    : AutofillHandler(driver,
                      client,
                      enable_download_manager,
                      version_info::Channel::UNKNOWN),
      provider_(provider) {}

AutofillHandlerProxy::~AutofillHandlerProxy() {}

void AutofillHandlerProxy::OnFormSubmittedImpl(const FormData& form,
                                               bool known_success,
                                               mojom::SubmissionSource source) {
  provider_->OnFormSubmitted(this, form, known_success, source);
}

void AutofillHandlerProxy::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const TimeTicks timestamp) {
  provider_->OnTextFieldDidChange(this, form, field, bounding_box, timestamp);
}

void AutofillHandlerProxy::OnTextFieldDidScrollImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnTextFieldDidScroll(this, form, field, bounding_box);
}

void AutofillHandlerProxy::OnQueryFormFieldAutofillImpl(
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  provider_->OnQueryFormFieldAutofill(this, query_id, form, field, bounding_box,
                                      autoselect_first_suggestion);
}

void AutofillHandlerProxy::OnFocusOnFormFieldImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnFocusOnFormField(this, form, field, bounding_box);
}

void AutofillHandlerProxy::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnSelectControlDidChange(this, form, field, bounding_box);
}

bool AutofillHandlerProxy::ShouldParseForms(
    const std::vector<FormData>& forms) {
  provider_->OnFormsSeen(this, forms);
  // Need to parse the |forms| to FormStructure, so heuristic_type can be
  // retrieved later.
  return true;
}

void AutofillHandlerProxy::OnFocusNoLongerOnForm(bool had_interacted_form) {
  provider_->OnFocusNoLongerOnForm(this, had_interacted_form);
}

void AutofillHandlerProxy::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  provider_->OnDidFillAutofillFormData(this, form, timestamp);
}

void AutofillHandlerProxy::OnHidePopup() {
  provider_->OnHidePopup(this);
}

void AutofillHandlerProxy::SelectFieldOptionsDidChange(const FormData& form) {}

void AutofillHandlerProxy::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  has_server_prediction_ = true;
  provider_->OnServerPredictionsAvailable(this);
}

void AutofillHandlerProxy::OnServerRequestError(
    FormSignature form_signature,
    AutofillDownloadManager::RequestType request_type,
    int http_error) {
  provider_->OnServerQueryRequestError(this, form_signature);
}

void AutofillHandlerProxy::Reset() {
  AutofillHandler::Reset();
  has_server_prediction_ = false;
  provider_->Reset(this);
}

}  // namespace autofill
