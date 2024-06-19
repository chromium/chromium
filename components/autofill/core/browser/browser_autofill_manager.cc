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
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/address_suggestion_generator.h"
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
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_filling_payments_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_autofill_history.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_suggestion_generator.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
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
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

using FillingProductSet = DenseSet<FillingProduct>;

using base::TimeTicks;
using mojom::SubmissionSource;

namespace {

// The minimum required number of fields for an user perception survey to be
// triggered. This makes sure that for example forms that only contain a single
// email field do not prompt a survey. Such survey answer would likely taint
// our analysis.
constexpr size_t kMinNumberAddressFieldsToTriggerAddressUserPerceptionSurvey =
    4;

// Checks if the user triggered address Autofill through the
// Chrome context menu on a field not classified as address.
// `type` defines the suggestion type shown.
// `autofill_field` is the `AutofillField` from where the user triggered
// suggestions.
bool IsAddressAutofillManuallyTriggeredOnNonAddressField(
    SuggestionType type,
    const AutofillField* autofill_field) {
  return GetFillingProductFromSuggestionType(type) ==
             FillingProduct::kAddress &&
         (!autofill_field ||
          !IsAddressType(autofill_field->Type().GetStorableType()));
}

// Checks if the user triggered payments Autofill through the
// Chrome context menu on a field not classified as credit card.
// `type` defines the suggestion type shown.
// `autofill_field` is the `AutofillField` from where the user triggered
// suggestions.
bool IsCreditCardAutofillManuallyTriggeredOnNonCreditCardField(
    SuggestionType type,
    const AutofillField* autofill_field) {
  if (GetFillingProductFromSuggestionType(type) !=
      FillingProduct::kCreditCard) {
    return false;
  }

  return !autofill_field ||
         !FieldTypeGroupSet::is_one_of(autofill_field->Type().group(),
                                       {FieldTypeGroup::kCreditCard,
                                        FieldTypeGroup::kStandaloneCvcField});
}

// Converts `filling_stats` to a key-value representation, where the key
// is the "stats category" and the value is the number of fields that match
// such category. This is used to show users a survey that will measure the
// perception of Autofill.
std::map<std::string, std::string> FormFillingStatsToSurveyStringData(
    autofill_metrics::FormGroupFillingStats& filling_stats) {
  return {
      {"Accepted fields", base::NumberToString(filling_stats.num_accepted)},
      {"Corrected to same type",
       base::NumberToString(filling_stats.num_corrected_to_same_type)},
      {"Corrected to a different type",
       base::NumberToString(filling_stats.num_corrected_to_different_type)},
      {"Corrected to an unknown type",
       base::NumberToString(filling_stats.num_corrected_to_unknown_type)},
      {"Corrected to empty",
       base::NumberToString(filling_stats.num_corrected_to_empty)},
      {"Manually filled to same type",
       base::NumberToString(filling_stats.num_manually_filled_to_same_type)},
      {"Manually filled to a different type",
       base::NumberToString(
           filling_stats.num_manually_filled_to_different_type)},
      {"Manually filled to an unknown type",
       base::NumberToString(filling_stats.num_manually_filled_to_unknown_type)},
      {"Total corrected", base::NumberToString(filling_stats.TotalCorrected())},
      {"Total filled", base::NumberToString(filling_stats.TotalFilled())},
      {"Total unfilled", base::NumberToString(filling_stats.TotalUnfilled())},
      {"Total manually filled",
       base::NumberToString(filling_stats.TotalManuallyFilled())},
      {"Total number of fields", base::NumberToString(filling_stats.Total())}};
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
  if (IsUPIVirtualPaymentAddress(value)) {
    return ValuePatternsMetric::kUpiVpa;
  }
  if (IsInternationalBankAccountNumber(value)) {
    return ValuePatternsMetric::kIban;
  }
  return ValuePatternsMetric::kNoPatternFound;
}

void LogValuePatternsMetric(const FormData& form) {
  for (const FormFieldData& field : form.fields) {
    if (!field.IsFocusable()) {
      continue;
    }
    std::u16string value;
    base::TrimWhitespace(field.value(), base::TRIM_ALL, &value);
    if (value.empty()) {
      continue;
    }
    base::UmaHistogramEnumeration("Autofill.SubmittedValuePatterns",
                                  GetValuePattern(value));
  }
}

bool IsSingleFieldFormFillerFillingProduct(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAutocomplete:
    case FillingProduct::kIban:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kStandaloneCvc:
      return true;
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kCompose:
    case FillingProduct::kPassword:
    case FillingProduct::kCreditCard:
    case FillingProduct::kAddress:
    case FillingProduct::kNone:
      return false;
  }
}

// Is `suggestions` contains Autocomplete suggestions, then this function logs
// a metric to record whether Autocomplete would have been suppressed due to
// a plus address suggestion.
// It only logs these metrics for users that are signed in and tabs that are not
// in incognito mode.
// TODO(b/327328460): Clean up once the metric is has been evaluated.
void MaybeLogAutocompleteSuppressionByPlusAddresses(
    AutofillClient& client,
    base::span<const Suggestion> suggestions,
    FieldTypeGroup focused_field_type_group) {
  if (client.IsOffTheRecord()) {
    return;
  }

  // Do not log metrics for users that are not signed in.
  if (signin::IdentityManager* identity_manager = client.GetIdentityManager();
      !identity_manager ||
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .IsEmpty()) {
    return;
  }

  if (suggestions.empty() ||
      GetFillingProductFromSuggestionType(suggestions[0].type) !=
          FillingProduct::kAutocomplete) {
    return;
  }

  // If the focused field is not classified as an email address, plus addresses
  // would never be shown.
  using enum AutocompleteSuppressionByPlusAddress;
  if (focused_field_type_group != FieldTypeGroup::kEmail) {
    base::UmaHistogramEnumeration(kAutocompleteSuppressionByPlusAddressUma,
                                  kNotSuppressed);
    return;
  }
  const bool has_email =
      base::ranges::any_of(suggestions, [](const Suggestion& suggestion) {
        return IsValidEmailAddress(suggestion.main_text.value);
      });
  base::UmaHistogramEnumeration(
      kAutocompleteSuppressionByPlusAddressUma,
      has_email ? kSuppressedWithEmailResults : kSuppressedWithoutEmailResults);
}

FillDataType GetEventTypeFromSingleFieldSuggestionType(SuggestionType type) {
  switch (type) {
    case SuggestionType::kAutocompleteEntry:
      return FillDataType::kSingleFieldFormFillerAutocomplete;
    case SuggestionType::kMerchantPromoCodeEntry:
      return FillDataType::kSingleFieldFormFillerPromoCode;
    case SuggestionType::kIbanEntry:
      return FillDataType::kSingleFieldFormFillerIban;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kClearForm:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressEntry:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return FillDataType::kUndefined;
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
  if (field.autocomplete_attribute() != "on" &&
      ShouldIgnoreAutocompleteAttribute(field.autocomplete_attribute())) {
    autocomplete_state = AutofillMetrics::AutocompleteState::kOff;
  } else if (field.parsed_autocomplete()) {
    autocomplete_state =
        field.parsed_autocomplete()->field_type != HtmlFieldType::kUnrecognized
            ? AutofillMetrics::AutocompleteState::kValid
            : AutofillMetrics::AutocompleteState::kGarbage;

    if (field.autocomplete_attribute() == "new-password" ||
        field.autocomplete_attribute() == "current-password") {
      autocomplete_state = AutofillMetrics::AutocompleteState::kPassword;
    }
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
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return "PROBABLY_FORM_SUBMITTED";
    case SubmissionSource::FORM_SUBMISSION:
      return "FORM_SUBMISSION";
    case SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      return "DOM_MUTATION_AFTER_AUTOFILL";
  }
  return "Unknown";
}

// Returns true if autocomplete=unrecognized (address) fields should receive
// suggestions. On desktop, suggestion can only be triggered for them through
// manual fallbacks. On mobile, suggestions are always shown.
bool ShouldShowSuggestionsForAutocompleteUnrecognizedFields(
    AutofillSuggestionTriggerSource trigger_source) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return IsAutofillManuallyTriggered(trigger_source);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

// Checks if the `credit_card` needs to be fetched in order to complete the
// current filling flow.
// TODO(crbug.com/40227496): Only use parsed data.
bool ShouldFetchCreditCard(const FormData& form,
                           const FormFieldData& field,
                           const FormStructure& form_structure,
                           const AutofillField& autofill_field,
                           const CreditCard& credit_card) {
  if (WillFillCreditCardNumber(form.fields, form_structure.fields(),
                               autofill_field)) {
    return true;
  }
  // This happens for web sites which cache all credit card details except for
  // the cvc, which is different every time the virtual credit card is being
  // used.
  return credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
         autofill_field.Type().GetStorableType() ==
             CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
}

// To reduce traffic, only a random sample of browser sessions upload UKM data.
// This function returns whether we should record autofill UKM events for the
// current session.
bool ShouldRecordUkm() {
  // We only need to generate this random number once while the current process
  // is running.
  static const int random_value_per_session = base::RandInt(0, 99);

  const int kSamplingRate =
      base::FeatureList::IsEnabled(
          features::kAutofillLogUKMEventsWithSamplingOnSession)
          ? features::kAutofillLogUKMEventsWithSamplingOnSessionRate.Get()
          : 0;

  return random_value_per_session < kSamplingRate;
}

// Returns true if the source is only relevant for Compose.
bool IsTriggerSourceOnlyRelevantForCompose(
    AutofillSuggestionTriggerSource source) {
  switch (source) {
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
      return true;
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kManualFallbackAddress:
    case AutofillSuggestionTriggerSource::kManualFallbackPayments:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
    case AutofillSuggestionTriggerSource::
        kShowPromptAfterDialogClosedNonManualFallback:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
      return false;
  }
}

void LogSuggestionsCount(const SuggestionsContext& context,
                         const std::vector<Suggestion>& suggestions) {
  if (suggestions.empty() || !context.is_autofill_available) {
    return;
  }

  if (context.filling_product == FillingProduct::kCreditCard) {
    AutofillMetrics::LogIsQueriedCreditCardFormSecure(
        context.is_context_secure);
    // TODO(b/41484171): Move to PaymentsSuggestionGenerator.
    autofill_metrics::LogSuggestionsCount(
        base::ranges::count_if(suggestions,
                               [](const Suggestion& suggestion) {
                                 return GetFillingProductFromSuggestionType(
                                            suggestion.type) ==
                                        FillingProduct::kCreditCard;
                               }),
        FillingProduct::kCreditCard);
  }
  if (context.filling_product == FillingProduct::kAddress) {
    // TODO(b/41484171): Move to AddressSuggestionGenerator.
    autofill_metrics::LogSuggestionsCount(
        base::ranges::count_if(suggestions,
                               [](const Suggestion& suggestion) {
                                 return GetFillingProductFromSuggestionType(
                                            suggestion.type) ==
                                        FillingProduct::kAddress;
                               }),
        FillingProduct::kAddress);
  }
}

FieldTypeSet GetTargetFieldsForAddressFillingSuggestionType(
    SuggestionType suggestion_type,
    FieldType trigger_field_type) {
  switch (suggestion_type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillEverythingFromAddressProfile:
      return kAllFieldTypes;
    case SuggestionType::kFillFullAddress:
      return GetAddressFieldsForGroupFilling();
    case SuggestionType::kFillFullName:
      return GetFieldTypesOfGroup(FieldTypeGroup::kName);
    case SuggestionType::kFillFullPhoneNumber:
      return GetFieldTypesOfGroup(FieldTypeGroup::kPhone);
    case SuggestionType::kFillFullEmail:
      return GetFieldTypesOfGroup(FieldTypeGroup::kEmail);
    case SuggestionType::kAddressFieldByFieldFilling:
      return FieldTypeSet{trigger_field_type};
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kClearForm:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressEntry:
      NOTREACHED_NORETURN();
  }
  NOTREACHED_NORETURN();
}

}  // namespace

