// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/autocomplete_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/prefs/pref_service.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"

namespace autofill {

namespace {
// Returns true if the field type is eligible to be saved in the autocomplete
// history. Some types (promo codes, IBANs, CCs, CVCs) are excluded. Loyalty
// card IDs are also excluded if they were autofilled.
bool IsFieldTypeSaveable(const FormStructure* form, FieldGlobalId field_id) {
  const AutofillField* field = form ? form->GetFieldById(field_id) : nullptr;
  if (!field) {
    return true;
  }
  for (const FieldType field_type : field->Type().GetTypes()) {
    switch (field_type) {
      case MERCHANT_PROMO_CODE:
      case IBAN_VALUE:
      case CREDIT_CARD_VERIFICATION_CODE:
      case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      case CREDIT_CARD_NUMBER:
        return false;
      case LOYALTY_MEMBERSHIP_ID:
        if (field->last_modifier() == FieldModifier::kAutofill) {
          return false;
        }
        break;
      case NO_SERVER_DATA:
      case UNKNOWN_TYPE:
      case EMPTY_TYPE:
      case NAME_FIRST:
      case NAME_MIDDLE:
      case NAME_LAST:
      case NAME_MIDDLE_INITIAL:
      case NAME_FULL:
      case NAME_SUFFIX:
      case EMAIL_ADDRESS:
      case PHONE_HOME_NUMBER:
      case PHONE_HOME_CITY_CODE:
      case PHONE_HOME_COUNTRY_CODE:
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_HOME_WHOLE_NUMBER:
      case ADDRESS_HOME_LINE1:
      case ADDRESS_HOME_LINE2:
      case ADDRESS_HOME_APT_NUM:
      case ADDRESS_HOME_CITY:
      case ADDRESS_HOME_STATE:
      case ADDRESS_HOME_ZIP:
      case ADDRESS_HOME_COUNTRY:
      case CREDIT_CARD_NAME_FULL:
      case CREDIT_CARD_EXP_MONTH:
      case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      case CREDIT_CARD_TYPE:
      case COMPANY_NAME:
      case MERCHANT_EMAIL_SIGNUP:
      case PASSWORD:
      case ACCOUNT_CREATION_PASSWORD:
      case ADDRESS_HOME_STREET_ADDRESS:
      case ADDRESS_HOME_SORTING_CODE:
      case ADDRESS_HOME_DEPENDENT_LOCALITY:
      case ADDRESS_HOME_LINE3:
      case NOT_ACCOUNT_CREATION_PASSWORD:
      case USERNAME:
      case USERNAME_AND_EMAIL_ADDRESS:
      case NEW_PASSWORD:
      case PROBABLY_NEW_PASSWORD:
      case NOT_NEW_PASSWORD:
      case CREDIT_CARD_NAME_FIRST:
      case CREDIT_CARD_NAME_LAST:
      case PHONE_HOME_EXTENSION:
      case CONFIRMATION_PASSWORD:
      case AMBIGUOUS_TYPE:
      case SEARCH_TERM:
      case PRICE:
      case NOT_PASSWORD:
      case SINGLE_USERNAME:
      case NOT_USERNAME:
      case ADDRESS_HOME_STREET_NAME:
      case ADDRESS_HOME_HOUSE_NUMBER:
      case ADDRESS_HOME_SUBPREMISE:
      case ADDRESS_HOME_OTHER_SUBUNIT:
      case NAME_LAST_FIRST:
      case NAME_LAST_CONJUNCTION:
      case NAME_LAST_SECOND:
      case NAME_HONORIFIC_PREFIX:
      case ADDRESS_HOME_ADDRESS:
      case ADDRESS_HOME_ADDRESS_WITH_NAME:
      case ADDRESS_HOME_FLOOR:
      case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
      case PHONE_HOME_NUMBER_PREFIX:
      case PHONE_HOME_NUMBER_SUFFIX:
      case NUMERIC_QUANTITY:
      case ONE_TIME_CODE:
      case DELIVERY_INSTRUCTIONS:
      case ADDRESS_HOME_OVERFLOW:
      case ADDRESS_HOME_LANDMARK:
      case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
      case ADDRESS_HOME_ADMIN_LEVEL2:
      case ADDRESS_HOME_STREET_LOCATION:
      case ADDRESS_HOME_BETWEEN_STREETS:
      case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
      case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
      case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
      case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
      case ADDRESS_HOME_BETWEEN_STREETS_1:
      case ADDRESS_HOME_BETWEEN_STREETS_2:
      case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
      case SINGLE_USERNAME_FORGOT_PASSWORD:
      case ADDRESS_HOME_APT:
      case ADDRESS_HOME_APT_TYPE:
      case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
      case ALTERNATIVE_FULL_NAME:
      case ALTERNATIVE_GIVEN_NAME:
      case ALTERNATIVE_FAMILY_NAME:
      case PASSPORT_NUMBER:
      case PASSPORT_ISSUING_COUNTRY:
      case PASSPORT_EXPIRATION_DATE:
      case PASSPORT_ISSUE_DATE:
      case LOYALTY_MEMBERSHIP_PROGRAM:
      case LOYALTY_MEMBERSHIP_PROVIDER:
      case VEHICLE_LICENSE_PLATE:
      case VEHICLE_VIN:
      case VEHICLE_MAKE:
      case VEHICLE_MODEL:
      case DRIVERS_LICENSE_REGION:
      case DRIVERS_LICENSE_NUMBER:
      case DRIVERS_LICENSE_EXPIRATION_DATE:
      case DRIVERS_LICENSE_ISSUE_DATE:
      case VEHICLE_YEAR:
      case VEHICLE_PLATE_STATE:
      case EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
      case NATIONAL_ID_CARD_NUMBER:
      case NATIONAL_ID_CARD_EXPIRATION_DATE:
      case NATIONAL_ID_CARD_ISSUE_DATE:
      case NATIONAL_ID_CARD_ISSUING_COUNTRY:
      case KNOWN_TRAVELER_NUMBER:
      case KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      case REDRESS_NUMBER:
      case ADDRESS_HOME_ZIP_PREFIX:
      case ADDRESS_HOME_ZIP_SUFFIX:
      case FLIGHT_RESERVATION_FLIGHT_NUMBER:
      case FLIGHT_RESERVATION_CONFIRMATION_CODE:
      case FLIGHT_RESERVATION_TICKET_NUMBER:
      case FLIGHT_RESERVATION_DEPARTURE_DATE:
      case ADDRESS_HOME_ZIP_AND_CITY:
      case ORDER_ID:
      case ORDER_DATE:
      case ORDER_MERCHANT_NAME:
      case SHIPMENT_TRACKING_NUMBER:
      case MAX_VALID_FIELD_TYPE:
        break;
    }
  }
  return true;
}
}  // namespace

AutocompleteHistoryManager::AutocompleteHistoryManager() = default;

AutocompleteHistoryManager::~AutocompleteHistoryManager() = default;

void AutocompleteHistoryManager::OnGetSingleFieldSuggestions(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& trigger_field,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    SingleFieldFillRouter::OnSuggestionsReturnedCallback
        on_suggestions_returned) {
  // Cancel the pending query if there is one.
  suggestion_generator_ = nullptr;
  if (!profile_database_) {
    std::move(on_suggestions_returned).Run(trigger_field.global_id(), {});
    return;
  }
  const GURL& main_frame_url = client.GetLastCommittedPrimaryMainFrameURL();
  const GURL& field_url = trigger_field.origin().GetURL();
  const bool is_enabled =
      MayPerformAtMemoryAction(AtMemoryAction::kShowAutocompleteAtMemoryButton,
                               client, main_frame_url) &&
      MayPerformAtMemoryAction(AtMemoryAction::kShowAutocompleteAtMemoryButton,
                               client, field_url);
  suggestion_generator_ = std::make_unique<AutocompleteSuggestionGenerator>(
      profile_database_, is_enabled);

  auto on_suggestions_generated = base::BindOnce(
      [](SingleFieldFillRouter::OnSuggestionsReturnedCallback callback,
         FieldGlobalId field_id,
         SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(field_id,
                                std::move(returned_suggestions.second));
      },
      std::move(on_suggestions_returned), trigger_field.global_id());

  suggestion_generator_->GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      std::move(on_suggestions_generated));
}

