// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/browser_autofill_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
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
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
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
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
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
         !FieldTypeGroupSet{FieldTypeGroup::kCreditCard,
                            FieldTypeGroup::kStandaloneCvcField}
              .contains(autofill_field->Type().group());
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
        autofill_metrics::GetFormTypesForLogging(form_structure),
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
  for (const FormFieldData& field : form.fields()) {
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
    case FillingProduct::kPredictionImprovements:
    case FillingProduct::kCompose:
    case FillingProduct::kPassword:
    case FillingProduct::kCreditCard:
    case FillingProduct::kAddress:
    case FillingProduct::kNone:
      return false;
  }
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
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreateNewPlusAddressInline:
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
    case SuggestionType::kPlusAddressError:
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
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kRetrievePredictionImprovements:
    case SuggestionType::kPredictionImprovementsLoadingState:
    case SuggestionType::kFillPredictionImprovements:
    case SuggestionType::kPredictionImprovementsFeedback:
    case SuggestionType::kPredictionImprovementsError:
    case SuggestionType::kEditPredictionImprovementsInformation:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return FillDataType::kUndefined;
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

    auto autocomplete_state =
        AutofillMetrics::AutocompleteStateForSubmittedField(*field);
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
  if (WillFillCreditCardNumberOrCvc(
          form.fields(), form_structure.fields(), autofill_field,
          /*card_has_cvc=*/!credit_card.cvc().empty())) {
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
    case AutofillSuggestionTriggerSource::kPredictionImprovements:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
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
    // TODO(crbug.com/41484171): Move to payments_suggestion_generator.cc.
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
    // TODO(crbug.com/41484171): Move to address_suggestion_generator.cc.
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
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kRetrievePredictionImprovements:
    case SuggestionType::kPredictionImprovementsLoadingState:
    case SuggestionType::kFillPredictionImprovements:
    case SuggestionType::kPredictionImprovementsFeedback:
    case SuggestionType::kPredictionImprovementsError:
    case SuggestionType::kEditPredictionImprovementsInformation:
      NOTREACHED();
  }
  NOTREACHED();
}

bool ShouldOfferSingleFieldFormFill(
    const FormFieldData& field,
    const AutofillField* autofill_field,
    AutofillSuggestionTriggerSource trigger_source,
    SuppressReason suppress_reason) {
  if (trigger_source ==
      AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick) {
    return false;
  }
  // Do not offer single field form fill suggestions for credit card number,
  // cvc, and expiration date related fields. Standalone cvc fields (used to
  // re-authenticate the use of a credit card the website has on file) will be
  // handled separately because those have the field type
  // CREDIT_CARD_STANDALONE_VERIFICATION_CODE.
  FieldType type =
      autofill_field ? autofill_field->Type().GetStorableType() : UNKNOWN_TYPE;
  if (data_util::IsCreditCardExpirationType(type) ||
      type == CREDIT_CARD_VERIFICATION_CODE || type == CREDIT_CARD_NUMBER) {
    return false;
  }

  // Do not offer single field form fill suggestions if popups are suppressed
  // due to an unrecognized autocomplete attribute. Note that in the context
  // of Autofill, the popup for credit card related fields is not getting
  // suppressed due to an unrecognized autocomplete attribute.
  // TODO(crbug.com/40853053): Revisit here to see whether we should offer
  // IBAN filling for fields with unrecognized autocomplete attribute
  if (suppress_reason == SuppressReason::kAutocompleteUnrecognized) {
    return false;
  }

  // Therefore, we check the attribute explicitly.
  if (autofill_field &&
      autofill_field->Type().html_type() == HtmlFieldType::kUnrecognized) {
    return false;
  }

  // Finally, check that the scheme is secure.
  return suppress_reason != SuppressReason::kInsecureForm;
}

// Returns whether suggestions should be suppressed for the given reason.
bool ShouldSuppressSuggestions(SuppressReason suppress_reason,
                               LogManager* log_manager) {
  switch (suppress_reason) {
    case SuppressReason::kNotSuppressed:
      return false;

    case SuppressReason::kAblation:
      LOG_AF(log_manager) << LoggingScope::kFilling
                          << LogMessage::kSuggestionSuppressed
                          << " Reason: Ablation experiment";
      return true;

    case SuppressReason::kInsecureForm:
      LOG_AF(log_manager) << LoggingScope::kFilling
                          << LogMessage::kSuggestionSuppressed
                          << " Reason: Insecure form";
      return true;
    case SuppressReason::kAutocompleteOff:
      LOG_AF(log_manager) << LoggingScope::kFilling
                          << LogMessage::kSuggestionSuppressed
                          << " Reason: autocomplete=off";
      return true;
    case SuppressReason::kAutocompleteUnrecognized:
      LOG_AF(log_manager) << LoggingScope::kFilling
                          << LogMessage::kSuggestionSuppressed
                          << " Reason: autocomplete=unrecognized";
      return true;
  }
}

void MaybeAddAddressSuggestionStrikes(AutofillClient& client,
                                      const FormStructure& form) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  for (const auto& field : form) {
    if (field->autocomplete_attribute() == "off" &&
        field->did_trigger_suggestions() && !field->is_autofilled() &&
        !field->previously_autofilled()) {
      // This means that the user triggered suggestions and ignored them. In
      // that case we record a strike for this specific field. Multiple strikes
      // will lead to automatic address suggestions to be suppressed.
      // Currently, this is only done for autocomplete=off fields.
      client.GetPersonalDataManager()
          ->address_data_manager()
          .AddStrikeToBlockAddressSuggestions(form.form_signature(),
                                              field->GetFieldSignature(),
                                              form.source_url());
    }
  }
#endif
}

// Returns the plus address to be used as the email override on profile
// suggestions matching the user's gaia email.
std::optional<std::string> GetPlusAddressOverride(
    const AutofillPlusAddressDelegate* delegate,
    const std::vector<std::string>& plus_addresses) {
  if (!delegate || !delegate->IsPlusAddressFullFormFillingEnabled() ||
      plus_addresses.empty()) {
    return std::nullopt;
  }
  // Except in very rare cases where affiliation data changes, `plus_addresses`
  // should contain exactly one item.
  return plus_addresses[0];
}

}  // namespace

BrowserAutofillManager::MetricsState::MetricsState(
    BrowserAutofillManager* owner)
    : address_form_event_logger(owner->driver().IsInAnyMainFrame(),
                                owner->form_interactions_ukm_logger(),
                                &owner->client()),
      credit_card_form_event_logger(owner->driver().IsInAnyMainFrame(),
                                    owner->form_interactions_ukm_logger(),
                                    owner->client().GetPersonalDataManager(),
                                    &owner->client()) {}

BrowserAutofillManager::MetricsState::~MetricsState() {
  credit_card_form_event_logger.OnDestroyed();
  address_form_event_logger.OnDestroyed();
}

BrowserAutofillManager::BrowserAutofillManager(AutofillDriver* driver,
                                               const std::string& app_locale)
    : AutofillManager(driver), app_locale_(app_locale) {}

BrowserAutofillManager::~BrowserAutofillManager() {
  if (metrics_->has_parsed_forms) {
    base::UmaHistogramBoolean(
        "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
        metrics_->has_observed_phone_number_field);
    base::UmaHistogramBoolean("Autofill.WebOTP.OneTimeCode.FromAutocomplete",
                              metrics_->has_observed_one_time_code_field);
  }

  // Process log events and record into UKM when the FormStructure is destroyed.
  for (const auto& [form_id, form_structure] : form_structures()) {
    ProcessFieldLogEventsInForm(*form_structure);
  }
  FlushPendingLogQualityAndVotesUploadCallbacks();

  single_field_form_fill_router_->CancelPendingQueries();
}