BrowserAutofillManager::BrowserAutofillManager(AutofillDriver* driver,
                                               const std::string& app_locale)
    : AutofillManager(driver),
      external_delegate_(std::make_unique<AutofillExternalDelegate>(this)),
      app_locale_(app_locale),
      address_suggestion_generator_(
          std::make_unique<AddressSuggestionGenerator>(client())),
      payments_suggestion_generator_(
          std::make_unique<PaymentsSuggestionGenerator>(client())),
      form_filler_(
          std::make_unique<FormFiller>(*this, log_manager(), app_locale)) {
  address_form_event_logger_ =
      std::make_unique<autofill_metrics::AddressFormEventLogger>(
          driver->IsInAnyMainFrame(), form_interactions_ukm_logger(),
          &client());
  credit_card_form_event_logger_ =
      std::make_unique<autofill_metrics::CreditCardFormEventLogger>(
          driver->IsInAnyMainFrame(), form_interactions_ukm_logger(),
          client().GetPersonalDataManager(), &client());
  autocomplete_unrecognized_fallback_logger_ = std::make_unique<
      autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger>();
  manual_fallback_logger_ =
      std::make_unique<autofill_metrics::ManualFallbackEventLogger>();
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

  single_field_form_fill_router_->CancelPendingQueries();

  address_form_event_logger_->OnDestroyed();
  credit_card_form_event_logger_->OnDestroyed();

  // We don't flush the `queued_vote_uploads_` here because that would trigger
  // network requests in the AutofillCrowdsourcingManager, which are managed
  // with by SimpleURLLoaders owned by the AutofillCrowdsourcingManager.
  // Destroying the BrowserAutofillManager destroys the
  // AutofillCrowdsourcingManager and its SimpleURLLoaders, which would
  // immediately cancel the uploads. As a consequence of this, votes are lost if
  // the user generates blur votes and closes the tab before the votes are sent
  // (due to a navigation).
}

base::WeakPtr<AutofillManager> BrowserAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CreditCardAccessManager& BrowserAutofillManager::GetCreditCardAccessManager() {
  if (!credit_card_access_manager_) {
    credit_card_access_manager_ = std::make_unique<CreditCardAccessManager>(
        this, credit_card_form_event_logger_.get());
  }
  return *credit_card_access_manager_;
}

const CreditCardAccessManager&
BrowserAutofillManager::GetCreditCardAccessManager() const {
  return const_cast<BrowserAutofillManager*>(this)
      ->GetCreditCardAccessManager();
}

bool BrowserAutofillManager::ShouldShowScanCreditCard(
    const FormData& form,
    const FormFieldData& field) const {
  if (!client().HasCreditCardScanFeature() ||
      !IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field) {
    return false;
  }

  bool is_card_number_field =
      autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER &&
      base::ContainsOnlyChars(CreditCard::StripSeparators(field.value()),
                              u"0123456789");

  if (!is_card_number_field) {
    return false;
  }

  if (IsFormNonSecure(form)) {
    return false;
  }

  static const int kShowScanCreditCardMaxValueLength = 6;
  return field.value().size() <= kShowScanCreditCardMaxValueLength;
}

bool BrowserAutofillManager::ShouldShowCardsFromAccountOption(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source) const {
  // If `trigger_source` is equal to `kShowCardsFromAccount`, that means that
  // the user accepted "Show cards from account" suggestions and it should not
  // be shown again.
  if (trigger_source ==
      AutofillSuggestionTriggerSource::kShowCardsFromAccount) {
    return false;
  }
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

  if (IsFormNonSecure(form)) {
    return false;
  }

  return client()
      .GetPersonalDataManager()
      ->payments_data_manager()
      .ShouldShowCardsFromAccountOption();
}

void BrowserAutofillManager::OnUserAcceptedCardsFromAccountOption() {
  client()
      .GetPersonalDataManager()
      ->payments_data_manager()
      .OnUserAcceptedCardsFromAccountOption();
}

void BrowserAutofillManager::RefetchCardsAndUpdatePopup(
    const FormData& form,
    const FormFieldData& field_data) {
  external_delegate_->OnQuery(
      form, field_data, /*caret_bounds=*/gfx::Rect(),
      AutofillSuggestionTriggerSource::kShowCardsFromAccount);
  AutofillField* autofill_field = GetAutofillField(form, field_data);
  FieldType field_type = autofill_field
                             ? autofill_field->Type().GetStorableType()
                             : CREDIT_CARD_NUMBER;
  DCHECK_EQ(FieldTypeGroup::kCreditCard, GroupTypeOfFieldType(field_type));

  auto cards = GetCreditCardSuggestions(
      form, field_data, field_type,
      AutofillSuggestionTriggerSource::kShowCardsFromAccount);
  DCHECK(!cards.empty());
  external_delegate_->OnSuggestionsReturned(field_data.global_id(), cards);
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
                ->payments_data_manager()
                .GetPaymentsSigninStateForMetrics()
          : AutofillMetrics::PaymentsSigninState::kUnknown;
  if (!has_logged_autofill_enabled_) {
    autofill_metrics::LogIsAutofillEnabledAtPageLoad(autofill_enabled,
                                                     signin_state_for_metrics_);
    autofill_metrics::LogIsAutofillProfileEnabledAtPageLoad(
        IsAutofillProfileEnabled(), signin_state_for_metrics_);
    autofill_metrics::LogIsAutofillCreditCardEnabledAtPageLoad(
        IsAutofillPaymentMethodsEnabled(), signin_state_for_metrics_);
    if (!IsAutofillProfileEnabled()) {
      autofill_metrics::LogAutofillProfileDisabledReasonAtPageLoad(
          CHECK_DEREF(client().GetPrefs()));
    }
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
    // TODO(crbug.com/40276862): Verify frequency of plus address (or the other
    // type(s) checked for below, for that matter) slipping through in this code
    // path.
    single_field_form_fill_router_->OnWillSubmitForm(
        form, submitted_form.get(), client().IsAutocompleteEnabled());
    return;
  }

  form_submitted_timestamp_ = base::TimeTicks::Now();

  // Log metrics about the autocomplete attribute usage in the submitted form.
  LogAutocompletePredictionCollisionTypeMetrics(*submitted_form);

  // Log interaction time metrics for the ablation study.
  if (!initial_interaction_timestamp_.is_null()) {
    base::TimeDelta time_from_interaction_to_submission =
        base::TimeTicks::Now() - initial_interaction_timestamp_;
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

  AutofillPlusAddressDelegate* plus_address_delegate =
      client().GetPlusAddressDelegate();

  std::vector<FormFieldData> fields_for_autocomplete;
  fields_for_autocomplete.reserve(submitted_form->fields().size());
  for (const auto& autofill_field : submitted_form->fields()) {
    fields_for_autocomplete.push_back(*autofill_field);
    if (autofill_field->Type().GetStorableType() ==
        CREDIT_CARD_VERIFICATION_CODE) {
      // However, if Autofill has recognized a field as CVC, that shouldn't be
      // saved.
      fields_for_autocomplete.back().set_should_autocomplete(false);
    }
    if (plus_address_delegate &&
        plus_address_delegate->IsPlusAddress(
            base::UTF16ToUTF8(autofill_field->value()))) {
      // Similarly to CVC, any plus addresses needn't be saved to autocomplete.
      // Note that the feature is experimental, and `plus_address_delegate`
      // will be null if the feature is not enabled (it's disabled by default).
      fields_for_autocomplete.back().set_should_autocomplete(false);
    }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    if (autofill_field->autocomplete_attribute() == "off" &&
        autofill_field->did_trigger_suggestions() &&
        !autofill_field->is_autofilled() &&
        !autofill_field->previously_autofilled() &&
        base::FeatureList::IsEnabled(
            features::kAutofillSuggestionNStrikeModel)) {
      // This means that the user triggered suggestions and ignored them. In
      // that case we record a strike for this specific field. Multiple strikes
      // will lead to automatic address suggestions to be suppressed.
      // Currently, this is only done for autocomplete=off fields.
      client()
          .GetPersonalDataManager()
          ->address_data_manager()
          .AddStrikeToBlockAddressSuggestions(
              submitted_form->form_signature(),
              autofill_field->GetFieldSignature(),
              submitted_form->source_url());
    }
#endif
  }

  // TODO crbug.com/40100455 - Eliminate `form_for_autocomplete`.
  FormData form_for_autocomplete = submitted_form->ToFormData();
  form_for_autocomplete.fields = std::move(fields_for_autocomplete);
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
      if (std::optional<FormStructure::FormAssociations> associations =
              form_data_importer->GetFormAssociations(
                  submitted_form->form_signature())) {
        submitted_form->set_form_associations(*associations);
      }
    }
  }

  MaybeStartVoteUploadProcess(std::move(submitted_form),
                              /*observed_submission=*/true);

  // TODO(crbug.com/41365645): Add FormStructure::Clone() method.
  // Create another FormStructure instance.
  submitted_form = ValidateSubmittedForm(form);
  DCHECK(submitted_form);
  if (!submitted_form) {
    return;
  }

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
  std::vector<const AutofillProfile*> profiles =
      client().GetPersonalDataManager()->address_data_manager().GetProfiles();
  if (observed_submission && form_structure->IsAutofillable()) {
    AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
        client()
            .GetPersonalDataManager()
            ->address_data_manager()
            .GetProfiles()
            .size());
  }

  const std::vector<CreditCard*>& credit_cards = client()
                                                     .GetPersonalDataManager()
                                                     ->payments_data_manager()
                                                     .GetCreditCards();

  if (profiles.empty() && credit_cards.empty()) {
    return false;
  }

  if (form_structure->field_count() * (profiles.size() + credit_cards.size()) >=
      kMaxTypeMatchingCalls) {
    return false;
  }

  // Copy the profile and credit card data, so that it can be accessed on a
  // separate thread.
  std::vector<AutofillProfile> copied_profiles;
  copied_profiles.reserve(profiles.size());
  for (const AutofillProfile* profile : profiles) {
    copied_profiles.push_back(*profile);
  }

  std::vector<CreditCard> copied_credit_cards;
  copied_credit_cards.reserve(credit_cards.size());
  for (const CreditCard* card : credit_cards) {
    copied_credit_cards.push_back(*card);
  }

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
  // BrowserAutofillManager::OnSubmissionFieldTypesDetermined() call.
  FormStructure* raw_form = form_structure.get();

  base::OnceClosure call_after_determine_field_types =
      base::BindOnce(&BrowserAutofillManager::OnSubmissionFieldTypesDetermined,
                     weak_ptr_factory_.GetWeakPtr(), std::move(form_structure),
                     initial_interaction_timestamp_, base::TimeTicks::Now(),
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
      base::BindOnce(&DeterminePossibleFieldTypesForUpload,
                     std::move(copied_profiles), std::move(copied_credit_cards),
                     last_unlocked_credit_card_cvc_, app_locale_, raw_form),
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
  if (!pending_form_data_) {
    return;
  }

  // We get the FormStructure corresponding to |pending_form_data_|, used in the
  // upload process. |pending_form_data_| is reset.
  std::unique_ptr<FormStructure> upload_form =
      ValidateSubmittedForm(*pending_form_data_);
  pending_form_data_.reset();
  if (!upload_form) {
    return;
  }

  MaybeStartVoteUploadProcess(std::move(upload_form),
                              /*observed_submission=*/false);
}

