// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/browser_autofill_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_autofill_history.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

using base::StartsWith;
using base::TimeTicks;
using mojom::SubmissionSource;

namespace {

constexpr size_t kMaxRecentFormSignaturesToRemember = 3;

// Time to wait after a dynamic form change before triggering a refill.
// This is used for sites that change multiple things consecutively.
constexpr base::TimeDelta kWaitTimeForDynamicForms = base::Milliseconds(200);

// Characters to be removed from the string before comparisons.
constexpr char16_t kCharsToBeRemoved[] = u"-_/\\.";

std::string_view GetSkipFieldFillLogMessage(
    FieldFillingSkipReason skip_reason) {
  switch (skip_reason) {
    case FieldFillingSkipReason::kNotInFilledSection:
      return "Skipped: Not part of filled section";
    case FieldFillingSkipReason::kNotFocused:
      return "Skipped: Only fill when focused";
    case FieldFillingSkipReason::kUnrecognizedAutocompleteAttribute:
      return "Skipped: Unrecognized autocomplete attribute";
    case FieldFillingSkipReason::kFormChanged:
      return "Skipped: Form has changed";
    case FieldFillingSkipReason::kInvisibleField:
      return "Skipped: Invisible field";
    case FieldFillingSkipReason::kValuePrefilled:
      return "Skipped: Value is prefilled";
    case FieldFillingSkipReason::kUserFilledFields:
      return "Skipped: User filled the field";
    case FieldFillingSkipReason::kAutofilledFieldsNotRefill:
      return "Skipped: Previously autofilled field and not a refill";
    case FieldFillingSkipReason::kNoFillableGroup:
      return "Skipped: Field type has no fillable group";
    case FieldFillingSkipReason::kRefillNotInInitialFill:
      return "Skipped: Refill field group different from initial filling group";
    case FieldFillingSkipReason::kExpiredCards:
      return "Skipped: Expired expiration date for credit card";
    case FieldFillingSkipReason::kFillingLimitReachedType:
      return "Skipped: Field type filling limit reached";
    case FieldFillingSkipReason::kFieldDoesNotMatchTargetFieldsSet:
      return "Skipped: The field type does not match the targeted fields.";
    case FieldFillingSkipReason::kNotSkipped:
      return "Fillable";
    case FieldFillingSkipReason::kUnknown:
      NOTREACHED_NORETURN();
  }
}

// Returns whether the |field| is predicted as being any kind of name.
bool IsNameType(const AutofillField& field) {
  return field.Type().group() == FieldTypeGroup::kName ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FULL ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FIRST ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_LAST;
}

// Selects the right name type from the |old_types| to insert into the
// |types_to_keep| based on |is_credit_card|. This is called when we have
// multiple possible types.
void SelectRightNameType(AutofillField* field, bool is_credit_card) {
  DCHECK(field);
  // There should be at least two possible field types.
  DCHECK_LE(2U, field->possible_types().size());

  ServerFieldTypeSet types_to_keep;
  const auto& old_types = field->possible_types();

  for (ServerFieldType type : old_types) {
    FieldTypeGroup group = GroupTypeOfServerFieldType(type);
    if ((is_credit_card && group == FieldTypeGroup::kCreditCard) ||
        (!is_credit_card && group == FieldTypeGroup::kName)) {
      types_to_keep.insert(type);
    }
  }

  ServerFieldTypeValidityStatesMap new_types_validities;
  // Since the disambiguation takes place when we up to four possible types,
  // here we can add up to three remaining types when only one is removed.
  for (auto type_to_keep : types_to_keep) {
    new_types_validities[type_to_keep] =
        field->get_validities_for_possible_type(type_to_keep);
  }
  field->set_possible_types(types_to_keep);
  field->set_possible_types_validities(new_types_validities);
}

void LogDeveloperEngagementUkm(ukm::UkmRecorder* ukm_recorder,
                               ukm::SourceId source_id,
                               const FormStructure& form_structure) {
  if (form_structure.developer_engagement_metrics()) {
    AutofillMetrics::LogDeveloperEngagementUkm(
        ukm_recorder, source_id, form_structure.main_frame_origin().GetURL(),
        form_structure.IsCompleteCreditCardForm(),
        form_structure.GetFormTypes(),
        form_structure.developer_engagement_metrics(),
        form_structure.form_signature());
  }
}

ValuePatternsMetric GetValuePattern(const std::u16string& value) {
  if (IsUPIVirtualPaymentAddress(value))
    return ValuePatternsMetric::kUpiVpa;
  if (IsInternationalBankAccountNumber(value))
    return ValuePatternsMetric::kIban;
  return ValuePatternsMetric::kNoPatternFound;
}

void LogValuePatternsMetric(const FormData& form) {
  for (const FormFieldData& field : form.fields) {
    if (!field.IsFocusable())
      continue;
    std::u16string value;
    base::TrimWhitespace(field.value, base::TRIM_ALL, &value);
    if (value.empty())
      continue;
    base::UmaHistogramEnumeration("Autofill.SubmittedValuePatterns",
                                  GetValuePattern(value));
  }
}

FillDataType GetEventTypeFromSingleFieldSuggestionPopupItemId(
    PopupItemId popup_item_id) {
  switch (popup_item_id) {
    case PopupItemId::kAutocompleteEntry:
      return FillDataType::kSingleFieldFormFillerAutocomplete;
    case PopupItemId::kMerchantPromoCodeEntry:
      return FillDataType::kSingleFieldFormFillerPromoCode;
    case PopupItemId::kIbanEntry:
      return FillDataType::kSingleFieldFormFillerIban;
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kAccountStorageUsernameEntry:
    case PopupItemId::kAddressEntry:
    case PopupItemId::kAllSavedPasswordsEntry:
    case PopupItemId::kAutofillOptions:
    case PopupItemId::kClearForm:
    case PopupItemId::kCompose:
    case PopupItemId::kCreateNewPlusAddress:
    case PopupItemId::kCreditCardEntry:
    case PopupItemId::kDatalistEntry:
    case PopupItemId::kDeleteAddressProfile:
    case PopupItemId::kEditAddressProfile:
    case PopupItemId::kFieldByFieldFilling:
    case PopupItemId::kFillEverythingFromAddressProfile:
    case PopupItemId::kFillExistingPlusAddress:
    case PopupItemId::kFillFullAddress:
    case PopupItemId::kFillFullName:
    case PopupItemId::kFillFullPhoneNumber:
    case PopupItemId::kFillFullEmail:
    case PopupItemId::kGeneratePasswordEntry:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kPasswordAccountStorageEmpty:
    case PopupItemId::kPasswordAccountStorageOptIn:
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
    case PopupItemId::kPasswordAccountStorageReSignin:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kScanCreditCard:
    case PopupItemId::kSeePromoCodeDetails:
    case PopupItemId::kSeparator:
    case PopupItemId::kShowAccountCards:
    case PopupItemId::kTitle:
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kVirtualCreditCardEntry:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
    case PopupItemId::kDevtoolsTestAddresses:
    case PopupItemId::kDevtoolsTestAddressEntry:
      NOTREACHED();
  }
  NOTREACHED();
  return FillDataType::kUndefined;
}

std::string FetchCountryCodeFromProfile(const AutofillProfile* profile) {
  return base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
}

void LogLanguageMetrics(const translate::LanguageState* language_state) {
  if (language_state) {
    AutofillMetrics::LogFieldParsingTranslatedFormLanguageMetric(
        language_state->current_language());
    AutofillMetrics::LogFieldParsingPageTranslationStatusMetric(
        language_state->IsPageTranslated());
  }
}

AutofillMetrics::AutocompleteState AutocompleteStateForSubmittedField(
    const AutofillField& field) {
  // An unparsable autocomplete attribute is treated like kNone.
  auto autocomplete_state = AutofillMetrics::AutocompleteState::kNone;
  // autocomplete=on is ignored as well. But for the purposes of metrics we care
  // about cases where the developer tries to disable autocomplete.
  if (field.autocomplete_attribute != "on" &&
      ShouldIgnoreAutocompleteAttribute(field.autocomplete_attribute)) {
    autocomplete_state = AutofillMetrics::AutocompleteState::kOff;
  } else if (field.parsed_autocomplete) {
    autocomplete_state =
        field.parsed_autocomplete->field_type != HtmlFieldType::kUnrecognized
            ? AutofillMetrics::AutocompleteState::kValid
            : AutofillMetrics::AutocompleteState::kGarbage;
  }

  return autocomplete_state;
}

void LogAutocompletePredictionCollisionTypeMetrics(
    const FormStructure& form_structure) {
  for (size_t i = 0; i < form_structure.field_count(); i++) {
    const AutofillField* field = form_structure.field(i);
    auto heuristic_type = field->heuristic_type();
    auto server_type = field->server_type();

    auto prediction_state = AutofillMetrics::PredictionState::kNone;
    if (IsFillableFieldType(heuristic_type)) {
      prediction_state = IsFillableFieldType(server_type)
                             ? AutofillMetrics::PredictionState::kBoth
                             : AutofillMetrics::PredictionState::kHeuristics;
    } else if (IsFillableFieldType(server_type)) {
      prediction_state = AutofillMetrics::PredictionState::kServer;
    }

    auto autocomplete_state = AutocompleteStateForSubmittedField(*field);
    AutofillMetrics::LogAutocompletePredictionCollisionState(
        prediction_state, autocomplete_state);
    AutofillMetrics::LogAutocompletePredictionCollisionTypes(
        autocomplete_state, server_type, heuristic_type);
  }
}

void LogContextMenuImpressionsForSubmittedField(const AutofillField& field) {
  auto autocomplete_state = AutocompleteStateForSubmittedField(field);
  AutofillMetrics::LogContextMenuImpressionsForField(
      field.Type().GetStorableType(), autocomplete_state);
}

// Finds the first field in |form_structure| with |field.value|=|value|.
AutofillField* FindFirstFieldWithValue(const FormStructure& form_structure,
                                       const std::u16string& value) {
  for (const auto& field : form_structure) {
    std::u16string trimmed_value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &trimmed_value);
    if (trimmed_value == value)
      return field.get();
  }
  return nullptr;
}

// Heuristically identifies all possible credit card verification fields.
AutofillField* HeuristicallyFindCVCFieldForUpload(
    const FormStructure& form_structure) {
  // Stores a pointer to the explicitly found expiration year.
  bool found_explicit_expiration_year_field = false;

  // The first pass checks the existence of an explicitly marked field for the
  // credit card expiration year.
  for (const auto& field : form_structure) {
    const ServerFieldTypeSet& type_set = field->possible_types();
    if (type_set.find(CREDIT_CARD_EXP_2_DIGIT_YEAR) != type_set.end() ||
        type_set.find(CREDIT_CARD_EXP_4_DIGIT_YEAR) != type_set.end()) {
      found_explicit_expiration_year_field = true;
      break;
    }
  }

  // Keeps track if a credit card number field was found.
  bool credit_card_number_found = false;

  // In the second pass, the CVC field is heuristically searched for.
  // A field is considered a CVC field, iff:
  // * it appears after the credit card number field;
  // * it has the |UNKNOWN_TYPE| prediction;
  // * it does not look like an expiration year or an expiration year was
  //   already found;
  // * it is filled with a 3-4 digit number;
  for (const auto& field : form_structure) {
    const ServerFieldTypeSet& type_set = field->possible_types();

    // Checks if the field is of |CREDIT_CARD_NUMBER| type.
    if (type_set.find(CREDIT_CARD_NUMBER) != type_set.end()) {
      credit_card_number_found = true;
      continue;
    }
    // Skip the field if no credit card number was found yet.
    if (!credit_card_number_found) {
      continue;
    }

    // Don't consider fields that already have any prediction.
    if (type_set.find(UNKNOWN_TYPE) == type_set.end())
      continue;
    // |UNKNOWN_TYPE| should come alone.
    DCHECK_EQ(1u, type_set.size());

    std::u16string trimmed_value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &trimmed_value);

    // Skip the field if it can be confused with a expiration year.
    if (!found_explicit_expiration_year_field &&
        IsPlausible4DigitExpirationYear(trimmed_value)) {
      continue;
    }

    // Skip the field if its value does not like a CVC value.
    if (!IsPlausibleCreditCardCVCNumber(trimmed_value))
      continue;

    return field.get();
  }
  return nullptr;
}

// Iff the CVC of the credit card is known, find the first field with this
// value (also set |properties_mask| to |kKnownValue|). Otherwise, heuristically
// search for the CVC field if any.
AutofillField* GetBestPossibleCVCFieldForUpload(
    const FormStructure& form_structure,
    std::u16string last_unlocked_credit_card_cvc) {
  if (!last_unlocked_credit_card_cvc.empty()) {
    AutofillField* result =
        FindFirstFieldWithValue(form_structure, last_unlocked_credit_card_cvc);
    if (result)
      result->properties_mask = FieldPropertiesFlags::kKnownValue;
    return result;
  }

  return HeuristicallyFindCVCFieldForUpload(form_structure);
}

const char* SubmissionSourceToString(SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return "NONE";
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return "SAME_DOCUMENT_NAVIGATION";
    case SubmissionSource::XHR_SUCCEEDED:
      return "XHR_SUCCEEDED";
    case SubmissionSource::FRAME_DETACHED:
      return "FRAME_DETACHED";
    case SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return "DOM_MUTATION_AFTER_XHR";
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return "PROBABLY_FORM_SUBMITTED";
    case SubmissionSource::FORM_SUBMISSION:
      return "FORM_SUBMISSION";
  }
  return "Unknown";
}

// Returns how many fields with type |field_type| may be filled in a form at
// maximum.
size_t TypeValueFormFillingLimit(ServerFieldType field_type) {
  switch (field_type) {
    case CREDIT_CARD_NUMBER:
      return kCreditCardTypeValueFormFillingLimit;
    case ADDRESS_HOME_STATE:
      return kStateTypeValueFormFillingLimit;
    default:
      return kTypeValueFormFillingLimit;
  }
}

// Removes whitespace and `kCharsToBeRemoved` from the `value` and returns it.
std::u16string RemoveWhiteSpaceAndConjugatingCharacters(
    const std::u16string& value) {
  std::u16string sanitized_value;
  base::TrimWhitespace(value, base::TRIM_ALL, &sanitized_value);
  base::RemoveChars(sanitized_value, kCharsToBeRemoved, &sanitized_value);
  return sanitized_value;
}

std::string_view ActionPersistenceToString(
    mojom::ActionPersistence action_persistence) {
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill:
      return "fill";
    case mojom::ActionPersistence::kPreview:
      return "preview";
  }
}

// Returns true if autocomplete=unrecognized (address) fields should receive
// suggestions. On desktop, suggestion can only be triggered for them through
// manual fallbacks. On mobile, it depends on
// `kAutofillSuggestionsForAutocompleteUnrecognizedFieldsOnMobile`.
// Note that this only affects address fields, since credit card fields ignore
// autocomplete=unrecognized.
bool ShouldShowSuggestionsForAutocompleteUnrecognizedFields(
    AutofillSuggestionTriggerSource trigger_source) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(
      features::kAutofillSuggestionsForAutocompleteUnrecognizedFieldsOnMobile);
#else
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackAddress;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

}  // namespace

BrowserAutofillManager::FillingContext::FillingContext(
    const AutofillField& field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::u16string* optional_cvc)
    : filled_field_id(field.global_id()),
      filled_field_signature(field.GetFieldSignature()),
      filled_origin(field.origin),
      original_fill_time(AutofillTickClock::NowTicks()) {
  DCHECK(absl::holds_alternative<const CreditCard*>(profile_or_credit_card) ||
         !optional_cvc);

  if (absl::holds_alternative<const AutofillProfile*>(profile_or_credit_card)) {
    profile_or_credit_card_with_cvc =
        *absl::get<const AutofillProfile*>(profile_or_credit_card);
  } else if (absl::holds_alternative<const CreditCard*>(
                 profile_or_credit_card)) {
    profile_or_credit_card_with_cvc =
        std::make_pair(*absl::get<const CreditCard*>(profile_or_credit_card),
                       optional_cvc ? *optional_cvc : std::u16string());
  }
}

BrowserAutofillManager::FillingContext::~FillingContext() = default;

BrowserAutofillManager::BrowserAutofillManager(AutofillDriver* driver,
                                               AutofillClient* client,
                                               const std::string& app_locale)
    : AutofillManager(driver, client),
      external_delegate_(std::make_unique<AutofillExternalDelegate>(this)),
      app_locale_(app_locale),
      field_filler_(app_locale, client->GetAddressNormalizer()),
      single_field_form_fill_router_(client->CreateSingleFieldFormFillRouter()),
      suggestion_generator_(std::make_unique<AutofillSuggestionGenerator>(
          client,
          client->GetPersonalDataManager())) {
  address_form_event_logger_ =
      std::make_unique<autofill_metrics::AddressFormEventLogger>(
          driver->IsInAnyMainFrame(), form_interactions_ukm_logger(), client);
  credit_card_form_event_logger_ =
      std::make_unique<autofill_metrics::CreditCardFormEventLogger>(
          driver->IsInAnyMainFrame(), form_interactions_ukm_logger(),
          client->GetPersonalDataManager(), client);
  autocomplete_unrecognized_fallback_logger_ = std::make_unique<
      autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger>();

  credit_card_access_manager_ = std::make_unique<CreditCardAccessManager>(
      driver, client, client->GetPersonalDataManager(),
      credit_card_form_event_logger_.get());
}

