// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROVIDER_H_

#include "components/autofill/core/browser/autofill_provider.h"

namespace autofill {

class TestAutofillProvider : public AutofillProvider {
 public:
  ~TestAutofillProvider() override {}

  // AutofillProvider:
  void OnQueryFormFieldAutofill(AndroidAutofillManager* manager,
                                int32_t id,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                bool autoselect_first_suggestion) override {}
  void OnTextFieldDidChange(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box,
                            const base::TimeTicks timestamp) override {}
  void OnTextFieldDidScroll(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box) override {}
  void OnSelectControlDidChange(AndroidAutofillManager* manager,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override {}
  void OnFormSubmitted(AndroidAutofillManager* manager,
                       const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource source) override {}
  void OnFocusNoLongerOnForm(AndroidAutofillManager* manager,
                             bool had_interacted_form) override {}
  void OnFocusOnFormField(AndroidAutofillManager* manager,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override {}
  void OnDidFillAutofillFormData(AndroidAutofillManager* manager,
                                 const FormData& form,
                                 base::TimeTicks timestamp) override {}
  void OnFormsSeen(AndroidAutofillManager* manager,
                   const std::vector<FormData>& forms) override {}
  void OnHidePopup(AndroidAutofillManager* manager) override {}
  void OnServerPredictionsAvailable(AndroidAutofillManager* manager) override {}
  void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                 FormSignature form_signature) override {}
  void Reset(AndroidAutofillManager* manager) override {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROVIDER_H_
