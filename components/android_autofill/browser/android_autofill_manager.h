// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_MANAGER_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

class AutofillProvider;
class ContentAutofillDriver;

// Creates an AndroidAutofillManager and attaches it to the `driver`.
//
// This hook is to be passed to CreateForWebContentsAndDelegate().
// It is the glue between ContentAutofillDriver[Factory] and
// AndroidAutofillManager.
//
// Other embedders (which don't want to use AndroidAutofillManager) shall use
// other implementations.
void AndroidDriverInitHook(
    AutofillClient* client,
    AutofillManager::EnableDownloadManager enable_download_manager,
    ContentAutofillDriver* driver);

// This class forwards AutofillManager calls to AutofillProvider.
class AndroidAutofillManager : public AutofillManager {
 public:
  AndroidAutofillManager(const AndroidAutofillManager&) = delete;
  AndroidAutofillManager& operator=(const AndroidAutofillManager&) = delete;

  ~AndroidAutofillManager() override;

  AutofillOfferManager* GetOfferManager() override;
  CreditCardAccessManager* GetCreditCardAccessManager() override;

  bool ShouldClearPreviewedForm() override;

  void FillCreditCardForm(int query_id,
                          const FormData& form,
                          const FormFieldData& field,
                          const CreditCard& credit_card,
                          const std::u16string& cvc) override;
  void FillProfileForm(const autofill::AutofillProfile& profile,
                       const FormData& form,
                       const FormFieldData& field) override;

  void OnFocusNoLongerOnForm(bool had_interacted_form) override;

  void OnDidFillAutofillFormData(const FormData& form,
                                 const base::TimeTicks timestamp) override;

  void OnDidPreviewAutofillFormData() override {}
  void OnDidEndTextFieldEditing() override {}
  void OnHidePopup() override;
  void SelectFieldOptionsDidChange(const FormData& form) override;

  void Reset() override;

  void ReportAutofillWebOTPMetrics(bool used_web_otp) override {}

  base::WeakPtr<AndroidAutofillManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool has_server_prediction() const { return has_server_prediction_; }

  // Send the |form| to the renderer for the specified |action|.
  void FillOrPreviewForm(int query_id,
                         mojom::RendererFormDataAction action,
                         const FormData& form);

 protected:
  friend void AndroidDriverInitHook(
      AutofillClient* client,
      AutofillManager::EnableDownloadManager enable_download_manager,
      ContentAutofillDriver* driver);

  AndroidAutofillManager(
      AutofillDriver* driver,
      AutofillClient* client,
      AutofillManager::EnableDownloadManager enable_download_manager);

  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override;

  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override;

  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override;

  void OnAskForValuesToFillImpl(
      int query_id,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      bool autoselect_first_suggestion,
      TouchToFillEligible touch_to_fill_eligible) override;

  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;

  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) override;

  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override;

  bool ShouldParseForms(const std::vector<FormData>& forms) override;

  void OnBeforeProcessParsedForms() override {}

  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override {}

  void OnAfterProcessParsedForms(
      const DenseSet<FormType>& form_types) override {}

  void PropagateAutofillPredictions(
      const std::vector<FormStructure*>& forms) override;

  void OnServerRequestError(FormSignature form_signature,
                            AutofillDownloadManager::RequestType request_type,
                            int http_error) override;

 protected:
#ifdef UNIT_TEST
  // For the unit tests where WebContents isn't available.
  void set_autofill_provider_for_testing(AutofillProvider* autofill_provider) {
    autofill_provider_for_testing_ = autofill_provider;
  }
#endif  // UNIT_TEST

 private:
  AutofillProvider* GetAutofillProvider();

  bool has_server_prediction_ = false;
  raw_ptr<AutofillProvider> autofill_provider_for_testing_ = nullptr;
  base::WeakPtrFactory<AndroidAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_MANAGER_H_
