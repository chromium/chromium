// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if defined(OS_IOS)
#include "components/autofill/core/browser/keyboard_accessory_metrics_logger.h"
#endif

namespace autofill {

using base::StartsWith;
using base::TimeTicks;
using mojom::SubmissionSource;

constexpr int kCreditCardSigninPromoImpressionLimit = 3;

namespace {

const size_t kMaxRecentFormSignaturesToRemember = 3;

// Time to wait, in ms, after a dynamic form change before triggering a refill.
// This is used for sites that change multiple things consecutively.
const size_t kWaitTimeForDynamicFormsMs = 200;

// The time limit, in ms, between a fill and when a refill can happen.
const int kLimitBeforeRefillMs = 1000;

// Returns the credit card field |value| trimmed from whitespace and with stop
// characters removed.
base::string16 SanitizeCreditCardFieldValue(const base::string16& value) {
  base::string16 sanitized;
  // We remove whitespace as well as some invisible unicode characters.
  base::TrimWhitespace(value, base::TRIM_ALL, &sanitized);
  base::TrimString(sanitized,
                   base::string16({base::i18n::kRightToLeftMark,
                                   base::i18n::kLeftToRightMark}),
                   &sanitized);
  // Some sites have ____-____-____-____ in their credit card number fields, for
  // example.
  base::RemoveChars(sanitized, base::ASCIIToUTF16("-_"), &sanitized);
  return sanitized;
}

// Returns whether the |field| is predicted as being any kind of name.
bool IsNameType(const AutofillField& field) {
  return field.Type().group() == NAME || field.Type().group() == NAME_BILLING ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FULL ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FIRST ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_LAST;
}

// Selects the right name type from the |old_types| to insert into the
// |types_to_keep| based on |is_credit_card|. This is called when we have
// multiple possible types.
void SelectRightNameType(AutofillField* field, bool is_credit_card) {
  DCHECK(field);
  // Currently, there can be up to four possible types for a field.
  DCHECK_GE(4U, field->possible_types().size());
  DCHECK_LE(2U, field->possible_types().size());

  ServerFieldTypeSet types_to_keep;
  const auto& old_types = field->possible_types();

  for (auto type : old_types) {
    FieldTypeGroup group = AutofillType(type).group();
    if ((is_credit_card && group == CREDIT_CARD) ||
        (!is_credit_card && group == NAME)) {
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
                               FormStructure* form_structure) {
  if (form_structure->developer_engagement_metrics()) {
    AutofillMetrics::LogDeveloperEngagementUkm(
        ukm_recorder, source_id, form_structure->main_frame_origin().GetURL(),
        form_structure->IsCompleteCreditCardForm(),
        form_structure->GetFormTypes(),
        form_structure->developer_engagement_metrics(),
        form_structure->form_signature());
  }
}

std::string GetAPIKeyForUrl(version_info::Channel channel) {
  // First look if we can get API key from command line flag.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAutofillAPIKey))
    return command_line.GetSwitchValueASCII(switches::kAutofillAPIKey);

  // Get the API key from Chrome baked keys.
  if (channel == version_info::Channel::STABLE)
    return google_apis::GetAPIKey();
  return google_apis::GetNonStableAPIKey();
}

ValuePatternsMetric GetValuePattern(const base::string16& value) {
  if (IsUPIVirtualPaymentAddress(value))
    return ValuePatternsMetric::kUpiVpa;
  if (IsInternationalBankAccountNumber(value))
    return ValuePatternsMetric::kIban;
  return ValuePatternsMetric::kNoPatternFound;
}

void LogValuePatternsMetric(const FormData& form) {
  for (const FormFieldData& field : form.fields) {
    if (!field.IsVisible()) {
      // Ignore hidden fields.
      continue;
    }
    base::string16 value;
    base::TrimWhitespace(field.value, base::TRIM_ALL, &value);
    if (value.empty())
      continue;
    base::UmaHistogramEnumeration("Autofill.SubmittedValuePatterns",
                                  GetValuePattern(value));
  }
}

void LogLanguageMetrics(const translate::LanguageState* language_state) {
  if (language_state) {
    AutofillMetrics::LogFieldParsingTranslatedFormLanguageMetric(
        language_state->current_language());
    AutofillMetrics::LogFieldParsingPageTranslationStatusMetric(
        language_state->IsPageTranslated());
  }
}

bool IsAddressForm(FieldTypeGroup field_type_group) {
  switch (field_type_group) {
    case NAME:
    case NAME_BILLING:
    case EMAIL:
    case COMPANY:
    case ADDRESS_HOME:
    case ADDRESS_BILLING:
    case PHONE_HOME:
    case PHONE_BILLING:
      return true;
    case CREDIT_CARD:
    case TRANSACTION:
    case PASSWORD_FIELD:
    case USERNAME_FIELD:
    case NO_GROUP:
    case UNFILLABLE:
      return false;
  }
  NOTREACHED();
  return false;
}

void LogAutofillTypePredictionsAvailable(
    LogManager* log_manager,
    const std::vector<FormStructure*>& forms) {
  if (VLOG_IS_ON(1)) {
    VLOG(1) << "Parsed forms:";
    for (FormStructure* form : forms)
      VLOG(1) << *form;
  }

  if (!log_manager || !log_manager->IsLoggingActive())
    return;

  LogBuffer buffer;
  for (FormStructure* form : forms)
    buffer << *form;

  log_manager->Log() << LoggingScope::kParsing << LogMessage::kParsedForms
                     << std::move(buffer);
}

// Finds the first field in |form_structure| with |field.value|=|value|.
AutofillField* FindFirstFieldWithValue(const FormStructure& form_structure,
                                       const base::string16& value) {
  for (const auto& field : form_structure) {
    base::string16 trimmed_value;
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

    base::string16 trimmed_value;
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
    base::string16 last_unlocked_credit_card_cvc) {
  if (!last_unlocked_credit_card_cvc.empty()) {
    AutofillField* result =
        FindFirstFieldWithValue(form_structure, last_unlocked_credit_card_cvc);
    if (result)
      result->properties_mask = FieldPropertiesFlags::kKnownValue;
    return result;
  }

  return HeuristicallyFindCVCFieldForUpload(form_structure);
}

// Some autofill types are detected based on values and not based on form
// features. We may decide that it's an autofill form after submission.
bool ContainsAutofillableValue(const autofill::FormStructure& form) {
  return base::ranges::any_of(form, [](const auto& field) {
    return base::Contains(field->possible_types(), UPI_VPA) ||
           IsUPIVirtualPaymentAddress(field->value);
  });
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Retrieves all valid credit card candidates for virtual card selection. A
// valid candidate must have exactly one cloud token.
std::vector<CreditCard*> GetVirtualCardCandidates(
    PersonalDataManager* personal_data_manager) {
  DCHECK(personal_data_manager);
  std::vector<CreditCard*> candidates =
      personal_data_manager->GetServerCreditCards();
  const std::vector<CreditCardCloudTokenData*> cloud_token_data =
      personal_data_manager->GetCreditCardCloudTokenData();

  // Constructs map.
  std::unordered_map<std::string, int> id_count;
  for (CreditCardCloudTokenData* data : cloud_token_data) {
    const auto& iterator = id_count.find(data->masked_card_id);
    if (iterator == id_count.end())
      id_count.emplace(data->masked_card_id, 1);
    else
      iterator->second += 1;
  }

  // Remove the card from the vector that either has multiple cloud token data
  // or has no cloud token data.
  base::EraseIf(candidates, [&](const auto& card) {
    const auto& iterator = id_count.find(card->server_id());
    return iterator == id_count.end() || iterator->second > 1;
  });

  // Returns the remaining valid cards.
  return candidates;
}
#endif

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
int TypeValueFormFillingLimit(ServerFieldType field_type) {
  return field_type == CREDIT_CARD_NUMBER ? kCreditCardTypeValueFormFillingLimit
                                          : kTypeValueFormFillingLimit;
}

}  // namespace

AutofillManager::FillingContext::FillingContext(
    const AutofillField& field,
    const AutofillProfile* optional_profile,
    const CreditCard* optional_credit_card,
    const base::string16* optional_cvc)
    : profile(optional_profile ? base::make_optional(*optional_profile)
                               : base::nullopt),
      credit_card(optional_credit_card
                      ? base::make_optional(std::make_pair(
                            *optional_credit_card,
                            optional_cvc ? *optional_cvc : base::string16()))
                      : base::nullopt),
      filled_field_name(field.unique_name()),
      original_fill_time(AutofillTickClock::NowTicks()) {
  DCHECK(optional_profile || optional_credit_card);
  DCHECK(optional_credit_card || !optional_cvc);
  DCHECK(profile || credit_card);
}

AutofillManager::FillingContext::~FillingContext() = default;

AutofillManager::AutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillDownloadManagerState enable_download_manager)
    : AutofillManager(driver,
                      client,
                      client->GetPersonalDataManager(),
                      client->GetAutocompleteHistoryManager(),
                      app_locale,
                      enable_download_manager) {}

AutofillManager::~AutofillManager() {
  if (has_parsed_forms_) {
    base::UmaHistogramBoolean(
        "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
        has_observed_phone_number_field_);
    base::UmaHistogramBoolean("Autofill.WebOTP.OneTimeCode.FromAutocomplete",
                              has_observed_one_time_code_field_);
  }

  if (autocomplete_history_manager_) {
    autocomplete_history_manager_->CancelPendingQueries(this);
  }
}

void AutofillManager::SetExternalDelegate(AutofillExternalDelegate* delegate) {
  // TODO(jrg): consider passing delegate into the ctor.  That won't
  // work if the delegate has a pointer to the AutofillManager, but
  // future directions may not need such a pointer.
  external_delegate_ = delegate;
}

void AutofillManager::ShowAutofillSettings(bool show_credit_card_settings) {
  client_->ShowAutofillSettings(show_credit_card_settings);
}

bool AutofillManager::ShouldShowScanCreditCard(const FormData& form,
                                               const FormFieldData& field) {
  if (!client_->HasCreditCardScanFeature())
    return false;

  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field)
    return false;

  bool is_card_number_field =
      autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER &&
      base::ContainsOnlyChars(CreditCard::StripSeparators(field.value),
                              base::ASCIIToUTF16("0123456789"));

  if (!is_card_number_field)
    return false;

  if (IsFormNonSecure(form))
    return false;

  static const int kShowScanCreditCardMaxValueLength = 6;
  return field.value.size() <= kShowScanCreditCardMaxValueLength;
}

PopupType AutofillManager::GetPopupType(const FormData& form,
                                        const FormFieldData& field) {
  const AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field)
    return PopupType::kUnspecified;

  switch (autofill_field->Type().group()) {
    case NO_GROUP:
    case PASSWORD_FIELD:
    case TRANSACTION:
    case USERNAME_FIELD:
    case UNFILLABLE:
      return PopupType::kUnspecified;

    case CREDIT_CARD:
      return PopupType::kCreditCards;

    case ADDRESS_HOME:
    case ADDRESS_BILLING:
      return PopupType::kAddresses;

    case NAME:
    case NAME_BILLING:
    case EMAIL:
    case COMPANY:
    case PHONE_HOME:
    case PHONE_BILLING:
      return FormHasAddressField(form) ? PopupType::kAddresses
                                       : PopupType::kPersonalInformation;

    default:
      NOTREACHED();
  }
}

bool AutofillManager::ShouldShowCreditCardSigninPromo(
    const FormData& form,
    const FormFieldData& field) {
  // Check whether we are dealing with a credit card field and whether it's
  // appropriate to show the promo (e.g. the platform is supported).
  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field || autofill_field->Type().group() != CREDIT_CARD ||
      !client_->ShouldShowSigninPromo())
    return false;

