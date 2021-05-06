// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/android_autofill_manager.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_provider.h"

namespace autofill {

using base::TimeTicks;

// static
std::unique_ptr<AutofillManager> AndroidAutofillManager::Create(
    AutofillProvider* provider,
    AutofillDriver* driver,
    AutofillClient* client,
    const std::string& /*app_locale*/,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  return base::WrapUnique(new AndroidAutofillManager(driver, client, provider,
                                                     enable_download_manager));
}

AndroidAutofillManager::AndroidAutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    AutofillProvider* provider,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : AutofillManager(driver,
                      client,
                      enable_download_manager,
                      version_info::Channel::UNKNOWN),
      provider_(provider) {}

AndroidAutofillManager::~AndroidAutofillManager() = default;

void AndroidAutofillManager::OnFormSubmittedImpl(
    const FormData& form,
    bool known_success,
    mojom::SubmissionSource source) {
  provider_->OnFormSubmitted(this, form, known_success, source);
}

void AndroidAutofillManager::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const TimeTicks timestamp) {
  provider_->OnTextFieldDidChange(this, form, field, bounding_box, timestamp);
}

void AndroidAutofillManager::OnTextFieldDidScrollImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnTextFieldDidScroll(this, form, field, bounding_box);
}

void AndroidAutofillManager::OnQueryFormFieldAutofillImpl(
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  provider_->OnQueryFormFieldAutofill(this, query_id, form, field, bounding_box,
                                      autoselect_first_suggestion);
}

void AndroidAutofillManager::OnFocusOnFormFieldImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnFocusOnFormField(this, form, field, bounding_box);
}

void AndroidAutofillManager::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  provider_->OnSelectControlDidChange(this, form, field, bounding_box);
}

bool AndroidAutofillManager::ShouldParseForms(
    const std::vector<FormData>& forms) {
  provider_->OnFormsSeen(this, forms);
  // Need to parse the |forms| to FormStructure, so heuristic_type can be
  // retrieved later.
  return true;
}

void AndroidAutofillManager::OnFocusNoLongerOnForm(bool had_interacted_form) {
  provider_->OnFocusNoLongerOnForm(this, had_interacted_form);
}

void AndroidAutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  provider_->OnDidFillAutofillFormData(this, form, timestamp);
}

void AndroidAutofillManager::OnHidePopup() {
  provider_->OnHidePopup(this);
}

void AndroidAutofillManager::SelectFieldOptionsDidChange(const FormData& form) {
}

void AndroidAutofillManager::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  has_server_prediction_ = true;
  provider_->OnServerPredictionsAvailable(this);
}

void AndroidAutofillManager::OnServerRequestError(
    FormSignature form_signature,
    AutofillDownloadManager::RequestType request_type,
    int http_error) {
  provider_->OnServerQueryRequestError(this, form_signature);
}

void AndroidAutofillManager::Reset() {
  AutofillManager::Reset();
  has_server_prediction_ = false;
  provider_->Reset(this);
}

}  // namespace autofill