BrowserAutofillManager::~BrowserAutofillManager() {
  if (has_parsed_forms_) {
    base::UmaHistogramBoolean(
        "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
        has_observed_phone_number_field_);
    base::UmaHistogramBoolean("Autofill.WebOTP.OneTimeCode.FromAutocomplete",
                              has_observed_one_time_code_field_);
  }

  // Process log events and record into UKM when the form is destroyed or
  // removed.
  for (const auto& [form_id, form_structure] : form_structures()) {
    ProcessFieldLogEventsInForm(*form_structure);
  }

  single_field_form_fill_router_->CancelPendingQueries(this);

  address_form_event_logger_->OnDestroyed();
  credit_card_form_event_logger_->OnDestroyed();

  // We don't flush the `queued_vote_uploads_` here because that would trigger
  // network requests in the AutofillDownloadManager, which are managed with
  // by SimpleURLLoaders owned by the AutofillDownloadManager. Destroying the
  // BrowserAutofillManager destroys the AutofillDownloadManager and its
  // SimpleURLLoaders, which would immediately cancel the uploads.
  // As a consequence of this, votes are lost if the user generates blur votes
  // and closes the tab before the votes are sent (due to a navigation).
}

base::WeakPtr<AutofillManager> BrowserAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CreditCardAccessManager* BrowserAutofillManager::GetCreditCardAccessManager() {
  return credit_card_access_manager_.get();
}

void BrowserAutofillManager::ShowAutofillSettings(PopupType popup_type) {
  client().ShowAutofillSettings(popup_type);
}

bool BrowserAutofillManager::ShouldShowScanCreditCard(
    const FormData& form,
    const FormFieldData& field) {
  if (!client().HasCreditCardScanFeature() ||
      !IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field)
    return false;

  bool is_card_number_field =
      autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER &&
      base::ContainsOnlyChars(CreditCard::StripSeparators(field.value),
                              u"0123456789");

  if (!is_card_number_field)
    return false;

  if (IsFormNonSecure(form))
    return false;

  static const int kShowScanCreditCardMaxValueLength = 6;
  return field.value.size() <= kShowScanCreditCardMaxValueLength;
}

PopupType BrowserAutofillManager::GetPopupType(const FormData& form,
                                               const FormFieldData& field) {
  const AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field)
    return PopupType::kUnspecified;

  switch (autofill_field->Type().group()) {
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kUnfillable:
      return PopupType::kUnspecified;

    case FieldTypeGroup::kCreditCard:
      return PopupType::kCreditCards;

    case FieldTypeGroup::kIban:
      return PopupType::kIbans;

    case FieldTypeGroup::kAddress:
      return PopupType::kAddresses;

    case FieldTypeGroup::kName:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kPhone:
    case FieldTypeGroup::kBirthdateField:
      return FormHasAddressField(form) ? PopupType::kAddresses
                                       : PopupType::kPersonalInformation;
  }
}

bool BrowserAutofillManager::ShouldShowCardsFromAccountOption(
    const FormData& form,
    const FormFieldData& field) {
  // Check whether we are dealing with a credit card field.
  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field ||
      autofill_field->Type().group() != FieldTypeGroup::kCreditCard ||
      // Exclude CVC and card type fields, because these will not have
      // suggestions available after the user opts in.
      autofill_field->Type().GetStorableType() ==
          CREDIT_CARD_VERIFICATION_CODE ||
      autofill_field->Type().GetStorableType() == CREDIT_CARD_TYPE) {
    return false;
  }

  if (IsFormNonSecure(form))
    return false;

  return client().GetPersonalDataManager()->ShouldShowCardsFromAccountOption();
}

void BrowserAutofillManager::OnUserAcceptedCardsFromAccountOption() {
  client().GetPersonalDataManager()->OnUserAcceptedCardsFromAccountOption();
}

void BrowserAutofillManager::RefetchCardsAndUpdatePopup(
    const FormData& form,
    const FormFieldData& field_data) {
  AutofillField* autofill_field = GetAutofillField(form, field_data);
  ServerFieldType field_type = autofill_field
                                   ? autofill_field->Type().GetStorableType()
                                   : CREDIT_CARD_NUMBER;
  DCHECK_EQ(FieldTypeGroup::kCreditCard,
            GroupTypeOfServerFieldType(field_type));

  bool should_display_gpay_logo = false;
  auto cards = GetCreditCardSuggestions(field_data, field_type,
                                        should_display_gpay_logo);
  DCHECK(!cards.empty());
  external_delegate_->OnSuggestionsReturned(
      field_data.global_id(), cards,
      AutofillSuggestionTriggerSource::kShowCardsFromAccount,
      should_display_gpay_logo);
}

bool BrowserAutofillManager::ShouldParseForms() {
  bool autofill_enabled = IsAutofillEnabled();
  // If autofill is disabled but the password manager is enabled, we still
  // need to parse the forms and query the server as the password manager
  // depends on server classifications.
  bool password_manager_enabled = client().IsPasswordManagerEnabled();
  signin_state_for_metrics_ =
      client().GetPersonalDataManager()
          ? client()
                .GetPersonalDataManager()
                ->GetPaymentsSigninStateForMetrics()
          : AutofillMetrics::PaymentsSigninState::kUnknown;
  if (!has_logged_autofill_enabled_) {
    AutofillMetrics::LogIsAutofillEnabledAtPageLoad(autofill_enabled,
                                                    signin_state_for_metrics_);
    AutofillMetrics::LogIsAutofillProfileEnabledAtPageLoad(
        IsAutofillProfileEnabled(), signin_state_for_metrics_);
    AutofillMetrics::LogIsAutofillCreditCardEnabledAtPageLoad(
        IsAutofillPaymentMethodsEnabled(), signin_state_for_metrics_);
    has_logged_autofill_enabled_ = true;
  }

  // Enable the parsing also for the password manager, so that we fetch server
  // classifications if the password manager is enabled but autofill is
  // disabled.
  return autofill_enabled || password_manager_enabled;
}

void BrowserAutofillManager::OnFormSubmittedImpl(const FormData& form,
                                                 bool known_success,
                                                 SubmissionSource source) {
  base::UmaHistogramEnumeration("Autofill.FormSubmission.PerProfileType",
                                client().GetProfileType());
  LOG_AF(log_manager()) << LoggingScope::kSubmission
                        << LogMessage::kFormSubmissionDetected << Br{}
                        << "known_success: " << known_success << Br{}
                        << "source: " << SubmissionSourceToString(source)
                        << Br{} << form;

  // Always upload page language metrics.
  LogLanguageMetrics(client().GetLanguageState());

  // Always let the value patterns metric upload data.
  LogValuePatternsMetric(form);

  // Note that `ValidateSubmittedForm()` returns nullptr in incognito mode.
  // Consequently, in incognito mode Autofill doesn't:
  // - Import
  // - Vote
  // - Collect any key metrics (since they are conditioned form submission - see
  //  `FormEventLoggerBase::OnWillSubmitForm()`)
  // - Collect profile token quality observations
  std::unique_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  CHECK(!client().IsOffTheRecord() || !submitted_form);
  if (!submitted_form) {
    // We always give Autocomplete a chance to save the data.
    // TODO(crbug.com/1467623): Verify frequency of plus address (or the other
    // type(s) checked for below, for that matter) slipping through in this code
    // path.
    single_field_form_fill_router_->OnWillSubmitForm(
        form, submitted_form.get(), client().IsAutocompleteEnabled());
    return;
  }

  form_submitted_timestamp_ = AutofillTickClock::NowTicks();

  // Log metrics about the autocomplete attribute usage in the submitted form.
  LogAutocompletePredictionCollisionTypeMetrics(*submitted_form);

  // Log interaction time metrics for the ablation study.
  if (!initial_interaction_timestamp_.is_null()) {
    base::TimeDelta time_from_interaction_to_submission =
        AutofillTickClock::NowTicks() - initial_interaction_timestamp_;
    DenseSet<FormType> form_types = submitted_form->GetFormTypes();
    bool card_form = base::Contains(form_types, FormType::kCreditCardForm);
    bool address_form = base::Contains(form_types, FormType::kAddressForm);
    if (card_form) {
      credit_card_form_event_logger_->SetTimeFromInteractionToSubmission(
          time_from_interaction_to_submission);
    }
    if (address_form) {
      address_form_event_logger_->SetTimeFromInteractionToSubmission(
          time_from_interaction_to_submission);
    }
  }

  plus_addresses::PlusAddressService* plus_address_service =
      client().GetPlusAddressService();

  FormData form_for_autocomplete = submitted_form->ToFormData();
  int num_fields_where_context_menu_was_shown = 0;
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    if (submitted_form->field(i)->Type().GetStorableType() ==
        CREDIT_CARD_VERIFICATION_CODE) {
      // However, if Autofill has recognized a field as CVC, that shouldn't be
      // saved.
      form_for_autocomplete.fields[i].should_autocomplete = false;
    }
    if (plus_address_service &&
        plus_address_service->IsPlusAddress(
            base::UTF16ToUTF8(submitted_form->field(i)->value))) {
      // Similarly to CVC, any plus addresses needn't be saved to autocomplete.
      // Note that the feature is experimental, and `plus_address_service` will
      // be null if the feature is not enabled (it's disabled by default).
      form_for_autocomplete.fields[i].should_autocomplete = false;
    }

    // If the field was edited by the user and there existed an autofillable
    // value for the field, log whether the value on submission is same as the
    // autofillable value.
    if (submitted_form->field(i)
            ->value_not_autofilled_over_existing_value_hash() &&
        (submitted_form->field(i)->properties_mask & kUserTyped)) {
      // Compare and record if the currently filled value is same as the
      // non-empty value that was to be autofilled in the field.
      std::u16string sanitized_submitted_value =
          RemoveWhiteSpaceAndConjugatingCharacters(
              submitted_form->field(i)->value);
      AutofillMetrics::
          LogIsValueNotAutofilledOverExistingValueSameAsSubmittedValue(
              *submitted_form->field(i)
                   ->value_not_autofilled_over_existing_value_hash() ==
              base::FastHash(base::UTF16ToUTF8(sanitized_submitted_value)));
    }

    // The context menu was shown in this field, log the metrics by
    // autocomplete type, form type and autofill type prediction of the field.
    if (submitted_form->field(i)->was_context_menu_shown()) {
      num_fields_where_context_menu_was_shown++;
      LogContextMenuImpressionsForSubmittedField(*submitted_form->field(i));
    }
  }

  AutofillMetrics::LogContextMenuImpressionsForForm(
      num_fields_where_context_menu_was_shown);

  single_field_form_fill_router_->OnWillSubmitForm(
      form_for_autocomplete, submitted_form.get(),
      client().IsAutocompleteEnabled());

  if (IsAutofillProfileEnabled()) {
    address_form_event_logger_->OnWillSubmitForm(signin_state_for_metrics_,
                                                 *submitted_form);
  }
  if (IsAutofillPaymentMethodsEnabled()) {
    credit_card_form_event_logger_->OnWillSubmitForm(signin_state_for_metrics_,
                                                     *submitted_form);
  }

  submitted_form->set_submission_source(source);

  // Update Personal Data with the form's submitted data.
  // Also triggers offering local/upload credit card save, if applicable.
  if (submitted_form->IsAutofillable()) {
    FormDataImporter* form_data_importer = client().GetFormDataImporter();
    form_data_importer->ImportAndProcessFormData(
        *submitted_form, IsAutofillProfileEnabled(),
        IsAutofillPaymentMethodsEnabled());
    // Associate the form signatures of recently submitted address/credit card
    // forms to `submitted_form`, if it is an address/credit card form itself.
    // This information is attached to the vote.
    if (base::FeatureList::IsEnabled(features::kAutofillAssociateForms)) {
      if (absl::optional<FormStructure::FormAssociations> associations =
              form_data_importer->GetFormAssociations(
                  submitted_form->form_signature())) {
        submitted_form->set_form_associations(*associations);
      }
    }
  }

  MaybeStartVoteUploadProcess(std::move(submitted_form),
                              /*observed_submission=*/true);

  // TODO(crbug.com/803334): Add FormStructure::Clone() method.
  // Create another FormStructure instance.
  submitted_form = ValidateSubmittedForm(form);
  DCHECK(submitted_form);
  if (!submitted_form)
    return;

  submitted_form->set_submission_source(source);

  if (IsAutofillProfileEnabled()) {
    address_form_event_logger_->OnFormSubmitted(signin_state_for_metrics_,
                                                *submitted_form);
  }
  if (IsAutofillPaymentMethodsEnabled()) {
    credit_card_form_event_logger_->OnFormSubmitted(signin_state_for_metrics_,
                                                    *submitted_form);
    if (touch_to_fill_delegate_) {
      touch_to_fill_delegate_->LogMetricsAfterSubmission(*submitted_form);
    }
  }

  ProfileTokenQuality::SaveObservationsForFilledFormForAllSubmittedProfiles(
      *submitted_form, form, *client().GetPersonalDataManager());
}

bool BrowserAutofillManager::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    bool observed_submission) {
  // It is possible for |client().GetPersonalDataManager()| to be null, such as
  // when used in the Android webview.
  if (!client().GetPersonalDataManager()) {
    return false;
  }

  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  std::vector<AutofillProfile*> profiles =
      client().GetPersonalDataManager()->GetProfiles();
  if (observed_submission && form_structure->IsAutofillable()) {
    AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
        client().GetPersonalDataManager()->GetProfiles().size());
  }

  const std::vector<CreditCard*>& credit_cards =
      client().GetPersonalDataManager()->GetCreditCards();

  if (profiles.empty() && credit_cards.empty())
    return false;

  if (form_structure->field_count() * (profiles.size() + credit_cards.size()) >=
      kMaxTypeMatchingCalls)
    return false;

  // Copy the profile and credit card data, so that it can be accessed on a
  // separate thread.
  std::vector<AutofillProfile> copied_profiles;
  copied_profiles.reserve(profiles.size());
  for (const AutofillProfile* profile : profiles)
    copied_profiles.push_back(*profile);

  std::vector<CreditCard> copied_credit_cards;
  copied_credit_cards.reserve(credit_cards.size());
  for (const CreditCard* card : credit_cards)
    copied_credit_cards.push_back(*card);

  // Annotate the form with the source language of the page.
  form_structure->set_current_page_language(GetCurrentPageLanguage());

  // Attach the Randomized Encoder.
  form_structure->set_randomized_encoder(
      RandomizedEncoder::Create(client().GetPrefs()));

  // Determine |ADDRESS_HOME_STATE| as a possible types for the fields in the
  // |form_structure| with the help of |AlternativeStateNameMap|.
  // |AlternativeStateNameMap| can only be accessed on the main UI thread.
  PreProcessStateMatchingTypes(copied_profiles, form_structure.get());

  // Ownership of |form_structure| is passed to the
  // BrowserAutofillManager::UploadVotesAndLogQuality() call.
  FormStructure* raw_form = form_structure.get();

  base::OnceClosure call_after_determine_field_types = base::BindOnce(
      &BrowserAutofillManager::UploadVotesAndLogQuality,
      weak_ptr_factory_.GetWeakPtr(), std::move(form_structure),
      initial_interaction_timestamp_, AutofillTickClock::NowTicks(),
      observed_submission, client().GetUkmSourceId());

  // If the form was not submitted (e.g. the user just removed the focus from
  // the form), it's possible that later modifications lead to more accurate
  // votes. In this case we just want to cache the upload and have a chance to
  // override it with better data.
  if (!observed_submission) {
    call_after_determine_field_types = base::BindOnce(
        &BrowserAutofillManager::StoreUploadVotesAndLogQualityCallback,
        weak_ptr_factory_.GetWeakPtr(), raw_form->form_signature(),
        std::move(call_after_determine_field_types));
  }

  if (!vote_upload_task_runner_) {
    // If the priority is BEST_EFFORT, the task can be preempted, which is
    // thought to cause high memory usage (as memory is retained by the task
    // while it is preempted), https://crbug.com/974249
    vote_upload_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }

  vote_upload_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &BrowserAutofillManager::DeterminePossibleFieldTypesForUpload,
          std::move(copied_profiles), std::move(copied_credit_cards),
          last_unlocked_credit_card_cvc_, app_locale_, observed_submission,
          raw_form),
      std::move(call_after_determine_field_types));

  return true;
}

void BrowserAutofillManager::UpdatePendingForm(const FormData& form) {
  // Process the current pending form if different than supplied |form|.
  if (pending_form_data_ && !pending_form_data_->SameFormAs(form)) {
    ProcessPendingFormForUpload();
  }
  // A new pending form is assigned.
  pending_form_data_ = std::make_unique<FormData>(form);
}

void BrowserAutofillManager::ProcessPendingFormForUpload() {
  if (!pending_form_data_)
    return;

  // We get the FormStructure corresponding to |pending_form_data_|, used in the
  // upload process. |pending_form_data_| is reset.
  std::unique_ptr<FormStructure> upload_form =
      ValidateSubmittedForm(*pending_form_data_);
  pending_form_data_.reset();
  if (!upload_form)
    return;

  MaybeStartVoteUploadProcess(std::move(upload_form),
                              /*observed_submission=*/false);
}

bool BrowserAutofillManager::WasSuggestionPreviouslyHidden(
    const FormData& form,
    const FormFieldData& field,
    Suggestion::BackendId backend_id,
    AutofillSuggestionTriggerSource trigger_source) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return false;
  }
  // Getting the filling-relevant fields so that suggestions are based only on
  // those fields. BrowserAutofillManager::GetFieldFillingSkipReasons() assumes
  // that the passed FormData and FormStructure have the same size. If it's not
  // the case we just assume as a fallback that all fields are relevant.
  std::vector<FieldFillingSkipReason> skip_reasons =
      form.fields.size() == form_structure->field_count()
          ? GetFieldFillingSkipReasons(
                form, *form_structure, field, autofill_field->section,
                /*optional_credit_card=*/nullptr, kAllServerFieldTypes,
                /*optional_type_groups_originally_filled=*/nullptr,
                /*skip_unrecognized_autocomplete_fields=*/
                trigger_source !=
                    AutofillSuggestionTriggerSource::kManualFallbackAddress,
                /*is_refill=*/false)
          : std::vector<FieldFillingSkipReason>(
                form_structure->field_count(),
                FieldFillingSkipReason::kNotSkipped);
  return suggestion_generator_->WasProfileSuggestionPreviouslyHidden(
      CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field), backend_id,
      skip_reasons);
}