  if (IsFormNonSecure(form))
    return false;

  // The last step is checking if we are under the limit of impressions.
  int impression_count = client_->GetPrefs()->GetInteger(
      prefs::kAutofillCreditCardSigninPromoImpressionCount);
  if (impression_count < kCreditCardSigninPromoImpressionLimit) {
    // The promo will be shown. Increment the impression count.
    client_->GetPrefs()->SetInteger(
        prefs::kAutofillCreditCardSigninPromoImpressionCount,
        impression_count + 1);
    return true;
  }

  return false;
}

bool AutofillManager::ShouldShowCardsFromAccountOption(
    const FormData& form,
    const FormFieldData& field) {
  // Check whether we are dealing with a credit card field.
  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field || autofill_field->Type().group() != CREDIT_CARD ||
      // Exclude CVC and card type fields, because these will not have
      // suggestions available after the user opts in.
      autofill_field->Type().GetStorableType() ==
          CREDIT_CARD_VERIFICATION_CODE ||
      autofill_field->Type().GetStorableType() == CREDIT_CARD_TYPE) {
    return false;
  }

  if (IsFormNonSecure(form))
    return false;

  return personal_data_->ShouldShowCardsFromAccountOption();
}

void AutofillManager::OnUserAcceptedCardsFromAccountOption() {
  personal_data_->OnUserAcceptedCardsFromAccountOption();
}

void AutofillManager::RefetchCardsAndUpdatePopup(
    int query_id,
    const FormData& form,
    const FormFieldData& field_data) {
  AutofillField* autofill_field = GetAutofillField(form, field_data);
  AutofillType type = autofill_field ? autofill_field->Type()
                                     : AutofillType(CREDIT_CARD_NUMBER);

  DCHECK_EQ(CREDIT_CARD, type.group());

  bool should_display_gpay_logo;
  auto cards =
      GetCreditCardSuggestions(field_data, type, &should_display_gpay_logo);

  DCHECK(!cards.empty());

  external_delegate_->OnSuggestionsReturned(
      query_id, cards,
      /*autoselect_first_suggestion=*/false, should_display_gpay_logo);
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void AutofillManager::FetchVirtualCardCandidates() {
  const std::vector<CreditCard*>& candidates =
      GetVirtualCardCandidates(personal_data_);
  // Make sure the |candidates| is not empty, otherwise the check in
  // ShouldShowVirtualCardOption() should fail.
  DCHECK(!candidates.empty());

  client_->OfferVirtualCardOptions(
      candidates,
      base::BindOnce(&AutofillManager::OnVirtualCardCandidateSelected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutofillManager::OnVirtualCardCandidateSelected(
    const std::string& selected_card_id) {
  // TODO(crbug.com/1020740): Implement this and the following flow in a
  // separate CL. The following flow will be sending a request to Payments
  // to fetched the up-to-date cloud token data for the selected card and fill
  // the information in the form.
}
#endif

bool AutofillManager::ShouldParseForms(const std::vector<FormData>& forms,
                                       const base::TimeTicks timestamp) {
  bool autofill_enabled = IsAutofillEnabled();
  sync_state_ = personal_data_ ? personal_data_->GetSyncSigninState()
                               : AutofillSyncSigninState::kNumSyncStates;
  if (!has_logged_autofill_enabled_) {
    AutofillMetrics::LogIsAutofillEnabledAtPageLoad(autofill_enabled,
                                                    sync_state_);
    AutofillMetrics::LogIsAutofillProfileEnabledAtPageLoad(
        IsAutofillProfileEnabled(), sync_state_);
    AutofillMetrics::LogIsAutofillCreditCardEnabledAtPageLoad(
        IsAutofillCreditCardEnabled(), sync_state_);
    has_logged_autofill_enabled_ = true;
  }

  return autofill_enabled;
}

void AutofillManager::OnFormSubmittedImpl(const FormData& form,
                                          bool known_success,
                                          SubmissionSource source) {
  if (log_manager_) {
    log_manager_->Log() << LoggingScope::kSubmission
                        << LogMessage::kFormSubmissionDetected << Br{}
                        << "known_success: " << known_success << Br{}
                        << "source: " << SubmissionSourceToString(source)
                        << Br{} << form;
  }

  // Always upload page language metrics.
  LogLanguageMetrics(client_->GetLanguageState());

  // Always let the value patterns metric upload data.
  LogValuePatternsMetric(form);

  // We will always give Autocomplete a chance to save the data.
  std::unique_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  if (!submitted_form) {
    autocomplete_history_manager_->OnWillSubmitForm(
        form, client_->IsAutocompleteEnabled());
    return;
  }

  // However, if Autofill has recognized a field as CVC, that shouldn't be
  // saved.
  FormData form_for_autocomplete = submitted_form->ToFormData();
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    if (submitted_form->field(i)->Type().GetStorableType() ==
        CREDIT_CARD_VERIFICATION_CODE) {
      form_for_autocomplete.fields[i].should_autocomplete = false;
    }
  }
  autocomplete_history_manager_->OnWillSubmitForm(
      form_for_autocomplete, client_->IsAutocompleteEnabled());

  if (IsAutofillProfileEnabled()) {
    address_form_event_logger_->OnWillSubmitForm(sync_state_, *submitted_form);
  }
  if (IsAutofillCreditCardEnabled()) {
    credit_card_form_event_logger_->OnWillSubmitForm(sync_state_,
                                                     *submitted_form);
  }

  submitted_form->set_submission_source(source);
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
    address_form_event_logger_->OnFormSubmitted(/*force_logging=*/false,
                                                sync_state_, *submitted_form);
  }
  if (IsAutofillCreditCardEnabled()) {
    credit_card_form_event_logger_->OnFormSubmitted(
        enable_ablation_logging_, sync_state_, *submitted_form);
  }

  if (!submitted_form->IsAutofillable() &&
      !ContainsAutofillableValue(*submitted_form)) {
    return;
  }

  // Update Personal Data with the form's submitted data.
  // Also triggers offering local/upload credit card save, if applicable.
  client_->GetFormDataImporter()->ImportFormData(*submitted_form,
                                                 IsAutofillProfileEnabled(),
                                                 IsAutofillCreditCardEnabled());
}

bool AutofillManager::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    bool observed_submission) {
  // It is possible for |personal_data_| to be null, such as when used in the
  // Android webview.
  if (!personal_data_)
    return false;

  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();
  personal_data_->UpdateProfilesServerValidityMapsIfNeeded(profiles);
  if (observed_submission && form_structure->IsAutofillable()) {
    AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
        personal_data_->GetProfiles().size());
  }

  const std::vector<CreditCard*>& credit_cards =
      credit_card_access_manager_->GetCreditCards();

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
  base::Optional<std::string> page_language = GetPageLanguage();
  if (page_language)
    form_structure->set_page_language(page_language.value());

  // Attach the Randomized Encoder.
  form_structure->set_randomized_encoder(
      RandomizedEncoder::Create(client_->GetPrefs()));

  // Note that ownership of |form_structure| is passed to the second task,
  // using |base::Owned|. We MUST temporarily hang on to the raw form pointer
  // so that we can safely pass the address to the first callback regardless of
  // the (undefined) order in which the callback parameters are computed.
  FormStructure* raw_form = form_structure.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      // If the priority is BEST_EFFORT, the task can be preempted, which is
      // thought to cause high memory usage (as memory is retained by the task
      // while it is preempted).
      //
      // TODO(fdoray): Update when the hypothesis that setting the priority to
      // USER_VISIBLE instead of BEST_EFFORT fixes memory usage. Consider
      // keeping BEST_EFFORT priority, but manually enforcing a limit on the
      // number of outstanding tasks. https://crbug.com/974249
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&AutofillManager::DeterminePossibleFieldTypesForUpload,
                     copied_profiles, copied_credit_cards,
                     last_unlocked_credit_card_cvc_, app_locale_, raw_form),
      base::BindOnce(&AutofillManager::UploadFormDataAsyncCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(form_structure.release()),
                     initial_interaction_timestamp_,
                     AutofillTickClock::NowTicks(), observed_submission));
  return true;
}

void AutofillManager::UpdatePendingForm(const FormData& form) {
  // Process the current pending form if different than supplied |form|.
  if (pending_form_data_ && !pending_form_data_->SameFormAs(form)) {
    ProcessPendingFormForUpload();
  }
  // A new pending form is assigned.
  pending_form_data_.reset(new FormData(form));
}