void BrowserAutofillManager::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const TimeTicks timestamp) {
  const FormFieldData* field = form.FindFieldByGlobalId(field_id);
  if (!field) {
    return;
  }
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, *field, &form_structure, &autofill_field)) {
    return;
  }

  // Log events when user edits the field.
  // If the user types into the same field multiple times, repeated
  // TypingFieldLogEvents are coalesced.
  autofill_field->AppendLogEventIfNotRepeated(TypingFieldLogEvent{
      .has_value_after_typing = ToOptionalBoolean(!field->value().empty())});

  UpdatePendingForm(form);

  if (!user_did_type_ || autofill_field->is_autofilled()) {
    user_did_type_ = true;
    form_interactions_ukm_logger()->LogTextFieldDidChange(*form_structure,
                                                          *autofill_field);
  }

  auto* logger = GetEventFormLogger(*autofill_field);
  if (!autofill_field->is_autofilled()) {
    if (logger) {
      logger->OnTypedIntoNonFilledField();
    }
  }

  if (autofill_field->is_autofilled()) {
    autofill_field->set_is_autofilled(false);
    autofill_field->set_previously_autofilled(true);
    if (logger) {
      logger->OnEditedAutofilledField();
    }
  }

  UpdateInitialInteractionTimestamp(timestamp);

  if (logger) {
    logger->OnTextFieldDidChange(autofill_field->global_id());
  }
}

bool BrowserAutofillManager::IsFormNonSecure(const FormData& form) const {
  // Check if testing override applies.
  if (consider_form_as_secure_for_testing_.has_value() &&
      consider_form_as_secure_for_testing_.value()) {
    return false;
  }

  return IsFormOrClientNonSecure(client(), form);
}

SuggestionsContext BrowserAutofillManager::BuildSuggestionsContext(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source) {
  SuggestionsContext context;

  // When Compose suggestions or manual fallback for plus addresses are
  // requested, there is no need to load Autofill suggestions.
  if (IsTriggerSourceOnlyRelevantForCompose(trigger_source) ||
      IsPlusAddressesManuallyTriggered(trigger_source)) {
    context.do_not_generate_autofill_suggestions = true;
    return context;
  }

  // Need to refresh models before using the form_event_loggers.
  RefreshDataModels();

  const bool got_autofillable_form =
      GetCachedFormAndField(form, field, &context.form_structure,
                            &context.focused_field) &&
      // Don't send suggestions or track forms that should not be parsed.
      context.form_structure->ShouldBeParsed();

  if (!ShouldShowSuggestionsForAutocompleteUnrecognizedFields(trigger_source) &&
      got_autofillable_form &&
      context.focused_field->ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Pre-`AutofillPredictionsForAutocompleteUnrecognized`, autocomplete
    // suggestions were shown if all types of the form were suppressed or
    // unknown. If at least a single field had predictions (and the form was
    // thus considered autofillable), autocomplete suggestions were suppressed
    // for fields with a suppressed prediction.
    // To retain this behavior, the `suppress_reason` is only set if the form
    // contains a field that triggers (non-fallback) suggestions.
    // By not setting it, the autocomplete suggestion logic downstream is
    // triggered, since no Autofill `suggestions` are available.
    if (!base::ranges::all_of(*context.form_structure, [](const auto& field) {
          return field->ShouldSuppressSuggestionsAndFillingByDefault() ||
                 field->Type().GetStorableType() == UNKNOWN_TYPE;
        })) {
      context.suppress_reason = SuppressReason::kAutocompleteUnrecognized;
    }
    context.do_not_generate_autofill_suggestions = true;
    return context;
  }
  if (got_autofillable_form) {
    auto* logger = GetEventFormLogger(*context.focused_field);
    if (logger) {
      logger->OnDidInteractWithAutofillableForm(*context.form_structure,
                                                signin_state_for_metrics_);
    }
  }

  context.filling_product = GetPreferredSuggestionFillingProduct(
      got_autofillable_form ? context.focused_field->Type().GetStorableType()
                            : UNKNOWN_TYPE,
      trigger_source);

  // If this is a mixed content form, we show a warning message and don't offer
  // autofill. The warning is shown even if there are no autofill suggestions
  // available.
  if (IsFormMixedContent(client(), form) &&
      client().GetPrefs()->FindPreference(
          ::prefs::kMixedFormsWarningsEnabled) &&
      client().GetPrefs()->GetBoolean(::prefs::kMixedFormsWarningsEnabled)) {
    context.do_not_generate_autofill_suggestions = true;
    // If the user begins typing, we interpret that as dismissing the warning.
    // No suggestions are allowed, but the warning is no longer shown.
    if (field.DidUserType()) {
      context.suppress_reason = SuppressReason::kInsecureForm;
    } else {
      context.should_show_mixed_content_warning = true;
    }
    return context;
  }
  context.is_context_secure = !IsFormNonSecure(form);

  context.is_autofill_available =
      IsAutofillEnabled() &&
      (IsAutofillManuallyTriggered(trigger_source) || got_autofillable_form);

  return context;
}

void BrowserAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  if (base::FeatureList::IsEnabled(features::kAutofillDisableFilling)) {
    return;
  }
  const FormFieldData* field = form.FindFieldByGlobalId(field_id);
  if (!field) {
    return;
  }

  if (FormStructure* form_structure = FindCachedFormById(form.global_id())) {
    AutofillMetrics::LogParsedFormUntilInteractionTiming(
        base::TimeTicks::Now() - form_structure->form_parsed_timestamp());
  }

  // Once the user triggers autofill from the context menu, this event is
  // recorded, because the IPH configuration limits how many times the IPH can
  // be shown.
  if (IsAutofillManuallyTriggered(trigger_source)) {
    client().NotifyAutofillManualFallbackUsed();
  }

  external_delegate_->SetCurrentDataListValues(field->datalist_options());
  external_delegate_->OnQuery(form, *field, caret_bounds, trigger_source);

  SuggestionsContext context =
      BuildSuggestionsContext(form, *field, trigger_source);

  GenerateSuggestionsAndMaybeShowUI(
      form, *field, trigger_source, context,
      base::BindOnce(&BrowserAutofillManager::OnGenerateSuggestionsComplete,
                     weak_ptr_factory_.GetWeakPtr(), form, *field,
                     trigger_source, context));
}

void BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUI(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    SuggestionsContext context,
    OnGenerateSuggestionsCallback callback) {
  std::vector<Suggestion> suggestions =
      GetAvailableAddressAndCreditCardSuggestions(form, field, trigger_source,
                                                  context);

  auto ShouldSuppressSuggestions = [&] {
    switch (context.suppress_reason) {
      case SuppressReason::kNotSuppressed:
        return false;

      case SuppressReason::kAblation:
        single_field_form_fill_router_->CancelPendingQueries();
        external_delegate_->OnSuggestionsReturned(field.global_id(),
                                                  suggestions);
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: Ablation experiment";
        return true;

      case SuppressReason::kInsecureForm:
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: Insecure form";
        return true;
      case SuppressReason::kAutocompleteOff:
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: autocomplete=off";
        return true;
      case SuppressReason::kAutocompleteUnrecognized:
        LOG_AF(log_manager())
            << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
            << " Reason: autocomplete=unrecognized";
        return true;
    }
  };

  if (context.is_autofill_available && ShouldSuppressSuggestions()) {
    return;
  }

  const bool form_element_was_clicked =
      trigger_source ==
      AutofillSuggestionTriggerSource::kFormControlElementClicked;

  // Try to show Fast Checkout.
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
    std::move(callback).Run(/*show_suggestions=*/false, std::move(suggestions));
    return;
  }

  // Try to show Touch to Fill.
  if (touch_to_fill_delegate_ &&
      (touch_to_fill_delegate_->IsShowingTouchToFill() ||
       (form_element_was_clicked &&
        touch_to_fill_delegate_->TryToShowTouchToFill(form, field)))) {
    // Touch To Fill surface is shown, so abort showing regular Autofill UI.
    // Now the flow is controlled by the `touch_to_fill_delegate_` instead
    // of `external_delegate_`.
    std::move(callback).Run(/*show_suggestions=*/false, std::move(suggestions));
    return;
  }

  // Try to show plus address suggestions. If manually triggered, only plus
  // addresses suggestions are shown. Otherwise plus address suggestions are
  // mixed with address suggestions.
  const bool should_offer_plus_addresses =
      IsPlusAddressesManuallyTriggered(trigger_source) ||
      (!context.should_show_mixed_content_warning &&
       context.is_autofill_available &&
       !context.do_not_generate_autofill_suggestions &&
       context.filling_product == FillingProduct::kAddress &&
       context.focused_field &&
       context.focused_field->Type().group() == FieldTypeGroup::kEmail &&
       client().GetPlusAddressDelegate());

  if (should_offer_plus_addresses) {
    const AutofillClient::PasswordFormType password_form_type =
        client().ClassifyAsPasswordForm(*this, form.global_id(),
                                        field.global_id());
    const AutofillPlusAddressDelegate::SuggestionContext suggestions_context =
        IsPlusAddressesManuallyTriggered(trigger_source)
            ? AutofillPlusAddressDelegate::SuggestionContext::kManualFallback
            : AutofillPlusAddressDelegate::SuggestionContext::
                  kAutofillProfileOnEmailField;
    client().GetPlusAddressDelegate()->GetSuggestions(
        client().GetLastCommittedPrimaryMainFrameOrigin(),
        client().IsOffTheRecord(), password_form_type, field.value(),
        trigger_source,
        base::BindOnce(&BrowserAutofillManager::OnGetPlusAddressSuggestions,
                       weak_ptr_factory_.GetWeakPtr(), suggestions_context,
                       password_form_type, form, field, std::move(suggestions),
                       std::move(callback)));

    return;
  }

  // Check if other suggestion sources should be queried. Other suggestions may
  // include Compose or single field form suggestions. Manual fallbacks can't
  // trigger different suggestion types.
  const bool should_offer_other_suggestions =
      suggestions.empty() && !IsAutofillManuallyTriggered(trigger_source) &&
      trigger_source != AutofillSuggestionTriggerSource::
                            kShowPromptAfterDialogClosedNonManualFallback;

  if (should_offer_other_suggestions &&
      (field.form_control_type() == FormControlType::kTextArea ||
       field.form_control_type() == FormControlType::kContentEditable)) {
    AutofillComposeDelegate* compose_delegate = client().GetComposeDelegate();
    std::optional<Suggestion> maybe_compose_suggestion =
        compose_delegate
            ? compose_delegate->GetSuggestion(form, field, trigger_source)
            : std::nullopt;
    if (maybe_compose_suggestion) {
      std::move(callback).Run(/*show_suggestions=*/true,
                              {*std::move(maybe_compose_suggestion)});
      return;
    }
  }

  // TODO(b/340494671): Move ShouldOfferSingleFieldFormFill out of
  // OnAskForValuesToFillImpl.
  auto ShouldOfferSingleFieldFormFill = [&] {
    if (!suggestions.empty() || !should_offer_other_suggestions) {
      return false;
    }

    if (trigger_source ==
        AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick) {
      return false;
    }

    // Do not offer single field form fill suggestions for credit card number,
    // cvc, and expiration date related fields. Standalone cvc fields (used to
    // re-authenticate the use of a credit card the website has on file) will be
    // handled separately because those have the field type
    // CREDIT_CARD_STANDALONE_VERIFICATION_CODE.
    FieldType server_type =
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
    // TODO(crbug.com/40853053): Revisit here to see whether we should offer
    // IBAN filling for fields with unrecognized autocomplete attribute
    if (context.suppress_reason == SuppressReason::kAutocompleteUnrecognized) {
      FormStructure* form_structure = nullptr;
      AutofillField* autofill_field = nullptr;
      // Display the IPH only if the form can be autofilled and the user has
      // profiles which can fill the current field.
      if (GetCachedFormAndField(form, field, &form_structure,
                                &autofill_field) &&
          FieldTypeGroupToFormType(autofill_field->Type().group()) ==
              FormType::kAddressForm &&
          base::ranges::any_of(
              client()
                  .GetPersonalDataManager()
                  ->address_data_manager()
                  .GetProfiles(),
              [field_type = autofill_field->Type().GetStorableType()](
                  const AutofillProfile* profile) {
                return profile->HasInfo(field_type);
              }) &&
          base::FeatureList::IsEnabled(
              features::kAutofillEnableManualFallbackIPH)) {
        client().ShowAutofillFieldIphForManualFallbackFeature(field);
      }
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

  if (ShouldOfferSingleFieldFormFill()) {
    bool handled_by_single_field_form_filler =
        single_field_form_fill_router_->OnGetSingleFieldSuggestions(
            field, client(),
            base::BindRepeating(
                &BrowserAutofillManager::OnGetSingleFieldSuggestionsCallback,
                weak_ptr_factory_.GetWeakPtr(), form_element_was_clicked, form,
                context.focused_field ? context.focused_field->Type().group()
                                      : FieldTypeGroup::kNoGroup),
            context);
    if (handled_by_single_field_form_filler) {
      // Suggestions come back asynchronously, so the SingleFieldFormFillRouter
      // will handle sending the results back to the renderer.
      // TODO(crbug.com/40100455): The callback will only be called once.
      std::move(callback).Run(/*show_suggestions=*/false,
                              std::move(suggestions));
      return;
    }
  }

  single_field_form_fill_router_->CancelPendingQueries();

  // Show the list of `suggestions`. These may include address, credit card or
  // plus address suggestions. Additionally, suggestions related to Compose or
  // warnings about mixed content might be present.
  std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions));
}

void BrowserAutofillManager::OnGenerateSuggestionsComplete(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    const SuggestionsContext& context,
    bool show_suggestions,
    std::vector<Suggestion> suggestions) {
  LogSuggestionsCount(context, suggestions);
  // When focusing on a field, log whether there is a suggestion for the user
  // and whether the suggestion is shown.
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (trigger_source ==
          AutofillSuggestionTriggerSource::kFormControlElementClicked &&
      GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    autofill_field->AppendLogEventIfNotRepeated(AskForValuesToFillFieldLogEvent{
        .has_suggestion = ToOptionalBoolean(!suggestions.empty()),
        .suggestion_is_shown = ToOptionalBoolean(show_suggestions),
    });
  }
  if (show_suggestions) {
    // Send Autofill suggestions (could be an empty list).
    external_delegate_->OnSuggestionsReturned(field.global_id(), suggestions);
  }
}

void BrowserAutofillManager::OnGetPlusAddressSuggestions(
    AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
    AutofillClient::PasswordFormType password_form_type,
    const FormData& form,
    const FormFieldData& field,
    std::vector<Suggestion> address_suggestions,
    OnGenerateSuggestionsCallback callback,
    std::vector<Suggestion> suggestions) {
  if (suggestions.empty()) {
    std::move(callback).Run(/*show_suggestions=*/true,
                            std::move(address_suggestions));
    return;
  }

  client().GetPlusAddressDelegate()->OnPlusAddressSuggestionShown(
      *this, form.global_id(), field.global_id(), suggestions_context,
      password_form_type, suggestions[0].type);
  if (address_suggestions.empty()) {
    std::optional<Suggestion> manage_plus_addresses =
        client().GetPlusAddressDelegate()->GetManagePlusAddressSuggestion();
    // Present only if the `kPlusAddressUIRedesign` flag is enabled.
    if (manage_plus_addresses) {
      suggestions.emplace_back(SuggestionType::kSeparator);
      suggestions.emplace_back(std::move(manage_plus_addresses.value()));
    }
  }
  suggestions.insert(suggestions.cend(),
                     std::make_move_iterator(address_suggestions.begin()),
                     std::make_move_iterator(address_suggestions.end()));

  std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions));
}

void BrowserAutofillManager::AuthenticateThenFillCreditCardForm(
    const FormData& form,
    const FormFieldData& field,
    const CreditCard& credit_card,
    const AutofillTriggerDetails& trigger_details) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }
  credit_card_ = credit_card;
  credit_card_form_event_logger_->OnDidSelectCardSuggestion(
      credit_card_, *form_structure, signin_state_for_metrics_);
  // If no authentication is needed, directly forward filling to FormFiller.
  if (!ShouldFetchCreditCard(form, field, *form_structure, *autofill_field,
                             credit_card_)) {
    form_filler_->FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, field, &credit_card_,
        /*optional_cvc=*/std::nullopt, form_structure, autofill_field,
        trigger_details);
    return;
  }
  credit_card_form_event_logger_->LogDeprecatedCreditCardSelectedMetric(
      credit_card_, *form_structure, signin_state_for_metrics_);

  credit_card_form_ = form;
  credit_card_field_ = field;

  // CreditCardAccessManager::FetchCreditCard() will trigger
  // OnCreditCardFetched() in this class after successfully fetching the
  // card.
  fetched_credit_card_trigger_source_ = trigger_details.trigger_source;
  GetCreditCardAccessManager().FetchCreditCard(
      &credit_card_,
      base::BindOnce(&BrowserAutofillManager::OnCreditCardFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserAutofillManager::FillOrPreviewProfileForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& field,
    const AutofillProfile& profile,
    const AutofillTriggerDetails& trigger_details) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }
  form_filler_->FillOrPreviewForm(action_persistence, form, field, &profile,
                                  /*cvc=*/std::nullopt, form_structure,
                                  autofill_field, trigger_details);
}

void BrowserAutofillManager::FillOrPreviewField(
    mojom::ActionPersistence action_persistence,
    mojom::FieldActionType action_type,
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& value,
    SuggestionType type,
    std::optional<FieldType> field_type_used) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  GetCachedFormAndField(form, field, &form_structure, &autofill_field);
  form_filler_->FillOrPreviewField(action_persistence, action_type, form, field,
                                   form_structure, autofill_field, value, type,
                                   field_type_used);
  if (action_persistence == mojom::ActionPersistence::kFill) {
    const FormFieldData* const_field = &field;
    const AutofillField* const_autofill_field = autofill_field;
    if (type == SuggestionType::kAddressFieldByFieldFilling) {
      address_form_event_logger_->OnFilledByFieldByFieldFilling(type);
      address_form_event_logger_->RecordFillingOperation(
          form.global_id(), base::make_span(&const_field, 1u),
          base::make_span(&const_autofill_field, 1u));
    } else if (type == SuggestionType::kCreditCardFieldByFieldFilling) {
      credit_card_form_event_logger_->OnFilledByFieldByFieldFilling(type);
      credit_card_form_event_logger_->RecordFillingOperation(
          form.global_id(), base::make_span(&const_field, 1u),
          base::make_span(&const_autofill_field, 1u));
    }

    const bool is_address_manual_fallback_on_non_address_field =
        IsAddressAutofillManuallyTriggeredOnNonAddressField(
            type, const_autofill_field);
    const bool is_payments_manual_fallback_on_non_payments_field =
        IsCreditCardAutofillManuallyTriggeredOnNonCreditCardField(
            type, const_autofill_field);
    if (is_address_manual_fallback_on_non_address_field ||
        is_payments_manual_fallback_on_non_payments_field) {
      manual_fallback_logger_->OnDidFillSuggestion(
          GetFillingProductFromSuggestionType(type));
    }
  }
}

void BrowserAutofillManager::OnDidFillAddressFormFillingSuggestion(
    const AutofillProfile& profile,
    const FormData& form,
    const FormFieldData& field,
    AutofillTriggerSource trigger_source) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  GetCachedFormAndField(form, field, &form_structure, &autofill_field);
  if (!form_structure || !autofill_field) {
    return;
  }
  address_form_event_logger_->OnDidFillFormFillingSuggestion(
      profile, *form_structure, *autofill_field, signin_state_for_metrics_,
      trigger_source);
}