base::WeakPtr<AutofillManager> BrowserAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CreditCardAccessManager& BrowserAutofillManager::GetCreditCardAccessManager() {
  if (!credit_card_access_manager_) {
    credit_card_access_manager_ = std::make_unique<CreditCardAccessManager>(
        this, &metrics_->credit_card_form_event_logger);
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
    const FormFieldData& field) {
  if (!client().GetPaymentsAutofillClient()->HasCreditCardScanFeature() ||
      !IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field) {
    return false;
  }

  bool is_card_number_field =
      autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER &&
      base::ContainsOnlyChars(StripCardNumberSeparators(field.value()),
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

  autofill_metrics::SuggestionRankingContext ranking_context;
  auto cards = GetCreditCardSuggestions(
      form, field_data, field_type,
      AutofillSuggestionTriggerSource::kShowCardsFromAccount, ranking_context);
  DCHECK(!cards.empty());
  external_delegate_->OnSuggestionsReturned(field_data.global_id(), cards,
                                            std::move(ranking_context));
}

bool BrowserAutofillManager::ShouldParseForms() {
  bool autofill_enabled = IsAutofillEnabled();
  // If autofill is disabled but the password manager is enabled, we still
  // need to parse the forms and query the server as the password manager
  // depends on server classifications.
  bool password_manager_enabled = client().IsPasswordManagerEnabled();
  metrics_->signin_state_for_metrics =
      client().GetPersonalDataManager()
          ? client()
                .GetPersonalDataManager()
                ->payments_data_manager()
                .GetPaymentsSigninStateForMetrics()
          : AutofillMetrics::PaymentsSigninState::kUnknown;
  if (!metrics_->has_logged_autofill_enabled) {
    autofill_metrics::LogIsAutofillEnabledAtPageLoad(
        autofill_enabled, metrics_->signin_state_for_metrics);
    autofill_metrics::LogIsAutofillProfileEnabledAtPageLoad(
        IsAutofillProfileEnabled(), metrics_->signin_state_for_metrics);
    if (!IsAutofillProfileEnabled()) {
      autofill_metrics::LogAutofillProfileDisabledReasonAtPageLoad(
          CHECK_DEREF(client().GetPrefs()));
    }
    autofill_metrics::LogIsAutofillPaymentMethodsEnabledAtPageLoad(
        IsAutofillPaymentMethodsEnabled(), metrics_->signin_state_for_metrics);
    if (!IsAutofillPaymentMethodsEnabled()) {
      autofill_metrics::LogAutofillPaymentMethodsDisabledReasonAtPageLoad(
          CHECK_DEREF(client().GetPrefs()));
    }
    metrics_->has_logged_autofill_enabled = true;
  }

  // Enable the parsing also for the password manager, so that we fetch server
  // classifications if the password manager is enabled but autofill is
  // disabled.
  return autofill_enabled || password_manager_enabled;
}

void BrowserAutofillManager::OnFormSubmittedImpl(const FormData& form,
                                                 bool known_success,
                                                 SubmissionSource source) {
  if (source == mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL) {
    // Autofill mostly ignores such submissions because we don't consider them
    // strong enough indicators and want to avoid false positives. Filling a
    // country field could sometimes result in fields being deleted or added,
    // which is definitely not a submission.
    return;
  }
  base::UmaHistogramEnumeration("Autofill.FormSubmission.PerProfileType",
                                client().GetProfileType());
  const base::TimeTicks form_submitted_timestamp = base::TimeTicks::Now();
  LOG_AF(log_manager())
      << LoggingScope::kSubmission << LogMessage::kFormSubmissionDetected
      << Br{} << "known_success: " << known_success << Br{} << "timestamp: "
      << form_submitted_timestamp.since_origin().InMilliseconds() << Br{}
      << "source: " << SubmissionSourceToString(source) << Br{} << form;

  // Always let the value patterns metric upload data.
  LogValuePatternsMetric(form);

  std::unique_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  CHECK(!client().IsOffTheRecord() || !submitted_form);
  // Try to import the `form` into user annotations via the
  // `AutofillPredictionImprovementsDelegate`. `MaybeImportFromSubmittedForm()`
  // will be called if the import was not successful.
  if (AutofillPredictionImprovementsDelegate* delegate =
          client().GetAutofillPredictionImprovementsDelegate();
      delegate && delegate->IsUserEligible() && submitted_form) {
    // Only upload server statistics and UMA metrics if at least some local data
    // is available to use as a baseline.
    std::vector<const AutofillProfile*> profiles =
        client().GetPersonalDataManager()->address_data_manager().GetProfiles(
            AddressDataManager::ProfileOrder::kHighestFrecencyDesc);
    std::vector<CreditCard*> credit_cards = client()
                                                .GetPersonalDataManager()
                                                ->payments_data_manager()
                                                .GetCreditCards();
    // Shrink the maximum size of the vectors for performance reasons.
    profiles.resize(
        std::min(profiles.size(), kMaxDataConsideredForPossibleTypes));
    credit_cards.resize(
        std::min(credit_cards.size(), kMaxDataConsideredForPossibleTypes));

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

    // Determine |ADDRESS_HOME_STATE| as a possible types for the fields in the
    // |form_structure| with the help of |AlternativeStateNameMap|.
    // |AlternativeStateNameMap| can only be accessed on the main UI thread.
    PreProcessStateMatchingTypes(copied_profiles, submitted_form.get());

    DeterminePossibleFieldTypesForUpload(
        std::move(copied_profiles), std::move(copied_credit_cards),
        last_unlocked_credit_card_cvc_, app_locale_, submitted_form.get());

    delegate->MaybeImportForm(
        std::move(submitted_form),
        base::BindOnce(
            &BrowserAutofillManager::OnUserAnnotationsMaybeImportableFormFound,
            weak_ptr_factory_.GetWeakPtr(), form, source,
            form_submitted_timestamp));
  } else {
    // TODO(crbug.com/40100455): Refactor this:
    // - It's impossible to know that OnUserAnnotationsMaybeImportableFormFound
    //   calls MaybeImportFromSubmittedForm and OnFormSubmittedAfterImport.
    //   These calls should not be repeated in different part of the code base.
    // - If possible, OnFormSubmittedAfterImport should only be referenced in
    //   OnFormSubmittedImpl and to build an opening and closing bracket around
    //   some executed code.
    // - OnUserAnnotationsMaybeImportableFormFound is a bad name because it
    //   creates the assumption that it's called if and only if a
    //   "MaybeImportableForm" was found.
    MaybeImportFromSubmittedForm(
        form, submitted_form.get(),
        /*attempt_to_import_into_form_data_importer=*/true);
    OnFormSubmittedAfterImport(form, std::move(submitted_form), source,
                               form_submitted_timestamp);
  }
}

void BrowserAutofillManager::OnFormSubmittedAfterImport(
    const FormData& form,
    std::unique_ptr<FormStructure> submitted_form,
    SubmissionSource source,
    base::TimeTicks form_submitted_timestamp) {
  if (!submitted_form) {
    return;
  }
  submitted_form->set_submission_source(source);
  if (submitted_form->IsAutofillable()) {
    // Associate the form signatures of recently submitted address/credit card
    // forms to `submitted_form`, if it is an address/credit card form itself.
    // This information is attached to the vote.
    if (std::optional<FormStructure::FormAssociations> associations =
            client().GetFormDataImporter()->GetFormAssociations(
                submitted_form->form_signature())) {
      submitted_form->set_form_associations(*associations);
    }
  }

  LogSubmissionMetrics(submitted_form.get(), form_submitted_timestamp);

  ProfileTokenQuality::SaveObservationsForFilledFormForAllSubmittedProfiles(
      *submitted_form, form, *client().GetPersonalDataManager());

  MaybeAddAddressSuggestionStrikes(client(), *submitted_form);
  MaybeStartVoteUploadProcess(std::move(submitted_form),
                              /*observed_submission=*/true);
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

  base::OnceClosure call_after_determine_field_types = base::BindOnce(
      &BrowserAutofillManager::OnSubmissionFieldTypesDetermined,
      weak_ptr_factory_.GetWeakPtr(), std::move(form_structure),
      metrics_->initial_interaction_timestamp, base::TimeTicks::Now(),
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

  // TODO(crbug.com/368306576): Bound the size of `copied_profiles` and
  // `copied_credit_cards` by `kMaxDataConsideredForPossibleTypes` and make
  // the call to DeterminePossibleFieldTypesForUpload synchronous.
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
  pending_form_data_ = std::make_optional<FormData>(form);
}

void BrowserAutofillManager::ProcessPendingFormForUpload() {
  if (!pending_form_data_) {
    return;
  }

  // We get the FormStructure corresponding to `pending_form_data_`, used in the
  // upload process. As a result, `pending_form_data_`'s field values are the
  // current values. `pending_form_data_` is reset.
  std::unique_ptr<FormStructure> upload_form =
      ValidateSubmittedForm(*pending_form_data_);
  pending_form_data_.reset();
  if (!upload_form) {
    return;
  }

  MaybeStartVoteUploadProcess(std::move(upload_form),
                              /*observed_submission=*/false);
}

void BrowserAutofillManager::OnUserAnnotationsMaybeImportableFormFound(
    const FormData& form,
    SubmissionSource source,
    base::TimeTicks form_submitted_timestamp,
    std::unique_ptr<FormStructure> submitted_form,
    std::vector<optimization_guide::proto::UserAnnotationsEntry>
        to_be_upserted_entries,
    base::OnceCallback<void(bool prompt_was_accepted)>
        prompt_acceptance_callback) {
  const bool should_show_prediction_improvements_bubble =
      !to_be_upserted_entries.empty();
  if (should_show_prediction_improvements_bubble) {
    client().ShowSaveAutofillPredictionImprovementsBubble(
        std::move(to_be_upserted_entries),
        std::move(prompt_acceptance_callback));
  }
  MaybeImportFromSubmittedForm(form, submitted_form.get(),
                               /*attempt_to_import_into_form_data_importer=*/
                               !should_show_prediction_improvements_bubble);
  OnFormSubmittedAfterImport(form, std::move(submitted_form), source,
                             form_submitted_timestamp);
}

void BrowserAutofillManager::MaybeImportFromSubmittedForm(
    const FormData& form,
    const FormStructure* const form_structure,
    bool attempt_to_import_into_form_data_importer) {
  if (!form_structure) {
    // We always give Autocomplete a chance to save the data.
    // TODO(crbug.com/40276862): Verify frequency of plus address (or the other
    // type(s) checked for below, for that matter) slipping through in this code
    // path.
    single_field_form_fill_router_->OnWillSubmitForm(
        form, nullptr, client().IsAutocompleteEnabled());
    return;
  }
  if (attempt_to_import_into_form_data_importer &&
      form_structure->IsAutofillable()) {
    // Update Personal Data with the form's submitted data.
    client().GetFormDataImporter()->ImportAndProcessFormData(
        *form_structure, IsAutofillProfileEnabled(),
        IsAutofillPaymentMethodsEnabled());
  }

  AutofillPlusAddressDelegate* plus_address_delegate =
      client().GetPlusAddressDelegate();

  std::vector<FormFieldData> fields_for_autocomplete;
  fields_for_autocomplete.reserve(form.fields().size());
  for (const auto& autofill_field : *form_structure) {
    fields_for_autocomplete.push_back(*autofill_field);
    if (autofill_field->Type().GetStorableType() ==
        CREDIT_CARD_VERIFICATION_CODE) {
      // However, if Autofill has recognized a field as CVC, that shouldn't be
      // saved.
      fields_for_autocomplete.back().set_should_autocomplete(false);
    }
    if (plus_address_delegate &&
        plus_address_delegate->IsPlusAddress(
            base::UTF16ToUTF8(autofill_field->value_for_import()))) {
      // Similarly to CVC, any plus addresses needn't be saved to autocomplete.
      // Note that the feature is experimental, and `plus_address_delegate`
      // will be null if the feature is not enabled (it's disabled by default).
      fields_for_autocomplete.back().set_should_autocomplete(false);
    }
  }

  // TODO crbug.com/40100455 - Eliminate `form_for_autocomplete`.
  FormData form_for_autocomplete = form_structure->ToFormData();
  form_for_autocomplete.set_fields(std::move(fields_for_autocomplete));
  single_field_form_fill_router_->OnWillSubmitForm(
      form_for_autocomplete, form_structure, client().IsAutocompleteEnabled());
}

void BrowserAutofillManager::LogSubmissionMetrics(
    const FormStructure* submitted_form,
    const base::TimeTicks& form_submitted_timestamp) {
  metrics_->form_submitted_timestamp = form_submitted_timestamp;

  // Log metrics about the autocomplete attribute usage in the submitted form.
  LogAutocompletePredictionCollisionTypeMetrics(*submitted_form);

  // Log interaction time metrics for the ablation study.
  if (!metrics_->initial_interaction_timestamp.is_null()) {
    base::TimeDelta time_from_interaction_to_submission =
        base::TimeTicks::Now() - metrics_->initial_interaction_timestamp;
    DenseSet<FormType> form_types = submitted_form->GetFormTypes();
    bool card_form = base::Contains(form_types, FormType::kCreditCardForm);
    bool address_form = base::Contains(form_types, FormType::kAddressForm);
    if (card_form) {
      metrics_->credit_card_form_event_logger
          .SetTimeFromInteractionToSubmission(
              time_from_interaction_to_submission);
    }
    if (address_form) {
      metrics_->address_form_event_logger.SetTimeFromInteractionToSubmission(
          time_from_interaction_to_submission);
    }
  }

  if (IsAutofillProfileEnabled()) {
    metrics_->address_form_event_logger.OnWillSubmitForm(*submitted_form);
  }
  if (IsAutofillPaymentMethodsEnabled()) {
    metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
        metrics_->signin_state_for_metrics);
    metrics_->credit_card_form_event_logger.OnWillSubmitForm(*submitted_form);
  }

  if (IsAutofillProfileEnabled()) {
    metrics_->address_form_event_logger.OnFormSubmitted(*submitted_form);
  }
  if (IsAutofillPaymentMethodsEnabled()) {
    metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
        metrics_->signin_state_for_metrics);
    metrics_->credit_card_form_event_logger.OnFormSubmitted(*submitted_form);
    if (touch_to_fill_delegate_) {
      touch_to_fill_delegate_->LogMetricsAfterSubmission(*submitted_form);
    }
  }
}

void BrowserAutofillManager::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const base::TimeTicks timestamp) {
  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }

  // Log events when user edits the field.
  // If the user types into the same field multiple times, repeated
  // TypingFieldLogEvents are coalesced.
  autofill_field->AppendLogEventIfNotRepeated(TypingFieldLogEvent{
      .has_value_after_typing = ToOptionalBoolean(!field.value().empty())});

  UpdatePendingForm(form);

  if (!metrics_->user_did_type || autofill_field->is_autofilled()) {
    metrics_->user_did_type = true;
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
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    AutofillSuggestionTriggerSource trigger_source) {
  SuggestionsContext context;

  // When Compose suggestions or manual fallback for plus addresses are
  // requested, there is no need to load Autofill suggestions.
  if (IsTriggerSourceOnlyRelevantForCompose(trigger_source) ||
      IsPlusAddressesManuallyTriggered(trigger_source)) {
    context.do_not_generate_autofill_suggestions = true;
    return context;
  }

  UpdateLoggersReadinessData();

  // Don't send suggestions or track forms that should not be parsed.
  const bool got_autofillable_form =
      form_structure && form_structure->ShouldBeParsed() && autofill_field;

  if (!ShouldShowSuggestionsForAutocompleteUnrecognizedFields(trigger_source) &&
      got_autofillable_form &&
      autofill_field->ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Pre-`AutofillPredictionsForAutocompleteUnrecognized`, autocomplete
    // suggestions were shown if all types of the form were suppressed or
    // unknown. If at least a single field had predictions (and the form was
    // thus considered autofillable), autocomplete suggestions were suppressed
    // for fields with a suppressed prediction.
    // To retain this behavior, the `suppress_reason` is only set if the form
    // contains a field that triggers (non-fallback) suggestions.
    // By not setting it, the autocomplete suggestion logic downstream is
    // triggered, since no Autofill `suggestions` are available.
    if (!std::ranges::all_of(*form_structure, [](const auto& field) {
          return field->ShouldSuppressSuggestionsAndFillingByDefault() ||
                 field->Type().GetStorableType() == UNKNOWN_TYPE;
        })) {
      context.suppress_reason = SuppressReason::kAutocompleteUnrecognized;
    }
    context.do_not_generate_autofill_suggestions = true;
    return context;
  }
  if (got_autofillable_form) {
    auto* logger = GetEventFormLogger(*autofill_field);
    if (logger) {
      if (logger == &metrics_->credit_card_form_event_logger) {
        metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
            metrics_->signin_state_for_metrics);
      }
      logger->OnDidInteractWithAutofillableForm(*form_structure);
    }
  }

  context.filling_product = GetPreferredSuggestionFillingProduct(
      got_autofillable_form ? autofill_field->Type().GetStorableType()
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

  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  // We cannot early-return here because GetCachedFormAndField() yields nullptr
  // even if there it finds a FormStructure but its `autofill_count()` is 0. In
  // such cases, we still need to offer Autocomplete. Therefore, the code below,
  // including called functions, must handle `form_structure == nullptr` and
  // `autofill_field == nullptr`.
  std::ignore =
      GetCachedFormAndField(form, field, &form_structure, &autofill_field);

  if (form_structure) {
    AutofillMetrics::LogParsedFormUntilInteractionTiming(
        base::TimeTicks::Now() - form_structure->form_parsed_timestamp());
  }

  if (autofill_field) {
    // TODO(crbug.com/349982907): Until the linked bug is fixed, Chrome on iOS
    // does not forward focus events. The OnAskForValuesToFillImpl() call
    // indicates that a field was focused on iOS. On desktop it's not capturing
    // all focus events (neglecting if the user presses the tab key or a field
    // acquires focus on page load). Therefore, this is a temporary workaround
    // that should be deleted with crbug.com/349982907.
    autofill_field->set_was_focused(true);
  }

  // Once the user triggers autofill from the context menu, this event is
  // recorded, because the IPH configuration limits how many times the IPH can
  // be shown.
  if (IsAutofillManuallyTriggered(trigger_source)) {
    client().NotifyAutofillManualFallbackUsed();
  }

  external_delegate_->SetCurrentDataListValues(field.datalist_options());
  external_delegate_->OnQuery(form, field, caret_bounds, trigger_source);

  SuggestionsContext context = BuildSuggestionsContext(
      form, form_structure, field, autofill_field, trigger_source);

  OnGenerateSuggestionsCallback callback = base::BindOnce(
      &BrowserAutofillManager::OnGenerateSuggestionsComplete,
      weak_ptr_factory_.GetWeakPtr(), form, field, trigger_source, context);

  // Check via the `AutofillPredictionImprovementsDelegate` if there's data
  // stored in user annotations.
  // IMPORTANT NOTE: If there's no data stored in user annotations,
  // `GenerateSuggestionsAndMaybeShowUI()` will be called and Autofill's regular
  // flow will continue.
  if (AutofillPredictionImprovementsDelegate* delegate =
          client().GetAutofillPredictionImprovementsDelegate();
      delegate && form_structure && autofill_field &&
      delegate->IsFormAndFieldEligible(*form_structure, *autofill_field)) {
    delegate->HasDataStored(base::BindOnce(
        &BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase1,
        weak_ptr_factory_.GetWeakPtr(), form, field, trigger_source,
        base::OwnedRef(context), std::move(callback)));
    return;
  }

  // IMPORTANT NOTE: DON'T ADD CODE HERE, but in
  // `GenerateSuggestionsAndMaybeShowUIPhase1()` instead.

  // If user annotations wasn't checked for readiness above, synchronously move
  // on with generating suggestions and maybe showing the UI.
  GenerateSuggestionsAndMaybeShowUIPhase1(
      form, field, trigger_source, context, std::move(callback),
      AutofillPredictionImprovementsDelegate::HasData(false));
}

void BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase1(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    SuggestionsContext& context,
    OnGenerateSuggestionsCallback callback,
    AutofillPredictionImprovementsDelegate::HasData
        has_prediction_improvements_data) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  // Note that this function cannot exit early in case GetCachedFormAndField()
  // yields nullptrs for form_structure and autofill_field. This happens in case
  // autofill_count() returns 0 (i.e. the number of autofillable fields is 0).
  // Even if autofill cannot fill the form, Autocomplete gets a chance to fill
  // the form. Therefore:
  // * the following code needs to be executed (autocomplete is handled further
  //   down in the code path)
  // * the following code needs to gracefully deal with the situation that
  //   form_structure and autofill_field are null.
  std::ignore =
      GetCachedFormAndField(form, field, &form_structure, &autofill_field);

  context.field_is_relevant_for_plus_addresses =
      IsPlusAddressesManuallyTriggered(trigger_source) ||
      (!context.should_show_mixed_content_warning &&
       context.is_autofill_available &&
       !context.do_not_generate_autofill_suggestions &&
       context.filling_product == FillingProduct::kAddress && autofill_field &&
       client().GetPlusAddressDelegate() &&
       client().GetPlusAddressDelegate()->IsPlusAddressFillingEnabled(
           client().GetLastCommittedPrimaryMainFrameOrigin()));

  auto generate_suggestions_and_maybe_show_ui_phase2 = base::BindOnce(
      &BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase2,
      weak_ptr_factory_.GetWeakPtr(), form, form_structure, field,
      autofill_field, trigger_source, has_prediction_improvements_data,
      base::OwnedRef(context), std::move(callback));

  if (context.field_is_relevant_for_plus_addresses) {
    client().GetPlusAddressDelegate()->GetAffiliatedPlusAddresses(
        client().GetLastCommittedPrimaryMainFrameOrigin(),
        std::move(generate_suggestions_and_maybe_show_ui_phase2));

    return;
  }

  std::move(generate_suggestions_and_maybe_show_ui_phase2)
      .Run(/*plus_addresses=*/{});
}

void BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase2(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    AutofillField* autofill_field,
    AutofillSuggestionTriggerSource trigger_source,
    AutofillPredictionImprovementsDelegate::HasData
        has_prediction_improvements_data,
    SuggestionsContext& context,
    OnGenerateSuggestionsCallback callback,
    std::vector<std::string> plus_addresses) {
  autofill_metrics::SuggestionRankingContext ranking_context;
  std::vector<Suggestion> suggestions =
      GetAvailableAddressAndCreditCardSuggestions(
          form, form_structure, field, autofill_field, trigger_source,
          GetPlusAddressOverride(client().GetPlusAddressDelegate(),
                                 plus_addresses),
          context, ranking_context);

  if (context.is_autofill_available &&
      ShouldSuppressSuggestions(context.suppress_reason, log_manager())) {
    if (context.suppress_reason == SuppressReason::kAblation) {
      CHECK(suggestions.empty());
      single_field_form_fill_router_->CancelPendingQueries();
      std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions),
                              std::nullopt);
    }
    return;
  }

  if (AutofillPredictionImprovementsDelegate* delegate =
          client().GetAutofillPredictionImprovementsDelegate();
      delegate && has_prediction_improvements_data &&
      (trigger_source ==
           AutofillSuggestionTriggerSource::kPredictionImprovements ||
       trigger_source ==
           AutofillSuggestionTriggerSource::kFormControlElementClicked)) {
    std::vector<Suggestion> prediction_improvements_suggestions =
        delegate->GetSuggestions(suggestions, form, field);
    if (!prediction_improvements_suggestions.empty()) {
      std::move(callback).Run(/*show_suggestions=*/true,
                              std::move(prediction_improvements_suggestions),
                              /*ranking_context=*/std::nullopt);
      return;
    }
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
    std::move(callback).Run(/*show_suggestions=*/false, std::move(suggestions),
                            std::nullopt);
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
    std::move(callback).Run(/*show_suggestions=*/false, std::move(suggestions),
                            std::nullopt);
    return;
  }
  // Only offer plus address suggestions together with address suggestions if
  // these exist. Otherwise, plus address suggestions will be generated and
  // shown alongside single field form fill suggestions.
  // TODO(crbug.com/324557053): Do not generate plus address suggestions if the
  // plus address email override was applied on at least one address suggestion.
  const bool should_offer_plus_addresses_with_profiles =
      context.field_is_relevant_for_plus_addresses &&
      autofill_field->Type().group() == FieldTypeGroup::kEmail &&
      !suggestions.empty();
  // Try to show plus address suggestions. If the user specifically requested
  // plus addresses, disregard any other requirements (like having profile
  // suggestions) and show only plus address suggestions. Otherwise plus address
  // suggestions are mixed with profile suggestions if these exist.
  if (IsPlusAddressesManuallyTriggered(trigger_source) ||
      should_offer_plus_addresses_with_profiles) {
    const PasswordFormClassification password_form_classification =
        client().ClassifyAsPasswordForm(*this, form.global_id(),
                                        field.global_id());
    const AutofillPlusAddressDelegate::SuggestionContext suggestions_context =
        IsPlusAddressesManuallyTriggered(trigger_source)
            ? AutofillPlusAddressDelegate::SuggestionContext::kManualFallback
            : AutofillPlusAddressDelegate::SuggestionContext::
                  kAutofillProfileOnEmailField;
    std::vector<Suggestion> plus_address_suggestions =
        client().GetPlusAddressDelegate()->GetSuggestionsFromPlusAddresses(
            plus_addresses, client().GetLastCommittedPrimaryMainFrameOrigin(),
            client().IsOffTheRecord(), password_form_classification, field,
            trigger_source);

    MixPlusAddressAndAddressSuggestions(
        std::move(plus_address_suggestions), std::move(suggestions),
        suggestions_context, password_form_classification.type, form, field,
        std::move(callback));

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
                              {*std::move(maybe_compose_suggestion)},
                              std::nullopt);
      return;
    }
  }

  if (!suggestions.empty()) {
    // Show the list of `suggestions` if not empty. These may include address or
    // credit card suggestions. Additionally, warnings about mixed content might
    // be present.
    std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions),
                            ranking_context);
    return;
  }

  if (should_offer_other_suggestions) {
    MaybeShowIphForManualFallback(field, autofill_field, trigger_source,
                                  context.suppress_reason);
  }

  // Whether or not to request single field form fill suggestions.
  const bool should_offer_single_field_form_fill =
      should_offer_other_suggestions &&
      ShouldOfferSingleFieldFormFill(field, autofill_field, trigger_source,
                                     context.suppress_reason);

  // Whether or not to show plus address suggestions.
  const bool should_offer_plus_addresses =
      context.field_is_relevant_for_plus_addresses &&
      autofill_field->Type().group() == FieldTypeGroup::kEmail;

  const size_t barrier_calls =
      static_cast<size_t>(should_offer_single_field_form_fill) +
      static_cast<size_t>(should_offer_plus_addresses);
  if (barrier_calls == 0) {
    std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions),
                            std::nullopt);
    return;
  }

  const PasswordFormClassification password_form_classification =
      client().ClassifyAsPasswordForm(*this, form.global_id(),
                                      field.global_id());
  // The barrier callback bundles requests to generate suggestions for plus
  // addresses and single field form fill suggestions.
  // TODO(crbug.com/324557053): Remove the BarrierCallback as one of the
  // branches is not async anymore.
  auto barrier_callback = base::BarrierCallback<std::vector<Suggestion>>(
      barrier_calls,
      base::BindOnce(
          &BrowserAutofillManager::
              OnGeneratedPlusAddressAndSingleFieldFormFillSuggestions,
          weak_ptr_factory_.GetWeakPtr(),
          AutofillPlusAddressDelegate::SuggestionContext::kAutocomplete,
          password_form_classification.type, form, field, std::move(callback)));

  if (should_offer_plus_addresses) {
    std::vector<Suggestion> plus_address_suggestions =
        client().GetPlusAddressDelegate()->GetSuggestionsFromPlusAddresses(
            plus_addresses, client().GetLastCommittedPrimaryMainFrameOrigin(),
            client().IsOffTheRecord(), password_form_classification, field,
            trigger_source);
    barrier_callback.Run(std::move(plus_address_suggestions));
  }

  if (should_offer_single_field_form_fill) {
    bool handled_by_single_field_form_filler =
        single_field_form_fill_router_->OnGetSingleFieldSuggestions(
            form_structure, field, autofill_field, client(),
            base::BindRepeating(
                [](base::OnceCallback<void(std::vector<Suggestion>)> callback,
                   FieldGlobalId field_id,
                   const std::vector<Suggestion>& suggestions) {
                  std::move(callback).Run(suggestions);
                },
                barrier_callback));
    if (!handled_by_single_field_form_filler) {
      single_field_form_fill_router_->CancelPendingQueries();
      std::move(barrier_callback).Run({});
      return;
    }
  }
}