void AutofillManager::ProcessPendingFormForUpload() {
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

void AutofillManager::DidSuppressPopup(const FormData& form,
                                       const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  auto* logger = GetEventFormLogger(autofill_field->Type().group());
  if (logger)
    logger->OnPopupSuppressed(*form_structure, *autofill_field);
}

void AutofillManager::OnTextFieldDidChangeImpl(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box,
                                               const TimeTicks timestamp) {
  if (test_delegate_)
    test_delegate_->OnTextFieldChanged();

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  UpdatePendingForm(form);

  uint32_t profile_form_bitmask = 0;
  if (!user_did_type_ || autofill_field->is_autofilled) {
    form_interactions_ukm_logger_->LogTextFieldDidChange(*form_structure,
                                                         *autofill_field);
    profile_form_bitmask = data_util::DetermineGroups(*form_structure);
  }

  if (!autofill_field->is_autofilled) {
    auto* logger = GetEventFormLogger(autofill_field->Type().group());
    if (logger)
      logger->OnTypedIntoNonFilledField();
  }

  if (!user_did_type_) {
    user_did_type_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_TYPE, autofill_field->Type().group(),
        client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  }

  if (autofill_field->is_autofilled) {
    autofill_field->is_autofilled = false;
    autofill_field->set_previously_autofilled(true);
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD,
        autofill_field->Type().group(),
        client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);

    auto* logger = GetEventFormLogger(autofill_field->Type().group());
    if (logger)
      logger->OnEditedAutofilledField();

    if (!user_did_edit_autofilled_field_) {
      user_did_edit_autofilled_field_ = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE,
          autofill_field->Type().group(),
          client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
    }
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

bool AutofillManager::IsFormNonSecure(const FormData& form) const {
  return IsFormOrClientNonSecure(client_, form);
}

void AutofillManager::OnQueryFormFieldAutofillImpl(
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& transformed_box,
    bool autoselect_first_suggestion) {
  SetDataList(field.datalist_values, field.datalist_labels);
  external_delegate_->OnQuery(query_id, form, field, transformed_box);

  std::vector<Suggestion> suggestions;
  SuggestionsContext context;
  GetAvailableSuggestions(form, field, &suggestions, &context);

  if (context.is_autofill_available) {
    switch (context.suppress_reason) {
      case SuppressReason::kNotSuppressed:
        break;

      case SuppressReason::kCreditCardsAblation:
        enable_ablation_logging_ = true;
        autocomplete_history_manager_->CancelPendingQueries(this);
        external_delegate_->OnSuggestionsReturned(query_id, suggestions,
                                                  autoselect_first_suggestion);
        return;

      case SuppressReason::kInsecureForm:
      case SuppressReason::kAutocompleteOff:
        return;
    }

    if (!suggestions.empty()) {
      if (context.is_filling_credit_card) {
        AutofillMetrics::LogIsQueriedCreditCardFormSecure(
            context.is_context_secure);
      }

      // The first time we show suggestions on this page, log the number of
      // suggestions available.
      // TODO(mathp): Differentiate between number of suggestions available
      // (current metric) and number shown to the user.
      if (!has_logged_address_suggestions_count_) {
        AutofillMetrics::LogAddressSuggestionsCount(suggestions.size());
        has_logged_address_suggestions_count_ = true;
      }
    }
  }

  // If there are no Autofill suggestions, consider showing Autocomplete
  // suggestions. We will not show Autocomplete suggestions for a field that
  // specifies autocomplete=off (or an unrecognized type), a field for which we
  // will show the credit card signin promo, a field that we think is a
  // credit card expiration, cvc or number, or on forms displayed on secure
  // (i.e. HTTPS) sites that submit insecurely (over HTTP).
  if (suggestions.empty() && !ShouldShowCreditCardSigninPromo(form, field) &&
      field.should_autocomplete &&
      !(context.focused_field &&
        (autofill::data_util::IsCreditCardExpirationType(
             context.focused_field->Type().GetStorableType()) ||
         context.focused_field->Type().html_type() == HTML_TYPE_UNRECOGNIZED ||
         context.focused_field->Type().GetStorableType() ==
             CREDIT_CARD_NUMBER ||
         context.focused_field->Type().GetStorableType() ==
             CREDIT_CARD_VERIFICATION_CODE)) &&
      context.suppress_reason != SuppressReason::kInsecureForm) {
    // Suggestions come back asynchronously, so the Autocomplete manager will
    // handle sending the results back to the renderer.
    autocomplete_history_manager_->OnGetAutocompleteSuggestions(
        query_id, client_->IsAutocompleteEnabled(), autoselect_first_suggestion,
        field.name, field.value, field.form_control_type,
        weak_ptr_factory_.GetWeakPtr());
    return;
  }

  // Send Autofill suggestions (could be an empty list).
  autocomplete_history_manager_->CancelPendingQueries(this);
  external_delegate_->OnSuggestionsReturned(query_id, suggestions,
                                            autoselect_first_suggestion,
                                            context.should_display_gpay_logo);
}

bool AutofillManager::WillFillCreditCardNumber(const FormData& form,
                                               const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return false;

  if (autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER)
    return true;

  DCHECK_EQ(form_structure->field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    if (form_structure->field(i)->section == autofill_field->section &&
        form_structure->field(i)->Type().GetStorableType() ==
            CREDIT_CARD_NUMBER &&
        form.fields[i].value.empty() && !form.fields[i].is_autofilled) {
      return true;
    }
  }

  return false;
}

void AutofillManager::FillOrPreviewCreditCardForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const CreditCard* credit_card) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  credit_card_ = credit_card ? *credit_card : CreditCard();
  bool is_preview = action != AutofillDriver::FORM_DATA_ACTION_FILL;
  bool should_fetch_card = !is_preview && WillFillCreditCardNumber(form, field);

  if (should_fetch_card) {
    credit_card_form_event_logger_->OnDidSelectCardSuggestion(
        credit_card_, *form_structure, sync_state_);

    credit_card_action_ = action;
    credit_card_query_id_ = query_id;
    credit_card_form_ = form;
    credit_card_field_ = field;

    // CreditCardAccessManager::FetchCreditCard() will call
    // OnCreditCardFetched() in this class after successfully fetching the card.
    credit_card_access_manager_->FetchCreditCard(
        credit_card, weak_ptr_factory_.GetWeakPtr(),
        form_structure->form_parsed_timestamp());
    return;
  }

  if (!is_preview) {
    credit_card_form_event_logger_->OnDidFillSuggestion(
        credit_card_, *form_structure, *autofill_field, sync_state_);
  }

  FillOrPreviewDataModelForm(action, query_id, form, field, /*profile=*/nullptr,
                             &credit_card_, /*cvc=*/nullptr, form_structure,
                             autofill_field);
}

void AutofillManager::FillOrPreviewProfileForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const AutofillProfile& profile) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL) {
    address_form_event_logger_->OnDidFillSuggestion(
        profile, *form_structure, *autofill_field, sync_state_);
  }

  FillOrPreviewDataModelForm(action, query_id, form, field, &profile,
                             /*credit_card=*/nullptr,
                             /*cvc=*/nullptr, form_structure, autofill_field);
}

void AutofillManager::FillOrPreviewForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    int unique_id) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  // NOTE: RefreshDataModels may invalidate |data_model|. Thus it must come
  // before GetProfile or GetCreditCard.
  if (!RefreshDataModels() || !driver()->RendererIsAvailable())
    return;

  const AutofillProfile* profile = GetProfile(unique_id);
  const CreditCard* credit_card = GetCreditCard(unique_id);

  if (credit_card)
    FillOrPreviewCreditCardForm(action, query_id, form, field, credit_card);
  else if (profile)
    FillOrPreviewProfileForm(action, query_id, form, field, *profile);
}

void AutofillManager::FillCreditCardForm(int query_id,
                                         const FormData& form,
                                         const FormFieldData& field,
                                         const CreditCard& credit_card,
                                         const base::string16& cvc) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field) ||
      !driver()->RendererIsAvailable()) {
    return;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  FillOrPreviewDataModelForm(AutofillDriver::FORM_DATA_ACTION_FILL, query_id,
                             form, field, /*profile=*/nullptr, &credit_card,
                             &cvc, form_structure, autofill_field);
}

void AutofillManager::FillProfileForm(const autofill::AutofillProfile& profile,
                                      const FormData& form,
                                      const FormFieldData& field) {
  FillOrPreviewProfileForm(AutofillDriver::FORM_DATA_ACTION_FILL,
                           /*query_id=*/-1, form, field, profile);
}

void AutofillManager::OnFocusNoLongerOnForm() {
  ProcessPendingFormForUpload();

#if defined(OS_CHROMEOS)
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

void AutofillManager::OnFocusOnFormFieldImpl(const FormData& form,
                                             const FormFieldData& field,
                                             const gfx::RectF& bounding_box) {
  // Notify installed screen readers if the focus is on a field for which there
  // are suggestions to present. Ignore if a screen reader is not present. If
  // the platform is ChromeOS, then assume ChromeVox is in use as there is no
  // way of determining whether it's being used from this point in the code.
#if !defined(OS_CHROMEOS)
  if (!external_delegate_->HasActiveScreenReader())
    return;
#endif

  // TODO(https://crbug.com/848427): Add metrics for performance impact.
  std::vector<Suggestion> suggestions;
  SuggestionsContext context;
  GetAvailableSuggestions(form, field, &suggestions, &context);

  external_delegate_->OnAutofillAvailabilityEvent(
      (context.suppress_reason == SuppressReason::kNotSuppressed &&
       !suggestions.empty())
          ? mojom::AutofillState::kAutofillAvailable
          : mojom::AutofillState::kNoSuggestions);
}

void AutofillManager::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  // TODO(crbug.com/814961): Handle select control change.
}

void AutofillManager::OnDidPreviewAutofillFormData() {
  if (test_delegate_)
    test_delegate_->DidPreviewFormData();
}

