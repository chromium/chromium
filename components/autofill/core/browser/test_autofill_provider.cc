// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_provider.h"

namespace autofill {

void TestAutofillProvider::OnQueryFormFieldAutofill(
    AutofillHandlerProxy* handler,
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {}

void TestAutofillProvider::OnTextFieldDidChange(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const base::TimeTicks timestamp) {}

void TestAutofillProvider::OnTextFieldDidScroll(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {}

void TestAutofillProvider::OnSelectControlDidChange(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {}

void TestAutofillProvider::OnFocusNoLongerOnForm(AutofillHandlerProxy* handler,
                                                 bool had_interacted_form) {}

void TestAutofillProvider::OnFocusOnFormField(AutofillHandlerProxy* handler,
                                              const FormData& form,
                                              const FormFieldData& field,
                                              const gfx::RectF& bounding_box) {}

void TestAutofillProvider::OnDidFillAutofillFormData(
    AutofillHandlerProxy* handler,
    const FormData& form,
    base::TimeTicks timestamp) {}

void TestAutofillProvider::OnFormsSeen(AutofillHandlerProxy* handler,
                                       const std::vector<FormData>& forms,
                                       const base::TimeTicks timestamp) {}

void TestAutofillProvider::OnHidePopup(AutofillHandlerProxy* handler) {}

void TestAutofillProvider::Reset(AutofillHandlerProxy* handler) {}

}  // namespace autofill
