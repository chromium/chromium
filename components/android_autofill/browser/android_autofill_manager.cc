// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/android_autofill/browser/android_form_event_logger.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/autofill_type_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

namespace {

// Returns the `FieldTypeGroup` for `field`. If `pw_form` is a login form and
// `field` is its username or password element, returns the corresponding
// credential group instead of `field`'s Autofill classification (e.g., an
// `<input type="email">` username field). Otherwise, returns `kNoGroup` if
// suggestions and filling are suppressed for `field` by default (e.g., due to
// an unrecognized autocomplete attribute), matching
// `FormStructure::GetFormTypes()`.
FieldTypeGroup ComputeFieldTypeGroup(
    const AutofillField& field,
    const password_manager::PasswordForm* pw_form,
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) {
  if (pw_form && pw_form->IsLikelyLoginForm()) {
    if (field.renderer_id() == pw_form->username_element_renderer_id) {
      return FieldTypeGroup::kUsernameField;
    }
    if (field.renderer_id() == pw_form->password_element_renderer_id) {
      return FieldTypeGroup::kPasswordField;
    }
  }
  if (field.ShouldSuppressSuggestionsAndFillingByDefault(
          ac_unrecognized_behavior)) {
    return FieldTypeGroup::kNoGroup;
  }
  return GroupTypeOfFieldType(GetMostRelevantFieldType(field.Type()));
}

DenseSet<FormType> GetFormTypesForLogging(
    const FormStructure& form_structure,
    const password_manager::PasswordForm* pw_form,
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) {
  DenseSet<FormType> form_types(
      form_structure.fields(),
      [&](const std::unique_ptr<AutofillField>& field) {
        return FieldTypeGroupToFormType(
            ComputeFieldTypeGroup(*field, pw_form, ac_unrecognized_behavior));
      });
  form_types.erase(FormType::kUnknownFormType);
  return form_types;
}

// Returns the `FormTypeNameForLogging`s that `form_type` is logged as for
// `form_structure`. Like `autofill_metrics::GetFormTypesForLogging()`, address
// forms are also logged as email-only or postal address forms.
DenseSet<FormTypeNameForLogging> GetFormTypeNamesForLogging(
    const FormStructure& form_structure,
    FormType form_type) {
  switch (form_type) {
    case FormType::kAddressForm:
      if (autofill_metrics::internal::IsEmailOnlyForm(form_structure)) {
        return {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kEmailOnlyForm};
      }
      if (autofill_metrics::internal::IsPostalAddressForm(form_structure)) {
        return {FormTypeNameForLogging::kAddressForm,
                FormTypeNameForLogging::kPostalAddressForm};
      }
      return {FormTypeNameForLogging::kAddressForm};
    case FormType::kCreditCardForm:
      return {FormTypeNameForLogging::kCreditCardForm};
    case FormType::kStandaloneCvcForm:
      return {FormTypeNameForLogging::kStandaloneCvcForm};
    case FormType::kLoyaltyCardForm:
      return {FormTypeNameForLogging::kLoyaltyCardForm};
    case FormType::kPasswordForm:
      return {FormTypeNameForLogging::kPasswordForm};
    case FormType::kOneTimePasswordForm:
      return {FormTypeNameForLogging::kOneTimePasswordForm};
    case FormType::kUnknownFormType:
      return {};
  }
  NOTREACHED();
}

}  // namespace

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
  const FormStructure* submitted_form = FindCachedFormById(form.global_id());
  const password_manager::PasswordForm* pw_form =
      submitted_form ? FindCachedPasswordForm(*submitted_form) : nullptr;
  // A form being hidden or removed after autofill is a weak submission signal
  // because filling can itself add or remove fields (e.g., after filling a
  // country). It is therefore only accepted for password forms, whose
  // mutations are rarer, and only if the feature is enabled.
  if (source == mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL &&
      (!pw_form ||
       !base::FeatureList::IsEnabled(
           features::kAutofillAcceptDomMutationAfterAutofillSubmission))) {
    return;
  }
  if (auto* provider = GetAutofillProvider()) {
    provider->OnFormSubmitted(this, form, source);
  }
  if (!submitted_form) {
    return;
  }
  for (AndroidFormEventLogger* logger : GetEventFormLoggers(*submitted_form)) {
    logger->OnWillSubmitForm();
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

  for (AndroidFormEventLogger* logger :
       GetEventFormLoggers(form.global_id(), field_id)) {
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

  for (AndroidFormEventLogger* logger :
       GetEventFormLoggers(form.global_id(), field_id)) {
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
  for (AndroidFormEventLogger* logger : GetEventFormLoggers(form_structure)) {
    logger->OnDidParseForm();
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
  password_forms_.clear();
  StartNewLoggingSession();
}

void AndroidAutofillManager::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form,
    FieldTypeSource source,
    bool small_forms_were_parsed) {
  CHECK_EQ(&manager, this);
  // The cached `PasswordForm` may be outdated now. `FindCachedPasswordForm()`
  // parses it again when it's needed.
  password_forms_.erase(form);
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

void AndroidAutofillManager::OnDidShowSuggestions(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id) {
  for (AndroidFormEventLogger* logger :
       GetEventFormLoggers(form_id, field_id)) {
    logger->OnDidShowSuggestions();
  }
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
    loggers.insert_range(GetEventFormLoggers(form_id, field.global_id()));
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
  address_logger_.reset();
  postal_address_logger_.reset();
  email_only_logger_.reset();
  loyalty_card_logger_.reset();
  payments_logger_.reset();
  password_logger_.reset();
  // There is nothing to measure if Android Autofill is disabled. Unlike
  // `GetAutofillProvider()`, this doesn't require `rfh` to be active, because
  // prerendered pages aren't reset when they're activated.
  if (auto* rfh =
          static_cast<ContentAutofillDriver&>(driver()).render_frame_host()) {
    if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh)) {
      if (auto* provider = AutofillProvider::FromWebContents(web_contents);
          provider && provider->IsAutofillDisabled()) {
        return;
      }
    }
  }
  address_logger_ = std::make_unique<AndroidFormEventLogger>("Address");
  postal_address_logger_ =
      std::make_unique<AndroidFormEventLogger>("PostalAddress");
  email_only_logger_ = std::make_unique<AndroidFormEventLogger>("EmailOnly");
  loyalty_card_logger_ =
      std::make_unique<AndroidFormEventLogger>("LoyaltyCard");
  payments_logger_ = std::make_unique<AndroidFormEventLogger>("CreditCard");
  password_logger_ = std::make_unique<AndroidFormEventLogger>("Password");
}

std::vector<AndroidFormEventLogger*>
AndroidAutofillManager::GetEventFormLoggers(
    const FormStructure& form_structure) {
  DenseSet<FormTypeNameForLogging> form_type_names;
  for (FormType form_type : GetFormTypesForLogging(
           form_structure, FindCachedPasswordForm(form_structure),
           GetAcUnrecognizedBehavior(client()))) {
    form_type_names.insert_all(
        GetFormTypeNamesForLogging(form_structure, form_type));
  }
  std::vector<AndroidFormEventLogger*> loggers;
  loggers.reserve(form_type_names.size());
  for (FormTypeNameForLogging form_type_name : form_type_names) {
    AndroidFormEventLogger* logger = GetEventFormLogger(form_type_name);
    if (logger) {
      loggers.push_back(logger);
    }
  }
  std::ranges::sort(loggers);
  auto [first_dup, last] = std::ranges::unique(loggers);
  loggers.erase(first_dup, last);
  return loggers;
}

std::vector<AndroidFormEventLogger*>
AndroidAutofillManager::GetEventFormLoggers(const FormGlobalId& form_id,
                                            const FieldGlobalId& field_id) {
  auto [form, field] = FindFormAndField(form_id, field_id);
  if (!form || !field) {
    return {};
  }
  const FormType form_type = FieldTypeGroupToFormType(
      ComputeFieldTypeGroup(*field, FindCachedPasswordForm(*form),
                            GetAcUnrecognizedBehavior(client())));
  std::vector<AndroidFormEventLogger*> loggers;
  for (FormTypeNameForLogging form_type_name :
       GetFormTypeNamesForLogging(*form, form_type)) {
    if (AndroidFormEventLogger* logger = GetEventFormLogger(form_type_name)) {
      loggers.push_back(logger);
    }
  }
  return loggers;
}

AndroidFormEventLogger* AndroidAutofillManager::GetEventFormLogger(
    FormTypeNameForLogging form_type) {
  switch (form_type) {
    case FormTypeNameForLogging::kAddressForm:
      return address_logger_.get();
    case FormTypeNameForLogging::kEmailOnlyForm:
      return email_only_logger_.get();
    case FormTypeNameForLogging::kPostalAddressForm:
      return postal_address_logger_.get();
    case FormTypeNameForLogging::kLoyaltyCardForm:
      return loyalty_card_logger_.get();
    case FormTypeNameForLogging::kCreditCardForm:
    case FormTypeNameForLogging::kStandaloneCvcForm:
      return payments_logger_.get();
    case FormTypeNameForLogging::kPasswordForm:
      return password_logger_.get();
    // TODO(crbug.com/443693025): Add event logger for OTP fields
    case FormTypeNameForLogging::kOneTimePasswordForm:
    case FormTypeNameForLogging::kUnknownFormType:
      return nullptr;
  }
  NOTREACHED();
}

const password_manager::PasswordForm*
AndroidAutofillManager::FindCachedPasswordForm(
    const FormStructure& form_structure) {
  auto [it, inserted] = password_forms_.try_emplace(form_structure.global_id());
  if (inserted) {
    it->second = ParseToPasswordForm(form_structure);
  }
  return it->second.get();
}

}  // namespace autofill