void AutofillManager::OnDidFillAutofillFormData(const FormData& form,
                                                const TimeTicks timestamp) {
  if (test_delegate_)
    test_delegate_->DidFillFormData();

  UpdatePendingForm(form);

  std::set<FormType> form_types;
  // Find the FormStructure that corresponds to |form|. Use default form type if
  // form is not present in our cache, which will happen rarely.

  FormStructure* form_structure =
      FindCachedFormByRendererId(form.unique_renderer_id);
  if (form_structure) {
    form_types = form_structure->GetFormTypes();
  }

  uint32_t profile_form_bitmask =
      form_structure ? data_util::DetermineGroups(*form_structure) : 0;

  AutofillMetrics::LogUserHappinessMetric(
      AutofillMetrics::USER_DID_AUTOFILL, form_types,
      client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  if (!user_did_autofill_) {
    user_did_autofill_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE, form_types,
        client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void AutofillManager::DidShowSuggestions(bool has_autofill_suggestions,
                                         const FormData& form,
                                         const FormFieldData& field) {
  if (test_delegate_)
    test_delegate_->DidShowSuggestions();

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
      client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);

  if (!did_show_suggestions_) {
    did_show_suggestions_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, autofill_field->Type().group(),
        client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);
  }

  auto* logger = GetEventFormLogger(autofill_field->Type().group());
  if (logger) {
    logger->OnDidShowSuggestions(*form_structure, *autofill_field,
                                 form_structure->form_parsed_timestamp(),
                                 sync_state_, driver()->IsIncognito());
  }

  if (autofill_field->Type().group() == CREDIT_CARD &&
      base::FeatureList::IsEnabled(
          features::kAutofillCreditCardAuthentication)) {
    credit_card_access_manager_->PrepareToFetchCreditCard();
  }
}

void AutofillManager::OnHidePopup() {
  if (!IsAutofillEnabled())
    return;

  autocomplete_history_manager_->CancelPendingQueries(this);
  client_->HideAutofillPopup(PopupHidingReason::kRendererEvent);
}

bool AutofillManager::GetDeletionConfirmationText(const base::string16& value,
                                                  int identifier,
                                                  base::string16* title,
                                                  base::string16* body) {
  if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    if (title)
      title->assign(value);
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
    }

    return true;
  }

  if (identifier < 0)
    return false;

  const CreditCard* credit_card = GetCreditCard(identifier);
  const AutofillProfile* profile = GetProfile(identifier);

  if (credit_card) {
    return credit_card_access_manager_->GetDeletionConfirmationText(
        credit_card, title, body);
  }

  if (profile) {
    if (profile->record_type() != AutofillProfile::LOCAL_PROFILE)
      return false;

    if (title) {
      base::string16 street_address = profile->GetRawInfo(ADDRESS_HOME_CITY);
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

  NOTREACHED();
  return false;
}

bool AutofillManager::RemoveAutofillProfileOrCreditCard(int unique_id) {
  const CreditCard* credit_card = GetCreditCard(unique_id);
  if (credit_card) {
    return credit_card_access_manager_->DeleteCard(credit_card);
  }

  const AutofillProfile* profile = GetProfile(unique_id);
  if (profile) {
    bool is_local = profile->record_type() == AutofillProfile::LOCAL_PROFILE;
    if (is_local)
      personal_data_->RemoveByGUID(profile->guid());

    return is_local;
  }

  NOTREACHED();
  return false;
}

void AutofillManager::RemoveAutocompleteEntry(const base::string16& name,
                                              const base::string16& value) {
  autocomplete_history_manager_->OnRemoveAutocompleteEntry(name, value);
}

void AutofillManager::OnAutocompleteEntrySelected(const base::string16& value) {
  autocomplete_history_manager_->OnAutocompleteEntrySelected(value);
}

void AutofillManager::OnUserHideSuggestions(const FormData& form,
                                            const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  auto* logger = GetEventFormLogger(autofill_field->Type().group());
  if (logger)
    logger->OnUserHideSuggestions(*form_structure, *autofill_field);
}

bool AutofillManager::ShouldClearPreviewedForm() {
  return credit_card_access_manager_->ShouldClearPreviewedForm();
}

payments::FullCardRequest* AutofillManager::GetOrCreateFullCardRequest() {
  return credit_card_access_manager_->GetOrCreateCVCAuthenticator()
      ->GetFullCardRequest();
}

base::WeakPtr<payments::FullCardRequest::UIDelegate>
AutofillManager::GetAsFullCardRequestUIDelegate() {
  return credit_card_access_manager_->GetOrCreateCVCAuthenticator()
      ->GetAsFullCardRequestUIDelegate();
}

void AutofillManager::SetTestDelegate(AutofillManagerTestDelegate* delegate) {
  test_delegate_ = delegate;
}

void AutofillManager::SetDataList(const std::vector<base::string16>& values,
                                  const std::vector<base::string16>& labels) {
  if (!IsValidString16Vector(values) || !IsValidString16Vector(labels) ||
      values.size() != labels.size())
    return;

  external_delegate_->SetCurrentDataListValues(values, labels);
}

void AutofillManager::SelectFieldOptionsDidChange(const FormData& form) {
  // Look for a cached version of the form. It will be a null pointer if none is
  // found, which is fine.
  FormStructure* cached_form =
      FindCachedFormByRendererId(form.unique_renderer_id);

  FormStructure* form_structure = ParseForm(form, cached_form);
  if (!form_structure)
    return;

  if (ShouldTriggerRefill(*form_structure))
    TriggerRefill(form);
}

void AutofillManager::OnLoadedServerPredictions(
    std::string response,
    const std::vector<FormSignature>& queried_form_signatures) {
  // Get the current valid FormStructures represented by
  // |queried_form_signatures|.
  std::vector<FormStructure*> queried_forms;
  queried_forms.reserve(queried_form_signatures.size());
  for (const auto& form_signature : queried_form_signatures) {
    FindCachedFormsBySignature(form_signature, &queried_forms);
  }

  // Each form signature in |queried_form_signatures| is supposed to be unique,
  // and therefore appear only once. This ensures that
  // FindCachedFormsBySignature() produces an output without duplicates in the
  // forms.
  // TODO(crbug/1064709): |queried_forms| could be a set data structure; their
  // order should be irrelevant.
  DCHECK_EQ(queried_forms.size(),
            std::set<FormStructure*>(queried_forms.begin(), queried_forms.end())
                .size());

  // If there are no current forms corresponding to the queried signatures, drop
  // the query response.
  if (queried_forms.empty())
    return;

  // Parse and store the server predictions.
  FormStructure::ParseApiQueryResponse(std::move(response), queried_forms,
                                       queried_form_signatures,
                                       form_interactions_ukm_logger_.get());

  // Will log quality metrics for each FormStructure based on the presence of
  // autocomplete attributes, if available.
  for (FormStructure* cur_form : queried_forms) {
    cur_form->LogQualityMetricsBasedOnAutocomplete(
        form_interactions_ukm_logger_.get());
  }

  // Send field type predictions to the renderer so that it can possibly
  // annotate forms with the predicted types or add console warnings.
  driver()->SendAutofillTypePredictionsToRenderer(queried_forms);

  LogAutofillTypePredictionsAvailable(log_manager_, queried_forms);

  // Forward form structures to the password generation manager to detect
  // account creation forms.
  driver()->PropagateAutofillPredictions(queried_forms);
}

void AutofillManager::OnCreditCardFetched(bool did_succeed,
                                          const CreditCard* credit_card,
                                          const base::string16& cvc) {
  if (!did_succeed) {
    driver()->RendererShouldClearPreviewedForm();
    return;
  }

  last_unlocked_credit_card_cvc_ = cvc;

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(credit_card_form_, credit_card_field_,
                             &form_structure, &autofill_field))
    return;

  // The originally selected masked card is |credit_card_|. So we must log
  // |credit_card_| as opposed to |credit_card| to correctly indicate that the
  // user filled the form using a masked card suggestion.
  credit_card_form_event_logger_->OnDidFillSuggestion(
      credit_card_, *form_structure, *autofill_field, sync_state_);

  DCHECK(credit_card);
  FillCreditCardForm(credit_card_query_id_, credit_card_form_,
                     credit_card_field_, *credit_card, cvc);
  if (base::FeatureList::IsEnabled(features::kAutofillCacheServerCardInfo) &&
      credit_card->record_type() == CreditCard::FULL_SERVER_CARD) {
    credit_card_access_manager_->CacheUnmaskedCardInfo(*credit_card, cvc);
  }
}

void AutofillManager::OnDidEndTextFieldEditing() {
  external_delegate_->DidEndTextFieldEditing();
}

bool AutofillManager::IsAutofillEnabled() const {
  return IsAutofillProfileEnabled() || IsAutofillCreditCardEnabled();
}

bool AutofillManager::IsAutofillProfileEnabled() const {
  return ::autofill::prefs::IsAutofillProfileEnabled(client_->GetPrefs());
}

bool AutofillManager::IsAutofillCreditCardEnabled() const {
  return ::autofill::prefs::IsAutofillCreditCardEnabled(client_->GetPrefs());
}

// static
bool AutofillManager::IsRichQueryEnabled(version_info::Channel channel) {
  return base::FeatureList::IsEnabled(features::kAutofillRichMetadataQueries) &&
         channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
}

const FormData& AutofillManager::last_query_form() const {
  return external_delegate_->query_form();
}

bool AutofillManager::ShouldUploadForm(const FormStructure& form) {
  return IsAutofillEnabled() && !driver()->IsIncognito() &&
         form.ShouldBeUploaded();
}

// AutocompleteHistoryManager::SuggestionsHandler implementation
void AutofillManager::OnSuggestionsReturned(
    int query_id,
    bool autoselect_first_suggestion,
    const std::vector<Suggestion>& suggestions) {
  external_delegate_->OnSuggestionsReturned(query_id, suggestions,
                                            autoselect_first_suggestion);
}

// Note that |submitted_form| is passed as a pointer rather than as a reference
// so that we can get memory management right across threads.  Note also that we
// explicitly pass in all the time stamps of interest, as the cached ones might
// get reset before this method executes.
void AutofillManager::UploadFormDataAsyncCallback(
    const FormStructure* submitted_form,
    const TimeTicks& interaction_time,
    const TimeTicks& submission_time,
    bool observed_submission) {
  if (submitted_form->ShouldRunHeuristics() ||
      submitted_form->ShouldBeQueried()) {
    submitted_form->LogQualityMetrics(
        submitted_form->form_parsed_timestamp(), interaction_time,
        submission_time, form_interactions_ukm_logger_.get(),
        did_show_suggestions_, observed_submission);
  }
  if (submitted_form->ShouldBeUploaded())
    UploadFormData(*submitted_form, observed_submission);
}