void BrowserAutofillManager::
    OnGeneratedPlusAddressAndSingleFieldFormFillSuggestions(
        AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
        PasswordFormClassification::Type password_form_type,
        const FormData& form,
        const FormFieldData& field,
        OnGenerateSuggestionsCallback callback,
        std::vector<std::vector<Suggestion>> suggestion_lists) {
  if (suggestion_lists.empty()) {
    std::move(callback).Run(/*show_suggestions=*/true, {}, std::nullopt);
    return;
  }

  std::vector<Suggestion> suggestions;
  for (std::vector<Suggestion>& suggestion_list : suggestion_lists) {
    suggestions.insert(suggestions.cend(),
                       std::make_move_iterator(suggestion_list.begin()),
                       std::make_move_iterator(suggestion_list.end()));
  }

  auto GetSuggestionPriority = [](autofill::FillingProduct product) {
    return product == FillingProduct::kPlusAddresses ? 1 : 2;
  };

  // Prioritize plus address over single field form fill suggestions.
  std::ranges::stable_sort(suggestions, [&](const Suggestion& s1,
                                            const Suggestion& s2) {
    return GetSuggestionPriority(GetFillingProductFromSuggestionType(s1.type)) <
           GetSuggestionPriority(GetFillingProductFromSuggestionType(s2.type));
  });

  const bool has_pa_suggestions =
      std::ranges::any_of(suggestions, [](const Suggestion& suggestion) {
        return GetFillingProductFromSuggestionType(suggestion.type) ==
               FillingProduct::kPlusAddresses;
      });

  if (has_pa_suggestions) {
    client().GetPlusAddressDelegate()->OnPlusAddressSuggestionShown(
        *this, form.global_id(), field.global_id(), suggestions_context,
        password_form_type, suggestions[0].type);

    // Include ManagePlusAddressSuggestion item.
    suggestions.emplace_back(SuggestionType::kSeparator);
    suggestions.push_back(
        client().GetPlusAddressDelegate()->GetManagePlusAddressSuggestion());
  }

  // Show the list of `suggestions`. These may include single field form field
  // and/or plus address suggestions.
  std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions),
                          std::nullopt);
}

void BrowserAutofillManager::MaybeShowIphForManualFallback(
    const FormFieldData& field,
    const AutofillField* autofill_field,
    AutofillSuggestionTriggerSource trigger_source,
    SuppressReason suppress_reason) {
  if (trigger_source ==
      AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick) {
    return;
  }
  if (suppress_reason != SuppressReason::kAutocompleteUnrecognized) {
    return;
  }
  if (!autofill_field) {
    return;
  }
  if (FieldTypeGroupToFormType(autofill_field->Type().group()) !=
      FormType::kAddressForm) {
    return;
  }
  if (std::ranges::none_of(client()
                               .GetPersonalDataManager()
                               ->address_data_manager()
                               .GetProfiles(),
                           [type = autofill_field->Type().GetStorableType()](
                               const AutofillProfile* profile) {
                             return profile->HasInfo(type);
                           })) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableManualFallbackIPH)) {
    return;
  }

  client().ShowAutofillFieldIphForFeature(
      field, AutofillClient::IphFeature::kManualFallback);
}

void BrowserAutofillManager::OnGenerateSuggestionsComplete(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    const SuggestionsContext& context,
    bool show_suggestions,
    std::vector<Suggestion> suggestions,
    std::optional<autofill_metrics::SuggestionRankingContext> ranking_context) {
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
    external_delegate_->OnSuggestionsReturned(field.global_id(), suggestions,
                                              std::move(ranking_context));
  }
}

void BrowserAutofillManager::MixPlusAddressAndAddressSuggestions(
    std::vector<Suggestion> plus_address_suggestions,
    std::vector<Suggestion> address_suggestions,
    AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
    PasswordFormClassification::Type password_form_type,
    const FormData& form,
    const FormFieldData& field,
    OnGenerateSuggestionsCallback callback) {
  if (plus_address_suggestions.empty()) {
    std::move(callback).Run(/*show_suggestions=*/true,
                            std::move(address_suggestions), std::nullopt);
    return;
  }

  client().GetPlusAddressDelegate()->OnPlusAddressSuggestionShown(
      *this, form.global_id(), field.global_id(), suggestions_context,
      password_form_type, plus_address_suggestions[0].type);
  if (address_suggestions.empty()) {
    plus_address_suggestions.emplace_back(SuggestionType::kSeparator);
    plus_address_suggestions.push_back(
        client().GetPlusAddressDelegate()->GetManagePlusAddressSuggestion());
  }
  // Mix both types of suggestions.
  plus_address_suggestions.insert(
      plus_address_suggestions.cend(),
      std::make_move_iterator(address_suggestions.begin()),
      std::make_move_iterator(address_suggestions.end()));

  std::move(callback).Run(/*show_suggestions=*/true,
                          std::move(plus_address_suggestions), std::nullopt);
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
  metrics_->last_selected_card = credit_card;
  metrics_->credit_card_form_event_logger.OnDidSelectCardSuggestion(
      credit_card, *form_structure, metrics_->signin_state_for_metrics);
  // If no authentication is needed, directly forward filling to FormFiller.
  if (!ShouldFetchCreditCard(form, field, *form_structure, *autofill_field,
                             credit_card)) {
    form_filler_->FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, field, &credit_card,
        /*optional_cvc=*/std::nullopt, form_structure, autofill_field,
        trigger_details);
    return;
  }
  metrics_->credit_card_form_event_logger.LogDeprecatedCreditCardSelectedMetric(
      credit_card, *form_structure, metrics_->signin_state_for_metrics);

  GetCreditCardAccessManager().FetchCreditCard(
      &credit_card, base::BindOnce(&BrowserAutofillManager::OnCreditCardFetched,
                                   weak_ptr_factory_.GetWeakPtr(), form, field,
                                   trigger_details.trigger_source));
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

void BrowserAutofillManager::FillOrPreviewFormExperimental(
    mojom::ActionPersistence action_persistence,
    FillingProduct filling_product,
    const FieldTypeSet& field_types_to_fill,
    const DenseSet<FieldFillingSkipReason>& ignorable_skip_reasons,
    const FormData& form,
    const FormFieldData& trigger_field,
    const base::flat_map<FieldGlobalId, std::u16string>& values_to_fill) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_trigger_field = nullptr;
  if (!GetCachedFormAndField(form, trigger_field, &form_structure,
                             &autofill_trigger_field)) {
    return;
  }
  form_filler_->FillOrPreviewFormExperimental(
      action_persistence, filling_product, field_types_to_fill,
      ignorable_skip_reasons, form, trigger_field, *form_structure,
      *autofill_trigger_field, values_to_fill);
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
  // We cannot early-return here because GetCachedFormAndField() yields nullptr
  // even if there it finds a FormStructure but its `autofill_count()` is 0. In
  // such cases, we still need to offer Autocomplete. Therefore, the code below,
  // including called functions, must handle `form_structure == nullptr` and
  // `autofill_field == nullptr`.
  // TODO: crbug.com/40232021 - Look into removing the `autofill_count() > 0`
  // condition from.
  std::ignore =
      GetCachedFormAndField(form, field, &form_structure, &autofill_field);
  const FillingProduct filling_product =
      GetFillingProductFromSuggestionType(type);
  form_filler_->FillOrPreviewField(action_persistence, action_type, form, field,
                                   form_structure, autofill_field, value,
                                   filling_product, field_type_used);
  if (action_persistence == mojom::ActionPersistence::kFill) {
    if (type == SuggestionType::kAddressFieldByFieldFilling) {
      metrics_->address_form_event_logger.OnFilledByFieldByFieldFilling(type);
      metrics_->address_form_event_logger.RecordFillingOperation(
          form.global_id(), std::to_array<const FormFieldData*>({&field}),
          std::to_array<const AutofillField*>({autofill_field}));
    } else if (type == SuggestionType::kCreditCardFieldByFieldFilling) {
      metrics_->credit_card_form_event_logger.OnFilledByFieldByFieldFilling(
          type);
      metrics_->credit_card_form_event_logger.RecordFillingOperation(
          form.global_id(), std::to_array<const FormFieldData*>({&field}),
          std::to_array<const AutofillField*>({autofill_field}));
    }

    const bool is_address_manual_fallback_on_non_address_field =
        IsAddressAutofillManuallyTriggeredOnNonAddressField(type,
                                                            autofill_field);
    const bool is_payments_manual_fallback_on_non_payments_field =
        IsCreditCardAutofillManuallyTriggeredOnNonCreditCardField(
            type, autofill_field);
    if (is_address_manual_fallback_on_non_address_field ||
        is_payments_manual_fallback_on_non_payments_field) {
      metrics_->manual_fallback_logger.OnDidFillSuggestion(filling_product);
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
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }
  metrics_->address_form_event_logger.OnDidFillFormFillingSuggestion(
      profile, *form_structure, *autofill_field, trigger_source);
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
      metrics_->address_form_event_logger.OnDidUndoAutofill();
    } else if (filling_product == FillingProduct::kCreditCard) {
      metrics_->credit_card_form_event_logger.OnDidUndoAutofill();
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

void BrowserAutofillManager::OnFocusOnNonFormFieldImpl() {
  // TODO(crbug.com/349982907): This function is not called on iOS.

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
  // TODO(crbug.com/349982907): This function is not called on iOS.

  if (pending_form_data_ &&
      pending_form_data_->global_id() != form.global_id()) {
    // A new form has received the focus, so we may have votes to upload for the
    // old form.
    ProcessPendingFormForUpload();
  }

  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }
  autofill_field->set_was_focused(true);

  // Notify installed screen readers if the focus is on a field for which there
  // are suggestions to present. Ignore if a screen reader is not present. If
  // the platform is ChromeOS, then assume ChromeVox is in use as there is no
  // way of determining whether it's being used from this point in the code.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!external_delegate_->HasActiveScreenReader()) {
    return;
  }