void BrowserAutofillManager::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const TimeTicks timestamp) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  // Log events when user edits the field.
  // If the user types into the same field multiple times, repeated
  // TypingFieldLogEvents are coalesced.
  autofill_field->AppendLogEventIfNotRepeated(TypingFieldLogEvent{
      .has_value_after_typing = ToOptionalBoolean(!field.value.empty())});

  UpdatePendingForm(form);

  uint32_t profile_form_bitmask = 0;
  if (!user_did_type_ || autofill_field->is_autofilled) {
    form_interactions_ukm_logger()->LogTextFieldDidChange(*form_structure,
                                                          *autofill_field);
    profile_form_bitmask = data_util::DetermineGroups(*form_structure);
  }

  auto* logger = GetEventFormLogger(*autofill_field);
  if (!autofill_field->is_autofilled) {
    if (logger)
      logger->OnTypedIntoNonFilledField();
  }

  if (!user_did_type_) {
    user_did_type_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_TYPE, autofill_field->Type().group(),
        client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  }

  if (autofill_field->is_autofilled) {
    autofill_field->is_autofilled = false;
    autofill_field->set_previously_autofilled(true);
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD,
        autofill_field->Type().group(),
        client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);

    if (logger)
      logger->OnEditedAutofilledField();

    if (!user_did_edit_autofilled_field_) {
      user_did_edit_autofilled_field_ = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE,
          autofill_field->Type().group(),
          client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
    }
  }

  UpdateInitialInteractionTimestamp(timestamp);

  if (logger)
    logger->OnTextFieldDidChange(autofill_field->global_id());
}

bool BrowserAutofillManager::IsFormNonSecure(const FormData& form) const {
  // Check if testing override applies.
  if (consider_form_as_secure_for_testing_.has_value() &&
      consider_form_as_secure_for_testing_.value()) {
    return false;
  }

  return IsFormOrClientNonSecure(client(), form);
}

void BrowserAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& transformed_box,
    AutofillSuggestionTriggerSource trigger_source) {
  if (base::FeatureList::IsEnabled(features::kAutofillDisableFilling)) {
    return;
  }

  external_delegate_->SetCurrentDataListValues(field.datalist_options);
  external_delegate_->OnQuery(form, field, transformed_box);

  std::vector<Suggestion> suggestions;
  SuggestionsContext context;
  GetAvailableSuggestions(form, field, trigger_source, &suggestions, &context);

  const bool form_element_was_clicked =
      trigger_source ==
      AutofillSuggestionTriggerSource::kFormControlElementClicked;

  if (context.is_autofill_available) {
    switch (context.suppress_reason) {
      case SuppressReason::kNotSuppressed:
        break;

      case SuppressReason::kAblation:
        single_field_form_fill_router_->CancelPendingQueries(this);
        external_delegate_->OnSuggestionsReturned(field.global_id(),
                                                  suggestions, trigger_source);
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: Ablation experiment";
        return;

      case SuppressReason::kInsecureForm:
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: Insecure form";
        return;
      case SuppressReason::kAutocompleteOff:
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: autocomplete=off";
        return;
      case SuppressReason::kAutocompleteUnrecognized:
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: autocomplete=unrecognized";
        return;
    }

    if (!suggestions.empty()) {
      if (context.is_filling_credit_card) {
        AutofillMetrics::LogIsQueriedCreditCardFormSecure(
            context.is_context_secure);
      } else {
        AutofillMetrics::LogAddressSuggestionsCount(suggestions.size());
      }
    }
  }

  if (suggestions.empty() &&
      (field.form_control_type == FormControlType::kTextArea ||
       field.form_control_type == FormControlType::kContentEditable)) {
    if (absl::optional<Suggestion> maybe_compose_suggestion =
            MaybeGetComposeSuggestion(field)) {
      suggestions.push_back(*std::move(maybe_compose_suggestion));
    }
  }

  auto ShouldOfferSingleFieldFormFill = [&] {
    // Do not offer single field form fill if there are already suggestions.
    if (!suggestions.empty()) {
      return false;
    }

    // Do not offer single field form fill suggestions for credit card number,
    // cvc, and expiration date related fields. Standalone cvc fields (used to
    // re-authenticate the use of a credit card the website has on file) will be
    // handled separately because those have the field type
    // CREDIT_CARD_STANDALONE_VERIFICATION_CODE.
    ServerFieldType server_type =
        context.focused_field ? context.focused_field->Type().GetStorableType()
                              : UNKNOWN_TYPE;
    if (data_util::IsCreditCardExpirationType(server_type) ||
        server_type == CREDIT_CARD_VERIFICATION_CODE ||
        server_type == CREDIT_CARD_NUMBER) {
      return false;
    }

    // Do not offer single field form fill suggestions if popups are suppressed
    // due to an unrecognized autocomplete attribute. Note that in the context
    // of Autofill, the popup for credit card related fields is not getting
    // suppressed due to an unrecognized autocomplete attribute.
    // TODO(crbug.com/1344590): Revisit here to see whether we should offer IBAN
    // filling for fields with unrecognized autocomplete attribute
    if (context.suppress_reason == SuppressReason::kAutocompleteUnrecognized) {
      return false;
    }

    // Therefore, we check the attribute explicitly.
    if (context.focused_field && context.focused_field->Type().html_type() ==
                                     HtmlFieldType::kUnrecognized) {
      return false;
    }

    // Finally, check that the scheme is secure.
    if (context.suppress_reason == SuppressReason::kInsecureForm) {
      return false;
    }
    return true;
  };

  auto ShouldShowSuggestion = [&] {
    if (fast_checkout_delegate_ &&
        (fast_checkout_delegate_->IsShowingFastCheckoutUI() ||
         (form_element_was_clicked &&
          fast_checkout_delegate_->TryToShowFastCheckout(form, field,
                                                         GetWeakPtr())))) {
      // The Fast Checkout surface is shown, so abort showing regular Autofill
      // UI. Now the flow is controlled by the `FastCheckoutClient` instead of
      // `external_delegate_`.
      // In principle, TTF and Fast Checkout triggering surfaces are different
      // and the two screens should never coincide.
      return false;
    }

    if (ShouldOfferSingleFieldFormFill()) {
      // Suggestions come back asynchronously, so the SingleFieldFormFillRouter
      // will handle sending the results back to the renderer.
      bool handled_by_single_field_form_filler =
          single_field_form_fill_router_->OnGetSingleFieldSuggestions(
              trigger_source, field, client(), weak_ptr_factory_.GetWeakPtr(),
              context);
      if (handled_by_single_field_form_filler) {
        return false;
      }
    }

    single_field_form_fill_router_->CancelPendingQueries(this);
    if (touch_to_fill_delegate_ &&
        (touch_to_fill_delegate_->IsShowingTouchToFill() ||
         (form_element_was_clicked &&
          touch_to_fill_delegate_->TryToShowTouchToFill(form, field)))) {
      // Touch To Fill surface is shown, so abort showing regular Autofill UI.
      // Now the flow is controlled by the |touch_to_fill_delegate_| instead
      // of |external_delegate_|.
      return false;
    }
    return true;
  };

  bool show_suggestion = ShouldShowSuggestion();
  // When focusing on a field, log whether there is a suggestion for the user
  // and whether the suggestion is shown.
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (form_element_was_clicked &&
      GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    autofill_field->AppendLogEventIfNotRepeated(AskForValuesToFillFieldLogEvent{
        .has_suggestion = ToOptionalBoolean(!suggestions.empty()),
        .suggestion_is_shown = ToOptionalBoolean(show_suggestion),
    });
  }
  if (show_suggestion) {
    // Send Autofill suggestions (could be an empty list).
    external_delegate_->OnSuggestionsReturned(field.global_id(), suggestions,
                                              trigger_source,
                                              context.should_display_gpay_logo);
  }
}

bool BrowserAutofillManager::WillFillCreditCardNumber(
    const FormData& form,
    const FormFieldData& triggered_field_data) {
  FormStructure* form_structure = nullptr;
  AutofillField* triggered_field = nullptr;
  if (!GetCachedFormAndField(form, triggered_field_data, &form_structure,
                             &triggered_field)) {
    return false;
  }

  if (triggered_field->Type().GetStorableType() == CREDIT_CARD_NUMBER)
    return true;

  // `form` is the latest version of the form received from the renderer and may
  // be more up to date than the `form_structure` in the cache. Therefore, we
  // need to validate for each `field` in the cache we try to fill whether
  // it still exists in the renderer and whether it is fillable.
  auto IsFillableField = [&form](FieldGlobalId id) {
    auto it = base::ranges::find(form.fields, id, &FormFieldData::global_id);
    return it != form.fields.end() && it->value.empty() && !it->is_autofilled;
  };

  auto IsFillableCreditCardNumberField = [&triggered_field,
                                          &IsFillableField](const auto& field) {
    return field->Type().GetStorableType() == CREDIT_CARD_NUMBER &&
           field->section == triggered_field->section &&
           IsFillableField(field->global_id());
  };

  // This runs O(N^2) in the worst case, but usually there aren't too many
  // credit card number fields in a form.
  return base::ranges::any_of(*form_structure, IsFillableCreditCardNumberField);
}

void BrowserAutofillManager::FillOrPreviewCreditCardForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& field,
    const CreditCard* credit_card,
    const AutofillTriggerDetails& trigger_details) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  credit_card_ = credit_card ? *credit_card : CreditCard();
  bool is_preview = action_persistence != mojom::ActionPersistence::kFill;
  bool is_virtual_card_standalone_cvc =
      credit_card->record_type() == CreditCard::RecordType::kVirtualCard &&
      (autofill_field->Type().GetStorableType() ==
       CREDIT_CARD_STANDALONE_VERIFICATION_CODE);
  bool should_fetch_card =
      !is_preview &&
      (WillFillCreditCardNumber(form, field) || is_virtual_card_standalone_cvc);

  if (should_fetch_card) {
    credit_card_form_event_logger_->OnDidSelectCardSuggestion(
        credit_card_, *form_structure, signin_state_for_metrics_);

    credit_card_form_ = form;
    credit_card_field_ = field;

    // CreditCardAccessManager::FetchCreditCard() will call
    // OnCreditCardFetched() in this class after successfully fetching the card.
    fetched_credit_card_trigger_source_ = trigger_details.trigger_source;
    credit_card_access_manager_->FetchCreditCard(
        credit_card, weak_ptr_factory_.GetWeakPtr());
    return;
  }

  FillOrPreviewDataModelForm(action_persistence, form, field, &credit_card_,
                             /*optional_cvc=*/nullptr, form_structure,
                             autofill_field, trigger_details);
}

void BrowserAutofillManager::FillOrPreviewProfileForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& field,
    const AutofillProfile& profile,
    const AutofillTriggerDetails& trigger_details) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;
  FillOrPreviewDataModelForm(action_persistence, form, field, &profile,
                             /*optional_cvc=*/nullptr, form_structure,
                             autofill_field, trigger_details);
}

void BrowserAutofillManager::UndoAutofill(
    mojom::ActionPersistence action_persistence,
    FormData form,
    const FormFieldData& trigger_field) {
  if (!form_autofill_history_.HasHistory(trigger_field.global_id())) {
    LOG_AF(log_manager())
        << "Could not undo the filling operation on field "
        << trigger_field.global_id()
        << " because history was dropped upon reaching history limit of "
        << kMaxStorableFieldFillHistory;
    return;
  }
  FormAutofillHistory::FillOperation operation =
      form_autofill_history_.GetLastFillingOperationForField(
          trigger_field.global_id());

  // Only process fields whose last autofill operation is the one being undone
  // on `field`. And among those fields, skip the ones that have been modified
  // since that operation. Remove the ones to be skipped so that we only pass
  // fields to be undone by the renderer.
  base::EraseIf(form.fields, [this, &operation](const FormFieldData& field) {
    return !field.is_autofilled ||
           form_autofill_history_.GetLastFillingOperationForField(
               field.global_id()) != operation;
  });

  for (FormFieldData& field : form.fields) {
    const FormAutofillHistory::FieldFillingEntry& previous_state =
        operation.GetFieldFillingEntry(field.global_id());
    // Update `field` to send for the renderer.
    field.value = previous_state.value;
    field.is_autofilled = previous_state.is_autofilled;

    // If the field is cached update the cached autofill state in the browser.
    // TODO(crbug.com/1345089): Consider updating the value too.
    if (AutofillField* autofill_field = GetAutofillField(form, field)) {
      autofill_field->is_autofilled = previous_state.is_autofilled;
      autofill_field->set_autofill_source_profile_guid(
          previous_state.autofill_source_profile_guid);
    }
  }

  // Since Undo only affects fields that were already filled, and only sets
  // values to fields to something that already existed in it prior to the
  // filling, it is okay to bypass the filling security checks and hence passing
  // dummy values for `triggered_origin` and `field_type_map`.
  driver().ApplyFormAction(mojom::ActionType::kUndo, action_persistence, form,
                           url::Origin(),
                           /*field_type_map=*/{});
  // Do not clear history on previews as it might be used for future previews or
  // for the filling.
  if (action_persistence == mojom::ActionPersistence::kFill) {
    form_autofill_history_.EraseFormFillEntry(operation);
  }
}

void BrowserAutofillManager::FillCreditCardForm(
    const FormData& form,
    const FormFieldData& field,
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const AutofillTriggerDetails& trigger_details) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field) ||
      !driver().RendererIsAvailable()) {
    return;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  FillOrPreviewDataModelForm(mojom::ActionPersistence::kFill, form, field,
                             &credit_card, &cvc, form_structure, autofill_field,
                             trigger_details,
                             /*is_refill=*/false);
}

void BrowserAutofillManager::FillOrPreviewVirtualCardInformation(
    mojom::ActionPersistence action_persistence,
    const std::string& guid,
    const FormData& form,
    const FormFieldData& field,
    const AutofillTriggerDetails& trigger_details) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field) ||
      !RefreshDataModels() || !driver().RendererIsAvailable()) {
    return;
  }

  const CreditCard* credit_card =
      client().GetPersonalDataManager()->GetCreditCardByGUID(guid);
  if (credit_card) {
    CreditCard copy = *credit_card;
    copy.set_record_type(CreditCard::RecordType::kVirtualCard);
    FillOrPreviewCreditCardForm(action_persistence, form, field, &copy,
                                trigger_details);
  }
}

void BrowserAutofillManager::OnFocusNoLongerOnFormImpl(
    bool had_interacted_form) {
  // For historical reasons, Chrome takes action on this message only if focus
  // was previously on a form with which the user had interacted.
  // TODO(crbug.com/1140473): Remove need for this short-circuit.
  if (!had_interacted_form)
    return;

  ProcessPendingFormForUpload();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // There is no way of determining whether ChromeVox is in use, so assume it's
  // being used.
  external_delegate_->OnAutofillAvailabilityEvent(
      mojom::AutofillState::kNoSuggestions);
#else
  if (external_delegate_->HasActiveScreenReader()) {
    external_delegate_->OnAutofillAvailabilityEvent(
        mojom::AutofillState::kNoSuggestions);
  }
#endif
}

void BrowserAutofillManager::OnFocusOnFormFieldImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  // Notify installed screen readers if the focus is on a field for which there
  // are suggestions to present. Ignore if a screen reader is not present. If
  // the platform is ChromeOS, then assume ChromeVox is in use as there is no
  // way of determining whether it's being used from this point in the code.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!external_delegate_->HasActiveScreenReader())
    return;
#endif

  // TODO(https://crbug.com/848427): Add metrics for performance impact.
  std::vector<Suggestion> suggestions;
  SuggestionsContext context;
  // This code path checks if suggestions to be announced to a screen reader are
  // available when the focus on a form field changes. This cannot happen in
  // `OnAskForValuesToFillImpl()`, since the `AutofillState` is a sticky flag
  // and needs to be reset when a non-autofillable field is focused.
  // The suggestion trigger source doesn't influence the set of suggestions
  // generated, but only the way suggestions behave when they are accepted. For
  // this reason, checking whether suggestions are available can be done with
  // the `kUnspecified` suggestion trigger source.
  GetAvailableSuggestions(form, field,
                          AutofillSuggestionTriggerSource::kUnspecified,
                          &suggestions, &context);
  external_delegate_->OnAutofillAvailabilityEvent(
      (context.suppress_reason == SuppressReason::kNotSuppressed &&
       !suggestions.empty())
          ? mojom::AutofillState::kAutofillAvailable
          : mojom::AutofillState::kNoSuggestions);
}

void BrowserAutofillManager::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  // TODO(crbug.com/814961): Handle select control change.
}