void AutofillManager::UploadFormData(const FormStructure& submitted_form,
                                     bool observed_submission) {
  if (!download_manager_)
    return;

  // Check if the form is among the forms that were recently auto-filled.
  bool was_autofilled = false;
  std::string form_signature = submitted_form.FormSignatureAsStr();
  for (const std::string& cur_sig : autofilled_form_signatures_) {
    if (cur_sig == form_signature) {
      was_autofilled = true;
      break;
    }
  }

  ServerFieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);
  // AS CVC is not stored, treat it separately.
  if (!last_unlocked_credit_card_cvc_.empty() ||
      non_empty_types.find(CREDIT_CARD_NUMBER) != non_empty_types.end()) {
    non_empty_types.insert(CREDIT_CARD_VERIFICATION_CODE);
  }

  download_manager_->StartUploadRequest(
      submitted_form, was_autofilled, non_empty_types,
      /*login_form_signature=*/std::string(), observed_submission,
      client_->GetPrefs());
}

void AutofillManager::Reset() {
  // Note that upload_request_ is not reset here because the prompt to
  // save a card is shown after page navigation.
  ProcessPendingFormForUpload();
  DCHECK(!pending_form_data_);
  AutofillHandler::Reset();
  form_interactions_ukm_logger_.reset(
      new AutofillMetrics::FormInteractionsUkmLogger(
          client_->GetUkmRecorder(), client_->GetUkmSourceId()));
  address_form_event_logger_.reset(new AddressFormEventLogger(
      driver()->IsInMainFrame(), form_interactions_ukm_logger_.get(), client_));
  credit_card_form_event_logger_.reset(new CreditCardFormEventLogger(
      driver()->IsInMainFrame(), form_interactions_ukm_logger_.get(),
      personal_data_, client_));
  credit_card_access_manager_.reset(new CreditCardAccessManager(
      driver(), client_, personal_data_, credit_card_form_event_logger_.get()));

  has_logged_autofill_enabled_ = false;
  has_logged_address_suggestions_count_ = false;
  did_show_suggestions_ = false;
  user_did_type_ = false;
  user_did_autofill_ = false;
  user_did_edit_autofilled_field_ = false;
  enable_ablation_logging_ = false;
  credit_card_ = CreditCard();
  credit_card_query_id_ = -1;
  credit_card_form_ = FormData();
  credit_card_field_ = FormFieldData();
  last_unlocked_credit_card_cvc_.clear();
  credit_card_action_ = AutofillDriver::FORM_DATA_ACTION_PREVIEW;
  initial_interaction_timestamp_ = TimeTicks();
  external_delegate_->Reset();
  filling_contexts_map_.clear();
}

AutofillManager::AutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    PersonalDataManager* personal_data,
    AutocompleteHistoryManager* autocomplete_history_manager,
    const std::string app_locale,
    AutofillDownloadManagerState enable_download_manager,
    std::unique_ptr<CreditCardAccessManager> cc_access_manager)
    : AutofillHandler(driver, client->GetLogManager()),
      client_(client),
      log_manager_(client_->GetLogManager()),
      app_locale_(app_locale),
      personal_data_(personal_data),
      field_filler_(app_locale, client->GetAddressNormalizer()),
      autocomplete_history_manager_(autocomplete_history_manager->GetWeakPtr()),
      form_interactions_ukm_logger_(
          std::make_unique<AutofillMetrics::FormInteractionsUkmLogger>(
              client->GetUkmRecorder(),
              client->GetUkmSourceId())),
      address_form_event_logger_(std::make_unique<AddressFormEventLogger>(
          driver->IsInMainFrame(),
          form_interactions_ukm_logger_.get(),
          client_)),
      credit_card_form_event_logger_(
          std::make_unique<CreditCardFormEventLogger>(
              driver->IsInMainFrame(),
              form_interactions_ukm_logger_.get(),
              personal_data_,
              client_)),
      is_rich_query_enabled_(IsRichQueryEnabled(client->GetChannel())) {
  DCHECK(driver);
  DCHECK(client_);
  credit_card_access_manager_ = cc_access_manager
                                    ? std::move(cc_access_manager)
                                    : std::make_unique<CreditCardAccessManager>(
                                          driver, client_, personal_data_,
                                          credit_card_form_event_logger_.get());
  if (enable_download_manager == ENABLE_AUTOFILL_DOWNLOAD_MANAGER) {
    version_info::Channel channel = client_->GetChannel();
    download_manager_.reset(new AutofillDownloadManager(
        driver, this, GetAPIKeyForUrl(channel), client_->GetLogManager()));
  }
  CountryNames::SetLocaleString(app_locale_);
  offer_manager_ = client_->GetAutofillOfferManager();
}

bool AutofillManager::RefreshDataModels() {
  if (!IsAutofillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
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
         !credit_card_access_manager_->GetCreditCards().empty();
}

CreditCard* AutofillManager::GetCreditCard(int unique_id) {
  // Unpack the |unique_id| into component parts.
  std::string credit_card_id;
  std::string profile_id;
  SplitFrontendID(unique_id, &credit_card_id, &profile_id);
  return credit_card_access_manager_->GetCreditCard(credit_card_id);
}

AutofillProfile* AutofillManager::GetProfile(int unique_id) {
  // Unpack the |unique_id| into component parts.
  std::string credit_card_id;
  std::string profile_id;
  SplitFrontendID(unique_id, &credit_card_id, &profile_id);

  if (base::IsValidGUID(profile_id))
    return personal_data_->GetProfileByGUID(profile_id);
  return nullptr;
}

void AutofillManager::FillOrPreviewDataModelForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const AutofillProfile* optional_profile,
    const CreditCard* optional_credit_card,
    const base::string16* optional_cvc,
    FormStructure* form_structure,
    AutofillField* autofill_field,
    bool is_refill) {
  DCHECK(optional_profile || optional_credit_card);
  DCHECK(optional_credit_card || !optional_cvc);
  DCHECK(form_structure);
  DCHECK(autofill_field);

  const AutofillDataModel& data_model = [&]() -> const AutofillDataModel& {
    if (optional_profile)
      return *optional_profile;
    else
      return *optional_credit_card;
  }();
  bool is_credit_card = !!optional_credit_card;

  LogBuffer buffer;
  buffer << "is credit card section: " << is_credit_card << Br{};
  buffer << "is refill: " << is_refill << Br{};
  buffer << *form_structure << Br{};
  buffer << Tag{"table"};

  form_structure->RationalizePhoneNumbersInSection(autofill_field->section);

  FormData result = form;
  DCHECK_EQ(form_structure->field_count(), form.fields.size());

  if (action == AutofillDriver::FORM_DATA_ACTION_FILL && !is_refill) {
    filling_contexts_map_[form_structure->GetIdentifierForRefill()] =
        std::make_unique<FillingContext>(*autofill_field, optional_profile,
                                         optional_credit_card, optional_cvc);
  }

  // Only record the types that are filled for an eventual refill if all the
  // following are satisfied:
  //  The refilling feature is enabled.
  //  A form with the given name is already filled.
  //  A refill has not been attempted for that form yet.
  //  This fill is not a refill attempt.
  FillingContext* filling_context = nullptr;
  auto itr =
      filling_contexts_map_.find(form_structure->GetIdentifierForRefill());
  if (itr != filling_contexts_map_.end())
    filling_context = itr->second.get();
  bool could_attempt_refill = filling_context != nullptr &&
                              !filling_context->attempted_refill && !is_refill;

  // Count the number of times the value of a specific type was filled into the
  // form.
  std::map<ServerFieldType, int> type_filling_count;

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    std::string field_number = base::StringPrintf("Field %zu", i);

    // On the renderer, the section is used regardless of the autofill status.
    result.fields[i].section = form_structure->field(i)->section;

    if (form_structure->field(i)->section != autofill_field->section) {
      buffer << Tr{} << field_number << "Skipped: not part of filled section";
      continue;
    }

    if (form_structure->field(i)->only_fill_when_focused() &&
        !form_structure->field(i)->SameFieldAs(field)) {
      buffer << Tr{} << field_number << "Skipped: only fill when focused";
      continue;
    }

    // The field order should be the same in |form_structure| and |result|.
    DCHECK(form_structure->field(i)->SameFieldAs(result.fields[i]));

    AutofillField* cached_field = form_structure->field(i);
    FieldTypeGroup field_group_type = cached_field->Type().group();

    // Don't fill hidden fields, with the exception of <select> fields, for
    // the sake of filling the synthetic fields.
    if (!cached_field->IsVisible()) {
      bool skip = result.fields[i].form_control_type != "select-one";
      form_interactions_ukm_logger_->LogHiddenRepresentationalFieldSkipDecision(
          *form_structure, *cached_field, skip);
      if (skip) {
        buffer << Tr{} << field_number << "Skipped: invisible field";
        continue;
      }
    }

    // Do not fill fields that have been edited by the user, except if the field
    // is empty and its initial value (= cached value) was empty as well. A
    // similar check is done in ForEachMatchingFormFieldCommon(), which
    // frequently has false negatives.
    if (base::FeatureList::IsEnabled(
            features::kAutofillSkipFillingFieldsWithChangedValues) &&
        ((form.fields[i].properties_mask & kUserTyped)) &&
        (!form.fields[i].value.empty() ||
         !form_structure->field(i)->value.empty()) &&
        !cached_field->SameFieldAs(field)) {
      buffer << Tr{} << field_number
             << "Skipped: don't fill user-filled fields";
      continue;
    }

    // Don't fill previously autofilled fields except the initiating field or
    // when it's a refill.
    if (result.fields[i].is_autofilled && !cached_field->SameFieldAs(field) &&
        !is_refill) {
      buffer << Tr{} << field_number
             << "Skipped: don't fill previously filled fields unless during a "
                "refill";
      continue;
    }

    if (field_group_type == NO_GROUP) {
      buffer << Tr{} << field_number
             << "Skipped: field type has no fillable group";
      continue;
    }

    // On a refill, only fill fields from type groups that were present during
    // the initial fill.
    if (is_refill &&
        !base::Contains(filling_context->type_groups_originally_filled,
                        field_group_type)) {
      buffer << Tr{} << field_number
             << "Skipped: in a refill, only fields from the group that was "
                "filled in the initial fill may be filled";
      continue;
    }

    ServerFieldType field_type = cached_field->Type().GetStorableType();

    // Don't fill expired cards expiration date.
    if (data_util::IsCreditCardExpirationType(field_type) &&
        (!optional_credit_card ||
         optional_credit_card->IsExpired(AutofillClock::Now()))) {
      buffer << Tr{} << field_number
             << "Skipped: don't fill expiration date of expired cards";
      continue;
    }

    // A field with a specific type is only allowed to be filled a limited
    // number of times given by |TypeValueFormFillingLimit(field_type)|.
    if (type_filling_count[field_type] >=
        TypeValueFormFillingLimit(field_type)) {
      buffer << Tr{} << field_number
             << "Skipped: field-type filling-limit reached";
      continue;
    }

    if (could_attempt_refill)
      filling_context->type_groups_originally_filled.insert(field_group_type);

    // Must match ForEachMatchingFormField() in form_autofill_util.cc.
    // Only notify autofilling of empty fields and the field that initiated
    // the filling (note that "select-one" controls may not be empty but will
    // still be autofilled).
    bool should_notify = !is_credit_card &&
                         (result.fields[i].SameFieldAs(field) ||
                          result.fields[i].form_control_type == "select-one" ||
                          result.fields[i].value.empty());

    bool has_value_before = !result.fields[i].value.empty();
    bool is_autofilled_before = result.fields[i].is_autofilled;

    const base::string16 kEmptyCvc{};
    std::string failure_to_fill;  // Reason for failing to fill.

    // Fill the non-empty value from |data_model| into the result vector, which
    // will be sent to the renderer.
    FillFieldWithValue(cached_field, data_model, &result.fields[i],
                       should_notify, optional_cvc ? *optional_cvc : kEmptyCvc,
                       data_util::DetermineGroups(*form_structure),
                       &failure_to_fill);

    bool has_value_after = !result.fields[i].value.empty();
    bool is_autofilled_after = result.fields[i].is_autofilled;

    // If the field was actually filled, increment the filling counter.
    if (is_autofilled_after)
      type_filling_count[field_type]++;

    buffer << Tr{} << field_number
           << base::StringPrintf(
                  "Fillable - has value: %d->%d; autofilled: %d->%d. %s",
                  has_value_before, is_autofilled_before, has_value_after,
                  is_autofilled_after, failure_to_fill.c_str());

    if (!cached_field->IsVisible() && result.fields[i].is_autofilled)
      AutofillMetrics::LogHiddenOrPresentationalSelectFieldsFilled();
  }
  buffer << CTag{"table"};

  autofilled_form_signatures_.push_front(form_structure->FormSignatureAsStr());
  // Only remember the last few forms that we've seen, both to avoid false
  // positives and to avoid wasting memory.
  if (autofilled_form_signatures_.size() > kMaxRecentFormSignaturesToRemember)
    autofilled_form_signatures_.pop_back();

  // Note that this may invalidate |data_model|.
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL && !is_refill)
    personal_data_->RecordUseOf(data_model);

  if (log_manager_) {
    log_manager_->Log() << LoggingScope::kFilling
                        << LogMessage::kSendFillingData << Br{}
                        << std::move(buffer);
  }
  driver()->SendFormDataToRenderer(query_id, action, result);
}