#endif

  SuggestionsContext context =
      BuildSuggestionsContext(form, form_structure, field, autofill_field,
                              AutofillSuggestionTriggerSource::kUnspecified);
  autofill_metrics::SuggestionRankingContext ranking_context;

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
          form, form_structure, field, autofill_field,
          AutofillSuggestionTriggerSource::kUnspecified,
          /*plus_address_email_override=*/std::nullopt, context,
          ranking_context);
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
    const base::TimeTicks timestamp) {
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
    DenseSet<SuggestionType> shown_suggestion_types,
    const FormData& form,
    const FormFieldData& field) {
  NotifyObservers(&Observer::OnSuggestionsShown);

  if (!std::ranges::any_of(
          shown_suggestion_types,
          AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId)) {
    return;
  }

  if (base::Contains(shown_suggestion_types, FillingProduct::kCreditCard,
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
      std::ranges::any_of(
          shown_suggestion_types, [autofill_field](SuggestionType type) {
            return IsAddressAutofillManuallyTriggeredOnNonAddressField(
                type, autofill_field);
          });
  const bool is_payments_manual_fallback_on_non_payments_field =
      std::ranges::any_of(
          shown_suggestion_types, [autofill_field](SuggestionType type) {
            return IsCreditCardAutofillManuallyTriggeredOnNonCreditCardField(
                type, autofill_field);
          });
  if (is_address_manual_fallback_on_non_address_field) {
    metrics_->manual_fallback_logger.OnDidShowSuggestions(
        FillingProduct::kAddress);
    return;
  }
  if (is_payments_manual_fallback_on_non_payments_field) {
    metrics_->manual_fallback_logger.OnDidShowSuggestions(
        FillingProduct::kCreditCard);
    return;
  }

  if (!has_cached_form_and_field) {
    return;
  }
  autofill_field->set_did_trigger_suggestions(true);

  auto* logger = GetEventFormLogger(*autofill_field);
  if (logger) {
    if (logger == &metrics_->credit_card_form_event_logger) {
      metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
          metrics_->signin_state_for_metrics);
    }
    logger->OnDidShowSuggestions(*form_structure, *autofill_field,
                                 form_structure->form_parsed_timestamp(),
                                 client().IsOffTheRecord());
  } else if (autofill_field->ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Suggestions were triggered on an ac=unrecognized address field.
    metrics_->autocomplete_unrecognized_fallback_logger.OnDidShowSuggestions();
  }
}

void BrowserAutofillManager::OnHidePopupImpl() {
  single_field_form_fill_router_->CancelPendingQueries();
  client().HideAutofillSuggestions(SuggestionHidingReason::kRendererEvent);
  client().HideAutofillFieldIph();
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
    const Suggestion& suggestion,
    const FormData& form,
    const FormFieldData& field) {
  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(suggestion);

  AutofillField* autofill_trigger_field = GetAutofillField(form, field);
  if (!autofill_trigger_field) {
    return;
  }
  if (IsSingleFieldFormFillerFillingProduct(
          GetFillingProductFromSuggestionType(suggestion.type))) {
    autofill_trigger_field->AppendLogEventIfNotRepeated(
        TriggerFillFieldLogEvent{
            .data_type =
                GetEventTypeFromSingleFieldSuggestionType(suggestion.type),
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

void BrowserAutofillManager::OnSelectFieldOptionsDidChangeImpl(
    const FormData& form) {
  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }

  driver().SendTypePredictionsToRenderer({form_structure});

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
    for (size_t i = 0; i < form.fields().size(); ++i) {
      if (form.fields()[i].global_id() == field_id) {
        return base::StringPrintf("Field %zu", i);
      }
    }
    return std::string("unknown");
  };
  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  LogBuffer change(IsLoggingActive(log_manager()));
  LOG_AF(change) << Tag{"div"} << Attrib{"class", "form"};
  LOG_AF(change) << field << Br{};
  LOG_AF(change) << "Old value structure: '"
                 << StructureOfString(old_value.substr(0, 80)) << "'" << Br{};
  LOG_AF(change) << "New value structure: '"
                 << StructureOfString(field.value().substr(0, 80)) << "'";
  LOG_AF(log_manager()) << LoggingScope::kWebsiteModifiedFieldValue
                        << LogMessage::kJavaScriptChangedAutofilledValue << Br{}
                        << Tag{"table"} << Tr{} << GetFieldNumber()
                        << std::move(change);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }
  AnalyzeJavaScriptChangedAutofilledValue(
      *form_structure, *autofill_field, field.value().empty(), formatting_only);
  if (formatting_only) {
    return;
  }
  form_filler_->MaybeTriggerRefillForExpirationDate(
      form, field, *form_structure, old_value,
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
  // TODO(crbug.com/41490871): Replace with form.last_filling_timestamp()
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
    const FormData& form,
    const FormFieldData& field,
    AutofillTriggerSource fetched_credit_card_trigger_source,
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
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field)) {
    return;
  }

  FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, form, field, *credit_card,
      credit_card->cvc(),
      {.trigger_source = fetched_credit_card_trigger_source});
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
  driver().GetFourDigitCombinationsFromDom(base::BindOnce(
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
  if (ShouldRecordUkm() && ShouldUploadUkm(*submitted_form,
                                           /*require_classified_field=*/true)) {
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

// Some members are intentionally not recreated or reset here:
// - Used for asynchronous form upload:
//   - `vote_upload_task_runner_`
//   - `weak_ptr_factory_`
// - No need to reset or recreate:
//   - external_delegate_
//   - fast_checkout_delegate_
//   - single_field_form_fill_router_
//   - consider_form_as_secure_for_testing_
void BrowserAutofillManager::Reset() {
  // Process log events and record into UKM when the FormStructure is destroyed.
  for (const auto& [form_id, form_structure] : form_structures()) {
    ProcessFieldLogEventsInForm(*form_structure);
  }
  ProcessPendingFormForUpload();
  FlushPendingLogQualityAndVotesUploadCallbacks();
  DCHECK(!pending_form_data_);

  four_digit_combinations_in_dom_.clear();
  last_unlocked_credit_card_cvc_.clear();
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->Reset();
  }
  form_filler_->Reset();

  // The order below is relevant:
  // - `credit_card_access_manager_` has a reference to `metrics_`.
  // - `metrics_` has references to
  //   AutofillManager::form_interactions_ukm_logger().
  credit_card_access_manager_.reset();
  metrics_.reset();
  AutofillManager::Reset();
  metrics_.emplace(this);
}

void BrowserAutofillManager::UpdateLoggersReadinessData() {
  if (!IsAutofillEnabled()) {
    return;
  }
  GetCreditCardAccessManager().UpdateCreditCardFormEventLogger();
  metrics_->address_form_event_logger.UpdateProfileAvailabilityForReadiness(
      client().GetPersonalDataManager()->address_data_manager().GetProfiles());
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
      metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
          metrics_->signin_state_for_metrics);
      metrics_->credit_card_form_event_logger.OnDidRefill(form_structure);
    } else {
      metrics_->credit_card_form_event_logger.RecordFillingOperation(
          form_structure.global_id(), safe_filled_fields,
          safe_filled_autofill_fields);
      // The originally selected masked card is `metrics_->last_selected_card`.
      // So we must log `metrics_->last_selected_card` as opposed to
      // `absl::get<const CreditCard*>(profile_or_credit_card)` to correctly
      // indicate whether the user filled the form using a masked card
      // suggestion.
      metrics_->credit_card_form_event_logger.OnDidFillFormFillingSuggestion(
          metrics_->last_selected_card, form_structure, trigger_autofill_field,
          filled_fields, safe_fields, metrics_->signin_state_for_metrics,
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
        metrics_->address_form_event_logger.OnDidRefill(form_structure);
      } else {
        metrics_->address_form_event_logger.RecordFillingOperation(
            form_structure.global_id(), safe_filled_fields,
            safe_filled_autofill_fields);
        metrics_->address_form_event_logger.OnDidFillFormFillingSuggestion(
            *profile, form_structure, trigger_autofill_field,
            trigger_details.trigger_source);
      }
    } else if (!is_refill) {
      metrics_->address_form_event_logger.RecordFillingOperation(
          form_structure.global_id(), safe_filled_fields,
          safe_filled_autofill_fields);
      metrics_->autocomplete_unrecognized_fallback_logger
          .OnDidFillFormFillingSuggestion();
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
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<std::string> plus_address_email_override) {
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
    if (should_suppress &&
        !base::FeatureList::IsEnabled(
            features::test::kAutofillDisableSuggestionStrikeDatabase)) {
      LOG_AF(log_manager())
          << LoggingScope::kFilling << LogMessage::kSuggestionSuppressed
          << " Reason: strike limit reached.";
      // If the user already reached the strike limit on this particular field,
      // address suggestions are suppressed.
      return {};
    }
  }
#endif
  metrics_->address_form_event_logger.OnDidPollSuggestions(trigger_field);

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
        IsAddressFieldSwappingEnabled()) {
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
          case FieldTypeGroup::kPredictionImprovements:
            // Since we early return on non-address types.
            NOTREACHED();
        }
        NOTREACHED();
      case SuggestionType::kAddressFieldByFieldFilling:
        return SuggestionType::kAddressFieldByFieldFilling;
      case SuggestionType::kFillPredictionImprovements:
        return SuggestionType::kFillPredictionImprovements;
      default:
        // `last_suggestion_type` is only one of the address filling suggestion
        // types, therefore no other type should be passed to this function.
        NOTREACHED();
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
    if (form_structure && form.fields().size() == num_fields) {
      skip_reasons = form_filler_->GetFieldFillingSkipReasons(
          form.fields(), *form_structure, *trigger_autofill_field,
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

  return GetSuggestionsForProfiles(client(), field_types, trigger_field,
                                   trigger_field_type, current_suggestion_type,
                                   trigger_source,
                                   std::move(plus_address_email_override));
}

std::vector<Suggestion> BrowserAutofillManager::GetCreditCardSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource trigger_source,
    autofill_metrics::SuggestionRankingContext& ranking_context) {
  metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
      metrics_->signin_state_for_metrics);
  metrics_->credit_card_form_event_logger.OnDidPollSuggestions(trigger_field);

  std::u16string card_number_field_value = u"";
  bool is_card_number_autofilled = false;
  std::vector<std::u16string> last_four_list_for_cvc_suggestion_filtering;

  // Preprocess the form to extract info about card number field.
  if (FormStructure* cached_form = FindCachedFormById(form.global_id())) {
    for (const FormFieldData& field : form.fields()) {
      AutofillField* autofill_field =
          cached_form->GetFieldById(field.global_id());
      if (!autofill_field ||
          autofill_field->Type().GetStorableType() != CREDIT_CARD_NUMBER) {
        continue;
      }
      card_number_field_value += SanitizeCreditCardFieldValue(field.value());
      if (field.is_autofilled()) {
        is_card_number_autofilled = true;
      }
    }
  }
  if (is_card_number_autofilled && card_number_field_value.size() >= 4) {
    last_four_list_for_cvc_suggestion_filtering.push_back(
        card_number_field_value.substr(card_number_field_value.size() - 4));
  }

  // Offer suggestion for expiration date field if the card number field is
  // empty or the card number field is autofilled.
  auto ShouldOfferSuggestionsForExpirationTypeField = [&] {
    return SanitizedFieldIsEmpty(card_number_field_value) ||
           is_card_number_autofilled;
  };

  if (data_util::IsCreditCardExpirationType(trigger_field_type) &&
      !ShouldOfferSuggestionsForExpirationTypeField()) {
    return {};
  }

  if (IsInAutofillSuggestionsDisabledExperiment()) {
    return {};
  }

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      client(), trigger_field, trigger_field_type, trigger_source, summary,
      ShouldShowScanCreditCard(form, trigger_field),
      ShouldShowCardsFromAccountOption(form, trigger_field, trigger_source),
      four_digit_combinations_in_dom_,
      last_four_list_for_cvc_suggestion_filtering);
  bool is_virtual_card_standalone_cvc_field = std::any_of(
      suggestions.begin(), suggestions.end(), [](Suggestion suggestion) {
        return suggestion.type == SuggestionType::kVirtualCreditCardEntry;
      });
  if (!is_virtual_card_standalone_cvc_field) {
    ranking_context = std::move(summary.ranking_context);
  }

  metrics_->credit_card_form_event_logger.OnDidFetchSuggestion(
      suggestions, summary.with_offer, summary.with_cvc,
      is_virtual_card_standalone_cvc_field,
      std::move(summary.metadata_logging_context));
  return suggestions;
}