void BrowserAutofillManager::OnDidFillAutofillFormDataImpl(
    const FormData& form,
    const TimeTicks timestamp) {
  UpdatePendingForm(form);

  // Find the FormStructure that corresponds to |form|. Use default form type if
  // form is not present in our cache, which will happen rarely.

  FormStructure* form_structure = FindCachedFormById(form.global_id());
  DenseSet<FormType> form_types;
  if (form_structure) {
    form_types = form_structure->GetFormTypes();
  }

  uint32_t profile_form_bitmask =
      form_structure ? data_util::DetermineGroups(*form_structure) : 0;

  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_AUTOFILL, form_types,
      client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  if (!user_did_autofill_) {
    user_did_autofill_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE, form_types,
        client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void BrowserAutofillManager::DidShowSuggestions(bool has_autofill_suggestions,
                                                const FormData& form,
                                                const FormFieldData& field) {
  NotifyObservers(&Observer::OnSuggestionsShown);

  if (!has_autofill_suggestions) {
    // If suggestions are not from Autofill, then it means they are from
    // Autocomplete.
    AutofillMetrics::OnAutocompleteSuggestionsShown();
    return;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  uint32_t profile_form_bitmask = data_util::DetermineGroups(*form_structure);
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::SUGGESTIONS_SHOWN, autofill_field->Type().group(),
      client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);

  if (!did_show_suggestions_) {
    did_show_suggestions_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, autofill_field->Type().group(),
        client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  }

  auto* logger = GetEventFormLogger(*autofill_field);
  if (logger) {
    logger->OnDidShowSuggestions(*form_structure, *autofill_field,
                                 form_structure->form_parsed_timestamp(),
                                 signin_state_for_metrics_,
                                 client().IsOffTheRecord());
  } else if (autofill_field->ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Suggestions were triggered on an ac=unrecognized address field.
    autocomplete_unrecognized_fallback_logger_->OnDidShowSuggestions();
  }

  if (autofill_field->Type().group() == FieldTypeGroup::kCreditCard &&
      IsCreditCardFidoAuthenticationEnabled()) {
    credit_card_access_manager_->PrepareToFetchCreditCard();
  }
}

void BrowserAutofillManager::OnHidePopupImpl() {
  if (!IsAutofillEnabled())
    return;

  single_field_form_fill_router_->CancelPendingQueries(this);
  client().HideAutofillPopup(PopupHidingReason::kRendererEvent);
  if (fast_checkout_delegate_) {
    fast_checkout_delegate_->HideFastCheckout(/*allow_further_runs=*/false);
  }
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->HideTouchToFill();
  }
}

bool BrowserAutofillManager::GetDeletionConfirmationText(
    const std::u16string& value,
    PopupItemId popup_item_id,
    Suggestion::BackendId backend_id,
    std::u16string* title,
    std::u16string* body) {
  if (popup_item_id == PopupItemId::kAutocompleteEntry) {
    if (title)
      title->assign(value);
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
    }

    return true;
  }

  if (popup_item_id != PopupItemId::kAddressEntry &&
      popup_item_id != PopupItemId::kCreditCardEntry) {
    return false;
  }

  const CreditCard* credit_card = GetCreditCard(backend_id);
  const AutofillProfile* profile = GetProfile(backend_id);

  if (credit_card) {
    return credit_card_access_manager_->GetDeletionConfirmationText(
        credit_card, title, body);
  }

  if (profile) {
    if (profile->record_type() != AutofillProfile::LOCAL_PROFILE)
      return false;

    if (title) {
      std::u16string street_address = profile->GetRawInfo(ADDRESS_HOME_CITY);
      if (!street_address.empty())
        title->swap(street_address);
      else
        title->assign(value);
    }
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
    }

    return true;
  }

  return false;  // The ID was valid. The entry may have been deleted in a race.
}

bool BrowserAutofillManager::RemoveAutofillProfileOrCreditCard(
    Suggestion::BackendId backend_id) {
  const CreditCard* credit_card = GetCreditCard(backend_id);
  if (credit_card) {
    return credit_card_access_manager_->DeleteCard(credit_card);
  }

  const AutofillProfile* profile = GetProfile(backend_id);
  if (profile) {
    bool is_local = profile->record_type() == AutofillProfile::LOCAL_PROFILE;
    if (is_local)
      client().GetPersonalDataManager()->RemoveByGUID(profile->guid());

    return is_local;
  }

  return false;  // The ID was valid. The entry may have been deleted in a race.
}

void BrowserAutofillManager::RemoveCurrentSingleFieldSuggestion(
    const std::u16string& name,
    const std::u16string& value,
    PopupItemId popup_item_id) {
  single_field_form_fill_router_->OnRemoveCurrentSingleFieldSuggestion(
      name, value, popup_item_id);
}

void BrowserAutofillManager::OnSingleFieldSuggestionSelected(
    const std::u16string& value,
    PopupItemId popup_item_id,
    const FormData& form,
    const FormFieldData& field) {
  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(
      value, popup_item_id);

  AutofillField* autofill_trigger_field = GetAutofillField(form, field);
  if (!autofill_trigger_field) {
    return;
  }
  autofill_trigger_field->AppendLogEventIfNotRepeated(TriggerFillFieldLogEvent{
      .data_type =
          GetEventTypeFromSingleFieldSuggestionPopupItemId(popup_item_id),
      .associated_country_code = "",
      .timestamp = AutofillClock::Now()});
}

void BrowserAutofillManager::OnUserHideSuggestions(const FormData& form,
                                                   const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  auto* logger = GetEventFormLogger(*autofill_field);
  if (logger)
    logger->OnUserHideSuggestions(*form_structure, *autofill_field);
}

bool BrowserAutofillManager::ShouldClearPreviewedForm() {
  return credit_card_access_manager_->ShouldClearPreviewedForm();
}

void BrowserAutofillManager::OnSelectOrSelectListFieldOptionsDidChangeImpl(
    const FormData& form) {
  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    // If AutofillParseAsync is enabled, the form has just been parsed
    // asynchronously if necessary.
    form_structure = ParseForm(form, form_structure);
  }
  if (!form_structure)
    return;

  driver().SendAutofillTypePredictionsToRenderer({form_structure});

  if (ShouldTriggerRefill(*form_structure))
    TriggerRefill(
        form, {.trigger_source = AutofillTriggerSource::kSelectOptionsChanged});
}

void BrowserAutofillManager::OnJavaScriptChangedAutofilledValueImpl(
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& old_value) {
  // Log to chrome://autofill-internals that a field's value was set by
  // JavaScript.
  auto StructureOfString = [](std::u16string str) {
    for (auto& c : str) {
      if (base::IsAsciiAlpha(c)) {
        c = 'a';
      } else if (base::IsAsciiDigit(c)) {
        c = '0';
      } else if (base::IsAsciiWhitespace(c)) {
        c = ' ';
      } else {
        c = '$';
      }
    }
    return str;
  };
  auto GetFieldNumber = [&]() {
    for (size_t i = 0; i < form.fields.size(); ++i) {
      if (form.fields[i].global_id() == field.global_id())
        return base::StringPrintf("Field %zu", i);
    }
    return std::string("unknown");
  };
  LogBuffer change(IsLoggingActive(log_manager()));
  LOG_AF(change) << Tag{"div"} << Attrib{"class", "form"};
  LOG_AF(change) << field << Br{};
  LOG_AF(change) << "Old value structure: '"
                 << StructureOfString(old_value.substr(0, 80)) << "'" << Br{};
  LOG_AF(change) << "New value structure: '"
                 << StructureOfString(field.value.substr(0, 80)) << "'";
  LOG_AF(log_manager()) << LoggingScope::kWebsiteModifiedFieldValue
                        << LogMessage::kJavaScriptChangedAutofilledValue << Br{}
                        << Tag{"table"} << Tr{} << GetFieldNumber()
                        << std::move(change);

  AnalyzeJavaScriptChangedAutofilledValue(form, field);
  MaybeTriggerRefillForExpirationDate(
      form, field, old_value,
      {.trigger_source =
           AutofillTriggerSource::kJavaScriptChangedAutofilledValue});
}

void BrowserAutofillManager::MaybeTriggerRefillForExpirationDate(
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& old_value,
    const AutofillTriggerDetails& trigger_details) {
  // We currently support a single case of refilling credit card expiration
  // dates: If we filled the expiration date in a format "05/2023" and the
  // website turned it into "05 / 20" (i.e. it broke the year by cutting the
  // last two digits instead of stripping the first two digits).
  constexpr size_t kSupportedLength = base::StringPiece("MM/YYYY").size();
  if (old_value.length() != kSupportedLength)
    return;
  if (old_value == field.value)
    return;

  static constexpr char16_t kFormatRegEx[] =
      uR"(^(\d\d)(\s?[/-]?\s?)?(\d\d|\d\d\d\d)$)";
  std::vector<std::u16string> old_groups;
  if (!MatchesRegex<kFormatRegEx>(old_value, &old_groups))
    return;
  DCHECK_EQ(old_groups.size(), 4u);

  std::vector<std::u16string> new_groups;
  if (!MatchesRegex<kFormatRegEx>(field.value, &new_groups))
    return;
  DCHECK_EQ(new_groups.size(), 4u);

  int old_month, old_year, new_month, new_year;
  if (!base::StringToInt(old_groups[1], &old_month) ||
      !base::StringToInt(old_groups[3], &old_year) ||
      !base::StringToInt(new_groups[1], &new_month) ||
      !base::StringToInt(new_groups[3], &new_year) ||
      old_groups[3].size() != 4 || new_groups[3].size() != 2 ||
      old_month != new_month ||
      // We need to refill if the first two digits of the year were preserved.
      old_year / 100 != new_year) {
    return;
  }

  std::u16string refill_value = field.value;
  CHECK(refill_value.size() >= 2);
  refill_value[refill_value.size() - 1] = '0' + (old_year % 10);
  refill_value[refill_value.size() - 2] = '0' + ((old_year % 100) / 10);

  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (form_structure && ShouldTriggerRefill(*form_structure)) {
    FillingContext* filling_context = GetFillingContext(*form_structure);
    DCHECK(filling_context);  // This is enforced by ShouldTriggerRefill.
    filling_context->forced_fill_values[field.global_id()] = refill_value;
    ScheduleRefill(form, trigger_details);
  }
}

void BrowserAutofillManager::AnalyzeJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FormFieldData& field) {
  // We are interested in reporting the events where JavaScript resets an
  // autofilled value immediately after filling. For a reset, the value
  // needs to be empty.
  if (!field.value.empty())
    return;

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  FillingContext* filling_context = GetFillingContext(*form_structure);
  if (!filling_context)
    return;

  base::TimeTicks now = AutofillTickClock::NowTicks();
  base::TimeDelta delta = now - filling_context->original_fill_time;

  // If the filling happened too long ago, maybe this is just an effect of
  // the user pressing a "reset form" button.
  if (delta >= kLimitBeforeRefill)
    return;

  auto* logger = GetEventFormLogger(*autofill_field);
  if (logger) {
    logger->OnAutofilledFieldWasClearedByJavaScriptShortlyAfterFill(
        *form_structure);
  }
}

void BrowserAutofillManager::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card) {
  if (result != CreditCardFetchResult::kSuccess) {
    driver().RendererShouldClearPreviewedForm();
    return;
  }

  // In the failure case, `credit_card` can be nullptr but in the success case
  // it is non-null.
  CHECK(credit_card);
  const std::u16string& cvc = credit_card->cvc();
  last_unlocked_credit_card_cvc_ = cvc;

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(credit_card_form_, credit_card_field_,
                             &form_structure, &autofill_field)) {
    return;
  }

  DCHECK(credit_card);

  // If synced down card is a virtual card, let the client know so that it can
  // show the UI to help user to manually fill the form, if needed.
  if (credit_card->record_type() == CreditCard::RecordType::kVirtualCard) {
    DCHECK(!cvc.empty());

    VirtualCardManualFallbackBubbleOptions options;
    options.masked_card_name = credit_card_.CardNameForAutofillDisplay();
    options.masked_card_number_last_four =
        credit_card_.ObfuscatedNumberWithVisibleLastFourDigits();
    options.virtual_card = *credit_card;
    // TODO(crbug.com/1473481): Remove CVC from
    // VirtualCardManualFallbackBubbleOptions.
    options.virtual_card_cvc = cvc;
    options.card_image = GetCardImage(*credit_card);
    client().OnVirtualCardDataAvailable(options);
  }

  // After a server card is fetched, save its instrument id.
  client().GetFormDataImporter()->SetFetchedCardInstrumentId(
      credit_card->instrument_id());

  FillCreditCardForm(
      credit_card_form_, credit_card_field_, *credit_card, cvc,
      {.trigger_source = fetched_credit_card_trigger_source_.value_or(
           AutofillTriggerSource::kCreditCardCvcPopup)});
  if (credit_card->record_type() == CreditCard::RecordType::kFullServerCard ||
      credit_card->record_type() == CreditCard::RecordType::kVirtualCard) {
    credit_card_access_manager_->CacheUnmaskedCardInfo(*credit_card, cvc);
  }
}

void BrowserAutofillManager::OnDidEndTextFieldEditingImpl() {
  external_delegate_->DidEndTextFieldEditing();
  // Should not hide the Touch To Fill surface, since it is an overlay UI
  // which ends editing.
}

bool BrowserAutofillManager::IsAutofillEnabled() const {
  return IsAutofillProfileEnabled() || IsAutofillPaymentMethodsEnabled();
}

bool BrowserAutofillManager::IsAutofillProfileEnabled() const {
  return prefs::IsAutofillProfileEnabled(client().GetPrefs());
}

bool BrowserAutofillManager::IsAutofillPaymentMethodsEnabled() const {
  return prefs::IsAutofillPaymentMethodsEnabled(client().GetPrefs());
}

const FormData& BrowserAutofillManager::last_query_form() const {
  return external_delegate_->query_form();
}

bool BrowserAutofillManager::ShouldUploadForm(const FormStructure& form) {
  return IsAutofillEnabled() && !client().IsOffTheRecord() &&
         form.ShouldBeUploaded();
}

// AutocompleteHistoryManager::SuggestionsHandler implementation
void BrowserAutofillManager::OnSuggestionsReturned(
    FieldGlobalId field_id,
    AutofillSuggestionTriggerSource trigger_source,
    const std::vector<Suggestion>& suggestions) {
  external_delegate_->OnSuggestionsReturned(field_id, suggestions,
                                            trigger_source);
}