void BrowserAutofillManager::UndoAutofill(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& trigger_field) {
  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }
  // This will apply the undo operation and return information about the
  // operation being undone, for metric purposes.
  FillingProduct filling_product = form_filler_->UndoAutofill(
      action_persistence, form, *form_structure, trigger_field);

  // The remaining logic is only relevant for filling.
  if (action_persistence != mojom::ActionPersistence::kPreview) {
    if (filling_product == FillingProduct::kAddress) {
      address_form_event_logger_->OnDidUndoAutofill();
    } else if (filling_product == FillingProduct::kCreditCard) {
      credit_card_form_event_logger_->OnDidUndoAutofill();
    }
  }
}

void BrowserAutofillManager::FillOrPreviewCreditCardForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& field,
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const AutofillTriggerDetails& trigger_details) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field)) {
    return;
  }
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }
  form_filler_->FillOrPreviewForm(action_persistence, form, field, &credit_card,
                                  &cvc, form_structure, autofill_field,
                                  trigger_details,
                                  /*is_refill=*/false);
}

void BrowserAutofillManager::OnFocusOnNonFormFieldImpl(
    bool had_interacted_form) {
  // For historical reasons, Chrome takes action on this message only if focus
  // was previously on a form with which the user had interacted.
  // TODO(crbug.com/40726656): Remove need for this short-circuit.
  if (!had_interacted_form) {
    return;
  }

  ProcessPendingFormForUpload();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // There is no way of determining whether ChromeVox is in use, so assume it's
  // being used.
  external_delegate_->OnAutofillAvailabilityEvent(
      mojom::AutofillSuggestionAvailability::kNoSuggestions);
#else
  if (external_delegate_->HasActiveScreenReader()) {
    external_delegate_->OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
  }
#endif
}

void BrowserAutofillManager::OnFocusOnFormFieldImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (pending_form_data_ &&
      pending_form_data_->global_id() != form.global_id()) {
    // A new form has received the focus, so we may have votes to upload for the
    // old form.
    ProcessPendingFormForUpload();
  }

  // Notify installed screen readers if the focus is on a field for which there
  // are suggestions to present. Ignore if a screen reader is not present. If
  // the platform is ChromeOS, then assume ChromeVox is in use as there is no
  // way of determining whether it's being used from this point in the code.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!external_delegate_->HasActiveScreenReader()) {
    return;
  }
#endif

  const FormFieldData* field = form.FindFieldByGlobalId(field_id);
  if (!field) {
    return;
  }

  // TODO(crbug.com/41392130): Add metrics for performance impact.
  SuggestionsContext context = BuildSuggestionsContext(
      form, *field, AutofillSuggestionTriggerSource::kUnspecified);
  // This code path checks if suggestions to be announced to a screen reader are
  // available when the focus on a form field changes. This cannot happen in
  // `OnAskForValuesToFillImpl()`, since the `AutofillSuggestionAvailability` is
  // a sticky flag and needs to be reset when a non-autofillable field is
  // focused. The suggestion trigger source doesn't influence the set of
  // suggestions generated, but only the way suggestions behave when they are
  // accepted. For this reason, checking whether suggestions are available can
  // be done with the `kUnspecified` suggestion trigger source.
  std::vector<Suggestion> suggestions =
      GetAvailableAddressAndCreditCardSuggestions(
          form, *field, AutofillSuggestionTriggerSource::kUnspecified, context);
  external_delegate_->OnAutofillAvailabilityEvent(
      (context.suppress_reason == SuppressReason::kNotSuppressed &&
       !suggestions.empty())
          ? mojom::AutofillSuggestionAvailability::kAutofillAvailable
          : mojom::AutofillSuggestionAvailability::kNoSuggestions);
}

void BrowserAutofillManager::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  // TODO(crbug.com/40564270): Handle select control change.
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
  UpdateInitialInteractionTimestamp(timestamp);
}

void BrowserAutofillManager::DidShowSuggestions(
    base::span<const SuggestionType> shown_suggestions_types,
    const FormData& form,
    const FormFieldData& field) {
  NotifyObservers(&Observer::OnSuggestionsShown);

  bool has_autofill_suggestions = base::ranges::any_of(
      shown_suggestions_types,
      AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId);
  if (!has_autofill_suggestions) {
    return;
  }

  if (base::Contains(shown_suggestions_types, FillingProduct::kCreditCard,
                     GetFillingProductFromSuggestionType) &&
      IsCreditCardFidoAuthenticationEnabled()) {
    GetCreditCardAccessManager().PrepareToFetchCreditCard();
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  const bool has_cached_form_and_field =
      GetCachedFormAndField(form, field, &form_structure, &autofill_field);

  // Check if Autofill was triggered via manual fallback on a field that was
  // either unclassified or classified differently as the target
  // `FillingProduct`.
  // Note that in this type of flow we purposely do not log key metrics so we do
  // not mess with the current denominator (classified forms).
  const bool is_address_manual_fallback_on_non_address_field =
      base::ranges::any_of(
          shown_suggestions_types, [autofill_field](SuggestionType type) {
            return IsAddressAutofillManuallyTriggeredOnNonAddressField(
                type, autofill_field);
          });
  const bool is_payments_manual_fallback_on_non_payments_field =
      base::ranges::any_of(
          shown_suggestions_types, [autofill_field](SuggestionType type) {
            return IsCreditCardAutofillManuallyTriggeredOnNonCreditCardField(
                type, autofill_field);
          });
  if (is_address_manual_fallback_on_non_address_field) {
    manual_fallback_logger_->OnDidShowSuggestions(FillingProduct::kAddress);
    return;
  }
  if (is_payments_manual_fallback_on_non_payments_field) {
    manual_fallback_logger_->OnDidShowSuggestions(FillingProduct::kCreditCard);
    return;
  }

  if (!has_cached_form_and_field) {
    return;
  }
  autofill_field->set_did_trigger_suggestions(true);

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
}

void BrowserAutofillManager::OnHidePopupImpl() {
  single_field_form_fill_router_->CancelPendingQueries();
  client().HideAutofillSuggestions(SuggestionHidingReason::kRendererEvent);
  client().HideAutofillFieldIphForManualFallbackFeature();
  if (fast_checkout_delegate_) {
    fast_checkout_delegate_->HideFastCheckout(/*allow_further_runs=*/false);
  }
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->HideTouchToFill();
  }
}

bool BrowserAutofillManager::RemoveAutofillProfileOrCreditCard(
    Suggestion::BackendId backend_id) {
  const std::string guid = absl::get<Suggestion::Guid>(backend_id).value();
  PersonalDataManager* pdm = client().GetPersonalDataManager();

  if (const CreditCard* credit_card =
          pdm->payments_data_manager().GetCreditCardByGUID(guid)) {
    // Server cards cannot be deleted from within Chrome.
    bool allowed_to_delete = CreditCard::IsLocalCard(credit_card);
    if (allowed_to_delete) {
      pdm->payments_data_manager().DeleteLocalCreditCards({*credit_card});
    }
    return allowed_to_delete;
  }

  if (const AutofillProfile* profile =
          pdm->address_data_manager().GetProfileByGUID(guid)) {
    pdm->RemoveByGUID(profile->guid());
    return true;
  }

  return false;  // The ID was valid. The entry may have been deleted in a race.
}

void BrowserAutofillManager::RemoveCurrentSingleFieldSuggestion(
    const std::u16string& name,
    const std::u16string& value,
    SuggestionType type) {
  single_field_form_fill_router_->OnRemoveCurrentSingleFieldSuggestion(
      name, value, type);
}

void BrowserAutofillManager::OnSingleFieldSuggestionSelected(
    const std::u16string& value,
    SuggestionType type,
    const FormData& form,
    const FormFieldData& field) {
  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(value, type);

  AutofillField* autofill_trigger_field = GetAutofillField(form, field);
  if (!autofill_trigger_field) {
    return;
  }
  if (IsSingleFieldFormFillerFillingProduct(
          GetFillingProductFromSuggestionType(type))) {
    autofill_trigger_field->AppendLogEventIfNotRepeated(
        TriggerFillFieldLogEvent{
            .data_type = GetEventTypeFromSingleFieldSuggestionType(type),
            .associated_country_code = "",
            .timestamp = AutofillClock::Now()});
  }
}

void BrowserAutofillManager::OnUserHideSuggestions(const FormData& form,
                                                   const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }

  auto* logger = GetEventFormLogger(*autofill_field);
  if (logger) {
    logger->OnUserHideSuggestions(*form_structure, *autofill_field);
  }
}

bool BrowserAutofillManager::ShouldClearPreviewedForm() {
  return GetCreditCardAccessManager().ShouldClearPreviewedForm();
}

void BrowserAutofillManager::OnSelectOrSelectListFieldOptionsDidChangeImpl(
    const FormData& form) {
  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }

  driver().SendAutofillTypePredictionsToRenderer({form_structure});

  if (form_filler_->ShouldTriggerRefill(
          *form_structure, RefillTriggerReason::kSelectOptionsChanged)) {
    form_filler_->TriggerRefill(
        form, {.trigger_source = AutofillTriggerSource::kSelectOptionsChanged});
  }
}

void BrowserAutofillManager::OnJavaScriptChangedAutofilledValueImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const std::u16string& old_value,
    bool formatting_only) {
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
      if (form.fields[i].global_id() == field_id) {
        return base::StringPrintf("Field %zu", i);
      }
    }
    return std::string("unknown");
  };
  const FormFieldData* field = form.FindFieldByGlobalId(field_id);
  if (!field) {
    return;
  }
  LogBuffer change(IsLoggingActive(log_manager()));
  LOG_AF(change) << Tag{"div"} << Attrib{"class", "form"};
  LOG_AF(change) << *field << Br{};
  LOG_AF(change) << "Old value structure: '"
                 << StructureOfString(old_value.substr(0, 80)) << "'" << Br{};
  LOG_AF(change) << "New value structure: '"
                 << StructureOfString(field->value().substr(0, 80)) << "'";
  LOG_AF(log_manager()) << LoggingScope::kWebsiteModifiedFieldValue
                        << LogMessage::kJavaScriptChangedAutofilledValue << Br{}
                        << Tag{"table"} << Tr{} << GetFieldNumber()
                        << std::move(change);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, *field, &form_structure, &autofill_field)) {
    return;
  }
  AnalyzeJavaScriptChangedAutofilledValue(*form_structure, *autofill_field,
                                          field->value().empty(),
                                          formatting_only);
  if (formatting_only) {
    return;
  }
  form_filler_->MaybeTriggerRefillForExpirationDate(
      form, *field, *form_structure, old_value,
      {.trigger_source =
           AutofillTriggerSource::kJavaScriptChangedAutofilledValue});
}

