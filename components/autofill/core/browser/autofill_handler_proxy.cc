// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_handler_proxy.h"

#include "components/autofill/core/browser/autofill_provider.h"

namespace autofill {

using base::TimeTicks;

AutofillHandlerProxy::AutofillHandlerProxy(AutofillDriver* driver,
                                           LogManager* log_manager,
                                           AutofillProvider* provider)
    : AutofillHandler(driver, log_manager), provider_(provider) {}

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

bool AutofillHandlerProxy::ShouldParseForms(const std::vector<FormData>& forms,
                                            const base::TimeTicks timestamp) {
  provider_->OnFormsSeen(this, forms, timestamp);
  // Need to parse the |forms| to FormStructure, so heuristic_type can be
  // retrieved later.
  return true;
}

void AutofillHandlerProxy::OnFormsParsed(
    const std::vector<FormStructure*>& form_structures,
    const base::TimeTicks timestamp) {}

void AutofillHandlerProxy::OnFocusNoLongerOnForm() {
  provider_->OnFocusNoLongerOnForm(this);
}

void AutofillHandlerProxy::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  provider_->OnDidFillAutofillFormData(this, form, timestamp);
}

void AutofillHandlerProxy::OnDidPreviewAutofillFormData() {}

void AutofillHandlerProxy::OnDidEndTextFieldEditing() {}

void AutofillHandlerProxy::OnHidePopup() {}

void AutofillHandlerProxy::OnSetDataList(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {}

void AutofillHandlerProxy::SelectFieldOptionsDidChange(const FormData& form) {}

void AutofillHandlerProxy::Reset() {
  provider_->Reset(this);
}

}  // namespace autofill