void BrowserAutofillManager::
    FetchPotentialCardLastFourDigitsCombinationFromDOM() {
  driver().GetFourDigitCombinationsFromDOM(base::BindOnce(
      [](base::WeakPtr<BrowserAutofillManager> self,
         const std::vector<std::string>& four_digit_combinations_in_dom) {
        if (!self) {
          return;
        }
        self->four_digit_combinations_in_dom_ = four_digit_combinations_in_dom;
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void BrowserAutofillManager::StoreUploadVotesAndLogQualityCallback(
    FormSignature form_signature,
    base::OnceClosure callback) {
  // Remove entries with the same FormSignature to replace them.
  WipeLogQualityAndVotesUploadCallback(form_signature);

  // Entries in queued_vote_uploads_ are submitted after navigations or form
  // submissions. To reduce the risk of collecting too much data that is not
  // send, we allow only `kMaxEntriesInQueue` entries. Anything in excess will
  // be sent when the queue becomes to long.
  constexpr int kMaxEntriesInQueue = 10;
  while (queued_vote_uploads_.size() >= kMaxEntriesInQueue) {
    base::OnceCallback oldest_callback =
        std::move(queued_vote_uploads_.back().second);
    queued_vote_uploads_.pop_back();
    std::move(oldest_callback).Run();
  }

  queued_vote_uploads_.emplace_front(form_signature, std::move(callback));
}

void BrowserAutofillManager::WipeLogQualityAndVotesUploadCallback(
    FormSignature form_signature) {
  base::EraseIf(queued_vote_uploads_, [form_signature](const auto& entry) {
    return entry.first == form_signature;
  });
}

void BrowserAutofillManager::FlushPendingLogQualityAndVotesUploadCallbacks() {
  std::list<std::pair<FormSignature, base::OnceClosure>> queued_vote_uploads =
      std::exchange(queued_vote_uploads_, {});
  for (auto& i : queued_vote_uploads)
    std::move(i.second).Run();
}

// We explicitly pass in all the time stamps of interest, as the cached ones
// might get reset before this method executes.
void BrowserAutofillManager::UploadVotesAndLogQuality(
    std::unique_ptr<FormStructure> submitted_form,
    base::TimeTicks interaction_time,
    base::TimeTicks submission_time,
    bool observed_submission,
    ukm::SourceId source_id) {
  // If the form is submitted, we don't need to send pending votes from blur
  // (un-focus) events.
  if (observed_submission)
    WipeLogQualityAndVotesUploadCallback(submitted_form->form_signature());

  if (submitted_form->ShouldRunHeuristics() ||
      submitted_form->ShouldRunHeuristicsForSingleFieldForms() ||
      submitted_form->ShouldBeQueried()) {
    FormInteractionCounts form_interaction_counts = {};
    if (submitted_form->field_count() > 0) {
      const AutofillField* autofill_field = submitted_form->field(0);
      auto* logger = GetEventFormLogger(*autofill_field);
      if (logger) {
        form_interaction_counts = logger->form_interaction_counts();
      }
    }

    autofill_metrics::LogQualityMetrics(
        *submitted_form, submitted_form->form_parsed_timestamp(),
        interaction_time, submission_time, form_interactions_ukm_logger(),
        did_show_suggestions_, observed_submission, form_interaction_counts);

    if (observed_submission) {
      // Ensure that callbacks for blur votes get sent as well here because
      // we are not sure whether a full navigation with a Reset() call follows.
      FlushPendingLogQualityAndVotesUploadCallbacks();
    }
  }

  if (!submitted_form->ShouldBeUploaded())
    return;

  if (base::FeatureList::IsEnabled(
          features::kAutofillLogUKMEventsWithSampleRate) &&
      ShouldUploadUkm(*submitted_form)) {
    AutofillMetrics::LogAutofillFieldInfoAfterSubmission(
        client().GetUkmRecorder(), source_id, *submitted_form, submission_time);
  }

  if (!download_manager())
    return;

  // Check if the form is among the forms that were recently auto-filled.
  bool was_autofilled = base::Contains(autofilled_form_signatures_,
                                       submitted_form->FormSignatureAsStr());

  ServerFieldTypeSet non_empty_types;
  client().GetPersonalDataManager()->GetNonEmptyTypes(&non_empty_types);
  // As CVC is not stored, treat it separately.
  if (!last_unlocked_credit_card_cvc_.empty() ||
      non_empty_types.contains(CREDIT_CARD_NUMBER)) {
    non_empty_types.insert(CREDIT_CARD_VERIFICATION_CODE);
  }

  download_manager()->StartUploadRequest(
      *submitted_form, was_autofilled, non_empty_types,
      /*login_form_signature=*/std::string(), observed_submission,
      client().GetPrefs(), GetWeakPtr());
}

const gfx::Image& BrowserAutofillManager::GetCardImage(
    const CreditCard& credit_card) {
  gfx::Image* card_art_image =
      client().GetPersonalDataManager()->GetCreditCardArtImageForUrl(
          credit_card.card_art_url());
  return card_art_image
             ? *card_art_image
             : ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                   CreditCard::IconResourceId(credit_card.network()));
}

void BrowserAutofillManager::Reset() {
  // Process log events and record into UKM when the form is destroyed or
  // removed.
  for (const auto& [form_id, form_structure] : form_structures()) {
    ProcessFieldLogEventsInForm(*form_structure);
  }

  // Note that upload_request_ is not reset here because the prompt to
  // save a card is shown after page navigation.
  ProcessPendingFormForUpload();
  FlushPendingLogQualityAndVotesUploadCallbacks();
  DCHECK(!pending_form_data_);
  // `credit_card_access_manager_` needs to be reset before resetting
  // `credit_card_form_event_logger_`, since it keeps a raw pointer to it.
  credit_card_access_manager_.reset();
  // {address, credit_card}_form_event_logger_ need to be reset before
  // AutofillManager::Reset() because ~FormEventLoggerBase() uses
  // form_interactions_ukm_logger_ that is created and assigned in
  // AutofillManager::Reset(). The new form_interactions_ukm_logger_ instance
  // is needed for constructing the new *form_event_logger_ instances which is
  // why calling AutofillManager::Reset() after constructing *form_event_logger_
  // instances is not an option.
  address_form_event_logger_->OnDestroyed();
  address_form_event_logger_.reset();
  credit_card_form_event_logger_->OnDestroyed();
  credit_card_form_event_logger_.reset();
  AutofillManager::Reset();
  address_form_event_logger_ =
      std::make_unique<autofill_metrics::AddressFormEventLogger>(
          driver().IsInAnyMainFrame(), form_interactions_ukm_logger(),
          &unsafe_client());
  credit_card_form_event_logger_ =
      std::make_unique<autofill_metrics::CreditCardFormEventLogger>(
          driver().IsInAnyMainFrame(), form_interactions_ukm_logger(),
          unsafe_client().GetPersonalDataManager(), &unsafe_client());
  credit_card_access_manager_ = std::make_unique<CreditCardAccessManager>(
      &driver(), &unsafe_client(), unsafe_client().GetPersonalDataManager(),
      credit_card_form_event_logger_.get());
  autocomplete_unrecognized_fallback_logger_ = std::make_unique<
      autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger>();

  has_logged_autofill_enabled_ = false;
  did_show_suggestions_ = false;
  user_did_type_ = false;
  user_did_autofill_ = false;
  user_did_edit_autofilled_field_ = false;
  credit_card_ = CreditCard();
  credit_card_form_ = FormData();
  credit_card_field_ = FormFieldData();
  last_unlocked_credit_card_cvc_.clear();
  initial_interaction_timestamp_ = TimeTicks();
  fetched_credit_card_trigger_source_ = absl::nullopt;
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->Reset();
  }
  filling_context_.clear();
  form_autofill_history_.Reset();
  form_submitted_timestamp_ = TimeTicks();
  four_digit_combinations_in_dom_.clear();
}

void BrowserAutofillManager::OnContextMenuShownInField(
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id) {
  FormStructure* form = FindCachedFormById(form_global_id);
  if (!form)
    return;
  auto field =
      base::ranges::find_if(*form, [&field_global_id](const auto& field) {
        return field->global_id() == field_global_id;
      });

  if (field != form->end())
    (*field)->set_was_context_menu_shown(true);
}

bool BrowserAutofillManager::RefreshDataModels() {
  if (!IsAutofillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  const std::vector<AutofillProfile*>& profiles =
      client().GetPersonalDataManager()->GetProfiles();
  credit_card_access_manager_->UpdateCreditCardFormEventLogger();

  // Updating the FormEventLogger for addresses.
  {
    size_t server_record_type_count = 0;
    size_t local_record_type_count = 0;
    for (AutofillProfile* profile : profiles) {
      if (profile->record_type() == AutofillProfile::LOCAL_PROFILE)
        local_record_type_count++;
      else if (profile->record_type() == AutofillProfile::SERVER_PROFILE)
        server_record_type_count++;
    }
    address_form_event_logger_->set_server_record_type_count(
        server_record_type_count);
    address_form_event_logger_->set_local_record_type_count(
        local_record_type_count);
  }

  return !profiles.empty() ||
         !client().GetPersonalDataManager()->GetCreditCards().empty();
}

CreditCard* BrowserAutofillManager::GetCreditCard(
    Suggestion::BackendId unique_id) {
  return client().GetPersonalDataManager()->GetCreditCardByGUID(
      unique_id.value());
}

AutofillProfile* BrowserAutofillManager::GetProfile(
    Suggestion::BackendId unique_id) {
  std::string guid = unique_id.value();
  if (base::Uuid::ParseCaseInsensitive(guid).is_valid()) {
    return client().GetPersonalDataManager()->GetProfileByGUID(guid);
  }
  return nullptr;
}

std::vector<FieldFillingSkipReason>
BrowserAutofillManager::GetFieldFillingSkipReasons(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& trigger_field,
    const Section& filling_section,
    const CreditCard* optional_credit_card,
    const ServerFieldTypeSet& field_types_to_fill,
    const DenseSet<FieldTypeGroup>* optional_type_groups_originally_filled,
    bool skip_unrecognized_autocomplete_fields,
    bool is_refill) const {
  // Counts the number of times a type was seen in the section to be filled.
  // This is used to limit the maximum number of fills per value.
  base::flat_map<ServerFieldType, size_t> type_count;
  type_count.reserve(form_structure.field_count());

  CHECK_EQ(form.fields.size(), form_structure.field_count());
  std::vector<FieldFillingSkipReason> skip_reasons(
      form_structure.field_count());
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    // Log events when the fields on the form are filled by autofill suggestion.
    const AutofillField* autofill_field = form_structure.field(i);
    const bool is_triggering_field =
        FormFieldData::DeepEqual(*autofill_field, trigger_field);

    if (autofill_field->section != filling_section) {
      skip_reasons[i] = FieldFillingSkipReason::kNotInFilledSection;
      continue;
    }

    if (autofill_field->only_fill_when_focused() && !is_triggering_field) {
      skip_reasons[i] = FieldFillingSkipReason::kNotFocused;
      continue;
    }

    // Address fields with unrecognized autocomplete attribute are only filled
    // when triggered through manual fallbacks.
    if (!is_triggering_field && skip_unrecognized_autocomplete_fields &&
        autofill_field->ShouldSuppressSuggestionsAndFillingByDefault()) {
      skip_reasons[i] =
          FieldFillingSkipReason::kUnrecognizedAutocompleteAttribute;
      continue;
    }

    // TODO(crbug/1203667#c9): Skip if the form has changed in the meantime,
    // which may happen with refills.
    if (autofill_field->global_id() != form.fields[i].global_id()) {
      skip_reasons[i] = FieldFillingSkipReason::kFormChanged;
      continue;
    }

    // Don't fill unfocusable fields, with the exception of <select> fields, for
    // the sake of filling the synthetic fields.
    if (!autofill_field->IsFocusable() && !autofill_field->IsSelectElement()) {
      skip_reasons[i] = FieldFillingSkipReason::kInvisibleField;
      continue;
    }

    // Do not fill fields that have been edited by the user, except if the field
    // is empty and its initial value (= cached value) was empty as well. A
    // similar check is done in ForEachMatchingFormFieldCommon(), which
    // frequently has false negatives.
    if ((form.fields[i].properties_mask & kUserTyped) &&
        (!form.fields[i].value.empty() || !autofill_field->value.empty()) &&
        !is_triggering_field) {
      skip_reasons[i] = FieldFillingSkipReason::kUserFilledFields;
      continue;
    }

    // Don't fill previously autofilled fields except the initiating field or
    // when it's a refill.
    if (form.fields[i].is_autofilled && !is_triggering_field && !is_refill) {
      skip_reasons[i] = FieldFillingSkipReason::kAutofilledFieldsNotRefill;
      continue;
    }

    FieldTypeGroup field_group_type = autofill_field->Type().group();
    if (field_group_type == FieldTypeGroup::kNoGroup) {
      skip_reasons[i] = FieldFillingSkipReason::kNoFillableGroup;
      continue;
    }

    // On a refill, only fill fields from type groups that were present during
    // the initial fill.
    if (is_refill && optional_type_groups_originally_filled &&
        !base::Contains(*optional_type_groups_originally_filled,
                        field_group_type)) {
      skip_reasons[i] = FieldFillingSkipReason::kRefillNotInInitialFill;
      continue;
    }

    ServerFieldType field_type = autofill_field->Type().GetStorableType();
    // Don't fill expired cards expiration date.
    if (data_util::IsCreditCardExpirationType(field_type) &&
        (optional_credit_card &&
         optional_credit_card->IsExpired(AutofillClock::Now()))) {
      skip_reasons[i] = FieldFillingSkipReason::kExpiredCards;
      continue;
    }

    if (base::FeatureList::IsEnabled(
            features::kAutofillGranularFillingAvailable)) {
      if (!field_types_to_fill.contains(field_type)) {
        skip_reasons[i] =
            FieldFillingSkipReason::kFieldDoesNotMatchTargetFieldsSet;
        continue;
      }
    }

    // A field with a specific type is only allowed to be filled a limited
    // number of times given by |TypeValueFormFillingLimit(field_type)|.
    if (++type_count[field_type] > TypeValueFormFillingLimit(field_type)) {
      skip_reasons[i] = FieldFillingSkipReason::kFillingLimitReachedType;
      continue;
    }
    skip_reasons[i] = FieldFillingSkipReason::kNotSkipped;
  }
  return skip_reasons;
}

void BrowserAutofillManager::FillOrPreviewDataModelForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::u16string* optional_cvc,
    FormStructure* form_structure,
    AutofillField* autofill_trigger_field,
    const AutofillTriggerDetails trigger_details,
    bool is_refill) {
  bool is_credit_card =
      absl::holds_alternative<const CreditCard*>(profile_or_credit_card);

  DCHECK(is_credit_card || !optional_cvc);
  DCHECK(form_structure);
  DCHECK(autofill_trigger_field);

  LogBuffer buffer(IsLoggingActive(log_manager()));
  LOG_AF(buffer) << "action_persistence: "
                 << ActionPersistenceToString(action_persistence);
  LOG_AF(buffer) << "is credit card section: " << is_credit_card << Br{};
  LOG_AF(buffer) << "is refill: " << is_refill << Br{};
  LOG_AF(buffer) << *form_structure << Br{};
  LOG_AF(buffer) << Tag{"table"};

  form_structure->RationalizePhoneNumbersInSection(
      autofill_trigger_field->section);

  // TODO(crbug/1203667#c9): Skip if the form has changed in the meantime, which
  // may happen with refills.
  if (action_persistence == mojom::ActionPersistence::kFill) {
    base::UmaHistogramBoolean(
        "Autofill.SkippingFormFillDueToChangedFieldCount",
        form_structure->field_count() != form.fields.size());
  }
  if (form_structure->field_count() != form.fields.size()) {
    LOG_AF(buffer)
        << Tr{} << "*"
        << "Skipped filling of form because the number of fields to be "
           "filled differs from the number of fields registered in the form "
           "cache."
        << CTag{"table"};
    LOG_AF(log_manager()) << LoggingScope::kFilling
                          << LogMessage::kSendFillingData << Br{}
                          << std::move(buffer);
    return;
  }

  if (action_persistence == mojom::ActionPersistence::kFill && !is_refill) {
    SetFillingContext(
        *form_structure,
        std::make_unique<FillingContext>(*autofill_trigger_field,
                                         profile_or_credit_card, optional_cvc));
  }

  // Only record the types that are filled for an eventual refill if all the
  // following are satisfied:
  //  The form is already filled.
  //  A refill has not been attempted for that form yet.
  //  This fill is not a refill attempt.
  FillingContext* filling_context = GetFillingContext(*form_structure);
  bool could_attempt_refill = filling_context != nullptr &&
                              !filling_context->attempted_refill && !is_refill;

  // Contains those fields that BrowserAutofillManager can and wants to fill.
  // This is used for logging in CreditCardFormEventLogger.
  base::flat_set<FieldGlobalId> newly_filled_field_ids;
  newly_filled_field_ids.reserve(form_structure->field_count());

  // Log events on the field which triggers the Autofill suggestion.
  absl::optional<FillEventId> fill_event_id;
  if (action_persistence == mojom::ActionPersistence::kFill) {
    std::string country_code;
    if (const autofill::AutofillProfile** address =
            absl::get_if<const AutofillProfile*>(&profile_or_credit_card)) {
      country_code = FetchCountryCodeFromProfile(*address);
    }

    TriggerFillFieldLogEvent trigger_fill_field_log_event =
        TriggerFillFieldLogEvent{
            .data_type = is_credit_card ? FillDataType::kCreditCard
                                        : FillDataType::kAutofillProfile,
            .associated_country_code = country_code,
            .timestamp = AutofillClock::Now()};

    autofill_trigger_field->AppendLogEventIfNotRepeated(
        trigger_fill_field_log_event);
    fill_event_id = trigger_fill_field_log_event.fill_event_id;
  }

  // Create a copy of the current form to fill and send to the renderer.
  FormData result = form;
  CHECK_EQ(result.fields.size(), form_structure->field_count());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    // On the renderer, the section is used regardless of the autofill status.
    result.fields[i].section = form_structure->field(i)->section;
  }

  std::vector<FieldFillingSkipReason> skip_reasons = GetFieldFillingSkipReasons(
      result, *form_structure, field, autofill_trigger_field->section,
      absl::holds_alternative<const CreditCard*>(profile_or_credit_card)
          ? absl::get<const CreditCard*>(profile_or_credit_card)
          : nullptr,
      trigger_details.field_types_to_fill,
      filling_context ? &filling_context->type_groups_originally_filled
                      : nullptr,
      /*skip_unrecognized_autocomplete_fields=*/
      trigger_details.trigger_source != AutofillTriggerSource::kManualFallback,
      is_refill);

  constexpr DenseSet<FieldFillingSkipReason> pre_ukm_logging_skips{
      FieldFillingSkipReason::kNotInFilledSection,
      FieldFillingSkipReason::kFormChanged,
      FieldFillingSkipReason::kNotFocused};
  CHECK_EQ(skip_reasons.size(), result.fields.size());
  for (size_t i = 0; i < result.fields.size(); ++i) {
    AutofillField* autofill_field = form_structure->field(i);

    if (!pre_ukm_logging_skips.contains(skip_reasons[i]) &&
        !autofill_field->IsFocusable()) {
      form_interactions_ukm_logger()
          ->LogHiddenRepresentationalFieldSkipDecision(
              *form_structure, *autofill_field,
              !autofill_field->IsSelectElement());
    }
    const bool has_value_before = !result.fields[i].value.empty();
    // Log when the suggestion is selected and log on non-checkable fields that
    // skip filling.
    if (skip_reasons[i] != FieldFillingSkipReason::kNotSkipped) {
      LOG_AF(buffer) << Tr{} << base::StringPrintf("Field %zu", i)
                     << GetSkipFieldFillLogMessage(skip_reasons[i]);
      if (fill_event_id && !IsCheckable(autofill_field->check_status)) {
        autofill_field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
            .fill_event_id = *fill_event_id,
            .had_value_before_filling = ToOptionalBoolean(has_value_before),
            .autofill_skipped_status = skip_reasons[i],
            .was_autofilled = OptionalBoolean::kFalse,
            .had_value_after_filling = ToOptionalBoolean(has_value_before),
            .filling_method = AutofillFillingMethod::kNone,
        });
      }
      continue;
    }

    if (could_attempt_refill) {
      filling_context->type_groups_originally_filled.insert(
          autofill_field->Type().group());
    }

    // Must match ForEachMatchingFormField() in form_autofill_util.cc.
    // Only notify autofilling of empty fields and the field that initiated the
    // filling (note that <select> and <selectlist> controls may not be empty
    // but will still be autofilled).
    const bool should_notify =
        !is_credit_card &&
        (result.fields[i].SameFieldAs(field) ||
         result.fields[i].IsSelectOrSelectListElement() || !has_value_before);
    std::string failure_to_fill;  // Reason for failing to fill.
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values =
        filling_context ? filling_context->forced_fill_values
                        : std::map<FieldGlobalId, std::u16string>();

    // Fill the non-empty value from |profile_or_credit_card| into the |result|
    // form, which will be sent to the renderer. FillFieldWithValue() may also
    // fill a field if it had been autofilled or manually filled before, and
    // also returns true in such a case; however, such fields don't reach this
    // code.
    const bool is_newly_autofilled = FillFieldWithValue(
        autofill_field, profile_or_credit_card, forced_fill_values,
        &result.fields[i], should_notify, optional_cvc ? *optional_cvc : u"",
        data_util::DetermineGroups(*form_structure), action_persistence,
        &failure_to_fill);
    if (is_newly_autofilled)
      newly_filled_field_ids.insert(result.fields[i].global_id());

    const bool has_value_after = !result.fields[i].value.empty();
    const bool is_autofilled_after = result.fields[i].is_autofilled;

    // Log when the suggestion is selected and log on non-checkable fields that
    // have been filled.
    if (fill_event_id && !IsCheckable(autofill_field->check_status)) {
      autofill_field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
          .fill_event_id = *fill_event_id,
          .had_value_before_filling = ToOptionalBoolean(has_value_before),
          .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
          .was_autofilled = ToOptionalBoolean(is_autofilled_after),
          .had_value_after_filling = ToOptionalBoolean(has_value_after),
          .filling_method = base::FeatureList::IsEnabled(
                                features::kAutofillGranularFillingAvailable)
                                ? GetFillingMethodFromTargetedFields(
                                      trigger_details.field_types_to_fill)
                                : AutofillFillingMethod::kFullForm,
      });
    }

    const bool is_autofilled_before = result.fields[i].is_autofilled;
    LOG_AF(buffer)
        << Tr{}
        << base::StringPrintf(
               "Field %zu Fillable - has value: %d->%d; autofilled: %d->%d. %s",
               i, has_value_before, has_value_after, is_autofilled_before,
               is_autofilled_after, failure_to_fill.c_str());

    if (!autofill_field->IsFocusable() && result.fields[i].is_autofilled) {
      AutofillMetrics::LogHiddenOrPresentationalSelectFieldsFilled();
    }
  }

  autofilled_form_signatures_.push_front(form_structure->FormSignatureAsStr());
  // Only remember the last few forms that we've seen, both to avoid false
  // positives and to avoid wasting memory.
  if (autofilled_form_signatures_.size() > kMaxRecentFormSignaturesToRemember)
    autofilled_form_signatures_.pop_back();

  auto field_types = base::MakeFlatMap<FieldGlobalId, ServerFieldType>(
      *form_structure, {}, [](const auto& field) {
        return std::make_pair(field->global_id(),
                              field->Type().GetStorableType());
      });
  std::vector<FieldGlobalId> safe_fields =
      driver().ApplyFormAction(mojom::ActionType::kFill, action_persistence,
                               result, field.origin, field_types);
  client().DidFillOrPreviewForm(action_persistence,
                                trigger_details.trigger_source, is_refill);

  // This will hold the fields (and autofill_fields) in the intersection of
  // safe_fields and newly_filled_fields_id.
  std::vector<const FormFieldData*> safe_newly_filled_fields;
  std::vector<const AutofillField*> safe_newly_filled_autofill_fields;

  // Report the fields that were not filled due to the iframe security policy.
  for (FieldGlobalId newly_filled_field_id : newly_filled_field_ids) {
    if (base::Contains(safe_fields, newly_filled_field_id)) {
      // A safe field was filled. Both functions will not return a nullptr
      // because they passed the `FieldFillingSkipReason::kFormChanged`
      // condition.
      safe_newly_filled_fields.push_back(
          form.FindFieldByGlobalId(newly_filled_field_id));
      safe_newly_filled_autofill_fields.push_back(
          form_structure->GetFieldById(newly_filled_field_id));
      continue;
    }
    // Find and report index of fields that were not filled.
    auto it = base::ranges::find(result.fields, newly_filled_field_id,
                                 &FormFieldData::global_id);
    if (it != result.fields.end()) {
      size_t index = it - result.fields.begin();
      std::string field_number = base::StringPrintf("Field %zu", index);
      LOG_AF(buffer) << Tr{} << field_number
                     << "Actually did not fill field because of the iframe "
                        "security policy.";
    }
  }

  // Save filling history to support undoing it later if needed.
  if (action_persistence == mojom::ActionPersistence::kFill) {
    form_autofill_history_.AddFormFillEntry(
        safe_newly_filled_fields, safe_newly_filled_autofill_fields, is_refill);
  }

  LOG_AF(buffer) << CTag{"table"};
  LOG_AF(log_manager()) << LoggingScope::kFilling
                        << LogMessage::kSendFillingData << Br{}
                        << std::move(buffer);

  NotifyObservers(&Observer::OnFillOrPreviewDataModelForm, form.global_id(),
                  action_persistence, safe_newly_filled_fields,
                  profile_or_credit_card);

  // Call OnDidFillSuggestion() to log the metrics.
  if (action_persistence == mojom::ActionPersistence::kFill && !is_refill) {
    if (is_credit_card) {
      // The originally selected masked card is `credit_card_`. So we must log
      // `credit_card_` as opposed to
      // `absl::get<CreditCard*>(profile_or_credit_card)` to correctly indicate
      // whether the user filled the form using a masked card suggestion.
      credit_card_form_event_logger_->OnDidFillSuggestion(
          credit_card_, *form_structure, *autofill_trigger_field,
          newly_filled_field_ids,
          base::flat_set<FieldGlobalId>(std::move(safe_fields)),
          signin_state_for_metrics_, trigger_details.trigger_source);
    } else {
      // An address form was filled.
      CHECK(absl::holds_alternative<const AutofillProfile*>(
          profile_or_credit_card));
      if (autofill_trigger_field
              ->ShouldSuppressSuggestionsAndFillingByDefault()) {
        autocomplete_unrecognized_fallback_logger_->OnDidFillSuggestion();
      } else {
        address_form_event_logger_->OnDidFillSuggestion(
            *absl::get<const AutofillProfile*>(profile_or_credit_card),
            *form_structure, *autofill_trigger_field, signin_state_for_metrics_,
            trigger_details.trigger_source);
      }
    }
  }

  // Note that this may invalidate |profile_or_credit_card|.
  if (action_persistence == mojom::ActionPersistence::kFill && !is_refill) {
    client().GetPersonalDataManager()->RecordUseOf(profile_or_credit_card);
  }

  if (filling_context) {
    // When a new preview/fill starts, previously forced_fill_values should be
    // ignored the operation could be for a different card or address.
    filling_context->forced_fill_values.clear();
  }
}