void BrowserAutofillManager::AnalyzeJavaScriptChangedAutofilledValue(
    const FormStructure& form,
    AutofillField& field,
    bool cleared_value,
    bool formatting_only) {
  if (!formatting_only &&
      base::FeatureList::IsEnabled(
          features::kAutofillFixCachingOnJavaScriptChanges)) {
    field.set_is_autofilled(false);
  }
  // We are interested in reporting the events where JavaScript resets an
  // autofilled value immediately after filling. For a reset, the value
  // needs to be empty.
  if (!cleared_value) {
    return;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  std::optional<base::TimeTicks> original_fill_time =
      form_filler_->GetOriginalFillingTime(form.global_id());
  if (!original_fill_time) {
    return;
  }
  base::TimeDelta delta = now - *original_fill_time;
  // If the filling happened too long ago, maybe this is just an effect of
  // the user pressing a "reset form" button.
  if (delta >= form_filler_->get_limit_before_refill()) {
    return;
  }
  if (auto* logger = GetEventFormLogger(field)) {
    logger->OnAutofilledFieldWasClearedByJavaScriptShortlyAfterFill(form);
  }
}

void BrowserAutofillManager::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card) {
  if (result != CreditCardFetchResult::kSuccess) {
    driver().RendererShouldClearPreviewedForm();
    return;
  }
  // In the failure case, `credit_card` can be `nullptr`, but in the success
  // case it is non-null.
  CHECK(credit_card);
  OnCreditCardFetchedSuccessfully(*credit_card);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(credit_card_form_, credit_card_field_,
                             &form_structure, &autofill_field)) {
    return;
  }

  FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, credit_card_form_, credit_card_field_,
      *credit_card, credit_card->cvc(),
      {.trigger_source = fetched_credit_card_trigger_source_.value_or(
           AutofillTriggerSource::kCreditCardCvcPopup)});
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

void BrowserAutofillManager::OnGetSingleFieldSuggestionsCallback(
    bool form_element_was_clicked,
    const FormData& form,
    FieldTypeGroup focused_field_type_group,
    FieldGlobalId field_id,
    const std::vector<Suggestion>& suggestions) {
  MaybeLogAutocompleteSuppressionByPlusAddresses(client(), suggestions,
                                                 focused_field_type_group);
  // TODO(b/309163415): Replace parameter of FormFieldData in
  // `TryToShowTouchToFill` by FieldGlobalId.
  const FormFieldData* form_field = form.FindFieldByGlobalId(field_id);
  if (form_field && form_element_was_clicked && touch_to_fill_delegate_ &&
      touch_to_fill_delegate_->TryToShowTouchToFill(form, *form_field)) {
    return;
  }
  external_delegate_->OnSuggestionsReturned(field_id, suggestions);
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
  std::erase_if(queued_vote_uploads_, [form_signature](const auto& entry) {
    return entry.first == form_signature;
  });
}

void BrowserAutofillManager::FlushPendingLogQualityAndVotesUploadCallbacks() {
  std::list<std::pair<FormSignature, base::OnceClosure>> queued_vote_uploads =
      std::exchange(queued_vote_uploads_, {});
  for (auto& i : queued_vote_uploads) {
    std::move(i.second).Run();
  }
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
  if (observed_submission) {
    WipeLogQualityAndVotesUploadCallback(submitted_form->form_signature());
  }
  if (submitted_form->ShouldRunHeuristics() ||
      submitted_form->ShouldRunHeuristicsForSingleFieldForms() ||
      submitted_form->ShouldBeQueried()) {
    autofill_metrics::LogQualityMetrics(
        *submitted_form, submitted_form->form_parsed_timestamp(),
        interaction_time, submission_time, form_interactions_ukm_logger(),
        observed_submission);
    if (observed_submission) {
      // Ensure that callbacks for blur votes get sent as well here because
      // we are not sure whether a full navigation with a Reset() call follows.
      FlushPendingLogQualityAndVotesUploadCallbacks();
    }
  }
  if (!submitted_form->ShouldBeUploaded()) {
    return;
  }
  if (ShouldRecordUkm() && ShouldUploadUkm(*submitted_form)) {
    AutofillMetrics::LogAutofillFieldInfoAfterSubmission(
        client().GetUkmRecorder(), source_id, *submitted_form, submission_time);
  }
  if (!client().GetCrowdsourcingManager()) {
    return;
  }
  const PersonalDataManager* pdm = client().GetPersonalDataManager();
  FieldTypeSet non_empty_types;
  for (const AutofillProfile* profile :
       pdm->address_data_manager().GetProfiles()) {
    profile->GetNonEmptyTypes(app_locale_, &non_empty_types);
  }
  for (const CreditCard* card : pdm->payments_data_manager().GetCreditCards()) {
    card->GetNonEmptyTypes(app_locale_, &non_empty_types);
  }
  // As CVC is not stored, treat it separately.
  if (!last_unlocked_credit_card_cvc_.empty() ||
      non_empty_types.contains(CREDIT_CARD_NUMBER)) {
    non_empty_types.insert(CREDIT_CARD_VERIFICATION_CODE);
  }
  client().GetCrowdsourcingManager()->StartUploadRequest(
      /*upload_contents=*/EncodeUploadRequest(*submitted_form, non_empty_types,
                                              /*login_form_signature=*/{},
                                              observed_submission),
      submitted_form->submission_source(),
      /*is_password_manager_upload=*/false);
}

const gfx::Image& BrowserAutofillManager::GetCardImage(
    const CreditCard& credit_card) {
  gfx::Image* card_art_image =
      client()
          .GetPersonalDataManager()
          ->payments_data_manager()
          .GetCreditCardArtImageForUrl(credit_card.card_art_url());
  return card_art_image
             ? *card_art_image
             : ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                   CreditCard::IconResourceId(credit_card.network()));
}

void BrowserAutofillManager::OnSubmissionFieldTypesDetermined(
    std::unique_ptr<FormStructure> submitted_form,
    base::TimeTicks interaction_time,
    base::TimeTicks submission_time,
    bool observed_submission,
    ukm::SourceId source_id) {
  auto count_types = [&submitted_form](FormType type) {
    return base::ranges::count_if(
        submitted_form->fields(),
        [=](const std::unique_ptr<AutofillField>& field) {
          return FieldTypeGroupToFormType(field->Type().group()) == type;
        });
  };

  size_t address_fields_count = count_types(FormType::kAddressForm);
  autofill_metrics::FormGroupFillingStats address_filling_stats =
      autofill_metrics::GetFormFillingStatsForFormType(FormType::kAddressForm,
                                                       *submitted_form);
  const bool can_trigger_address_survey =
      address_fields_count >=
          kMinNumberAddressFieldsToTriggerAddressUserPerceptionSurvey &&
      address_filling_stats.TotalFilled() > 0 &&
      base::FeatureList::IsEnabled(
          features::kAutofillAddressUserPerceptionSurvey);

  size_t credit_card_fields_count = count_types(FormType::kCreditCardForm);
  autofill_metrics::FormGroupFillingStats credit_card_filling_stats =
      autofill_metrics::GetFormFillingStatsForFormType(
          FormType::kCreditCardForm, *submitted_form);
  const bool can_trigger_credit_card_survey =
      credit_card_fields_count > 0 &&
      credit_card_filling_stats.TotalFilled() > 0;

  if (can_trigger_address_survey) {
    client().TriggerUserPerceptionOfAutofillSurvey(
        FillingProduct::kAddress,
        FormFillingStatsToSurveyStringData(address_filling_stats));
  } else if (can_trigger_credit_card_survey &&
             base::FeatureList::IsEnabled(
                 features::kAutofillCreditCardUserPerceptionSurvey)) {
    client().TriggerUserPerceptionOfAutofillSurvey(
        FillingProduct::kCreditCard,
        FormFillingStatsToSurveyStringData(credit_card_filling_stats));
  }
  UploadVotesAndLogQuality(std::move(submitted_form), interaction_time,
                           submission_time, observed_submission, source_id);
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
          &client());
  credit_card_form_event_logger_ =
      std::make_unique<autofill_metrics::CreditCardFormEventLogger>(
          driver().IsInAnyMainFrame(), form_interactions_ukm_logger(),
          client().GetPersonalDataManager(), &client());
  autocomplete_unrecognized_fallback_logger_ = std::make_unique<
      autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger>();
  manual_fallback_logger_ =
      std::make_unique<autofill_metrics::ManualFallbackEventLogger>();

  has_logged_autofill_enabled_ = false;
  user_did_type_ = false;
  credit_card_ = CreditCard();
  credit_card_form_ = FormData();
  credit_card_field_ = FormFieldData();
  last_unlocked_credit_card_cvc_.clear();
  initial_interaction_timestamp_ = TimeTicks();
  fetched_credit_card_trigger_source_ = std::nullopt;
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->Reset();
  }
  form_filler_->Reset();
  form_submitted_timestamp_ = TimeTicks();
  four_digit_combinations_in_dom_.clear();
}

bool BrowserAutofillManager::RefreshDataModels() {
  if (!IsAutofillEnabled()) {
    return false;
  }

  GetCreditCardAccessManager().UpdateCreditCardFormEventLogger();

  const std::vector<const AutofillProfile*>& profiles =
      client().GetPersonalDataManager()->address_data_manager().GetProfiles();
  address_form_event_logger_->set_record_type_count(profiles.size());

  return !profiles.empty() || !client()
                                   .GetPersonalDataManager()
                                   ->payments_data_manager()
                                   .GetCreditCards()
                                   .empty();
}