// TODO(crbug.com/40219607) Eliminate and replace with a listener?
// Should we do the same with all the other BrowserAutofillManager events?
void BrowserAutofillManager::OnBeforeProcessParsedForms() {
  metrics_->has_parsed_forms = true;

  // Record the current sync state to be used for metrics on this page.
  metrics_->signin_state_for_metrics = client()
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
        std::ranges::any_of(form_structure.fields(), [](const auto& field) {
          return field->Type().GetStorableType() ==
                 CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
        });
    if (contains_standalone_cvc_field) {
      FetchPotentialCardLastFourDigitsCombinationFromDOM();
    }
  }
  if (data_util::ContainsPhone(data_util::DetermineGroups(form_structure))) {
    metrics_->has_observed_phone_number_field = true;
  }
  // TODO(crbug.com/41405154): avoid logging developer engagement multiple
  // times for a given form if it or other forms on the page are dynamic.
  LogDeveloperEngagementUkm(client().GetUkmRecorder(),
                            client().GetUkmSourceId(), form_structure);

  for (const auto& field : form_structure) {
    if (field->Type().html_type() == HtmlFieldType::kOneTimeCode) {
      metrics_->has_observed_one_time_code_field = true;
      break;
    }
  }
  // Log the type of form that was parsed.
  DenseSet<FormType> form_types = form_structure.GetFormTypes();
  bool card_form = base::Contains(form_types, FormType::kCreditCardForm) ||
                   base::Contains(form_types, FormType::kStandaloneCvcForm);
  bool address_form = base::Contains(form_types, FormType::kAddressForm);
  if (card_form) {
    metrics_->credit_card_form_event_logger.OnDidParseForm(form_structure);
  }
  if (address_form) {
    metrics_->address_form_event_logger.OnDidParseForm(form_structure);
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
    base::TimeTicks interaction_timestamp) {
  if (metrics_->initial_interaction_timestamp.is_null() ||
      interaction_timestamp < metrics_->initial_interaction_timestamp) {
    metrics_->initial_interaction_timestamp = interaction_timestamp;
  }
}

bool BrowserAutofillManager::EvaluateAblationStudy(
    const std::vector<Suggestion>& address_and_credit_card_suggestions,
    AutofillField* autofill_field,
    SuggestionsContext& context) {
  if (context.filling_product != FillingProduct::kAddress &&
      context.filling_product != FillingProduct::kCreditCard) {
    // The ablation study only supports addresses and credit cards.
    return false;
  }

  FormTypeForAblationStudy form_type =
      context.filling_product == FillingProduct::kCreditCard
          ? FormTypeForAblationStudy::kPayment
          : FormTypeForAblationStudy::kAddress;

  // The `ablation_group` indicates if the form filling is under ablation,
  // meaning that autofill popups are suppressed. If ablation_group is
  // AblationGroup::kDefault or AblationGroup::kControl, no ablation happens
  // in the following.
  AblationGroup ablation_group = client().GetAblationStudy().GetAblationGroup(
      client().GetLastCommittedPrimaryMainFrameURL(), form_type,
      client().GetAutofillOptimizationGuide());

  // The conditional_ablation_group indicates whether the form filling is
  // under ablation, under the condition that the user has data to fill on
  // file. All users that don't have data to fill are in the
  // AbationGroup::kDefault. Note that it is possible (due to implementation
  // details) that this is incorrectly set to kDefault: If the user has typed
  // some characters into a text field, it may look like no suggestions are
  // available, but in practice the suggestions are just filtered out
  // (Autofill only suggests matches that start with the typed prefix). Any
  // consumers of the conditional_ablation_group attribute should monitor it
  // over time. Any transitions of conditional_ablation_group from {kAblation,
  // kControl} to kDefault should just be ignored and the previously reported
  // value should be used. As the ablation experience is stable within period
  // of time, such a transition typically indicates that the user has typeed a
  // prefix which led to the filtering of all autofillable data. In short:
  // once either kAblation or kControl were reported, consumers should stick
  // to that. Note that we don't set the ablation group if there are no
  // suggestions. In that case we stick to kDefault.
  AblationGroup conditional_ablation_group =
      !address_and_credit_card_suggestions.empty() ? ablation_group
                                                   : AblationGroup::kDefault;

  // For both form types (credit card and address forms), we inform the other
  // event logger also about the ablation. This prevents for example that for
  // an encountered address form we log a sample
  // Autofill.Funnel.ParsedAsType.CreditCard = 0 (which would be recorded by
  // the metrics_->credit_card_form_event_logger). For the complementary event
  // logger, the conditional ablation status is logged as kDefault to not
  // imply that data would be filled without ablation.
  if (context.filling_product == FillingProduct::kCreditCard) {
    metrics_->credit_card_form_event_logger.SetAblationStatus(
        ablation_group, conditional_ablation_group);
    metrics_->address_form_event_logger.SetAblationStatus(
        ablation_group, AblationGroup::kDefault);
  } else if (context.filling_product == FillingProduct::kAddress) {
    metrics_->address_form_event_logger.SetAblationStatus(
        ablation_group, conditional_ablation_group);
    metrics_->credit_card_form_event_logger.SetAblationStatus(
        ablation_group, AblationGroup::kDefault);
  }

  if (autofill_field && ablation_group != AblationGroup::kDefault) {
    autofill_field->AppendLogEventIfNotRepeated(
        AblationFieldLogEvent{ablation_group, conditional_ablation_group,
                              GetDayInAblationWindow(AutofillClock::Now())});
  }

  if (!address_and_credit_card_suggestions.empty() &&
      ablation_group == AblationGroup::kAblation &&
      !features::kAutofillAblationStudyIsDryRun.Get()) {
    // Logic for disabling/ablating autofill.
    context.suppress_reason = SuppressReason::kAblation;
    return true;
  }

  return false;
}