std::unique_ptr<FormStructure> AutofillManager::ValidateSubmittedForm(
    const FormData& form) {
  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  FormStructure* cached_submitted_form =
      FindCachedFormByRendererId(form.unique_renderer_id);
  if (!cached_submitted_form || !ShouldUploadForm(*cached_submitted_form)) {
    return nullptr;
  }

  auto submitted_form = std::make_unique<FormStructure>(form);
  submitted_form->RetrieveFromCache(*cached_submitted_form,
                                    /*should_keep_cached_value=*/false,
                                    /*only_server_and_autofill_state=*/false);
  if (value_from_dynamic_change_form_) {
    submitted_form->set_value_from_dynamic_change_form(true);
  }

  return submitted_form;
}

AutofillField* AutofillManager::GetAutofillField(const FormData& form,
                                                 const FormFieldData& field) {
  if (!personal_data_)
    return nullptr;

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return nullptr;

  if (!form_structure->IsAutofillable())
    return nullptr;

  return autofill_field;
}

bool AutofillManager::FormHasAddressField(const FormData& form) {
  for (const FormFieldData& field : form.fields) {
    const AutofillField* autofill_field = GetAutofillField(form, field);
    if (autofill_field && (autofill_field->Type().group() == ADDRESS_HOME ||
                           autofill_field->Type().group() == ADDRESS_BILLING)) {
      return true;
    }
  }

  return false;
}

std::vector<Suggestion> AutofillManager::GetProfileSuggestions(
    const FormStructure& form,
    const FormFieldData& field,
    const AutofillField& autofill_field) const {
  address_form_event_logger_->OnDidPollSuggestions(field, sync_state_);

  std::vector<ServerFieldType> field_types(form.field_count());
  for (size_t i = 0; i < form.field_count(); ++i) {
    field_types.push_back(form.field(i)->Type().GetStorableType());
  }

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      autofill_field.Type(), field.value, field.is_autofilled, field_types);

  // Adjust phone number to display in prefix/suffix case.
  if (autofill_field.Type().GetStorableType() == PHONE_HOME_NUMBER) {
    for (size_t i = 0; i < suggestions.size(); ++i) {
      suggestions[i].value = FieldFiller::GetPhoneNumberValue(
          autofill_field, suggestions[i].value, field);
    }
  }

  for (size_t i = 0; i < suggestions.size(); ++i) {
    suggestions[i].frontend_id =
        MakeFrontendID(std::string(), suggestions[i].backend_id);
  }
  return suggestions;
}

std::vector<Suggestion> AutofillManager::GetCreditCardSuggestions(
    const FormFieldData& field,
    const AutofillType& type,
    bool* should_display_gpay_logo) const {
  credit_card_form_event_logger_->OnDidPollSuggestions(field, sync_state_);

  // The field value is sanitized before attempting to match it to the user's
  // data.
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          type, SanitizeCreditCardFieldValue(field.value),
          client_->AreServerCardsSupported());
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableOffersInDownstream) &&
      offer_manager_) {
    offer_manager_->UpdateSuggestionsWithOffers(client_->GetLastCommittedURL(),
                                                suggestions);
  }
  *should_display_gpay_logo =
      credit_card_access_manager_->ShouldDisplayGPayLogo();

  for (size_t i = 0; i < suggestions.size(); i++) {
    suggestions[i].frontend_id =
        MakeFrontendID(suggestions[i].backend_id, std::string());
  }

  credit_card_form_event_logger_->set_suggestions(suggestions);
  return suggestions;
}

void AutofillManager::OnFormsParsed(const std::vector<const FormData*>& forms,
                                    const base::TimeTicks timestamp) {
  DCHECK(!forms.empty());
  has_parsed_forms_ = true;

  // Record the current sync state to be used for metrics on this page.
  sync_state_ = personal_data_->GetSyncSigninState();

  // Setup the url for metrics that we will collect for this form.
  form_interactions_ukm_logger_->OnFormsParsed(client_->GetUkmSourceId());

  driver()->HandleParsedForms(forms);

  std::vector<FormStructure*> non_queryable_forms;
  std::vector<FormStructure*> queryable_forms;
  std::set<FormType> form_types;
  for (const FormData* form : forms) {
    FormStructure* form_structure =
        FindCachedFormByRendererId(form->unique_renderer_id);
    if (!form_structure) {
      NOTREACHED();
      continue;
    }

    if (data_util::ContainsPhone(data_util::DetermineGroups(*form_structure))) {
      has_observed_phone_number_field_ = true;
    }

    // TODO(crbug.com/869482): avoid logging developer engagement multiple
    // times for a given form if it or other forms on the page are dynamic.
    LogDeveloperEngagementUkm(client_->GetUkmRecorder(),
                              client_->GetUkmSourceId(), form_structure);
    std::set<FormType> current_form_types = form_structure->GetFormTypes();
    form_types.insert(current_form_types.begin(), current_form_types.end());

    // Configure the query encoding for this form and add it to the appropriate
    // collection of forms: queryable vs non-queryable.
    form_structure->set_is_rich_query_enabled(is_rich_query_enabled_);
    if (form_structure->ShouldBeQueried())
      queryable_forms.push_back(form_structure);
    else
      non_queryable_forms.push_back(form_structure);

    // Log the type of form that was parsed.
    bool card_form = false;
    bool address_form = false;
    for (const auto& field : *form_structure) {
      if (field->Type().group() == CREDIT_CARD) {
        card_form = true;
      } else if (IsAddressForm(field->Type().group())) {
        address_form = true;
      } else if (field->Type().html_type() == HTML_TYPE_ONE_TIME_CODE) {
        has_observed_one_time_code_field_ = true;
      }
    }
    if (card_form) {
      credit_card_form_event_logger_->OnDidParseForm(*form_structure);
    }
    if (address_form) {
      address_form_event_logger_->OnDidParseForm(*form_structure);
    }

    // If a form with the same name was previously filled, and there has not
    // been a refill attempt on that form yet, start the process of triggering a
    // refill.
    if (ShouldTriggerRefill(*form_structure)) {
      auto itr =
          filling_contexts_map_.find(form_structure->GetIdentifierForRefill());
      DCHECK(itr != filling_contexts_map_.end());
      FillingContext* filling_context = itr->second.get();

      // If a timer for the refill was already running, it means the form
      // changed again. Stop the timer and start it again.
      if (filling_context->on_refill_timer.IsRunning())
        filling_context->on_refill_timer.AbandonAndStop();

      // Start a new timer to trigger refill.
      filling_context->on_refill_timer.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kWaitTimeForDynamicFormsMs),
          base::BindRepeating(&AutofillManager::TriggerRefill,
                              weak_ptr_factory_.GetWeakPtr(), *form));
    }
  }

  if (!queryable_forms.empty() || !non_queryable_forms.empty()) {
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::FORMS_LOADED, form_types,
        client_->GetSecurityLevelForUmaHistograms(),
        /*profile_form_bitmask=*/0);

#if defined(OS_IOS)
    // Log this from same location as AutofillMetrics::FORMS_LOADED to ensure
    // that KeyboardAccessoryButtonsIOS and UserHappiness UMA metrics will be
    // directly comparable.
    KeyboardAccessoryMetricsLogger::OnFormsLoaded();