void AutocompleteHistoryManager::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    const FormStructure* form,
    bool is_autocomplete_enabled) {
  if (!is_autocomplete_enabled || is_off_the_record_) {
    return;
  }
  std::vector<FormFieldData> autocomplete_saveable_fields;
  autocomplete_saveable_fields.reserve(fields.size());
  for (const FormFieldData& field : fields) {
    if (IsFieldValueSaveable(field, form)) {
      autocomplete_saveable_fields.push_back(field);
    }
  }
  if (!autocomplete_saveable_fields.empty() && profile_database_.get()) {
    profile_database_->AddFormFields(autocomplete_saveable_fields);
  }
}

void AutocompleteHistoryManager::CancelPendingQuery() {
  if (suggestion_generator_) {
    suggestion_generator_->CancelPendingQuery();
  }
}

void AutocompleteHistoryManager::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    SuggestionType type) {
  if (profile_database_) {
    profile_database_->RemoveFormValueForElementName(field_name, value);
  }
}

void AutocompleteHistoryManager::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion) {
  CHECK_EQ(suggestion.type, SuggestionType::kAutocompleteEntry);
  const AutocompleteEntry& entry =
      CHECK_DEREF(std::get_if<AutocompleteEntry>(&suggestion.payload));
  // The AutocompleteEntry was found, use it to log the DaysSinceLastUsed.
  base::TimeDelta time_delta = base::Time::Now() - entry.date_last_used();
  AutofillMetrics::LogAutocompleteDaysSinceLastUse(time_delta.InDays());

  if (profile_database_ &&
      base::FeatureList::IsEnabled(
          features::kAutofillPreventAutofillFromSavingToAutocomplete)) {
    // When the feature is enabled, form submission will skip saving any fields
    // that were autofilled. Therefore, we must update the autocomplete entry's
    // metadata immediately when the suggestion is selected.
    FormFieldData field;
    field.set_name(entry.key().name());
    field.set_value(entry.key().value());
    profile_database_->AddFormFields({field});
  }
}

