// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/android_autofill/browser/android_form_event_logger.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/autofill_type_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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
    mojom::SubmissionSource source) {
  if (auto* provider = GetAutofillProvider()) {
    provider->OnFormSubmitted(this, form, source);
  }
  const FormStructure* submitted_form = FindCachedFormById(form.global_id());
  if (!submitted_form) {
    return;
  }
  for (FormType form_type :
       submitted_form->GetFormTypes(GetAcUnrecognizedBehavior(client()))) {
    if (auto* logger = GetEventFormLogger(form_type)) {
      logger->OnWillSubmitForm();
    }
  }
}

void AndroidAutofillManager::OnTextFieldValueChangedImpl(
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

  // We cannot use `field.is_autofilled_according_to_renderer()` because it
  // has already been cleared by blink. Check `provider` cache.
  bool cached_is_autofilled = provider->GetCachedIsAutofilled(*field);

  provider->OnTextFieldValueChanged(this, form, *field, timestamp);

  if (auto* logger = GetEventFormLogger(form.global_id(), field_id)) {
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
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<PasswordSuggestionRequest> password_request,
    base::ScopedClosureRunner scoped_on_after_ask_for_values_to_fill) {
  auto* provider = GetAutofillProvider();
  if (!provider) {
    return;
  }
  const FormFieldData* field = form.FindFieldByGlobalId(field_id);
  if (!field) {
    return;
  }

  provider->OnAskForValuesToFill(this, form, *field, trigger_source);

  if (auto* logger = GetEventFormLogger(form.global_id(), field_id)) {
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

void AndroidAutofillManager::OnSelectControlSelectionChangedImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (auto* provider = GetAutofillProvider()) {
    if (const FormFieldData* field = form.FindFieldByGlobalId(field_id)) {
      provider->OnSelectControlSelectionChanged(this, form, *field);
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

void AndroidAutofillManager::OnDidAutofillFormImpl(const FormData& form) {
  if (auto* provider = GetAutofillProvider())
    provider->OnDidAutofillForm(this, form);
}

void AndroidAutofillManager::OnHidePopupImpl() {
  if (auto* provider = GetAutofillProvider())
    provider->OnHidePopup(this);
}

void AndroidAutofillManager::OnFormProcessed(
    const FormStructure& form_structure) {
  DenseSet<FormType> form_types =
      form_structure.GetFormTypes(GetAcUnrecognizedBehavior(client()));
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

void AndroidAutofillManager::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form,
    FieldTypeSource source,
    bool small_forms_were_parsed) {
  CHECK_EQ(&manager, this);
  switch (source) {
    case FieldTypeSource::kAutofillAiModel:
    case FieldTypeSource::kAutofillServer:
      forms_with_server_predictions_.insert(form);
      if (auto* provider = GetAutofillProvider()) {
        provider->OnServerPredictionsAvailable(*this, form);
      }
      break;
    case FieldTypeSource::kHeuristicsOrAutocomplete:
      break;
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

CreditCardAccessManager* AndroidAutofillManager::GetCreditCardAccessManager() {
  return nullptr;
}

const CreditCardAccessManager*
AndroidAutofillManager::GetCreditCardAccessManager() const {
  return nullptr;
}

void AndroidAutofillManager::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    FormData form,
    const url::Origin& triggered_origin) {
  DCHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);

  const FormGlobalId form_id = form.global_id();
  std::vector<FormFieldData> fields = form.ExtractFields();
  std::erase_if(fields, [&](const FormFieldData& field) {
    // The renderer doesn't fill such fields, and therefore they can be removed
    // from here to reduce IPC traffic and avoid accidental filling.
    return !field.is_autofilled_according_to_renderer() ||
           field.value().empty();
  });

  absl::flat_hash_map<FieldGlobalId, FieldType> field_type_map;
  field_type_map.reserve(fields.size());
  for (const FormFieldData& field : fields) {
    // The security policy is slightly weaker on WebView because it has no
    // visibility on the types filled into the fields.
    field_type_map.emplace(field.global_id(), UNKNOWN_TYPE);
  }

  driver().ApplyFormAction(mojom::FormActionType::kFill, action_persistence,
                           fields, FillId::Create(),
                           /*supports_refill=*/false, triggered_origin,
                           field_type_map);
  // WebView does not have visibility into what type of dataset the Android
  // Autofill service used to fill the form, so it infers the affected loggers
  // from the classified types of the filled `fields`. Looking at all filled
  // fields ensures fills are still attributed when predictions arrive
  // asynchronously after focus, when the trigger field itself is unclassified,
  // or when a single fill populates multiple field groups. However, this
  // remains an approximation and may call the wrong logger if a filled field
  // was misclassified or has multiple possible types.
  base::flat_set<AndroidFormEventLogger*> loggers;
  for (const FormFieldData& field : fields) {
    if (auto* logger = GetEventFormLogger(form_id, field.global_id())) {
      loggers.insert(logger);
    }
  }
  for (AndroidFormEventLogger* logger : loggers) {
    logger->OnDidInteractWithAutofillableForm();
    logger->OnDidFillSuggestion();
  }
}

void AndroidAutofillManager::FillOrPreviewField(
    mojom::ActionPersistence action_persistence,
    mojom::FieldActionType action_type,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const std::u16string& value,
    FillingProduct filling_product,
    std::optional<FieldType> field_type_used) {
  driver().ApplyFieldAction(action_type, action_persistence, field_id, value);
}

void AndroidAutofillManager::StartNewLoggingSession() {
  address_logger_ = std::make_unique<AndroidFormEventLogger>("Address");
  loyalty_card_logger_ =
      std::make_unique<AndroidFormEventLogger>("LoyaltyCard");
  payments_logger_ = std::make_unique<AndroidFormEventLogger>("CreditCard");
  password_logger_ = std::make_unique<AndroidFormEventLogger>("Password");
}

AndroidFormEventLogger* AndroidAutofillManager::GetEventFormLogger(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id) {
  auto [form, field] = FindFormAndField(form_id, field_id);
  if (!field) {
    return nullptr;
  }
  return GetEventFormLogger(FieldTypeGroupToFormType(
      GroupTypeOfFieldType(GetMostRelevantFieldType(field->Type()))));
}

AndroidFormEventLogger* AndroidAutofillManager::GetEventFormLogger(
    FormType form_type) {
  switch (form_type) {
    case FormType::kAddressForm:
      return address_logger_.get();
    case FormType::kLoyaltyCardForm:
      return loyalty_card_logger_.get();
    case FormType::kCreditCardForm:
    case FormType::kStandaloneCvcForm:
      return payments_logger_.get();
    case FormType::kPasswordForm:
      return password_logger_.get();
    // TODO(crbug.com/443693025): Add event logger for OTP fields
    case FormType::kOneTimePasswordForm:
    case FormType::kUnknownFormType:
      return nullptr;
  }
  NOTREACHED();
}

}  // namespace autofill
