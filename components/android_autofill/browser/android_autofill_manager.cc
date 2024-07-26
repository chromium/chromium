// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/android_autofill/browser/android_form_event_logger.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

using base::TimeTicks;

AndroidAutofillManager::AndroidAutofillManager(AutofillDriver* driver)
    : AutofillManager(driver) {
  StartNewLoggingSession();
  autofill_manager_observation.Observe(this);
}

AndroidAutofillManager::~AndroidAutofillManager() {
  Reset();
}

base::WeakPtr<AutofillManager> AndroidAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool AndroidAutofillManager::ShouldClearPreviewedForm() {
  return false;
}

void AndroidAutofillManager::OnFormSubmittedImpl(
    const FormData& form,
    bool known_success,
    mojom::SubmissionSource source) {
  address_logger_->OnWillSubmitForm();
  payments_logger_->OnWillSubmitForm();
  password_logger_->OnWillSubmitForm();
  if (auto* provider = GetAutofillProvider())
    provider->OnFormSubmitted(this, form, known_success, source);
}

void AndroidAutofillManager::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const TimeTicks timestamp) {
  auto* provider = GetAutofillProvider();
  if (!provider) {
    return;
  }
  const FormFieldData* field = form.FindFieldByGlobalId(field_id);
  if (!field) {
    return;
  }

  // We cannot use `field` is_autofilled state because it has already been
  // cleared by blink. Check `provider` cache.
  bool cached_is_autofilled = provider->GetCachedIsAutofilled(*field);

  provider->OnTextFieldDidChange(this, form, *field, timestamp);

  if (auto* logger = GetEventFormLogger(form, *field)) {
    if (cached_is_autofilled) {
      logger->OnEditedAutofilledField();
    } else {
      logger->OnTypedIntoNonFilledField();
    }
  }
}

void AndroidAutofillManager::OnTextFieldDidScrollImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (auto* provider = GetAutofillProvider())
    if (const FormFieldData* field = form.FindFieldByGlobalId(field_id)) {
      provider->OnTextFieldDidScroll(this, form, *field);
    }
}

void AndroidAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  auto* provider = GetAutofillProvider();
  if (!provider) {
    return;
  }
  const FormFieldData* field = form.FindFieldByGlobalId(field_id);
  if (!field) {
    return;
  }

  provider->OnAskForValuesToFill(this, form, *field, trigger_source);

  if (auto* logger = GetEventFormLogger(form, *field)) {
    logger->OnDidInteractWithAutofillableForm();
  }
}

void AndroidAutofillManager::OnFocusOnFormFieldImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (auto* provider = GetAutofillProvider()) {
    if (const FormFieldData* field = form.FindFieldByGlobalId(field_id)) {
      provider->OnFocusOnFormField(this, form, *field);
    }
  }
}

void AndroidAutofillManager::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (auto* provider = GetAutofillProvider()) {
    if (const FormFieldData* field = form.FindFieldByGlobalId(field_id)) {
      provider->OnSelectControlDidChange(this, form, *field);
    }
  }
}

bool AndroidAutofillManager::ShouldParseForms() {
  return true;
}

void AndroidAutofillManager::OnFocusOnNonFormFieldImpl() {
  if (auto* provider = GetAutofillProvider())
    provider->OnFocusOnNonFormField(this);
}

void AndroidAutofillManager::OnDidFillAutofillFormDataImpl(
    const FormData& form,
    const base::TimeTicks timestamp) {
  if (auto* provider = GetAutofillProvider())
    provider->OnDidFillAutofillFormData(this, form, timestamp);
}

void AndroidAutofillManager::OnHidePopupImpl() {
  if (auto* provider = GetAutofillProvider())
    provider->OnHidePopup(this);
}

void AndroidAutofillManager::OnFormProcessed(
    const FormData& form,
    const FormStructure& form_structure) {
  DenseSet<FormType> form_types = form_structure.GetFormTypes();
  for (FormType form_type : form_types) {
    if (auto* logger = GetEventFormLogger(form_type)) {
      logger->OnDidParseForm();
    }
  }
}