void BrowserAutofillManager::OnDidFillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormStructure& form_structure,
    const AutofillField& trigger_autofill_field,
    base::span<const FormFieldData*> safe_filled_fields,
    base::span<const AutofillField*> safe_filled_autofill_fields,
    const base::flat_set<FieldGlobalId>& filled_fields,
    const base::flat_set<FieldGlobalId>& safe_fields,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const AutofillTriggerDetails& trigger_details,
    bool is_refill) {
  client().DidFillOrPreviewForm(action_persistence,
                                trigger_details.trigger_source, is_refill);
  NotifyObservers(&Observer::OnFillOrPreviewDataModelForm,
                  form_structure.global_id(), action_persistence,
                  safe_filled_fields, profile_or_credit_card);
  if (action_persistence == mojom::ActionPersistence::kPreview) {
    return;
  }
  CHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);
  if (absl::holds_alternative<const CreditCard*>(profile_or_credit_card)) {
    if (is_refill) {
      credit_card_form_event_logger_->OnDidRefill(signin_state_for_metrics_,
                                                  form_structure);
    } else {
      credit_card_form_event_logger_->RecordFillingOperation(
          form_structure.global_id(), safe_filled_fields,
          safe_filled_autofill_fields);
      // The originally selected masked card is `credit_card_`. So we must log
      // `credit_card_` as opposed to
      // `absl::get<CreditCard*>(profile_or_credit_card)` to correctly indicate
      // whether the user filled the form using a masked card suggestion.
      credit_card_form_event_logger_->OnDidFillFormFillingSuggestion(
          credit_card_, form_structure, trigger_autofill_field, filled_fields,
          safe_fields, signin_state_for_metrics_,
          trigger_details.trigger_source);

      client()
          .GetPersonalDataManager()
          ->payments_data_manager()
          .RecordUseOfCard(
              absl::get<const CreditCard*>(profile_or_credit_card));
    }
  } else {
    CHECK(absl::holds_alternative<const AutofillProfile*>(
        profile_or_credit_card));
    const AutofillProfile* profile =
        absl::get<const AutofillProfile*>(profile_or_credit_card);
    if (!trigger_autofill_field
             .ShouldSuppressSuggestionsAndFillingByDefault()) {
      if (is_refill) {
        address_form_event_logger_->OnDidRefill(signin_state_for_metrics_,
                                                form_structure);
      } else {
        address_form_event_logger_->RecordFillingOperation(
            form_structure.global_id(), safe_filled_fields,
            safe_filled_autofill_fields);
        address_form_event_logger_->OnDidFillFormFillingSuggestion(
            *profile, form_structure, trigger_autofill_field,
            signin_state_for_metrics_, trigger_details.trigger_source);
      }
    } else if (!is_refill) {
      address_form_event_logger_->RecordFillingOperation(
          form_structure.global_id(), safe_filled_fields,
          safe_filled_autofill_fields);
      autocomplete_unrecognized_fallback_logger_
          ->OnDidFillFormFillingSuggestion();
    }
    if (!is_refill) {
      client().GetPersonalDataManager()->address_data_manager().RecordUseOf(
          *profile);
    }
  }
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
    const FormFieldData& field) const {
  if (!client().GetPersonalDataManager()) {
    return nullptr;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return nullptr;
  }

  if (!form_structure->IsAutofillable()) {
    return nullptr;
  }

  return autofill_field;
}

void BrowserAutofillManager::OnCreditCardFetchedSuccessfully(
    const CreditCard& credit_card) {
  last_unlocked_credit_card_cvc_ = credit_card.cvc();
  // If the synced down card is a virtual card, let the client know so that it
  // can show the UI to help user to manually fill the form, if needed.
  if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard) {
    DCHECK(!credit_card.cvc().empty());
    client().GetFormDataImporter()->CacheFetchedVirtualCard(
        credit_card.LastFourDigits());

    VirtualCardManualFallbackBubbleOptions options;
    options.masked_card_name = credit_card.CardNameForAutofillDisplay();
    options.masked_card_number_last_four =
        credit_card.ObfuscatedNumberWithVisibleLastFourDigits();
    options.virtual_card = credit_card;
    // TODO(crbug.com/40927041): Remove CVC from
    // VirtualCardManualFallbackBubbleOptions.
    options.virtual_card_cvc = credit_card.cvc();
    options.card_image = GetCardImage(credit_card);
    client().GetPaymentsAutofillClient()->OnVirtualCardDataAvailable(options);
  }

  // After a server card is fetched, save its instrument id.
  client().GetFormDataImporter()->SetFetchedCardInstrumentId(
      credit_card.instrument_id());

  if (credit_card.record_type() == CreditCard::RecordType::kFullServerCard ||
      credit_card.record_type() == CreditCard::RecordType::kVirtualCard) {
    GetCreditCardAccessManager().CacheUnmaskedCardInfo(credit_card,
                                                       credit_card.cvc());
  }
}

std::vector<Suggestion> BrowserAutofillManager::GetProfileSuggestions(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& trigger_field,
    const AutofillField* trigger_autofill_field,
    AutofillSuggestionTriggerSource trigger_source) const {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (trigger_source !=
      AutofillSuggestionTriggerSource::kManualFallbackAddress) {
    bool should_suppress =
        client()
            .GetPersonalDataManager()
            ->address_data_manager()
            .AreAddressSuggestionsBlocked(
                CalculateFormSignature(form),
                CalculateFieldSignatureForField(trigger_field), form.url());
    base::UmaHistogramBoolean("Autofill.Suggestion.StrikeSuppression.Address",
                              should_suppress);
    if (should_suppress) {
      // If the user already reached the strike limit on this particular field,
      // address suggestions are suppressed.
      return {};
    }
  }
#endif
  address_form_event_logger_->OnDidPollSuggestions(trigger_field,
                                                   signin_state_for_metrics_);

  const FieldType trigger_field_type =
      trigger_autofill_field ? trigger_autofill_field->Type().GetStorableType()
                             : UNKNOWN_TYPE;

  // Given the current `trigger_field` and previous suggestions shown (if any),
  // compute what type of address suggestions granularity shall be currently
  // offered.
  SuggestionType current_suggestion_type = [&] {
    if (!IsAddressType(trigger_field_type)) {
      // If Autofill was triggered from a field that is not classified as
      // address, `current_suggestion_type is irrelevant and we just use
      // `SuggestionType::kAddressEntry` as a placeholder.
      return SuggestionType::kAddressEntry;
    }
    if (trigger_field.is_autofilled() &&
        trigger_autofill_field->autofilled_type() ==
            trigger_autofill_field->Type().GetStorableType() &&
        base::FeatureList::IsEnabled(features::kAutofillAddressFieldSwapping)) {
      // If the user triggers suggestions on an autofilled field filled
      // traditionally with data matching its classification, field-by-field
      // filling suggestions should be shown so that the user could easily
      // correct values to something present in different stored addresses.
      return SuggestionType::kAddressFieldByFieldFilling;
    }
    switch (external_delegate_->GetLastAcceptedSuggestionToFillForSection(
        trigger_autofill_field->section())) {
      case SuggestionType::kAddressEntry:
      case SuggestionType::kFillEverythingFromAddressProfile:
        return SuggestionType::kAddressEntry;
      case SuggestionType::kFillFullAddress:
      case SuggestionType::kFillFullName:
      case SuggestionType::kFillFullPhoneNumber:
      case SuggestionType::kFillFullEmail:
        switch (GroupTypeOfFieldType(trigger_field_type)) {
          case FieldTypeGroup::kName:
            return SuggestionType::kFillFullName;
          case FieldTypeGroup::kEmail:
            return SuggestionType::kFillFullEmail;
          case FieldTypeGroup::kCompany:
          case FieldTypeGroup::kAddress:
            return SuggestionType::kFillFullAddress;
          case FieldTypeGroup::kPhone:
            return SuggestionType::kFillFullPhoneNumber;
          case FieldTypeGroup::kCreditCard:
          case FieldTypeGroup::kStandaloneCvcField:
          case FieldTypeGroup::kPasswordField:
          case FieldTypeGroup::kTransaction:
          case FieldTypeGroup::kUsernameField:
          case FieldTypeGroup::kUnfillable:
          case FieldTypeGroup::kIban:
          case FieldTypeGroup::kNoGroup:
            // Since we early return on non-address types.
            NOTREACHED_NORETURN();
        }
        NOTREACHED_NORETURN();
      case SuggestionType::kAddressFieldByFieldFilling:
        return SuggestionType::kAddressFieldByFieldFilling;
      default:
        // `last_suggestion_type` is only one of the address filling suggestion
        // types, therefore no other type should be passed to this function.
        NOTREACHED_NORETURN();
    }
  }();

  FieldTypeSet field_types = [&] {
    if (!IsAddressType(trigger_field_type)) {
      // Since Autofill was triggered from a field that is not classified as
      // address, we consider the `field_types` (i.e, the fields found in the
      // "form") to be a single unclassified field. Note that in this flow it is
      // not used and only holds semantic value.
      // TODO(crbug.com/339543182): Is this special case reasonable? Shouldn't
      // we pass the fields that are available?
      return FieldTypeSet{UNKNOWN_TYPE};
    }
    // If the FormData and FormStructure do not have the same size, we assume
    // as a fallback that all fields are fillable.
    base::flat_map<FieldGlobalId, FieldFillingSkipReason> skip_reasons;
    size_t num_fields = form_structure ? form_structure->field_count() : 0;
    if (form_structure && form.fields.size() == num_fields) {
      skip_reasons = form_filler_->GetFieldFillingSkipReasons(
          form.fields, *form_structure, *trigger_autofill_field,
          GetTargetFieldsForAddressFillingSuggestionType(
              current_suggestion_type, trigger_field_type),
          /*type_groups_originally_filled=*/std::nullopt,
          FillingProduct::kAddress,
          /*skip_unrecognized_autocomplete_fields=*/trigger_source !=
              AutofillSuggestionTriggerSource::kManualFallbackAddress,
          /*is_refill=*/false, /*is_expired_credit_card=*/false);
    }
    FieldTypeSet field_types;
    for (size_t i = 0; i < num_fields; ++i) {
      const AutofillField* autofill_field = form_structure->field(i);
      auto it = skip_reasons.find(autofill_field->global_id());
      if (it == skip_reasons.end() ||
          it->second == FieldFillingSkipReason::kNotSkipped) {
        field_types.insert(autofill_field->Type().GetStorableType());
      }
    }
    return field_types;
  }();

  return address_suggestion_generator_->GetSuggestionsForProfiles(
      field_types, trigger_field, trigger_field_type, current_suggestion_type,
      trigger_source);
}

