// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_MANAGER_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

class AutofillProvider;
class AndroidFormEventLogger;

// This class forwards AutofillManager calls to AutofillProvider.
class AndroidAutofillManager : public AutofillManager,
                               public AutofillManager::Observer {
 public:
  explicit AndroidAutofillManager(AutofillDriver* driver);

  AndroidAutofillManager(const AndroidAutofillManager&) = delete;
  AndroidAutofillManager& operator=(const AndroidAutofillManager&) = delete;

  ~AndroidAutofillManager() override;

  base::WeakPtr<AndroidAutofillManager> GetWeakPtrToLeafClass() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtr<AutofillManager> GetWeakPtr() override;

  bool ShouldClearPreviewedForm() override;

  void OnFocusOnNonFormFieldImpl() override;

  void OnDidFillAutofillFormDataImpl(const FormData& form,
                                     const base::TimeTicks timestamp) override;

  void OnDidEndTextFieldEditingImpl() override {}
  void OnHidePopupImpl() override;
  void OnSelectFieldOptionsDidChangeImpl(const FormData& form) override {}

  void ReportAutofillWebOTPMetrics(bool used_web_otp) override {}

  bool has_server_prediction(FormGlobalId form) const {
    return forms_with_server_predictions_.contains(form);
  }

  FieldTypeGroup ComputeFieldTypeGroupForField(const FormData& form,
                                               const FormFieldData& field);

  // Send the |form| to the renderer for the specified |action|.
  //
  // |triggered_origin| is the origin of the field from which the autofill is
  // triggered; this affects the security policy for cross-frame fills. See
  // AutofillDriver::FillOrPreviewForm() for further details.
  void FillOrPreviewForm(mojom::ActionPersistence action_persistence,
                         FormData form,
                         const FieldTypeGroup field_type_group,
                         const url::Origin& triggered_origin);

 protected:
  void Reset() override;

  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override;

  void OnCaretMovedInFormFieldImpl(const FormData& form,
                                   const FieldGlobalId& field_id,
                                   const gfx::Rect& caret_bounds) override {}

  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FieldGlobalId& field_id,
                                const base::TimeTicks timestamp) override;

  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FieldGlobalId& field_id) override;

  void OnAskForValuesToFillImpl(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override;

  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FieldGlobalId& field_id) override;

  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FieldGlobalId& field_id) override;

  void OnJavaScriptChangedAutofilledValueImpl(const FormData& form,
                                              const FieldGlobalId& field_id,
                                              const std::u16string& old_value,
                                              bool formatting_only) override {}

  bool ShouldParseForms() override;

  void OnBeforeProcessParsedForms() override {}

  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override;

 private:
  // AutofillManager::Observer:
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              FieldTypeSource source) override;

  AutofillProvider* GetAutofillProvider();

  // Records metrics for loggers and creates new logging session.
  void StartNewLoggingSession();

  // Returns logger associated with the passed-in `form` and `field`.
  AndroidFormEventLogger* GetEventFormLogger(const FormData& form,
                                             const FormFieldData& field);

  // Returns logger associated with the passed-in `field_type_group`.
  AndroidFormEventLogger* GetEventFormLogger(FieldTypeGroup field_type_group);

  // Returns logger associated with the passed-in `form_type`.
  AndroidFormEventLogger* GetEventFormLogger(FormType form_type);

  // The forms that have received server predictions.
  base::flat_set<FormGlobalId> forms_with_server_predictions_;
  std::unique_ptr<AndroidFormEventLogger> address_logger_;
  std::unique_ptr<AndroidFormEventLogger> payments_logger_;
  std::unique_ptr<AndroidFormEventLogger> password_logger_;

  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observation{this};

  base::WeakPtrFactory<AndroidAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_MANAGER_H_