bool BrowserAutofillManager::ShouldPreventAutofillFromOverridingPrefilledField(
    mojom::ActionPersistence action_persistence,
    AutofillField* cached_field,
    FormFieldData* field_data,
    bool is_initiating_field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::u16string* optional_cvc) {
  // Keeping the credit card fields out of the experiment group.
  // The default behaviour would be to override the credit card pre-filled
  // fields.
  if (cached_field->Type().group() == FieldTypeGroup::kCreditCard)
    return false;

  if (!base::FeatureList::IsEnabled(
          features::kAutofillPreventOverridingPrefilledValues)) {
    return false;
  }

  cached_field->set_value_not_autofilled_over_existing_value_hash(
      absl::nullopt);

  // Some sites have empty values in the fields, for example.
  if (std::u16string sanitized_field_value =
          RemoveWhiteSpaceAndConjugatingCharacters(field_data->value);
      !field_data->IsSelectOrSelectListElement() &&
      !sanitized_field_value.empty() && !is_initiating_field) {
    std::string unused_failure_to_fill;
    const std::u16string kEmptyCvc{};
    std::u16string fill_value = field_filler_.GetValueForFilling(
        *cached_field, profile_or_credit_card, field_data,
        optional_cvc ? *optional_cvc : kEmptyCvc, action_persistence,
        &unused_failure_to_fill);
    std::u16string sanitized_fill_value =
        RemoveWhiteSpaceAndConjugatingCharacters(fill_value);

    if (action_persistence == mojom::ActionPersistence::kFill &&
        !sanitized_fill_value.empty() &&
        !base::EqualsCaseInsensitiveASCII(sanitized_field_value,
                                          sanitized_fill_value) &&
        !cached_field->value.empty()) {
      // Save the value that was supposed to be autofilled for this
      // field if the field contained an initial value.
      cached_field->set_value_not_autofilled_over_existing_value_hash(
          base::FastHash(base::UTF16ToUTF8(sanitized_fill_value)));
    }
    return true;
  }

  return false;
}

std::unique_ptr<FormStructure> BrowserAutofillManager::ValidateSubmittedForm(
    const FormData& form) {
  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  FormStructure* cached_submitted_form = FindCachedFormById(form.global_id());
  if (!cached_submitted_form || !ShouldUploadForm(*cached_submitted_form)) {
    return nullptr;
  }

  auto submitted_form = std::make_unique<FormStructure>(form);
  submitted_form->RetrieveFromCache(
      *cached_submitted_form,
      FormStructure::RetrieveFromCacheReason::kFormImport);

  return submitted_form;
}

AutofillField* BrowserAutofillManager::GetAutofillField(
    const FormData& form,
    const FormFieldData& field) {
  if (!client().GetPersonalDataManager()) {
    return nullptr;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return nullptr;

  if (!form_structure->IsAutofillable())
    return nullptr;

  return autofill_field;
}

bool BrowserAutofillManager::FormHasAddressField(const FormData& form) {
  for (const FormFieldData& field : form.fields) {
    const AutofillField* autofill_field = GetAutofillField(form, field);
    if (autofill_field &&
        autofill_field->Type().group() == FieldTypeGroup::kAddress) {
      return true;
    }
  }

  return false;
}

std::vector<Suggestion> BrowserAutofillManager::GetProfileSuggestions(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& field,
    const AutofillField& autofill_field,
    AutofillSuggestionTriggerSource trigger_source) const {
  address_form_event_logger_->OnDidPollSuggestions(field,
                                                   signin_state_for_metrics_);
  absl::optional<ServerFieldTypeSet> last_address_fields_to_fill_for_section =
      external_delegate_->GetLastFieldTypesToFillForSection(
          autofill_field.section);
  // Getting the filling-relevant fields so that suggestions are based only on
  // those fields. Function BrowserAutofillManager::GetFieldFillingSkipReasons
  // assumes that the passed FormData and FormStructure have the same size. If
  // it's not the case we just assume as a fallback that all fields are
  // relevant.
  std::vector<FieldFillingSkipReason> skip_reasons =
      form.fields.size() == form_structure.field_count()
          ? GetFieldFillingSkipReasons(
                form, form_structure, field, autofill_field.section,
                /*optional_credit_card=*/nullptr,
                last_address_fields_to_fill_for_section
                    ? GetTargetServerFieldsForTypeAndLastTargetedFields(
                          *last_address_fields_to_fill_for_section,
                          autofill_field.Type().GetStorableType())
                    : kAllServerFieldTypes,
                /*optional_type_groups_originally_filled=*/nullptr,
                /*skip_unrecognized_autocomplete_fields=*/trigger_source !=
                    AutofillSuggestionTriggerSource::kManualFallbackAddress,
                /*is_refill=*/false)
          : std::vector<FieldFillingSkipReason>(
                form_structure.field_count(),
                FieldFillingSkipReason::kNotSkipped);
  ServerFieldTypeSet field_types;
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    if (skip_reasons[i] == FieldFillingSkipReason::kNotSkipped) {
      field_types.insert(form_structure.field(i)->Type().GetStorableType());
    }
  }
  return suggestion_generator_->GetSuggestionsForProfiles(
      field_types, field, autofill_field.Type().GetStorableType(),
      last_address_fields_to_fill_for_section, trigger_source);
}

std::vector<Suggestion> BrowserAutofillManager::GetCreditCardSuggestions(
    const FormFieldData& field,
    ServerFieldType trigger_field_type,
    bool& should_display_gpay_logo) const {
  credit_card_form_event_logger_->OnDidPollSuggestions(
      field, signin_state_for_metrics_);

  std::vector<Suggestion> suggestions;
  bool with_offer = false;
  autofill_metrics::CardMetadataLoggingContext context;
  if (!IsInAutofillSuggestionsDisabledExperiment()) {
    if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
        !four_digit_combinations_in_dom_.empty()) {
      base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
          virtual_card_guid_to_last_four_map =
              GetVirtualCreditCardsForStandaloneCvcField(field.origin);
      if (!virtual_card_guid_to_last_four_map.empty()) {
        suggestions =
            suggestion_generator_->GetSuggestionsForVirtualCardStandaloneCvc(
                context, virtual_card_guid_to_last_four_map);
        // Always display GPay logo for virtual card suggestions.
        should_display_gpay_logo = true;
      }
    } else {
      suggestions = suggestion_generator_->GetSuggestionsForCreditCards(
          field, trigger_field_type, should_display_gpay_logo, with_offer,
          context);
    }
  }

  credit_card_form_event_logger_->OnDidFetchSuggestion(suggestions, with_offer,
                                                       context);
  return suggestions;
}

base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
BrowserAutofillManager::GetVirtualCreditCardsForStandaloneCvcField(
    const url::Origin& origin) const {
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  const std::vector<CreditCard*> cards =
      client().GetPersonalDataManager()->GetCreditCards();
  const std::vector<VirtualCardUsageData*> usage_data =
      client().GetPersonalDataManager()->GetVirtualCardUsageData();

  for (const CreditCard* credit_card : cards) {
    // As we only provide virtual card suggestions for standalone CVC fields,
    // check if the card is an enrolled virtual card.
    if (credit_card->virtual_card_enrollment_state() !=
        CreditCard::VirtualCardEnrollmentState::kEnrolled) {
      continue;
    }
    // Check if card has virtual card usage data on the url origin.
    auto usage_data_iter = base::ranges::find_if(
        usage_data,
        [&origin, &credit_card](VirtualCardUsageData* virtual_card_usage_data) {
          return virtual_card_usage_data->instrument_id().value() ==
                     credit_card->instrument_id() &&
                 virtual_card_usage_data->merchant_origin() == origin;
        });

    // If card has eligible usage data, check if last four is in the url DOM.
    if (usage_data_iter != usage_data.end()) {
      VirtualCardUsageData::VirtualCardLastFour virtual_card_last_four =
          (*usage_data_iter)->virtual_card_last_four();
      if (base::Contains(four_digit_combinations_in_dom_,
                         base::UTF16ToUTF8(virtual_card_last_four.value()))) {
        // Card has usage data on webpage and last four is present in DOM.
        virtual_card_guid_to_last_four_map.insert(
            {credit_card->guid(), virtual_card_last_four});
      }
    }
  }
  return virtual_card_guid_to_last_four_map;
}

// TODO(crbug.com/1309848) Eliminate and replace with a listener?
// Should we do the same with all the other BrowserAutofillManager events?
void BrowserAutofillManager::OnBeforeProcessParsedForms() {
  has_parsed_forms_ = true;

  // Record the current sync state to be used for metrics on this page.
  signin_state_for_metrics_ =
      client().GetPersonalDataManager()->GetPaymentsSigninStateForMetrics();

  // Setup the url for metrics that we will collect for this form.
  form_interactions_ukm_logger()->OnFormsParsed(client().GetUkmSourceId());
}

void BrowserAutofillManager::OnFormProcessed(
    const FormData& form,
    const FormStructure& form_structure) {
  // If a standalone cvc field is found in the form, query the DOM for last four
  // combinations. Used to search for the virtual card last four for a virtual
  // card saved on file of a merchant webpage.
  if (base::FeatureList::IsEnabled(
          features::kAutofillParseVcnCardOnFileStandaloneCvcFields)) {
    auto contains_standalone_cvc_field =
        base::ranges::any_of(form_structure.fields(), [](const auto& field) {
          return field->Type().GetStorableType() ==
                 CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
        });
    if (contains_standalone_cvc_field) {
      FetchPotentialCardLastFourDigitsCombinationFromDOM();
    }
  }

  if (data_util::ContainsPhone(data_util::DetermineGroups(form_structure))) {
    has_observed_phone_number_field_ = true;
  }

  // TODO(crbug.com/869482): avoid logging developer engagement multiple
  // times for a given form if it or other forms on the page are dynamic.
  LogDeveloperEngagementUkm(client().GetUkmRecorder(),
                            client().GetUkmSourceId(), form_structure);

  for (const auto& field : form_structure) {
    if (field->Type().html_type() == HtmlFieldType::kOneTimeCode) {
      has_observed_one_time_code_field_ = true;
      break;
    }
  }

  // Log the type of form that was parsed.
  DenseSet<FormType> form_types = form_structure.GetFormTypes();
  bool card_form = base::Contains(form_types, FormType::kCreditCardForm);
  bool address_form = base::Contains(form_types, FormType::kAddressForm);
  if (card_form) {
    credit_card_form_event_logger_->OnDidParseForm(form_structure);
  }
  if (address_form) {
    address_form_event_logger_->OnDidParseForm(form_structure);
  }

  // `autofill_optimization_guide_` is not present on unsupported platforms.
  if (auto* autofill_optimization_guide =
          client().GetAutofillOptimizationGuide()) {
    // Initiate necessary pre-processing based on the forms and fields that are
    // parsed, as well as the information that the user has saved in the web
    // database based on `client().GetPersonalDataManager()`.
    autofill_optimization_guide->OnDidParseForm(
        form_structure, client().GetPersonalDataManager());
  }

  // If a form with the same name was previously filled, and there has not
  // been a refill attempt on that form yet, start the process of triggering a
  // refill.
  if (ShouldTriggerRefill(form_structure))
    ScheduleRefill(form, {.trigger_source = AutofillTriggerSource::kFormsSeen});
}

void BrowserAutofillManager::OnAfterProcessParsedForms(
    const DenseSet<FormType>& form_types) {
  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::FORMS_LOADED, form_types,
      client().GetSecurityLevelForUmaHistograms(),
      /*profile_form_bitmask=*/0);
}

void BrowserAutofillManager::UpdateInitialInteractionTimestamp(
    const TimeTicks& interaction_timestamp) {
  if (initial_interaction_timestamp_.is_null() ||
      interaction_timestamp < initial_interaction_timestamp_) {
    initial_interaction_timestamp_ = interaction_timestamp;
  }
}