std::vector<Suggestion> BrowserAutofillManager::GetCreditCardSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource trigger_source) const {
  credit_card_form_event_logger_->OnDidPollSuggestions(
      trigger_field, signin_state_for_metrics_);

  std::vector<Suggestion> suggestions;
  bool with_offer = false;
  bool with_cvc = false;
  bool is_virtual_card_standalone_cvc_field = false;
  autofill_metrics::CardMetadataLoggingContext context;

  // If credit card number field is not empty and is not autofilled, do not
  // offer suggestions for expiration type field.
  auto ShouldOfferSuggestionsForExpirationTypeField = [&] {
    FormStructure* cached_form = FindCachedFormById(form.global_id());
    if (!cached_form) {
      return true;
    }
    for (const FormFieldData& field : form.fields) {
      AutofillField* autofill_field =
          cached_form->GetFieldById(field.global_id());
      if (autofill_field && autofill_field->Type().GetStorableType() ==
                                autofill::CREDIT_CARD_NUMBER) {
        return SanitizedFieldIsEmpty(field.value()) || field.is_autofilled();
      }
    }
    return true;
  };

  if (data_util::IsCreditCardExpirationType(trigger_field_type) &&
      !ShouldOfferSuggestionsForExpirationTypeField()) {
    return {};
  }

  if (!IsInAutofillSuggestionsDisabledExperiment()) {
    if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
        !four_digit_combinations_in_dom_.empty()) {
      base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
          virtual_card_guid_to_last_four_map =
              GetVirtualCreditCardsForStandaloneCvcField(
                  trigger_field.origin());
      if (!virtual_card_guid_to_last_four_map.empty()) {
        suggestions =
            payments_suggestion_generator_
                ->GetSuggestionsForVirtualCardStandaloneCvc(
                    trigger_field, context, virtual_card_guid_to_last_four_map);
        is_virtual_card_standalone_cvc_field = true;
      }
    } else {
      suggestions =
          payments_suggestion_generator_->GetSuggestionsForCreditCards(
              trigger_field, trigger_field_type, trigger_source,
              ShouldShowScanCreditCard(form, trigger_field),
              ShouldShowCardsFromAccountOption(form, trigger_field,
                                               trigger_source),
              with_offer, with_cvc, context);
    }
  }

  credit_card_form_event_logger_->OnDidFetchSuggestion(
      suggestions, with_offer, with_cvc, is_virtual_card_standalone_cvc_field,
      std::move(context));
  return suggestions;
}

base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
BrowserAutofillManager::GetVirtualCreditCardsForStandaloneCvcField(
    const url::Origin& origin) const {
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  const std::vector<CreditCard*> cards = client()
                                             .GetPersonalDataManager()
                                             ->payments_data_manager()
                                             .GetCreditCards();
  const std::vector<VirtualCardUsageData*> usage_data =
      client()
          .GetPersonalDataManager()
          ->payments_data_manager()
          .GetVirtualCardUsageData();

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

// TODO(crbug.com/40219607) Eliminate and replace with a listener?
// Should we do the same with all the other BrowserAutofillManager events?
void BrowserAutofillManager::OnBeforeProcessParsedForms() {
  has_parsed_forms_ = true;

  // Record the current sync state to be used for metrics on this page.
  signin_state_for_metrics_ = client()
                                  .GetPersonalDataManager()
                                  ->payments_data_manager()
                                  .GetPaymentsSigninStateForMetrics();

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
  // TODO(crbug.com/41405154): avoid logging developer engagement multiple
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
  // If a form with the same FormGlobalId was previously filled, the structure
  // of the form changed, and there has not been a refill attempt on that form
  // yet, start the process of triggering a refill.
  if (form_filler_->ShouldTriggerRefill(form_structure,
                                        RefillTriggerReason::kFormChanged)) {
    form_filler_->ScheduleRefill(
        form, form_structure,
        {.trigger_source = AutofillTriggerSource::kFormsSeen});
  }
}

void BrowserAutofillManager::UpdateInitialInteractionTimestamp(
    const TimeTicks& interaction_timestamp) {
  if (initial_interaction_timestamp_.is_null() ||
      interaction_timestamp < initial_interaction_timestamp_) {
    initial_interaction_timestamp_ = interaction_timestamp;
  }
}

std::vector<Suggestion>
BrowserAutofillManager::GetAvailableAddressAndCreditCardSuggestions(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    SuggestionsContext& context) {
  if (IsPlusAddressesManuallyTriggered(trigger_source)) {
    return {};
  }

  if (context.should_show_mixed_content_warning) {
    Suggestion warning_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_MIXED_FORM));
    warning_suggestion.type = SuggestionType::kMixedFormMessage;
    return {warning_suggestion};
  }

  if (!context.is_autofill_available ||
      context.do_not_generate_autofill_suggestions) {
    return {};
  }

  std::vector<Suggestion> suggestions;
  if (FillingProductSet::is_one_of(
          context.filling_product,
          {FillingProduct::kCreditCard, FillingProduct::kStandaloneCvc})) {
    FieldType trigger_field_type =
        context.focused_field ? context.focused_field->Type().GetStorableType()
                              : UNKNOWN_TYPE;
    suggestions = GetCreditCardSuggestions(form, field, trigger_field_type,
                                           trigger_source);
  } else if (context.filling_product == FillingProduct::kAddress) {
    // Profile suggestions fill ac=unrecognized fields only when triggered
    // through manual fallbacks. As such, suggestion labels differ depending on
    // the `trigger_source`.
    suggestions = GetProfileSuggestions(form, context.form_structure, field,
                                        context.focused_field, trigger_source);
  }

  // Ablation experiment
  if (context.filling_product == FillingProduct::kAddress ||
      context.filling_product == FillingProduct::kCreditCard) {
    FormTypeForAblationStudy form_type =
        context.filling_product == FillingProduct::kCreditCard
            ? FormTypeForAblationStudy::kPayment
            : FormTypeForAblationStudy::kAddress;
    // If ablation_group is AblationGroup::kDefault or AblationGroup::kControl,
    // no ablation happens in the following.
    AblationGroup ablation_group = client().GetAblationStudy().GetAblationGroup(
        client().GetLastCommittedPrimaryMainFrameURL(), form_type);
    context.ablation_group = ablation_group;
    // Note that we don't set the ablation group if there are no suggestions.
    // In that case we stick to kDefault.
    context.conditional_ablation_group =
        !suggestions.empty() ? ablation_group : AblationGroup::kDefault;

    // In both cases (credit card and address forms), we inform the other event
    // logger also about the ablation.
    // This prevents for example that for an encountered address form we log a
    // sample Autofill.Funnel.ParsedAsType.CreditCard = 0 (which would be
    // recorded by the credit_card_form_event_logger_). For the complementary
    // event logger, the conditional ablation status is logged as kDefault to
    // not imply that data would be filled without ablation.
    if (context.filling_product == FillingProduct::kCreditCard) {
      credit_card_form_event_logger_->SetAblationStatus(
          context.ablation_group, context.conditional_ablation_group);
      address_form_event_logger_->SetAblationStatus(context.ablation_group,
                                                    AblationGroup::kDefault);
    } else if (context.filling_product == FillingProduct::kAddress) {
      address_form_event_logger_->SetAblationStatus(
          context.ablation_group, context.conditional_ablation_group);
      credit_card_form_event_logger_->SetAblationStatus(
          context.ablation_group, AblationGroup::kDefault);
    }

    if (!suggestions.empty() && ablation_group == AblationGroup::kAblation) {
      // Logic for disabling/ablating autofill.
      context.suppress_reason = SuppressReason::kAblation;
      return {};
    }
  }
  if (suggestions.empty() ||
      context.filling_product != FillingProduct::kCreditCard) {
    return suggestions;
  }
  // Don't provide credit card suggestions for non-secure pages, but do
  // provide them for secure pages with passive mixed content (see
  // implementation of IsContextSecure).
  if (!context.is_context_secure) {
    // Replace the suggestion content with a warning message explaining why
    // Autofill is disabled for a website. The string is different if the
    // credit card autofill HTTP warning experiment is enabled.
    Suggestion warning_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
    warning_suggestion.type =
        SuggestionType::kInsecureContextPaymentDisabledMessage;
    suggestions.assign(1, warning_suggestion);
  }
  return suggestions;
}

autofill_metrics::FormEventLoggerBase*
BrowserAutofillManager::GetEventFormLogger(const AutofillField& field) const {
  if (field.ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Ignore ac=unrecognized fields in key metrics.
    return nullptr;
  }
  switch (FieldTypeGroupToFormType(field.Type().group())) {
    case FormType::kAddressForm:
      return address_form_event_logger_.get();
    case FormType::kCreditCardForm:
    case FormType::kStandaloneCvcForm:
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
    std::optional<AlternativeStateNameMap::CanonicalStateName>
        canonical_state_name_from_profile =
            profile.GetAddress().GetCanonicalizedStateName();

    if (!canonical_state_name_from_profile) {
      continue;
    }

    const std::u16string& country_code =
        profile.GetInfo(AutofillType(HtmlFieldType::kCountryCode), app_locale_);

    for (auto& field : *form_structure) {
      if (field->state_is_a_matching_type()) {
        continue;
      }

      std::optional<AlternativeStateNameMap::CanonicalStateName>
          canonical_state_name_from_text =
              AlternativeStateNameMap::GetCanonicalStateName(
                  base::UTF16ToUTF8(country_code), field->value());

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
  if (!has_parsed_forms() && !used_web_otp) {
    return;
  }

  if (has_observed_phone_number_field()) {
    phone_collection_metric_state_ |= phone_collection_metric::kPhoneCollected;
  }
  if (has_observed_one_time_code_field()) {
    phone_collection_metric_state_ |= phone_collection_metric::kOTCUsed;
  }
  if (used_web_otp) {
    phone_collection_metric_state_ |= phone_collection_metric::kWebOTPUsed;
  }

  ukm::UkmRecorder* recorder = client().GetUkmRecorder();
  ukm::SourceId source_id = client().GetUkmSourceId();
  AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
      recorder, source_id, phone_collection_metric_state_);

  base::UmaHistogramEnumeration(
      "Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
      static_cast<PhoneCollectionMetricState>(phone_collection_metric_state_));
}

void BrowserAutofillManager::ProcessFieldLogEventsInForm(
    const FormStructure& form_structure) {
  // TODO(crbug.com/40225658): Log metrics if at least one field in the form was
  // classified as a certain type.
  LogEventCountsUMAMetric(form_structure);

  // ShouldUploadUkm reduces the UKM load by ignoring e.g. search boxes at best
  // effort.
  bool should_upload_ukm = ShouldRecordUkm() && ShouldUploadUkm(form_structure);

  for (const auto& autofill_field : form_structure) {
    if (should_upload_ukm) {
      form_interactions_ukm_logger()->LogAutofillFieldInfoAtFormRemove(
          form_structure, *autofill_field,
          AutocompleteStateForSubmittedField(*autofill_field));
    }

    // Clear log events.
    // Not conditions on kAutofillLogUKMEventsWithSamplingOnSession because
    // there may be other reasons to log events.
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
    if (base::ToLowerASCII((*it)->placeholder()).find(u"search") !=
            std::string::npos ||
        base::ToLowerASCII((*it)->name()).find(u"search") !=
            std::string::npos ||
        base::ToLowerASCII((*it)->label()).find(u"search") !=
            std::string::npos ||
        base::ToLowerASCII((*it)->aria_label()).find(u"search") !=
            std::string::npos) {
      return false;
    }
  }

  return true;
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
        NOTREACHED_IN_MIGRATION();
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
    case FormType::kStandaloneCvcForm:
      credit_card_form_event_logger_->SetFastCheckoutRunId(run_id);
      break;
    case FormType::kPasswordForm:
    case FormType::kUnknownFormType:
      // FastCheckout only supports address and credit card forms.
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace autofill