void AndroidAutofillManager::Reset() {
  // Inform the provider before resetting state in case it needs to access it.
  if (auto* rfh =
          static_cast<ContentAutofillDriver&>(driver()).render_frame_host()) {
    if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh)) {
      if (auto* provider = AutofillProvider::FromWebContents(web_contents)) {
        // Note that this doesn't use `GetAutofillProvider()` because we might
        // need to reset even when `rfh` is pending deletion.
        provider->OnManagerResetOrDestroyed(this);
      }
    }
  }
  AutofillManager::Reset();
  forms_with_server_predictions_.clear();
  StartNewLoggingSession();
}

void AndroidAutofillManager::OnFieldTypesDetermined(AutofillManager& manager,
                                                    FormGlobalId form,
                                                    FieldTypeSource source) {
  CHECK_EQ(&manager, this);
  if (source != FieldTypeSource::kAutofillServer) {
    return;
  }

  forms_with_server_predictions_.insert(form);
  if (auto* provider = GetAutofillProvider()) {
    provider->OnServerPredictionsAvailable(*this, form);
  }
}

AutofillProvider* AndroidAutofillManager::GetAutofillProvider() {
  if (auto* rfh =
          static_cast<ContentAutofillDriver&>(driver()).render_frame_host()) {
    if (rfh->IsActive()) {
      if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh)) {
        return AutofillProvider::FromWebContents(web_contents);
      }
    }
  }
  return nullptr;
}

FieldTypeGroup AndroidAutofillManager::ComputeFieldTypeGroupForField(
    const FormData& form,
    const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  return GetCachedFormAndField(form, field, &form_structure, &autofill_field)
             ? autofill_field->Type().group()
             : FieldTypeGroup::kNoGroup;
}

void AndroidAutofillManager::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    FormData form,
    FieldTypeGroup field_type_group,
    const url::Origin& triggered_origin) {
  DCHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);

  std::vector<FormFieldData> fields = form.ExtractFields();
  std::erase_if(fields, [&](const FormFieldData& field) {
    // The renderer doesn't fill such fields, and therefore they can be removed
    // from here to reduce IPC traffic and avoid accidental filling.
    return !field.is_autofilled() || field.value().empty();
  });

  driver().ApplyFormAction(mojom::FormActionType::kFill, action_persistence,
                           fields, triggered_origin, {});
  // We do not call OnAutofillProfileOrCreditCardFormFilled() because WebView
  // doesn't have AutofillProfile or CreditCard.
  if (auto* logger = GetEventFormLogger(field_type_group)) {
    logger->OnDidFillSuggestion();
  }
}

void AndroidAutofillManager::StartNewLoggingSession() {
  address_logger_ = std::make_unique<AndroidFormEventLogger>("Address");
  payments_logger_ = std::make_unique<AndroidFormEventLogger>("CreditCard");
  password_logger_ = std::make_unique<AndroidFormEventLogger>("Password");
}

AndroidFormEventLogger* AndroidAutofillManager::GetEventFormLogger(
    const FormData& form,
    const FormFieldData& field) {
  return GetEventFormLogger(ComputeFieldTypeGroupForField(form, field));
}

AndroidFormEventLogger* AndroidAutofillManager::GetEventFormLogger(
    FieldTypeGroup group) {
  return GetEventFormLogger(FieldTypeGroupToFormType(group));
}

AndroidFormEventLogger* AndroidAutofillManager::GetEventFormLogger(
    FormType form_type) {
  switch (form_type) {
    case FormType::kAddressForm:
      return address_logger_.get();
    case FormType::kCreditCardForm:
    case FormType::kStandaloneCvcForm:
      return payments_logger_.get();
    case FormType::kPasswordForm:
      return password_logger_.get();
    case FormType::kUnknownFormType:
      return nullptr;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace autofill