#endif
  }

  // Send the current type predictions to the renderer. For non-queryable forms
  // this is all the information about them that will ever be available. The
  // queryable forms will be updated once the field type query is complete.
  driver()->SendAutofillTypePredictionsToRenderer(non_queryable_forms);
  driver()->SendAutofillTypePredictionsToRenderer(queryable_forms);
  LogAutofillTypePredictionsAvailable(log_manager_, non_queryable_forms);
  LogAutofillTypePredictionsAvailable(log_manager_, queryable_forms);

  // Query the server if at least one of the forms was parsed.
  if (!queryable_forms.empty() && download_manager_) {
    download_manager_->StartQueryRequest(queryable_forms);
  }
}

int AutofillManager::BackendIDToInt(const std::string& backend_id) const {
  if (!base::IsValidGUID(backend_id))
    return 0;

  const auto found = backend_to_int_map_.find(backend_id);
  if (found == backend_to_int_map_.end()) {
    // Unknown one, make a new entry.
    int int_id = backend_to_int_map_.size() + 1;
    backend_to_int_map_[backend_id] = int_id;
    int_to_backend_map_[int_id] = backend_id;
    return int_id;
  }
  return found->second;
}

std::string AutofillManager::IntToBackendID(int int_id) const {
  if (int_id == 0)
    return std::string();

  const auto found = int_to_backend_map_.find(int_id);
  if (found == int_to_backend_map_.end()) {
    NOTREACHED();
    return std::string();
  }
  return found->second;
}

// When sending IDs (across processes) to the renderer we pack credit card and
// profile IDs into a single integer.  Credit card IDs are sent in the high
// word and profile IDs are sent in the low word.
int AutofillManager::MakeFrontendID(
    const std::string& cc_backend_id,
    const std::string& profile_backend_id) const {
  int cc_int_id = BackendIDToInt(cc_backend_id);
  int profile_int_id = BackendIDToInt(profile_backend_id);

  // Should fit in signed 16-bit integers. We use 16-bits each when combining
  // below, and negative frontend IDs have special meaning so we can never use
  // the high bit.
  DCHECK(cc_int_id <= std::numeric_limits<int16_t>::max());
  DCHECK(profile_int_id <= std::numeric_limits<int16_t>::max());

  // Put CC in the high half of the bits.
  return (cc_int_id << std::numeric_limits<uint16_t>::digits) | profile_int_id;
}

// When receiving IDs (across processes) from the renderer we unpack credit card
// and profile IDs from a single integer.  Credit card IDs are stored in the
// high word and profile IDs are stored in the low word.
void AutofillManager::SplitFrontendID(int frontend_id,
                                      std::string* cc_backend_id,
                                      std::string* profile_backend_id) const {
  int cc_int_id = (frontend_id >> std::numeric_limits<uint16_t>::digits) &
                  std::numeric_limits<uint16_t>::max();
  int profile_int_id = frontend_id & std::numeric_limits<uint16_t>::max();

  *cc_backend_id = IntToBackendID(cc_int_id);
  *profile_backend_id = IntToBackendID(profile_int_id);
}

void AutofillManager::UpdateInitialInteractionTimestamp(
    const TimeTicks& interaction_timestamp) {
  if (initial_interaction_timestamp_.is_null() ||
      interaction_timestamp < initial_interaction_timestamp_) {
    initial_interaction_timestamp_ = interaction_timestamp;
  }
}

// static
void AutofillManager::DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const base::string16& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure* submitted_form) {
  // For each field in the |submitted_form|, extract the value.  Then for each
  // profile or credit card, identify any stored types that match the value.
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    AutofillField* field = submitted_form->field(i);
    if (!field->possible_types().empty() && field->IsEmpty()) {
      // This is a password field in a sign-in form. Skip checking its type
      // since |field->value| is not set.
      DCHECK_EQ(1u, field->possible_types().size());
      DCHECK_EQ(PASSWORD, *field->possible_types().begin());
      continue;
    }

    ServerFieldTypeSet matching_types;
    base::string16 value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    for (const AutofillProfile& profile : profiles) {
      ServerFieldTypeValidityStateMap matching_types_validities;
      profile.GetMatchingTypesAndValidities(value, app_locale, &matching_types,
                                            &matching_types_validities);
      field->add_possible_types_validities(matching_types_validities);
    }

    // TODO(crbug/880531) set possible_types_validities for credit card too.
    for (const CreditCard& card : credit_cards) {
      card.GetMatchingTypes(value, app_locale, &matching_types);
    }

    if (IsUPIVirtualPaymentAddress(value))
      matching_types.insert(UPI_VPA);

    if (matching_types.empty()) {
      matching_types.insert(UNKNOWN_TYPE);
      ServerFieldTypeValidityStateMap matching_types_validities;
      matching_types_validities[UNKNOWN_TYPE] = AutofillDataModel::UNVALIDATED;
      field->add_possible_types_validities(matching_types_validities);
    }

    field->set_possible_types(matching_types);
  }

  // As CVCs are not stored, run special heuristics to detect CVC-like values.
  AutofillField* cvc_field = GetBestPossibleCVCFieldForUpload(
      *submitted_form, last_unlocked_credit_card_cvc);
  if (cvc_field) {
    ServerFieldTypeSet possible_types = cvc_field->possible_types();
    possible_types.erase(UNKNOWN_TYPE);
    possible_types.insert(CREDIT_CARD_VERIFICATION_CODE);
    cvc_field->set_possible_types(possible_types);
  }

  AutofillManager::DisambiguateUploadTypes(submitted_form);
}

// static
void AutofillManager::DisambiguateUploadTypes(FormStructure* form) {
  for (size_t i = 0; i < form->field_count(); ++i) {
    AutofillField* field = form->field(i);
    const ServerFieldTypeSet& upload_types = field->possible_types();

    if (upload_types.size() == 2) {
      if (upload_types.count(ADDRESS_HOME_LINE1) &&
          upload_types.count(ADDRESS_HOME_STREET_ADDRESS)) {
        AutofillManager::DisambiguateAddressUploadTypes(form, i);
      } else if (upload_types.count(PHONE_HOME_CITY_AND_NUMBER) &&
                 upload_types.count(PHONE_HOME_WHOLE_NUMBER)) {
        AutofillManager::DisambiguatePhoneUploadTypes(form, i);
      }
    }

    // In case for credit cards and names there are many other possibilities
    // because a field can be of type NAME_FULL, NAME_LAST,
    // NAME_LAST_FIRST/SECOND at the same time.
    int credit_card_type_count = 0;
    int name_type_count = 0;

    bool undisambiuatable_types = false;
    for (const auto& type : upload_types) {
      switch (AutofillType(type).group()) {
        case CREDIT_CARD:
          ++credit_card_type_count;
          break;
        case NAME:
          ++name_type_count;
          break;
        // If there is any other type left, do not disambiguate.
        default:
          undisambiuatable_types = true;
      }
      if (undisambiuatable_types)
        break;
    }
    if (undisambiuatable_types)
      continue;

    if (credit_card_type_count == 1 && name_type_count >= 1)
      AutofillManager::DisambiguateNameUploadTypes(form, i, upload_types);
  }
}

// static
void AutofillManager::DisambiguateAddressUploadTypes(FormStructure* form,
                                                     size_t current_index) {
  // This happens when we have exactly two possible types, and the profile
  // has only one address line. Therefore the address line one and the street
  // address (the whole address) have the same value and match.

  // If the field is followed by a field that is predicted to be an
  // address line two and is empty, we can safely assume that this field
  // is an address line one field. Otherwise it's a whole address field.

  ServerFieldTypeSet matching_types;
  ServerFieldTypeValidityStatesMap matching_types_validities;
  AutofillField* field = form->field(current_index);

  size_t next_index = current_index + 1;
  if (next_index < form->field_count() &&
      form->field(next_index)->Type().GetStorableType() == ADDRESS_HOME_LINE2 &&
      form->field(next_index)->possible_types().count(EMPTY_TYPE)) {
    matching_types.insert(ADDRESS_HOME_LINE1);
    matching_types_validities[ADDRESS_HOME_LINE1] =
        field->get_validities_for_possible_type(ADDRESS_HOME_LINE1);
  } else {
    matching_types.insert(ADDRESS_HOME_STREET_ADDRESS);
    matching_types_validities[ADDRESS_HOME_STREET_ADDRESS] =
        field->get_validities_for_possible_type(ADDRESS_HOME_STREET_ADDRESS);
  }

  field->set_possible_types(matching_types);
  field->set_possible_types_validities(matching_types_validities);
}

// static
void AutofillManager::DisambiguatePhoneUploadTypes(FormStructure* form,
                                                   size_t current_index) {
  // This case happens  when we have exactly two possible types, and only for
  // profiles that have no country code saved. Therefore, both the whole number
  // and the city code and number have the same value and match.

  // Since the form was submitted, it is safe to assume that the form
  // didn't require a country code. Thus, only PHONE_HOME_CITY_AND_NUMBER
  // needs to be uploaded.

  ServerFieldTypeSet matching_types;
  ServerFieldTypeValidityStatesMap matching_types_validities;
  AutofillField* field = form->field(current_index);

  matching_types.insert(PHONE_HOME_CITY_AND_NUMBER);
  matching_types_validities[PHONE_HOME_CITY_AND_NUMBER] =
      field->get_validities_for_possible_type(PHONE_HOME_CITY_AND_NUMBER);

  field->set_possible_types(matching_types);
  field->set_possible_types_validities(matching_types_validities);
}

// static
void AutofillManager::DisambiguateNameUploadTypes(
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
      is_previous_credit_card = prev_field->Type().group() == CREDIT_CARD;
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
      is_next_credit_card = next_field->Type().group() == CREDIT_CARD;
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

void AutofillManager::FillFieldWithValue(AutofillField* autofill_field,
                                         const AutofillDataModel& data_model,
                                         FormFieldData* field_data,
                                         bool should_notify,
                                         const base::string16& cvc,
                                         uint32_t profile_form_bitmask,
                                         std::string* failure_to_fill) {
  if (field_filler_.FillFormField(*autofill_field, data_model, field_data, cvc,
                                  failure_to_fill)) {
    if (failure_to_fill)
      *failure_to_fill = "Decided to fill";
    // Mark the cached field as autofilled, so that we can detect when a
    // user edits an autofilled field (for metrics).
    autofill_field->is_autofilled = true;

    // Mark the field as autofilled when a non-empty value is assigned to
    // it. This allows the renderer to distinguish autofilled fields from
    // fields with non-empty values, such as select-one fields.
    field_data->is_autofilled = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::FIELD_WAS_AUTOFILLED, autofill_field->Type().group(),
        client_->GetSecurityLevelForUmaHistograms(), profile_form_bitmask);

    if (should_notify) {
      client_->DidFillOrPreviewField(
          /*value=*/data_model.GetInfo(autofill_field->Type(), app_locale_),
          /*profile_full_name=*/data_model.GetInfo(AutofillType(NAME_FULL),
                                                   app_locale_));
    }
  }
}