void AutocompleteHistoryManager::Init(
    scoped_refptr<AutofillWebDataService> profile_database,
    PrefService* pref_service,
    bool is_off_the_record) {
  profile_database_ = profile_database;
  pref_service_ = pref_service;
  is_off_the_record_ = is_off_the_record;

  if (!profile_database_) {
    // In some tests, there are no dbs.
    return;
  }

  // No need to run the retention policy in OTR.
  if (!is_off_the_record_) {
    // Upon successful cleanup, the last cleaned-up major version is being
    // stored in this pref.
    int last_cleaned_version = pref_service_->GetInteger(
        prefs::kAutocompleteLastVersionRetentionPolicy);
    if (version_info::GetMajorVersionNumberAsInt() > last_cleaned_version) {
      // Trigger the cleanup.
      profile_database_->RemoveExpiredAutocompleteEntries(
          base::BindOnce(&AutocompleteHistoryManager::OnAutofillCleanupReturned,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

bool AutocompleteHistoryManager::IsFieldNameMeaningfulForAutocomplete(
    const std::u16string& name) {
  static constexpr char16_t kRegex[] =
      // Full matches.
      u"^(?:(?:field|input|mat-input)[-_]?\\d+|title|tan|mfa_text_box|pw|pin)$"
      // Prefix and suffix matches.
      u"|^otp|otp$"
      // Infix matches.
      u"|\\botp\\b|captcha|passw|pass2|passcode|pwd|senha|pincode|"
      // Suppress flight verification fields.
      u"flight[-_]?verification|"
      // Suppress CVC fields.
      u"csc|cvd|ccv|cvc|cvn|cvv|card[-_]?verification|verification[-_]?code|"
      u"verify[-_]?(?:card|code)|security[-_]?(?:code|value|number)";
  return !MatchesRegex<kRegex>(name);
}

void AutocompleteHistoryManager::OnAutofillCleanupReturned(
    WebDataServiceBase::Handle current_handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(result);
  DCHECK_EQ(AUTOFILL_CLEANUP_RESULT, result->GetType());
  if (!static_cast<const WDResult<bool>*>(result.get())->GetValue()) {
    DLOG(WARNING) << "Autofill cleanup returned false. This should not happen.";
  }

  // Cleanup was successful, update the latest run milestone.
  pref_service_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                            version_info::GetMajorVersionNumberAsInt());
}

// We put the following restriction on stored FormFields:
//  - non-empty name
//  - neither empty nor whitespace-only value
//  - text field
//  - autocomplete is not disabled
//  - value is not a credit card number nor IBAN
//  - field is not a credit card verification code (CVC)
//  - field is not an autofilled loyalty card ID
//  - field has user typed input or is focusable (this is a mild criteria but
//    this way it is consistent for all platforms)
//  - not a presentation field
bool AutocompleteHistoryManager::IsFieldValueSaveable(
    const FormFieldData& field,
    const FormStructure* form) {
  // Only save values from text-like input elements that are not password
  // or number inputs.
  if (!field.IsTextInputElement() || field.IsPasswordInputElement() ||
      field.form_control_type() == FormControlType::kInputNumber) {
    return false;
  }

  // Only save values if the page allows autocomplete for the field.
  if (!field.should_autocomplete()) {
    return false;
  }

  // Reject fields with empty names or names that are not meaningful for
  // autocomplete (e.g., placeholder names generated by frameworks).
  if (!IsFieldNameMeaningfulForAutocomplete(field.name()) ||
      field.name().empty()) {
    return false;
  }

  // We don't want to save a trimmed string, but we want to make sure that the
  // value is neither empty nor only whitespaces.
  if (std::ranges::none_of(field.value(),
                           std::not_fn(base::IsUnicodeWhitespace<char16_t>))) {
    return false;
  }

  // Reject fields with types that are ineligible for autocomplete such as
  // credit card numbers, CVCs, IBANs, promo codes, or autofilled loyalty cards.
  if (!IsFieldTypeSaveable(form, field.global_id())) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillPreventAutofillFromSavingToAutocomplete)) {
    const AutofillField* autofill_field =
        form ? form->GetFieldById(field.global_id()) : nullptr;
    if (autofill_field &&
        autofill_field->all_modifiers().contains(FieldModifier::kAutofill) &&
        (autofill_field->last_modifier() != FieldModifier::kUser ||
         autofill_field->filling_product() != FillingProduct::kAutocomplete)) {
      // If a field has been autofilled by a structured product (e.g. Address,
      // Payments, Autofill AI), we avoid saving the submitted value to
      // Autocomplete, even if the user edited it.
      //
      // However, if the field was filled by Autocomplete and then edited by
      // the user, we should save the edited value as it represents a new
      // user-edited autocomplete value.
      return false;
    }
  }

  // Do not save sensitive values like credit card numbers, IBANs, or Social
  // Security Numbers.
  if (IsValidCreditCardNumber(field.value()) ||
      IsInternationalBankAccountNumber(field.value()) || IsSSN(field.value())) {
    return false;
  }

  // Reject fields that the user did not type into and are not currently
  // focusable, or fields that have a presentation role (ARIA
  // role="presentation").
  if ((!(field.properties_mask() & kUserTyped) && !field.is_focusable()) ||
      field.role() == FormFieldData::RoleAttribute::kPresentation) {
    return false;
  }

  return true;
}

}  // namespace autofill