// static
void BrowserAutofillManager::DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::u16string& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    bool observed_submission,
    FormStructure* form) {
  // Temporary helper structure for measuring the impact of
  // autofill::features::kAutofillVoteForSelectOptionValues.
  // TODO(crbug.com/1395740) Remove this once the feature has settled.
  struct AutofillVoteForSelectOptionValuesMetrics {
    // Whether kAutofillVoteForSelectOptionValues classified more fields
    // than the original version of this function w/o
    // kAutofillVoteForSelectOptionValuesMetrics.
    bool classified_more_field_types = false;
    // Whether any field types were detected and assigned to fields for the
    // current form.
    bool classified_any_field_types = false;
    // Whether any field was classified as a country field.
    bool classified_field_as_country_field = false;
    // Whether any <select> element was reclassified from a country field
    // to a phone country code field due to
    // kAutofillVoteForSelectOptionValuesMetrics.
    bool switched_from_country_to_phone_country_code = false;
  } metrics;

  // For each field in the |form|, extract the value.  Then for each
  // profile or credit card, identify any stored types that match the value.
  for (size_t i = 0; i < form->field_count(); ++i) {
    AutofillField* field = form->field(i);
    if (!field->possible_types().empty() && field->IsEmpty()) {
      // This is a password field in a sign-in form. Skip checking its type
      // since |field->value| is not set.
      DCHECK_EQ(1u, field->possible_types().size());
      DCHECK_EQ(PASSWORD, *field->possible_types().begin());
      continue;
    }

    ServerFieldTypeSet matching_types;
    std::u16string value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // Consider the textual values of <select> element <option>s as well.
    // If a phone country code <select> element looks as follows:
    // <select> <option value="US">+1</option> </select>
    // We want to consider the <option>'s content ("+1") to classify this as a
    // PHONE_HOME_COUNTRY_CODE field. It is insufficient to just consider the
    // <option>'s value ("US").
    absl::optional<std::u16string> select_content;
    // TODO(crbug.com/1395740) Remove the flag check once the feature has
    // settled.
    if (field->IsSelectOrSelectListElement() &&
        base::FeatureList::IsEnabled(
            features::kAutofillVoteForSelectOptionValues)) {
      auto it = base::ranges::find(field->options, field->value,
                                   &SelectOption::value);
      if (it != field->options.end()) {
        select_content = it->content;
        base::TrimWhitespace(*select_content, base::TRIM_ALL, &*select_content);
      }
    }

    for (const AutofillProfile& profile : profiles) {
      profile.GetMatchingTypes(value, app_locale, &matching_types);
      if (select_content) {
        ServerFieldTypeSet matching_types_backup = matching_types;
        profile.GetMatchingTypes(*select_content, app_locale, &matching_types);
        if (matching_types_backup != matching_types)
          metrics.classified_more_field_types = true;
      }
    }

    // TODO(crbug/880531) set possible_types_validities for credit card too.
    for (const CreditCard& card : credit_cards) {
      card.GetMatchingTypes(value, app_locale, &matching_types);
      if (select_content) {
        ServerFieldTypeSet matching_types_backup = matching_types;
        card.GetMatchingTypes(*select_content, app_locale, &matching_types);
        if (matching_types_backup != matching_types)
          metrics.classified_more_field_types = true;
      }
    }

    // In case a select element has options like this
    //  <option value="US">+1</option>,
    // meaning that it contains a phone country code, we treat that as
    // sufficient evidence to only vote for phone country code.
    if (matching_types.contains(ADDRESS_HOME_COUNTRY))
      metrics.classified_field_as_country_field = true;
    if (select_content && matching_types.contains(ADDRESS_HOME_COUNTRY) &&
        MatchesRegex<kAugmentedPhoneCountryCodeRe>(*select_content)) {
      matching_types.erase(ADDRESS_HOME_COUNTRY);
      matching_types.insert(PHONE_HOME_COUNTRY_CODE);
      metrics.switched_from_country_to_phone_country_code = true;
    }

    if (field->state_is_a_matching_type())
      matching_types.insert(ADDRESS_HOME_STATE);

    if (!matching_types.empty())
      metrics.classified_any_field_types = true;

    if (matching_types.empty()) {
      matching_types.insert(UNKNOWN_TYPE);
      ServerFieldTypeValidityStateMap matching_types_validities;
      matching_types_validities[UNKNOWN_TYPE] = AutofillDataModel::UNVALIDATED;
      field->add_possible_types_validities(matching_types_validities);
    }

    field->set_possible_types(matching_types);
  }

  // As CVCs are not stored, run special heuristics to detect CVC-like values.
  AutofillField* cvc_field =
      GetBestPossibleCVCFieldForUpload(*form, last_unlocked_credit_card_cvc);
  if (cvc_field) {
    ServerFieldTypeSet possible_types = cvc_field->possible_types();
    possible_types.erase(UNKNOWN_TYPE);
    possible_types.insert(CREDIT_CARD_VERIFICATION_CODE);
    cvc_field->set_possible_types(possible_types);
  }

  if (observed_submission && metrics.classified_any_field_types) {
    enum class Bucket {
      kClassifiedAnyField = 0,
      kClassifiedMoreFields = 1,
      kClassifiedFieldAsCountryField = 2,
      kSwitchedFromCountryToPhoneCountryCode = 3,
      kMaxValue = 3
    };
    base::UmaHistogramEnumeration("Autofill.VoteForSelecteOptionValues",
                                  Bucket::kClassifiedAnyField);
    if (metrics.classified_more_field_types) {
      base::UmaHistogramEnumeration("Autofill.VoteForSelecteOptionValues",
                                    Bucket::kClassifiedMoreFields);
    }
    if (metrics.classified_field_as_country_field) {
      base::UmaHistogramEnumeration("Autofill.VoteForSelecteOptionValues",
                                    Bucket::kClassifiedFieldAsCountryField);
    }
    if (metrics.switched_from_country_to_phone_country_code) {
      base::UmaHistogramEnumeration(
          "Autofill.VoteForSelecteOptionValues",
          Bucket::kSwitchedFromCountryToPhoneCountryCode);
    }
  }

  DisambiguateUploadTypes(form);
}

// static
void BrowserAutofillManager::DisambiguateUploadTypes(FormStructure* form) {
  for (size_t i = 0; i < form->field_count(); ++i) {
    AutofillField* field = form->field(i);
    const ServerFieldTypeSet& upload_types = field->possible_types();

    // In case for credit cards and names there are many other possibilities
    // because a field can be of type NAME_FULL, NAME_LAST,
    // NAME_LAST_FIRST/SECOND at the same time.
    // Also, a single line street address is ambiguous to address line 1.
    // However, this case is handled on the server and here only the name
    // disambiguation for address and credit card related name fields is
    // performed.

    // Disambiguation is only applicable if there is a mixture of one or more
    // address related name fields and exactly one credit card related name
    // field.
    const size_t credit_card_type_count =
        NumberOfPossibleFieldTypesInGroup(*field, FieldTypeGroup::kCreditCard);
    const size_t name_type_count =
        NumberOfPossibleFieldTypesInGroup(*field, FieldTypeGroup::kName);
    if (upload_types.size() == (credit_card_type_count + name_type_count) &&
        credit_card_type_count == 1 && name_type_count >= 1) {
      DisambiguateNameUploadTypes(form, i, upload_types);
    }
  }
}

// static
void BrowserAutofillManager::DisambiguateNameUploadTypes(
    FormStructure* form,
    size_t current_index,
    const ServerFieldTypeSet& upload_types) {
  // This case happens when both a profile and a credit card have the same
  // name, and when we have exactly two possible types.

  // If the ambiguous field has either a previous or next field that is
  // not name related, use that information to determine whether the field
  // is a name or a credit card name.
  // If the ambiguous field has both a previous or next field that is not
  // name related, if they are both from the same group, use that group to
  // decide this field's type. Otherwise, there is no safe way to
  // disambiguate.

  // Look for a previous non name related field.
  bool has_found_previous_type = false;
  bool is_previous_credit_card = false;
  size_t index = current_index;
  while (index != 0 && !has_found_previous_type) {
    --index;
    AutofillField* prev_field = form->field(index);
    if (!IsNameType(*prev_field)) {
      has_found_previous_type = true;
      is_previous_credit_card =
          prev_field->Type().group() == FieldTypeGroup::kCreditCard;
    }
  }

  // Look for a next non name related field.
  bool has_found_next_type = false;
  bool is_next_credit_card = false;
  index = current_index;
  while (++index < form->field_count() && !has_found_next_type) {
    AutofillField* next_field = form->field(index);
    if (!IsNameType(*next_field)) {
      has_found_next_type = true;
      is_next_credit_card =
          next_field->Type().group() == FieldTypeGroup::kCreditCard;
    }
  }

  // At least a previous or next field type must have been found in order to
  // disambiguate this field.
  if (has_found_previous_type || has_found_next_type) {
    // If both a previous type and a next type are found and not from the same
    // name group there is no sure way to disambiguate.
    if (has_found_previous_type && has_found_next_type &&
        (is_previous_credit_card != is_next_credit_card)) {
      return;
    }

    // Otherwise, use the previous (if it was found) or next field group to
    // decide whether the field is a name or a credit card name.
    if (has_found_previous_type) {
      SelectRightNameType(form->field(current_index), is_previous_credit_card);
    } else {
      SelectRightNameType(form->field(current_index), is_next_credit_card);
    }
  }
}

bool BrowserAutofillManager::FillFieldWithValue(
    AutofillField* autofill_field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
    FormFieldData* field_data,
    bool should_notify,
    const std::u16string& cvc,
    uint32_t profile_form_bitmask,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  bool filled_field = field_filler_.FillFormField(
      *autofill_field, profile_or_credit_card, forced_fill_values, field_data,
      cvc, action_persistence, failure_to_fill);
  if (filled_field) {
    if (failure_to_fill) {
      *failure_to_fill = "Decided to fill";
    }
    if (action_persistence == mojom::ActionPersistence::kFill ||
        !base::FeatureList::IsEnabled(
            features::kAutofillOnlyCacheIsAutofilledOnFill)) {
      // Mark the cached field as autofilled, so that we can detect when a
      // user edits an autofilled field (for metrics).
      autofill_field->is_autofilled = true;
      if (const AutofillProfile** profile =
              absl::get_if<const AutofillProfile*>(&profile_or_credit_card)) {
        autofill_field->set_autofill_source_profile_guid((*profile)->guid());
      }
    }

    // Mark the field as autofilled when a non-empty value is assigned to
    // it. This allows the renderer to distinguish autofilled fields from
    // fields with non-empty values, such as select-one fields.
    field_data->is_autofilled = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::FIELD_WAS_AUTOFILLED, autofill_field->Type().group(),
        client().GetSecurityLevelForUmaHistograms(), profile_form_bitmask);

    if (should_notify) {
      DCHECK(absl::holds_alternative<const AutofillProfile*>(
          profile_or_credit_card));
      const AutofillProfile* profile =
          absl::get<const AutofillProfile*>(profile_or_credit_card);
      client().DidFillOrPreviewField(
          /*autofilled_value=*/profile->GetInfo(autofill_field->Type(),
                                                app_locale_),
          /*profile_full_name=*/profile->GetInfo(AutofillType(NAME_FULL),
                                                 app_locale_));
    }
  }
  return filled_field;
}

void BrowserAutofillManager::SetFillingContext(
    const FormStructure& form,
    std::unique_ptr<FillingContext> context) {
  filling_context_[form.global_id()] = std::move(context);
}

BrowserAutofillManager::FillingContext*
BrowserAutofillManager::GetFillingContext(const FormStructure& form) {
  auto it = filling_context_.find(form.global_id());
  return it != filling_context_.end() ? it->second.get() : nullptr;
}

bool BrowserAutofillManager::ShouldTriggerRefill(
    const FormStructure& form_structure) {
  // Should not refill if a form with the same FormGlobalId that has not been
  // filled before.
  FillingContext* filling_context = GetFillingContext(form_structure);
  if (filling_context == nullptr)
    return false;

  address_form_event_logger_->OnDidSeeFillableDynamicForm(
      signin_state_for_metrics_, form_structure);

  base::TimeTicks now = AutofillTickClock::NowTicks();
  base::TimeDelta delta = now - filling_context->original_fill_time;

  if (filling_context->attempted_refill && delta < kLimitBeforeRefill) {
    address_form_event_logger_->OnSubsequentRefillAttempt(
        signin_state_for_metrics_, form_structure);
  }

  return !filling_context->attempted_refill && delta < kLimitBeforeRefill;
}

void BrowserAutofillManager::ScheduleRefill(
    const FormData& form,
    const AutofillTriggerDetails& trigger_details) {
  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!form_structure)
    return;

  FillingContext* filling_context = GetFillingContext(*form_structure);
  DCHECK(filling_context != nullptr);

  // If a timer for the refill was already running, it means the form
  // changed again. Stop the timer and start it again.
  if (filling_context->on_refill_timer.IsRunning())
    filling_context->on_refill_timer.AbandonAndStop();

  // Start a new timer to trigger refill.
  filling_context->on_refill_timer.Start(
      FROM_HERE, kWaitTimeForDynamicForms,
      base::BindRepeating(&BrowserAutofillManager::TriggerRefill,
                          weak_ptr_factory_.GetWeakPtr(), form,
                          trigger_details));
}

void BrowserAutofillManager::TriggerRefill(
    const FormData& form,
    const AutofillTriggerDetails& trigger_details) {
  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!form_structure)
    return;

  address_form_event_logger_->OnDidRefill(signin_state_for_metrics_,
                                          *form_structure);

  FillingContext* filling_context = GetFillingContext(*form_structure);
  DCHECK(filling_context);

  // The refill attempt can happen from different paths, some of which happen
  // after waiting for a while. Therefore, although this condition has been
  // checked prior to calling TriggerRefill, it may not hold, when we get
  // here.
  if (filling_context->attempted_refill)
    return;

  filling_context->attempted_refill = true;

  // Try to find the field from which the original fill originated.
  // The precedence for the look up is the following:
  //  - focusable `filled_field_id`
  //  - focusable `filled_field_signature`
  //  - non-focusable `filled_field_id`
  //  - non-focusable `filled_field_signature`
  // and prefer newer renderer ids.
  auto comparison_attributes =
      [&](const std::unique_ptr<AutofillField>& field) {
        return std::make_tuple(
            field->origin == filling_context->filled_origin,
            field->IsFocusable(),
            field->global_id() == filling_context->filled_field_id,
            field->GetFieldSignature() ==
                filling_context->filled_field_signature,
            field->unique_renderer_id);
      };
  auto it =
      base::ranges::max_element(*form_structure, {}, comparison_attributes);
  AutofillField* autofill_field =
      it != form_structure->end() ? it->get() : nullptr;
  bool found_matching_element =
      autofill_field &&
      autofill_field->origin == filling_context->filled_origin &&
      (autofill_field->global_id() == filling_context->filled_field_id ||
       autofill_field->GetFieldSignature() ==
           filling_context->filled_field_signature);
  if (!found_matching_element)
    return;

  FormFieldData field = *autofill_field;
  if (absl::holds_alternative<std::pair<CreditCard, std::u16string>>(
          filling_context->profile_or_credit_card_with_cvc)) {
    const auto& [credit_card, cvc] =
        absl::get<std::pair<CreditCard, std::u16string>>(
            filling_context->profile_or_credit_card_with_cvc);
    FillOrPreviewDataModelForm(mojom::ActionPersistence::kFill, form, field,
                               &credit_card, &cvc, form_structure,
                               autofill_field, trigger_details,
                               /*is_refill=*/true);
  } else if (absl::holds_alternative<AutofillProfile>(
                 filling_context->profile_or_credit_card_with_cvc)) {
    FillOrPreviewDataModelForm(
        mojom::ActionPersistence::kFill, form, field,
        &absl::get<AutofillProfile>(
            filling_context->profile_or_credit_card_with_cvc),
        /*optional_cvc=*/nullptr, form_structure, autofill_field,
        trigger_details,
        /*is_refill=*/true);
  } else {
    NOTREACHED();
  }
}