bool AutofillManager::ShouldTriggerRefill(const FormStructure& form_structure) {
  // Should not refill if a form with the same name has not been filled
  // before.
  auto itr =
      filling_contexts_map_.find(form_structure.GetIdentifierForRefill());
  if (itr == filling_contexts_map_.end())
    return false;

  address_form_event_logger_->OnDidSeeFillableDynamicForm(sync_state_,
                                                          form_structure);

  FillingContext* filling_context = itr->second.get();
  base::TimeTicks now = AutofillTickClock::NowTicks();
  base::TimeDelta delta = now - filling_context->original_fill_time;

  if (filling_context->attempted_refill &&
      delta.InMilliseconds() < kLimitBeforeRefillMs) {
    address_form_event_logger_->OnSubsequentRefillAttempt(sync_state_,
                                                          form_structure);
  }

  return !filling_context->attempted_refill &&
         delta.InMilliseconds() < kLimitBeforeRefillMs;
}

void AutofillManager::TriggerRefill(const FormData& form) {
  FormStructure* form_structure =
      FindCachedFormByRendererId(form.unique_renderer_id);
  if (!form_structure)
    return;

  DCHECK(form_structure);

  address_form_event_logger_->OnDidRefill(sync_state_, *form_structure);

  auto itr =
      filling_contexts_map_.find(form_structure->GetIdentifierForRefill());

  // Since GetIdentifierForRefill() is not stable across dynamic changes,
  // |filling_contexts_map_| may not contain the element anymore.
  if (itr == filling_contexts_map_.end())
    return;

  FillingContext* filling_context = itr->second.get();

  // The refill attempt can happen from different paths, some of which happen
  // after waiting for a while. Therefore, although this condition has been
  // checked prior to calling TriggerRefill, it may not hold, when we get
  // here.
  if (filling_context->attempted_refill)
    return;

  filling_context->attempted_refill = true;

  // Try to find the field from which the original field originated.
  AutofillField* autofill_field = nullptr;
  for (const std::unique_ptr<AutofillField>& field : *form_structure) {
    if (field->unique_name() == filling_context->filled_field_name) {
      autofill_field = field.get();
      break;
    }
  }

  // If it was not found cancel the refill.
  if (!autofill_field)
    return;

  FormFieldData field = *autofill_field;
  if (filling_context->credit_card) {
    FillOrPreviewDataModelForm(
        AutofillDriver::RendererFormDataAction::FORM_DATA_ACTION_FILL,
        /*query_id=*/-1, form, field,
        /*profile=*/nullptr, &filling_context->credit_card.value().first,
        &filling_context->credit_card.value().second, form_structure,
        autofill_field,
        /*is_refill=*/true);
  }
  if (filling_context->profile) {
    FillOrPreviewDataModelForm(
        AutofillDriver::RendererFormDataAction::FORM_DATA_ACTION_FILL,
        /*query_id=*/-1, form, field, &filling_context->profile.value(),
        /*credic_card=*/nullptr, /*cvc=*/nullptr, form_structure,
        autofill_field,
        /*is_refill=*/true);
  }
}

void AutofillManager::GetAvailableSuggestions(
    const FormData& form,
    const FormFieldData& field,
    std::vector<Suggestion>* suggestions,
    SuggestionsContext* context) {
  DCHECK(suggestions);
  DCHECK(context);

  // Need to refresh models before using the form_event_loggers.
  bool is_autofill_possible = RefreshDataModels();

  bool got_autofillable_form =
      GetCachedFormAndField(form, field, &context->form_structure,
                            &context->focused_field) &&
      // Don't send suggestions or track forms that should not be parsed.
      context->form_structure->ShouldBeParsed();

  // Log interactions of forms that are autofillable.
  if (got_autofillable_form) {
    if (context->focused_field->Type().group() == CREDIT_CARD) {
      context->is_filling_credit_card = true;
    }
    auto* logger = GetEventFormLogger(context->focused_field->Type().group());
    if (logger) {
      logger->OnDidInteractWithAutofillableForm(*(context->form_structure),
                                                sync_state_);
    }
  }

  // If the feature is enabled and this is a mixed content form, we show a
  // warning message and don't offer autofill. The warning is shown even if
  // there are no autofill suggestions available.
  if (IsFormMixedContent(client_, form) &&
      base::FeatureList::IsEnabled(
          features::kAutofillPreventMixedFormsFilling) &&
      client_->GetPrefs()->GetBoolean(::prefs::kMixedFormsWarningsEnabled)) {
    suggestions->clear();
    // If the user begins typing, we interpret that as dismissing the warning.
    // No suggestions are allowed, but the warning is no longer shown.
    if (field.DidUserType()) {
      context->suppress_reason = SuppressReason::kInsecureForm;
    } else {
      Suggestion warning_suggestion(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_MIXED_FORM));
      warning_suggestion.frontend_id = POPUP_ITEM_ID_MIXED_FORM_MESSAGE;
      suggestions->emplace_back(warning_suggestion);
    }
    return;
  }

  context->is_context_secure = !IsFormNonSecure(form);

  // TODO(rogerm): Early exit here on !driver()->RendererIsAvailable()?
  // We skip populating autofill data, but might generate warnings and or
  // signin promo to show over the unavailable renderer. That seems a mistake.

  if (!is_autofill_possible || !driver()->RendererIsAvailable() ||
      !got_autofillable_form)
    return;

  context->is_autofill_available = true;

  if (context->is_filling_credit_card) {
    *suggestions =
        GetCreditCardSuggestions(field, context->focused_field->Type(),
                                 &context->should_display_gpay_logo);

    // Logic for disabling/ablating credit card autofill.
    if (base::FeatureList::IsEnabled(
            features::kAutofillCreditCardAblationExperiment) &&
        !suggestions->empty()) {
      context->suppress_reason = SuppressReason::kCreditCardsAblation;
      suggestions->clear();
      return;
    }
  } else {
    // On desktop, don't return non credit card related suggestions for forms
    // or fields that have the "autocomplete" attribute set to off, only if
    // the feature to always fill addresses is off.
    if (!base::FeatureList::IsEnabled(features::kAutofillAlwaysFillAddresses) &&
        IsDesktopPlatform() && !field.should_autocomplete) {
      context->suppress_reason = SuppressReason::kAutocompleteOff;
      return;
    }

    *suggestions = GetProfileSuggestions(*context->form_structure, field,
                                         *context->focused_field);
  }

  // Returns early if no suggestion is available or suggestions are not for
  // cards.
  if (suggestions->empty() || !context->is_filling_credit_card)
    return;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // This section adds the "Use a virtual card number" option in the autofill
  // dropdown menu, if applicable.
  if (ShouldShowVirtualCardOption(context->form_structure)) {
    suggestions->emplace_back(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CLOUD_TOKEN_DROPDOWN_OPTION_LABEL));
    suggestions->back().frontend_id = POPUP_ITEM_ID_USE_VIRTUAL_CARD;
  }
#endif

  // Don't provide credit card suggestions for non-secure pages, but do
  // provide them for secure pages with passive mixed content (see
  // implementation of IsContextSecure).
  if (!context->is_context_secure) {
    // Replace the suggestion content with a warning message explaining why
    // Autofill is disabled for a website. The string is different if the
    // credit card autofill HTTP warning experiment is enabled.
    Suggestion warning_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
    warning_suggestion.frontend_id =
        POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
    suggestions->assign(1, warning_suggestion);
  }
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// TODO(crbug.com/1020740): Add metrics logging.
bool AutofillManager::ShouldShowVirtualCardOption(
    FormStructure* form_structure) {
  // If experiment is disabled, return false.
  if (!base::FeatureList::IsEnabled(features::kAutofillEnableVirtualCard))
    return false;

  // If credit card upload is disabled, return false.
  if (!IsAutofillCreditCardEnabled())
    return false;

  // If merchant is not allowed, return false.
  std::vector<std::string> allowed_merchants =
      client_->GetAllowedMerchantsForVirtualCards();
  if (std::find(allowed_merchants.begin(), allowed_merchants.end(),
                form_structure->source_url().spec()) ==
      allowed_merchants.end()) {
    return false;
  }

  // If no credit card candidate has related cloud token data available,
  // return false.
  if (GetVirtualCardCandidates(personal_data_).empty())
    return false;

  // If card number field or expiration date field is not detected, return
  // false.
  if (!form_structure->IsCompleteCreditCardForm())
    return false;

  // If CVC field is detected, then all requirements are met, otherwise return
  // false.
  for (auto& field : *form_structure) {
    if (field->Type().GetStorableType() == CREDIT_CARD_VERIFICATION_CODE)
      return true;
  }
  return false;
}
#endif

FormEventLoggerBase* AutofillManager::GetEventFormLogger(
    FieldTypeGroup field_type_group) const {
  switch (field_type_group) {
    case NAME:
    case NAME_BILLING:
    case EMAIL:
    case COMPANY:
    case ADDRESS_HOME:
    case ADDRESS_BILLING:
    case PHONE_HOME:
    case PHONE_BILLING:
      return address_form_event_logger_.get();
    case CREDIT_CARD:
      return credit_card_form_event_logger_.get();
    case TRANSACTION:
    case PASSWORD_FIELD:
    case USERNAME_FIELD:
    case NO_GROUP:
    case UNFILLABLE:
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

std::string AutofillManager::GetPageLanguage() const {
  DCHECK(client_);
  const translate::LanguageState* language_state = client_->GetLanguageState();
  if (language_state)
    return language_state->original_language();
  return std::string();
}

}  // namespace autofill
