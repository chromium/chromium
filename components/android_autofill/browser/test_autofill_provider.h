// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_TEST_AUTOFILL_PROVIDER_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_TEST_AUTOFILL_PROVIDER_H_

#include "components/android_autofill/browser/autofill_provider.h"

#include "content/public/browser/web_contents.h"

namespace autofill {

class TestAutofillProvider : public AutofillProvider {
 public:
  // Create a instance owned by |web_contents|.
  explicit TestAutofillProvider(content::WebContents* web_contents)
      : AutofillProvider(web_contents) {}

  ~TestAutofillProvider() override = default;

  // AutofillProvider:
  void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutoselectFirstSuggestion autoselect_first_suggestion,
      FormElementWasClicked form_element_was_clicked) override {}
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

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_TEST_AUTOFILL_PROVIDER_H_