std::vector<Suggestion>
BrowserAutofillManager::GetAvailableAddressAndCreditCardSuggestions(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    AutofillField* autofill_field,
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<std::string> plus_address_email_override,
    SuggestionsContext& context,
    autofill_metrics::SuggestionRankingContext& ranking_context) {
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
  if (FillingProductSet{FillingProduct::kCreditCard,
                        FillingProduct::kStandaloneCvc}
          .contains(context.filling_product)) {
    FieldType trigger_field_type =
        autofill_field ? autofill_field->Type().GetStorableType()
                       : UNKNOWN_TYPE;
    suggestions = GetCreditCardSuggestions(form, field, trigger_field_type,
                                           trigger_source, ranking_context);
  } else if (context.filling_product == FillingProduct::kAddress) {
    // Profile suggestions fill ac=unrecognized fields only when triggered
    // through manual fallbacks. As such, suggestion labels differ depending on
    // the `trigger_source`.
    suggestions = GetProfileSuggestions(form, form_structure, field,
                                        autofill_field, trigger_source,
                                        std::move(plus_address_email_override));
  }

  if (EvaluateAblationStudy(suggestions, autofill_field, context)) {
    return {};
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
BrowserAutofillManager::GetEventFormLogger(const AutofillField& field) {
  if (field.ShouldSuppressSuggestionsAndFillingByDefault()) {
    // Ignore ac=unrecognized fields in key metrics.
    return nullptr;
  }
  switch (FieldTypeGroupToFormType(field.Type().group())) {
    case FormType::kAddressForm:
      return &metrics_->address_form_event_logger;
    case FormType::kCreditCardForm:
    case FormType::kStandaloneCvcForm:
      return &metrics_->credit_card_form_event_logger;
    case FormType::kPasswordForm:
    case FormType::kUnknownFormType:
      return nullptr;
  }
  NOTREACHED();
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
                  base::UTF16ToUTF8(country_code), field->value_for_import());

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
  if (!metrics_->has_parsed_forms && !used_web_otp) {
    return;
  }

  constexpr uint32_t kOtcUsed = 1 << 0;
  constexpr uint32_t kWebOtpUsed = 1 << 1;
  constexpr uint32_t kPhoneCollected = 1 << 2;
  constexpr uint32_t kMaxValue = kOtcUsed | kWebOtpUsed | kPhoneCollected;

  uint32_t phone_collection_metric_state = 0;
  if (metrics_->has_observed_phone_number_field) {
    phone_collection_metric_state |= kPhoneCollected;
  }
  if (metrics_->has_observed_one_time_code_field) {
    phone_collection_metric_state |= kOtcUsed;
  }
  if (used_web_otp) {
    phone_collection_metric_state |= kWebOtpUsed;
  }

  ukm::UkmRecorder* recorder = client().GetUkmRecorder();
  ukm::SourceId source_id = client().GetUkmSourceId();
  AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
      recorder, source_id, phone_collection_metric_state);

  base::UmaHistogramExactLinear("Autofill.WebOTP.PhonePlusWebOTPPlusOTC",
                                phone_collection_metric_state, kMaxValue + 1);
}

void BrowserAutofillManager::ProcessFieldLogEventsInForm(
    const FormStructure& form_structure) {
  // TODO(crbug.com/40225658): Log metrics if at least one field in the form was
  // classified as a certain type.
  LogEventCountsUMAMetric(form_structure);

  // ShouldUploadUkm reduces the UKM load by ignoring e.g. search boxes at best
  // effort.
  bool should_upload_ukm =
      ShouldRecordUkm() &&
      ShouldUploadUkm(form_structure, /*require_classified_field=*/true);

  for (const auto& autofill_field : form_structure) {
    if (should_upload_ukm) {
      form_interactions_ukm_logger()->LogAutofillFieldInfoAtFormRemove(
          form_structure, *autofill_field,
          AutofillMetrics::AutocompleteStateForSubmittedField(*autofill_field));
    }
  }

  // Log FormSummary UKM event.
  if (should_upload_ukm) {
    AutofillMetrics::FormEventSet form_events;
    form_events.insert_all(metrics_->address_form_event_logger.GetFormEvents(
        form_structure.global_id()));
    form_events.insert_all(
        metrics_->credit_card_form_event_logger.GetFormEvents(
            form_structure.global_id()));
    form_interactions_ukm_logger()->LogAutofillFormSummaryAtFormRemove(
        form_structure, form_events, metrics_->initial_interaction_timestamp,
        metrics_->form_submitted_timestamp);
    form_interactions_ukm_logger()->LogFocusedComplexFormAtFormRemove(
        form_structure, form_events, metrics_->initial_interaction_timestamp,
        metrics_->form_submitted_timestamp);
  }

  if (base::FeatureList::IsEnabled(features::kAutofillUKMExperimentalFields) &&
      !metrics_->form_submitted_timestamp.is_null() &&
      ShouldUploadUkm(form_structure,
                      /*require_classified_field=*/false)) {
    form_interactions_ukm_logger()
        ->LogAutofillFormWithExperimentalFieldsCountAtFormRemove(
            form_structure);
  }

  for (const auto& autofill_field : form_structure) {
    // Clear log events.
    // Not conditioned on kAutofillLogUKMEventsWithSamplingOnSession because
    // there may be other reasons to log events.
    autofill_field->ClearLogEvents();
  }
}

bool BrowserAutofillManager::ShouldUploadUkm(
    const FormStructure& form_structure,
    bool require_classified_field) {
  if (!form_structure.ShouldBeParsed()) {
    return false;
  }

  auto is_focusable_text_field =
      [](const std::unique_ptr<AutofillField>& field) {
        return field->IsTextInputElement() && field->IsFocusable();
      };

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
      form_structure.fields(), require_classified_field
                                   ? is_focusable_predicted_text_field
                                   : is_focusable_text_field);
  if (num_text_fields == 0) {
    return false;
  }

  // If the form contains a single text field and this contains the string
  // "search" in its name/id/placeholder, the function return false and the form
  // is not recorded into UKM. The form is considered a search box.
  if (num_text_fields == 1) {
    auto it = base::ranges::find_if(form_structure.fields(),
                                    require_classified_field
                                        ? is_focusable_predicted_text_field
                                        : is_focusable_text_field);
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
  size_t num_ablation_event = 0;

  for (const auto& autofill_field : form_structure) {
    for (const auto& log_event : autofill_field->field_log_events()) {
      static_assert(
          absl::variant_size<AutofillField::FieldLogEventType>() == 10,
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
      } else if (absl::holds_alternative<AblationFieldLogEvent>(log_event)) {
        ++num_ablation_event;
      } else {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

  size_t total_num_log_events =
      num_ask_for_values_to_fill_event + num_trigger_fill_event +
      num_fill_event + num_typing_event + num_heuristic_prediction_event +
      num_autocomplete_attribute_event + num_server_prediction_event +
      num_rationalization_event + num_ablation_event;
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
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.AblationEvent",
                             num_ablation_event);
  UMA_HISTOGRAM_COUNTS_10000("Autofill.LogEvent.All", total_num_log_events);
}

void BrowserAutofillManager::SetFastCheckoutRunId(
    FieldTypeGroup field_type_group,
    int64_t run_id) {
  switch (FieldTypeGroupToFormType(field_type_group)) {
    case FormType::kAddressForm:
      metrics_->address_form_event_logger.SetFastCheckoutRunId(run_id);
      return;
    case FormType::kCreditCardForm:
    case FormType::kStandaloneCvcForm:
      metrics_->credit_card_form_event_logger.SetFastCheckoutRunId(run_id);
      break;
    case FormType::kPasswordForm:
    case FormType::kUnknownFormType:
      // FastCheckout only supports address and credit card forms.
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace autofill