void BrowserAutofillManager::GetAvailableSuggestions(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    std::vector<Suggestion>* suggestions,
    SuggestionsContext* context) {
  DCHECK(suggestions);
  DCHECK(context);

  // Need to refresh models before using the form_event_loggers.
  RefreshDataModels();

  bool got_autofillable_form =
      GetCachedFormAndField(form, field, &context->form_structure,
                            &context->focused_field) &&
      // Don't send suggestions or track forms that should not be parsed.
      context->form_structure->ShouldBeParsed();

  if (!ShouldShowSuggestionsForAutocompleteUnrecognizedFields(trigger_source) &&
      got_autofillable_form &&
      context->focused_field->ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Pre-`AutofillPredictionsForAutocompleteUnrecognized`, autocomplete
    // suggestions were shown if all types of the form were suppressed or
    // unknown. If at least a single field had predictions (and the form was
    // thus considered autofillable), autocomplete suggestions were suppressed
    // for fields with a suppressed prediction.
    // To retain this behavior, the `suppress_reason` is only set if the form
    // contains a field that triggers (non-fallback) suggestions.
    // By not setting it, the autocomplete suggestion logic downstream is
    // triggered, since no Autofill `suggestions` are available.
    if (!base::ranges::all_of(*context->form_structure, [](const auto& field) {
          return field->ShouldSuppressSuggestionsAndFillingByDefault() ||
                 field->Type().GetStorableType() == UNKNOWN_TYPE;
        })) {
      context->suppress_reason = SuppressReason::kAutocompleteUnrecognized;
    }
    suggestions->clear();
    return;
  }

  // Log interactions of forms that are autofillable.
  if (got_autofillable_form) {
    if (context->focused_field->Type().group() == FieldTypeGroup::kCreditCard) {
      context->is_filling_credit_card = true;
    }
    auto* logger = GetEventFormLogger(*context->focused_field);
    if (logger) {
      logger->OnDidInteractWithAutofillableForm(*(context->form_structure),
                                                signin_state_for_metrics_);
    }
  }

  // If the feature is enabled and this is a mixed content form, we show a
  // warning message and don't offer autofill. The warning is shown even if
  // there are no autofill suggestions available.
  if (IsFormMixedContent(client(), form) &&
      client().GetPrefs()->FindPreference(
          ::prefs::kMixedFormsWarningsEnabled) &&
      client().GetPrefs()->GetBoolean(::prefs::kMixedFormsWarningsEnabled)) {
    suggestions->clear();
    // If the user begins typing, we interpret that as dismissing the warning.
    // No suggestions are allowed, but the warning is no longer shown.
    if (field.DidUserType()) {
      context->suppress_reason = SuppressReason::kInsecureForm;
    } else {
      Suggestion warning_suggestion(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_MIXED_FORM));
      warning_suggestion.popup_item_id = PopupItemId::kMixedFormMessage;
      suggestions->emplace_back(warning_suggestion);
    }
    return;
  }

  context->is_context_secure = !IsFormNonSecure(form);

  // TODO(rogerm): Early exit here on !driver().RendererIsAvailable()?
  // We skip populating autofill data, but might generate warnings and or
  // signin promo to show over the unavailable renderer. That seems a mistake.

  if (!driver().RendererIsAvailable() || !got_autofillable_form ||
      !IsAutofillEnabled()) {
    return;
  }

  context->is_autofill_available = true;

  if (context->is_filling_credit_card) {
    // Credit cards suggestions don't depend the `trigger_source`.
    *suggestions = GetCreditCardSuggestions(
        field, context->focused_field->Type().GetStorableType(),
        context->should_display_gpay_logo);
  } else {
    // Profile suggestions fill ac=unrecognized fields only when triggered
    // through manual fallbacks. As such, suggestion labels differ depending on
    // the `trigger_source`.
    *suggestions =
        GetProfileSuggestions(form, *context->form_structure, field,
                              *context->focused_field, trigger_source);
    if (context->focused_field->Type().group() == FieldTypeGroup::kEmail) {
      absl::optional<Suggestion> maybe_plus_address_suggestion =
          MaybeGetPlusAddressSuggestion();
      if (maybe_plus_address_suggestion.has_value()) {
        suggestions->push_back(maybe_plus_address_suggestion.value());
      }
    }
  }

  // Ablation experiment:
  FormTypeForAblationStudy form_type = context->is_filling_credit_card
                                           ? FormTypeForAblationStudy::kPayment
                                           : FormTypeForAblationStudy::kAddress;
  // If ablation_group is AblationGroup::kDefault or AblationGroup::kControl,
  // no ablation happens in the following.
  AblationGroup ablation_group = client().GetAblationStudy().GetAblationGroup(
      client().GetLastCommittedPrimaryMainFrameURL(), form_type);
  context->ablation_group = ablation_group;
  // Note that we don't set the ablation group if there are no suggestions.
  // In that case we stick to kDefault.
  context->conditional_ablation_group =
      !suggestions->empty() ? ablation_group : AblationGroup::kDefault;

  // In both cases (credit card and address forms), we inform the other event
  // logger also about the ablation.
  // This prevents for example that for an encountered address form we log a
  // sample Autofill.Funnel.ParsedAsType.CreditCard = 0 (which would be recorded
  // by the credit_card_form_event_logger_).
  // For the complementary event logger, the conditional ablation status is
  // logged as kDefault to not imply that data would be filled without ablation.
  if (context->is_filling_credit_card) {
    credit_card_form_event_logger_->SetAblationStatus(
        context->ablation_group, context->conditional_ablation_group);
    address_form_event_logger_->SetAblationStatus(context->ablation_group,
                                                  AblationGroup::kDefault);
  } else {
    address_form_event_logger_->SetAblationStatus(
        context->ablation_group, context->conditional_ablation_group);
    credit_card_form_event_logger_->SetAblationStatus(context->ablation_group,
                                                      AblationGroup::kDefault);
  }

  if (!suggestions->empty() && ablation_group == AblationGroup::kAblation) {
    // Logic for disabling/ablating autofill.
    context->suppress_reason = SuppressReason::kAblation;
    suggestions->clear();
    return;
  }

  // Returns early if no suggestion is available or suggestions are not for
  // cards.
  if (suggestions->empty() || !context->is_filling_credit_card)
    return;

  // Don't provide credit card suggestions for non-secure pages, but do
  // provide them for secure pages with passive mixed content (see
  // implementation of IsContextSecure).
  if (!context->is_context_secure) {
    // Replace the suggestion content with a warning message explaining why
    // Autofill is disabled for a website. The string is different if the
    // credit card autofill HTTP warning experiment is enabled.
    Suggestion warning_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
    warning_suggestion.popup_item_id =
        PopupItemId::kInsecureContextPaymentDisabledMessage;
    suggestions->assign(1, warning_suggestion);
  }
}

autofill_metrics::FormEventLoggerBase*
BrowserAutofillManager::GetEventFormLogger(const AutofillField& field) const {
  if (field.ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Ignore ac=unrecognized fields in key metrics: Prior to
    // `kAutofillPredictionsForAutocompleteUnrecognized`, ac=unrecognized fields
    // did not receive a type prediction and were consequently not associated to
    // any event logger. To retain the same baseline for key metrics, continue
    // to exclude such fields.
    return nullptr;
  }
  switch (FieldTypeGroupToFormType(field.Type().group())) {
    case FormType::kAddressForm:
      return address_form_event_logger_.get();
    case FormType::kCreditCardForm:
      return credit_card_form_event_logger_.get();
    case FormType::kPasswordForm:
    case FormType::kUnknownFormType:
      return nullptr;
  }
  NOTREACHED_NORETURN();
}

void BrowserAutofillManager::PreProcessStateMatchingTypes(
    const std::vector<AutofillProfile>& profiles,
    FormStructure* form_structure) {
  for (const auto& profile : profiles) {
    absl::optional<AlternativeStateNameMap::CanonicalStateName>
        canonical_state_name_from_profile =
            profile.GetAddress().GetCanonicalizedStateName();

    if (!canonical_state_name_from_profile)
      continue;

    const std::u16string& country_code =
        profile.GetInfo(AutofillType(HtmlFieldType::kCountryCode), app_locale_);

    for (auto& field : *form_structure) {
      if (field->state_is_a_matching_type())
        continue;

      absl::optional<AlternativeStateNameMap::CanonicalStateName>
          canonical_state_name_from_text =
              AlternativeStateNameMap::GetCanonicalStateName(
                  base::UTF16ToUTF8(country_code), field->value);

      if (canonical_state_name_from_text &&
          canonical_state_name_from_text.value() ==
              canonical_state_name_from_profile.value()) {
        field->set_state_is_a_matching_type();
      }
    }
  }
}

void BrowserAutofillManager::ReportAutofillWebOTPMetrics(bool used_web_otp) {
  // It's possible that a frame without any form uses WebOTP. e.g. a server may
  // send the verification code to a phone number that was collected beforehand
  // and uses the WebOTP API for authentication purpose without user manually
  // entering the code.
  if (!has_parsed_forms() && !used_web_otp)
    return;

  if (has_observed_phone_number_field())
    phone_collection_metric_state_ |= phone_collection_metric::kPhoneCollected;
  if (has_observed_one_time_code_field())
    phone_collection_metric_state_ |= phone_collection_metric::kOTCUsed;
  if (used_web_otp)
    phone_collection_metric_state_ |= phone_collection_metric::kWebOTPUsed;

  ukm::UkmRecorder* recorder = client().GetUkmRecorder();
  ukm::SourceId source_id = client().GetUkmSourceId();
  AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
      recorder, source_id, phone_collection_metric_state_);

  base::UmaHistogramEnumeration(
      "Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
      static_cast<PhoneCollectionMetricState>(phone_collection_metric_state_));
}

void BrowserAutofillManager::OnSeePromoCodeOfferDetailsSelected(
    const GURL& offer_details_url,
    const std::u16string& value,
    PopupItemId popup_item_id,
    const FormData& form,
    const FormFieldData& field) {
  client().OpenPromoCodeOfferDetailsURL(offer_details_url);
  OnSingleFieldSuggestionSelected(value, popup_item_id, form, field);
}

void BrowserAutofillManager::ProcessFieldLogEventsInForm(
    const FormStructure& form_structure) {
  // TODO(crbug.com/1325851): Log metrics if at least one field in the form was
  // classified as a certain type.

  if (base::FeatureList::IsEnabled(
          features::kAutofillLogUKMEventsWithSampleRate) ||
      base::FeatureList::IsEnabled(features::kAutofillFeedback)) {
    LogEventCountsUMAMetric(form_structure);
  }

  // ShouldUploadUkm reduces the UKM load by ignoring e.g. search boxes at best
  // effort.
  bool should_upload_ukm = base::FeatureList::IsEnabled(
                               features::kAutofillLogUKMEventsWithSampleRate) &&
                           ShouldUploadUkm(form_structure);

  for (const auto& autofill_field : form_structure) {
    if (should_upload_ukm) {
      form_interactions_ukm_logger()->LogAutofillFieldInfoAtFormRemove(
          form_structure, *autofill_field,
          AutocompleteStateForSubmittedField(*autofill_field));
    }

    // Clear log events.
    // Not conditions on kAutofillLogUKMEventsWithSampleRate because there may
    // be other reasons to log events.
    autofill_field->ClearLogEvents();
  }

  // Log FormSummary UKM event.
  if (should_upload_ukm) {
    AutofillMetrics::FormEventSet form_events;
    form_events.insert_all(
        address_form_event_logger_->GetFormEvents(form_structure.global_id()));
    form_events.insert_all(credit_card_form_event_logger_->GetFormEvents(
        form_structure.global_id()));
    form_interactions_ukm_logger()->LogAutofillFormSummaryAtFormRemove(
        form_structure, form_events, initial_interaction_timestamp_,
        form_submitted_timestamp_);
  }
}

bool BrowserAutofillManager::ShouldUploadUkm(
    const FormStructure& form_structure) {
  if (!form_structure.ShouldBeParsed()) {
    return false;
  }

  // Return true if the field is a visible text input field which has predicted
  // types from heuristics or the server.
  auto is_focusable_predicted_text_field =
      [](const std::unique_ptr<AutofillField>& field) {
        return field->IsTextInputElement() && field->IsFocusable() &&
               ((field->server_type() != NO_SERVER_DATA &&
                 field->server_type() != UNKNOWN_TYPE) ||
                field->heuristic_type() != UNKNOWN_TYPE ||
                field->html_type() != HtmlFieldType::kUnspecified);
      };

  size_t num_text_fields = base::ranges::count_if(
      form_structure.fields(), is_focusable_predicted_text_field);
  if (num_text_fields == 0) {
    return false;
  }

  // If the form contains a single text field and this contains the string
  // "search" in its name/id/placeholder, the function return false and the form
  // is not recorded into UKM. The form is considered a search box.
  if (num_text_fields == 1) {
    auto it = base::ranges::find_if(form_structure.fields(),
                                    is_focusable_predicted_text_field);
    if (base::ToLowerASCII((*it)->placeholder).find(u"search") !=
            std::string::npos ||
        base::ToLowerASCII((*it)->name).find(u"search") != std::string::npos ||
        base::ToLowerASCII((*it)->label).find(u"search") != std::string::npos ||
        base::ToLowerASCII((*it)->aria_label).find(u"search") !=
            std::string::npos) {
      return false;
    }
  }

  return true;
}

absl::optional<Suggestion>
BrowserAutofillManager::MaybeGetPlusAddressSuggestion() {
  plus_addresses::PlusAddressService* plus_address_service =
      client().GetPlusAddressService();
  if (!plus_address_service ||
      !plus_address_service->SupportsPlusAddresses(
          client().GetLastCommittedPrimaryMainFrameOrigin())) {
    return absl::nullopt;
  }
  absl::optional<std::string> maybe_address =
      plus_address_service->GetPlusAddress(
          client().GetLastCommittedPrimaryMainFrameOrigin());
  if (maybe_address == absl::nullopt) {
    Suggestion create_plus_address_suggestion(
        plus_address_service->GetCreateSuggestionLabel());
    create_plus_address_suggestion.popup_item_id =
        PopupItemId::kCreateNewPlusAddress;
    plus_addresses::PlusAddressMetrics::RecordAutofillSuggestionEvent(
        plus_addresses::PlusAddressMetrics::PlusAddressAutofillSuggestionEvent::
            kCreateNewPlusAddressSuggested);
    return create_plus_address_suggestion;
  }
  Suggestion existing_plus_address_suggestion(
      base::UTF8ToUTF16(maybe_address.value()));
  existing_plus_address_suggestion.popup_item_id =
      PopupItemId::kFillExistingPlusAddress;
  plus_addresses::PlusAddressMetrics::RecordAutofillSuggestionEvent(
      plus_addresses::PlusAddressMetrics::PlusAddressAutofillSuggestionEvent::
          kExistingPlusAddressSuggested);
  return existing_plus_address_suggestion;
}

absl::optional<Suggestion> BrowserAutofillManager::MaybeGetComposeSuggestion(
    const FormFieldData& field) {
  AutofillComposeDelegate* compose_delegate = client().GetComposeDelegate();
  if (!compose_delegate || !compose_delegate->ShouldOfferComposePopup(field)) {
    return absl::nullopt;
  }
  std::u16string suggestion_text;
  std::u16string label_text;
  if (compose_delegate->HasSavedState(field.global_id())) {
    // The nudge text indicates that the user can resume where they left off in
    // the Compose dialog.
    suggestion_text =
        l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_SAVED_TEXT);
    label_text = l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_SAVED_LABEL);
  } else {
    // Text for a new Compose session.
    suggestion_text =
        l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_MAIN_TEXT);
    label_text = l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_LABEL);
  }
  Suggestion suggestion(std::move(suggestion_text));
  suggestion.labels = {{Suggestion::Text(std::move(label_text))}};
  suggestion.popup_item_id = PopupItemId::kCompose;
  suggestion.icon = "keyIcon";
  return suggestion;
}

void BrowserAutofillManager::LogEventCountsUMAMetric(
    const FormStructure& form_structure) {
  size_t num_ask_for_values_to_fill_event = 0;
  size_t num_trigger_fill_event = 0;
  size_t num_fill_event = 0;
  size_t num_typing_event = 0;
  size_t num_heuristic_prediction_event = 0;
  size_t num_autocomplete_attribute_event = 0;
  size_t num_server_prediction_event = 0;
  size_t num_rationalization_event = 0;

  for (const auto& autofill_field : form_structure) {
    for (const auto& log_event : autofill_field->field_log_events()) {
      static_assert(
          absl::variant_size<AutofillField::FieldLogEventType>() == 9,
          "When adding new variants check that this function does not "
          "need to be updated.");
      if (absl::holds_alternative<AskForValuesToFillFieldLogEvent>(log_event)) {
        ++num_ask_for_values_to_fill_event;
      } else if (absl::holds_alternative<TriggerFillFieldLogEvent>(log_event)) {
        ++num_trigger_fill_event;
      } else if (absl::holds_alternative<FillFieldLogEvent>(log_event)) {
        ++num_fill_event;
      } else if (absl::holds_alternative<TypingFieldLogEvent>(log_event)) {
        ++num_typing_event;
      } else if (absl::holds_alternative<HeuristicPredictionFieldLogEvent>(
                     log_event)) {
        ++num_heuristic_prediction_event;
      } else if (absl::holds_alternative<AutocompleteAttributeFieldLogEvent>(
                     log_event)) {
        ++num_autocomplete_attribute_event;
      } else if (absl::holds_alternative<ServerPredictionFieldLogEvent>(
                     log_event)) {
        ++num_server_prediction_event;
      } else if (absl::holds_alternative<RationalizationFieldLogEvent>(
                     log_event)) {
        ++num_rationalization_event;
      } else {
        NOTREACHED();
      }
    }
  }

  size_t total_num_log_events =
      num_ask_for_values_to_fill_event + num_trigger_fill_event +
      num_fill_event + num_typing_event + num_heuristic_prediction_event +
      num_autocomplete_attribute_event + num_server_prediction_event +
      num_rationalization_event;
  // Record the number of each type of log events into UMA to decide if we need
  // to clear them before the form is submitted or destroyed.
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.AskForValuesToFillEvent",
                             num_ask_for_values_to_fill_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.TriggerFillEvent",
                             num_trigger_fill_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.FillEvent", num_fill_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.TypingEvent", num_typing_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.HeuristicPredictionEvent",
                             num_heuristic_prediction_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.AutocompleteAttributeEvent",
                             num_autocomplete_attribute_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.ServerPredictionEvent",
                             num_server_prediction_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.RationalizationEvent",
                             num_rationalization_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.All", total_num_log_events);
}

void BrowserAutofillManager::SetFastCheckoutRunId(
    FieldTypeGroup field_type_group,
    int64_t run_id) {
  switch (FieldTypeGroupToFormType(field_type_group)) {
    case FormType::kAddressForm:
      address_form_event_logger_->SetFastCheckoutRunId(run_id);
      return;
    case FormType::kCreditCardForm:
      credit_card_form_event_logger_->SetFastCheckoutRunId(run_id);
      break;
    case FormType::kPasswordForm:
    case FormType::kUnknownFormType:
      // FastCheckout only supports address and credit card forms.
      NOTREACHED();
  }
}

}  // namespace autofill
