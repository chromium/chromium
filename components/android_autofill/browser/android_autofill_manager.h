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
class ContentAutofillDriver;
class FormEventLoggerWeblayerAndroid;

// Creates an AndroidAutofillManager and attaches it to the `driver`.
//
// This hook is to be passed to CreateForWebContentsAndDelegate().
// It is the glue between ContentAutofillDriver[Factory] and
// AndroidAutofillManager.
//
// Other embedders (which don't want to use AndroidAutofillManager) shall use
// other implementations.
void AndroidDriverInitHook(AutofillClient* client,
                           ContentAutofillDriver* driver);

// This class forwards AutofillManager calls to AutofillProvider.
class AndroidAutofillManager : public AutofillManager,
                               public AutofillManager::Observer {
 public:
  AndroidAutofillManager(const AndroidAutofillManager&) = delete;
  AndroidAutofillManager& operator=(const AndroidAutofillManager&) = delete;

  ~AndroidAutofillManager() override;

  base::WeakPtr<AndroidAutofillManager> GetWeakPtrToLeafClass() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtr<AutofillManager> GetWeakPtr() override;

  bool ShouldClearPreviewedForm() override;

  void OnFocusNoLongerOnFormImpl(bool had_interacted_form) override;

  void OnDidFillAutofillFormDataImpl(const FormData& form,
                                     const base::TimeTicks timestamp) override;

  void OnDidEndTextFieldEditingImpl() override {}
  void OnHidePopupImpl() override;
  void OnSelectOrSelectListFieldOptionsDidChangeImpl(
      const FormData& form) override {}

  void Reset() override;
  void OnContextMenuShownInField(const FormGlobalId& form_global_id,
                                 const FieldGlobalId& field_global_id) override;

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
                         const FormData& form,
                         const FieldTypeGroup field_type_group,
                         const url::Origin& triggered_origin);

  void FillOrPreviewField(mojom::ActionPersistence action_persistence,
                          mojom::TextReplacement text_replacement,
                          const FormData& form,
                          const FormFieldData& field,
                          const std::u16string& value,
                          PopupItemId popup_item_id) override;

 protected:
  friend void AndroidDriverInitHook(AutofillClient* client,
                                    ContentAutofillDriver* driver);

  AndroidAutofillManager(AutofillDriver* driver, AutofillClient* client);

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
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source) override;

  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;

  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) override;

  void OnJavaScriptChangedAutofilledValueImpl(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override {}

  bool ShouldParseForms() override;

  void OnBeforeProcessParsedForms() override {}

  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override;

  void OnAfterProcessParsedForms(
      const DenseSet<FormType>& form_types) override {}

  void OnServerRequestError(
      FormSignature form_signature,
      AutofillCrowdsourcingManager::RequestType request_type,
      int http_error) override;

 private:
  // AutofillManager::Observer:
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              FieldTypeSource source) override;

  AutofillProvider* GetAutofillProvider();

  // Records metrics for loggers and creates new logging session.
  void StartNewLoggingSession();

  // Returns logger associated with the passed-in `form` and `field`.
  FormEventLoggerWeblayerAndroid* GetEventFormLogger(
      const FormData& form,
      const FormFieldData& field);

  // Returns logger associated with the passed-in `field_type_group`.
  FormEventLoggerWeblayerAndroid* GetEventFormLogger(
      FieldTypeGroup field_type_group);

  // Returns logger associated with the passed-in `form_type`.
  FormEventLoggerWeblayerAndroid* GetEventFormLogger(FormType form_type);

  // The forms that have received server predictions.
  base::flat_set<FormGlobalId> forms_with_server_predictions_;
  std::unique_ptr<FormEventLoggerWeblayerAndroid> address_logger_;
  std::unique_ptr<FormEventLoggerWeblayerAndroid> payments_logger_;
  std::unique_ptr<FormEventLoggerWeblayerAndroid> password_logger_;

  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observation{this};

  base::WeakPtrFactory<AndroidAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_MANAGER_H_
