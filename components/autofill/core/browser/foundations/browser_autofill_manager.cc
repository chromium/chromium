// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
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
#include <variant>
#include <vector>

#include "autofill_client.h"
#include "base/barrier_callback.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/types/zip.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/phone_number.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/data_model/usage_history_information.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/filling/form_autofill_history.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/filling/payments/field_filling_payments_util.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/per_fill_metrics.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_manager.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/iban_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestions_context.h"
#include "components/autofill/core/browser/suggestions/valuables/valuable_suggestion_generator.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
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
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
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
using payments::AmountExtractionManager;

namespace {

FillDataType GetFillDataTypeFromFillingPayload(
    const FillingPayload& filling_payload) {
  return std::visit(
      base::Overloaded{
          [](const AutofillProfile*) { return FillDataType::kAutofillProfile; },
          [](const CreditCard*) { return FillDataType::kCreditCard; },
          [](const EntityInstance*) { return FillDataType::kAutofillAi; },
          [](const VerifiedProfile*) { return FillDataType::kAutofillProfile; },
      },
      filling_payload);
}

void LogDeveloperEngagementUkm(ukm::UkmRecorder* ukm_recorder,
                               ukm::SourceId source_id,
                               const FormStructure& form_structure) {
  if (form_structure.developer_engagement_metrics()) {
    AutofillMetrics::LogDeveloperEngagementUkm(
        ukm_recorder, source_id, form_structure.main_frame_origin().GetURL(),
        form_structure.IsCompleteCreditCardForm(
            FormStructure::CreditCardFormCompleteness::kCompleteCreditCardForm),
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

bool IsSingleFieldFillerFillingProduct(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAutocomplete:
    case FillingProduct::kIban:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kLoyaltyCard:
      return true;
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kCompose:
    case FillingProduct::kPassword:
    case FillingProduct::kCreditCard:
    case FillingProduct::kAddress:
    case FillingProduct::kNone:
    case FillingProduct::kIdentityCredential:
      return false;
  }
}

FillDataType GetEventTypeFromSingleFieldSuggestionType(SuggestionType type) {
  switch (type) {
    case SuggestionType::kAutocompleteEntry:
      return FillDataType::kSingleFieldFillerAutocomplete;
    case SuggestionType::kMerchantPromoCodeEntry:
      return FillDataType::kSingleFieldFillerPromoCode;
    case SuggestionType::kIbanEntry:
      return FillDataType::kSingleFieldFillerIban;
    case SuggestionType::kLoyaltyCardEntry:
      return FillDataType::kSingleFieldFillerLoyaltyCard;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
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
    case SuggestionType::kBnplEntry:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kHomeAndWorkAddressEntry:
      NOTREACHED();
  }
  NOTREACHED();
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
                           const FormStructure& form_structure,
                           const AutofillField& autofill_field,
                           const CreditCard& credit_card) {
  if (credit_card.is_bnpl_card()) {
    // This is a BNPL VCN, so fetching is not needed because an authentication
    // already happened.
    return false;
  }
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
    case AutofillSuggestionTriggerSource::kTextFieldValueChanged:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
    case AutofillSuggestionTriggerSource::
        kShowPromptAfterDialogClosedNonManualFallback:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kAutofillAi:
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
        std::ranges::count_if(suggestions,
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
        std::ranges::count_if(suggestions,
                              [](const Suggestion& suggestion) {
                                return GetFillingProductFromSuggestionType(
                                           suggestion.type) ==
                                       FillingProduct::kAddress;
                              }),
        FillingProduct::kAddress);
  }
}

bool ShouldOfferSingleFieldFill(const FormFieldData& field,
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
          .address_data_manager()
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

base::flat_map<FieldGlobalId, FieldTypeGroup>
GetFieldTypeGroupsFromFormStructure(const FormStructure* form_structure) {
  return form_structure ? base::MakeFlatMap<FieldGlobalId, FieldTypeGroup>(
                              form_structure->fields(), /*comp=*/{},
                              [](const std::unique_ptr<AutofillField>& field) {
                                return std::make_pair(field->global_id(),
                                                      field->Type().group());
                              })
                        : base::flat_map<FieldGlobalId, FieldTypeGroup>();
}

// Returns if the email address was modified on any of the suggested addresses
// using a plus profile.
bool WasEmailOverrideAppliedOnSuggestions(
    const std::vector<Suggestion>& address_suggestions) {
  return std::ranges::any_of(
      address_suggestions, [](const Suggestion& suggestion) {
        const Suggestion::AutofillProfilePayload* profile_payload =
            std::get_if<Suggestion::AutofillProfilePayload>(
                &suggestion.payload);
        return profile_payload && !profile_payload->email_override.empty();
      });
}

// Triggers the possible import of submitted data at submission time.
void MaybeImportFromSubmittedForm(AutofillClient& client,
                                  ukm::SourceId ukm_source_id,
                                  const FormStructure& form_structure,
                                  const FormData& form_data,
                                  bool autofill_ai_shows_bubble) {
  // This intentionally happens prior to `ImportAndProcessFormData()`. See
  // crbug.com/381205586.
  ProfileTokenQuality::SaveObservationsForFilledFormForAllSubmittedProfiles(
      form_structure, form_data,
      client.GetPersonalDataManager().address_data_manager());

  if (!autofill_ai_shows_bubble) {
    // Update Personal Data with the form's submitted data.
    client.GetFormDataImporter()->ImportAndProcessFormData(
        form_structure, client.IsAutofillProfileEnabled(),
        client.IsAutofillPaymentMethodsEnabled(), ukm_source_id);
  }

  AutofillPlusAddressDelegate* plus_address_delegate =
      client.GetPlusAddressDelegate();

  std::vector<FormFieldData> fields_for_autocomplete;
  fields_for_autocomplete.reserve(form_structure.fields().size());
  for (const auto& autofill_field : form_structure) {
    fields_for_autocomplete.push_back(*autofill_field);
    if (autofill_field->Type().GetStorableType() ==
        CREDIT_CARD_VERIFICATION_CODE) {
      // However, if Autofill has recognized a field as CVC, that shouldn't be
      // saved.
      fields_for_autocomplete.back().set_should_autocomplete(false);
    }

    if (autofill_field->Type().GetStorableType() == LOYALTY_MEMBERSHIP_ID &&
        autofill_field->is_autofilled()) {
      // Only store loyalty cards values in Autocomplete if they were filled
      // manually.
      fields_for_autocomplete.back().set_should_autocomplete(false);
    }
    const std::u16string& value = autofill_field->value_for_import();
    if (plus_address_delegate &&
        (plus_address_delegate->IsPlusAddress(base::UTF16ToUTF8(value)) ||
         plus_address_delegate->MatchesPlusAddressFormat(value))) {
      // Similarly to CVC, any plus addresses needn't be saved to autocomplete.
      // Note that the feature is experimental, and `plus_address_delegate`
      // will be null if the feature is not enabled (it's disabled by default).
      // If the plus address format happens to change or gets extended, we still
      // keep filtering existing plus addresses.
      fields_for_autocomplete.back().set_should_autocomplete(false);
    }
  }

  // TODO crbug.com/40100455 - Eliminate `form_for_autocomplete`.
  FormData form_for_autocomplete = form_structure.ToFormData();
  form_for_autocomplete.set_fields(std::move(fields_for_autocomplete));
  client.GetSingleFieldFillRouter().OnWillSubmitForm(
      form_for_autocomplete, &form_structure, client.IsAutocompleteEnabled());
}

// Retrieves the AutofillAI predictions for `form` in `cache` and adds them to
// `form`'s fields.
void AddCachedAutofillAiPredictions(const AutofillAiModelCache& cache,
                                    FormStructure& form) {
  // Mixing Autofill AI model predictions (which come from the online LLM) and
  // Autofill AI server predictions (which come from the Autofill crowdsourcing
  // server) may lead to too many false positives. We therefore favor server
  // predictions over model predictions. (There's no specific reason for this
  // precedence -- preferring model predictions may work just as well.)
  if (std::ranges::any_of(
          form.fields(), [](const std::unique_ptr<AutofillField>& field) {
            return field->GetAutofillAiServerTypePredictions().has_value();
          })) {
    return;
  }

  using FieldIdentifier = AutofillAiModelCache::FieldIdentifier;
  using ModelFieldPrediction = AutofillAiModelCache::FieldPrediction;
  const base::flat_map<FieldIdentifier, ModelFieldPrediction> predictions =
      cache.GetFieldPredictions(form.form_signature());
  if (predictions.empty()) {
    return;
  }
  for (const std::unique_ptr<AutofillField>& field : form.fields()) {
    auto it = predictions.find(FieldIdentifier{
        .signature = field->GetFieldSignature(),
        .rank_in_signature_group = field->rank_in_signature_group()});
    if (it == predictions.end()) {
      continue;
    }
    const ModelFieldPrediction& prediction = it->second;
    if (prediction.field_type != NO_SERVER_DATA) {
      using ServerPrediction = AutofillQueryResponse::FormSuggestion::
          FieldSuggestion::FieldPrediction;
      ServerPrediction server_prediction;
      server_prediction.set_type(prediction.field_type);
      server_prediction.set_source(ServerPrediction::SOURCE_AUTOFILL_AI);
      field->MaybeAddServerPrediction(std::move(server_prediction));
    }
    if (!prediction.format_string.empty()) {
      field->set_format_string_unless_overruled(
          prediction.format_string,
          AutofillField::FormatStringSource::kModelResult);
    }
  }
}

}  // namespace

BrowserAutofillManager::MetricsState::MetricsState(
    BrowserAutofillManager* owner)
    : address_form_event_logger(owner), credit_card_form_event_logger(owner) {}

BrowserAutofillManager::MetricsState::~MetricsState() {
  if (has_parsed_forms) {
    base::UmaHistogramBoolean(
        "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
        has_observed_phone_number_field);
    base::UmaHistogramBoolean("Autofill.WebOTP.OneTimeCode.FromAutocomplete",
                              has_observed_one_time_code_field);
  }
  credit_card_form_event_logger.OnDestroyed();
  address_form_event_logger.OnDestroyed();
}

BrowserAutofillManager::BrowserAutofillManager(AutofillDriver* driver)
    : AutofillManager(driver) {}

BrowserAutofillManager::~BrowserAutofillManager() {
  // Process log events and record into UKM when the FormStructure is destroyed.
  for (const auto& [form_id, form_structure] : form_structures()) {
    ProcessFieldLogEventsInForm(*form_structure);
  }
  client().GetSingleFieldFillRouter().CancelPendingQueries();
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

payments::BnplManager* BrowserAutofillManager::GetPaymentsBnplManager() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (!bnpl_manager_) {
    bnpl_manager_ = std::make_unique<payments::BnplManager>(this);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  return bnpl_manager_.get();
}

bool BrowserAutofillManager::ShouldShowScanCreditCard(
    const FormData& form,
    const FormFieldData& field) {
  if (!client().GetPaymentsAutofillClient()->HasCreditCardScanFeature() ||
      !client().IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  AutofillField* autofill_field =
      GetAutofillField(form.global_id(), field.global_id());
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

bool BrowserAutofillManager::ShouldParseForms() {
  bool autofill_enabled = client().IsAutofillEnabled();
  // If autofill is disabled but the password manager is enabled, we still
  // need to parse the forms and query the server as the password manager
  // depends on server classifications.
  bool password_manager_enabled = client().IsPasswordManagerEnabled();
  metrics_->signin_state_for_metrics = client()
                                           .GetPersonalDataManager()
                                           .payments_data_manager()
                                           .GetPaymentsSigninStateForMetrics();
  if (!metrics_->has_logged_autofill_enabled) {
    autofill_metrics::LogIsAutofillEnabledAtPageLoad(
        autofill_enabled, metrics_->signin_state_for_metrics);
    autofill_metrics::LogIsAutofillProfileEnabledAtPageLoad(
        client().IsAutofillProfileEnabled(),
        metrics_->signin_state_for_metrics);
    if (!client().IsAutofillProfileEnabled()) {
      autofill_metrics::LogAutofillProfileDisabledReasonAtPageLoad(
          CHECK_DEREF(client().GetPrefs()));
    }
    autofill_metrics::LogIsAutofillPaymentMethodsEnabledAtPageLoad(
        client().IsAutofillPaymentMethodsEnabled(),
        metrics_->signin_state_for_metrics);
    if (!client().IsAutofillPaymentMethodsEnabled()) {
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
      << Br{} << "timestamp: "
      << form_submitted_timestamp.since_origin().InMilliseconds() << Br{}
      << "source: " << SubmissionSourceToString(source) << Br{} << form;

  // Always let the value patterns metric upload data.
  LogValuePatternsMetric(form);

  std::unique_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  CHECK(!client().IsOffTheRecord() || !submitted_form);

  if (!submitted_form) {
    // We always give Autocomplete a chance to save the data.
    // TODO(crbug.com/40276862): Verify frequency of plus address (or the other
    // type(s) checked for below, for that matter) slipping through in this code
    // path.
    client().GetSingleFieldFillRouter().OnWillSubmitForm(
        form, nullptr, client().IsAutocompleteEnabled());
    return;
  }

  submitted_form->set_submission_source(source);
  LogSubmissionMetrics(submitted_form.get(), form_submitted_timestamp);

  bool autofill_ai_shows_bubble = false;
  if (AutofillAiDelegate* delegate = client().GetAutofillAiDelegate()) {
    autofill_ai_shows_bubble = delegate->OnFormSubmitted(
        *submitted_form, driver().GetPageUkmSourceId());
  }

  MaybeImportFromSubmittedForm(client(), driver().GetPageUkmSourceId(),
                               *submitted_form, form, autofill_ai_shows_bubble);
  MaybeAddAddressSuggestionStrikes(client(), *submitted_form);
  client().GetVotesUploader().MaybeStartVoteUploadProcess(
      std::move(submitted_form),
      /*observed_submission=*/true, GetCurrentPageLanguage(),
      metrics_->initial_interaction_timestamp, last_unlocked_credit_card_cvc_,
      driver().GetPageUkmSourceId());
}

void BrowserAutofillManager::UpdatePendingForm(const FormData& form) {
  // Process the current pending form if different than supplied |form|.
  if (pending_form_data_ && CalculateFormSignature(*pending_form_data_) !=
                                CalculateFormSignature(form)) {
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

  client().GetVotesUploader().MaybeStartVoteUploadProcess(
      std::move(upload_form),
      /*observed_submission=*/false, GetCurrentPageLanguage(),
      metrics_->initial_interaction_timestamp, last_unlocked_credit_card_cvc_,
      driver().GetPageUkmSourceId());
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

  if (client().IsAutofillProfileEnabled()) {
    metrics_->address_form_event_logger.OnWillSubmitForm(*submitted_form);
  }
  if (client().IsAutofillPaymentMethodsEnabled()) {
    metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
        metrics_->signin_state_for_metrics);
    metrics_->credit_card_form_event_logger.OnWillSubmitForm(*submitted_form);
  }

  if (client().IsAutofillProfileEnabled()) {
    metrics_->address_form_event_logger.OnFormSubmitted(*submitted_form);
    metrics_->address_form_event_logger
        .LogAutofillAddressOnTypingCorrectnessMetrics(*submitted_form);
  }
  if (client().IsAutofillPaymentMethodsEnabled()) {
    metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
        metrics_->signin_state_for_metrics);
    metrics_->credit_card_form_event_logger.OnFormSubmitted(*submitted_form);
    if (touch_to_fill_delegate_) {
      touch_to_fill_delegate_->LogMetricsAfterSubmission(*submitted_form);
    }
  }
}

void BrowserAutofillManager::OnTextFieldValueChangedImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const base::TimeTicks timestamp) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form.global_id(), field_id, &form_structure,
                             &autofill_field)) {
    return;
  }

  // Log events when user edits the field.
  // If the user types into the same field multiple times, repeated
  // TypingFieldLogEvents are coalesced.
  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  autofill_field->AppendLogEventIfNotRepeated(TypingFieldLogEvent{
      .has_value_after_typing = ToOptionalBoolean(!field.value().empty())});

  UpdatePendingForm(form);

  if (!metrics_->user_did_type || autofill_field->is_autofilled()) {
    metrics_->user_did_type = true;
    client().GetFormInteractionsUkmLogger().LogTextFieldValueChanged(
        driver().GetPageUkmSourceId(), *form_structure, *autofill_field);
  }

  auto* logger = GetEventFormLogger(*autofill_field);
  if (autofill_field->is_autofilled()) {
    autofill_field->set_is_autofilled(false);
    autofill_field->set_previously_autofilled(true);
    if (logger) {
      logger->OnEditedAutofilledField(field.global_id());
    }
    if (AutofillAiDelegate* delegate = client().GetAutofillAiDelegate();
        delegate &&
        autofill_field->filling_product() == FillingProduct::kAutofillAi) {
      delegate->OnEditedAutofilledField(*form_structure, *autofill_field,
                                        driver().GetPageUkmSourceId());
    }
  } else {
    if (logger) {
      logger->OnEditedNonFilledField(field.global_id());
    }
  }
  UpdateInitialInteractionTimestamp(timestamp);
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
    if (field.properties_mask() & kUserTyped) {
      context.suppress_reason = SuppressReason::kInsecureForm;
    } else {
      context.should_show_mixed_content_warning = true;
    }
    return context;
  }
  context.is_context_secure = !IsFormNonSecure(form);

  context.is_autofill_available =
      client().IsAutofillEnabled() &&
      (IsAutofillManuallyTriggered(trigger_source) || got_autofillable_form);

  return context;
}

void BrowserAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source,
    base::optional_ref<const PasswordSuggestionRequest> password_request) {
  if (password_request.has_value()) {
    if (PasswordManagerDelegate* password_delegate =
            client().GetPasswordManagerDelegate(field_id)) {
#if !BUILDFLAG(IS_ANDROID)
      password_delegate->ShowSuggestions(password_request->field);
#else
      password_delegate->ShowKeyboardReplacingSurface(password_request.value());
#endif  // !BUILDFLAG(IS_ANDROID)
      return;
    }
  }

  if (base::FeatureList::IsEnabled(features::kAutofillDisableFilling)) {
    return;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  // We cannot early-return here because GetCachedFormAndField() yields nullptr
  // even if there it finds a FormStructure but its `autofill_count()` is 0. In
  // such cases, we still need to offer Autocomplete. Therefore, the code below,
  // including called functions, must handle `form_structure == nullptr` and
  // `autofill_field == nullptr`.
  std::ignore = GetCachedFormAndField(form.global_id(), field_id,
                                      &form_structure, &autofill_field);

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

  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  external_delegate_->OnQuery(form, field, caret_bounds, trigger_source,
                              /*update_datalist=*/true);
  // TODO(crbug.com/409962888): Cleanup once the new logic is launched.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillNewSuggestionGeneration)) {
    GenerateSuggestionsAndMaybeShowUIPhase1(form, field, trigger_source);
    return;
  }
  // Suggestion generators lifespan should be limited to only when they are
  // needed.
  suggestion_generators_.clear();
  // TODO(crbug.com/409962888): Populate `suggestion_generators_` here.
  suggestion_generators_.emplace_back(
      std::make_unique<IbanSuggestionGenerator>());

  SuggestionsContext context = BuildSuggestionsContext(
      form, form_structure, field, autofill_field, trigger_source);

  auto barrier_callback = base::BarrierCallback<std::pair<
      FillingProduct, std::vector<SuggestionGenerator::SuggestionData>>>(
      suggestion_generators_.size(),
      base::BindOnce(&BrowserAutofillManager::OnSuggestionDataFetched,
                     weak_ptr_factory_.GetWeakPtr(), form, field,
                     trigger_source, context));

  for (const auto& suggestion_generator : suggestion_generators_) {
    suggestion_generator->FetchSuggestionData(*form_structure, *autofill_field,
                                              client(), barrier_callback);
  }
}

void BrowserAutofillManager::OnSuggestionDataFetched(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    SuggestionsContext context,
    std::vector<std::pair<FillingProduct,
                          std::vector<SuggestionGenerator::SuggestionData>>>
        suggestion_data) {
  auto barrier_callback =
      base::BarrierCallback<SuggestionGenerator::ReturnedSuggestions>(
          suggestion_generators_.size(),
          base::BindOnce(
              &BrowserAutofillManager::OnIndividualSuggestionsGenerated,
              weak_ptr_factory_.GetWeakPtr(), form, field, trigger_source,
              context));

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form.global_id(), field.global_id(),
                             &form_structure, &autofill_field)) {
    // Form is not autofillable, or either the form or the field cannot be
    // found.
    return;
  }

  for (const auto& suggestion_generator : suggestion_generators_) {
    suggestion_generator->GenerateSuggestions(
        *form_structure, *autofill_field, suggestion_data, barrier_callback);
  }
}

void BrowserAutofillManager::OnIndividualSuggestionsGenerated(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    SuggestionsContext context,
    std::vector<SuggestionGenerator::ReturnedSuggestions>
        returned_suggestions) {
  // TODO(crbug.com/409962888): Add logic to discard/merge
  // `returned_suggestions` into a single list.
  std::vector<Suggestion> suggestions;
  for (const auto& [filling_product, filling_suggestions] :
       returned_suggestions) {
    suggestions.insert(suggestions.end(), filling_suggestions.begin(),
                       filling_suggestions.end());
  }

  OnGenerateSuggestionsComplete(form, field, trigger_source, context, true,
                                suggestions, std::nullopt);
  // Suggestion generators lifespan should be limited to only when they are
  // needed.
  suggestion_generators_.clear();
}

void BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase1(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  const AutofillPlusAddressDelegate* plus_address_delegate =
      client().GetPlusAddressDelegate();
  // Note that this function cannot exit early in case GetCachedFormAndField()
  // yields nullptrs for form_structure and autofill_field. This happens in case
  // autofill_count() returns 0 (i.e. the number of autofillable fields is 0).
  // Even if autofill cannot fill the form, Autocomplete gets a chance to fill
  // the form. Therefore:
  // * the following code needs to be executed (autocomplete is handled further
  //   down in the code path)
  // * the following code needs to gracefully deal with the situation that
  //   form_structure and autofill_field are null.
  std::ignore = GetCachedFormAndField(form.global_id(), field.global_id(),
                                      &form_structure, &autofill_field);

  SuggestionsContext context = BuildSuggestionsContext(
      form, form_structure, field, autofill_field, trigger_source);
  context.field_is_relevant_for_plus_addresses =
      IsPlusAddressesManuallyTriggered(trigger_source) ||
      (!context.should_show_mixed_content_warning &&
       context.is_autofill_available &&
       !context.do_not_generate_autofill_suggestions && autofill_field &&
       plus_address_delegate &&
       plus_address_delegate->IsFieldEligibleForPlusAddress(*autofill_field) &&
       plus_address_delegate->IsPlusAddressFillingEnabled(
           client().GetLastCommittedPrimaryMainFrameOrigin()));

  auto generate_suggestions_and_maybe_show_ui_phase2 = base::BindOnce(
      &BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase2,
      weak_ptr_factory_.GetWeakPtr(), form, field, trigger_source, context);

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
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    SuggestionsContext context,
    std::vector<std::string> plus_addresses) {
  OnGenerateSuggestionsCallback callback = base::BindOnce(
      &BrowserAutofillManager::OnGenerateSuggestionsComplete,
      weak_ptr_factory_.GetWeakPtr(), form, field, trigger_source, context);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  // This function cannot exit early in case GetCachedFormAndField() yields
  // `nullptrs` for `form_structure` and `autofill_field`. See the comment in
  // `GenerateSuggestionsAndMaybeShowUIPhase1` for context.
  std::ignore = GetCachedFormAndField(form.global_id(), field.global_id(),
                                      &form_structure, &autofill_field);
  autofill_metrics::SuggestionRankingContext ranking_context;
  std::vector<Suggestion> suggestions = GetAvailableSuggestions(
      form, form_structure, field, autofill_field, trigger_source,
      GetPlusAddressOverride(client().GetPlusAddressDelegate(), plus_addresses),
      context, ranking_context);

  if (context.is_autofill_available &&
      ShouldSuppressSuggestions(context.suppress_reason, log_manager())) {
    if (context.suppress_reason == SuppressReason::kAblation) {
      CHECK(suggestions.empty());
      client().GetSingleFieldFillRouter().CancelPendingQueries();
      std::move(callback).Run(/*show_suggestions=*/true, {}, std::nullopt);
    }
    return;
  }
  AutofillAiDelegate* delegate = client().GetAutofillAiDelegate();
  if (form_structure && autofill_field &&
      !context.do_not_generate_autofill_suggestions &&
      GetFieldsFillableByAutofillAi(*form_structure, client())
          .contains(field.global_id())) {
    std::move(callback).Run(
        /*show_suggestions=*/true,
        delegate->GetSuggestions(form.global_id(), field.global_id()),
        /*ranking_context=*/std::nullopt);
    return;
  } else if (suggestions.empty() && delegate &&
             delegate->ShouldDisplayIph(form.global_id(), field.global_id()) &&
             client().ShowAutofillFieldIphForFeature(
                 field, AutofillClient::IphFeature::kAutofillAi)) {
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
  // shown alongside single field form fill suggestions. Plus address
  // suggestions are not shown if the plus address email override was applied on
  // at least one address suggestion.
  const bool should_offer_plus_addresses_with_profiles =
      context.field_is_relevant_for_plus_addresses && autofill_field &&
      autofill_field->Type().group() == FieldTypeGroup::kEmail &&
      !suggestions.empty() &&
      !WasEmailOverrideAppliedOnSuggestions(suggestions);
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
            client().IsOffTheRecord(), form, field,
            GetFieldTypeGroupsFromFormStructure(form_structure),
            password_form_classification, trigger_source);

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

  // Whether or not to request single field form fill suggestions.
  const bool should_offer_single_field_form_fill =
      should_offer_other_suggestions &&
      ShouldOfferSingleFieldFill(field, autofill_field, trigger_source,
                                 context.suppress_reason);

  // Whether or not to show plus address suggestions.
  const bool should_offer_plus_addresses =
      context.field_is_relevant_for_plus_addresses && autofill_field &&
      (autofill_field->Type().group() == FieldTypeGroup::kEmail ||
       autofill_field->Type().GetStorableType() == FieldType::USERNAME ||
       autofill_field->Type().GetStorableType() == FieldType::SINGLE_USERNAME);

  // Early return to avoid running password form classifications.
  if (!should_offer_plus_addresses && !should_offer_single_field_form_fill) {
    std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions),
                            std::nullopt);
    return;
  }

  const PasswordFormClassification password_form_classification =
      client().ClassifyAsPasswordForm(*this, form.global_id(),
                                      field.global_id());

  std::vector<Suggestion> plus_address_suggestions;
  if (should_offer_plus_addresses) {
    plus_address_suggestions =
        client().GetPlusAddressDelegate()->GetSuggestionsFromPlusAddresses(
            plus_addresses, client().GetLastCommittedPrimaryMainFrameOrigin(),
            client().IsOffTheRecord(), form, field,
            GetFieldTypeGroupsFromFormStructure(form_structure),
            password_form_classification, trigger_source);
  }

  auto on_single_field_suggestions_callback = base::BindOnce(
      &BrowserAutofillManager::
          OnGeneratedPlusAddressAndSingleFieldFillSuggestions,
      weak_ptr_factory_.GetWeakPtr(),
      AutofillPlusAddressDelegate::SuggestionContext::kAutocomplete,
      password_form_classification.type, form, field,
      should_offer_single_field_form_fill, std::move(callback),
      std::move(plus_address_suggestions));

  if (should_offer_single_field_form_fill) {
    // Generating single field suggestions.
    auto on_suggestions_returned = base::BindOnce(
        [](base::OnceCallback<void(std::vector<Suggestion>)> callback,
           FieldGlobalId field_id, const std::vector<Suggestion>& suggestions) {
          std::move(callback).Run(suggestions);
        },
        std::move(on_single_field_suggestions_callback));
    if (form_structure && autofill_field &&
        client().GetPaymentsAutofillClient()->GetMerchantPromoCodeManager() &&
        client()
            .GetPaymentsAutofillClient()
            ->GetMerchantPromoCodeManager()
            ->OnGetSingleFieldSuggestions(*form_structure, field,
                                          *autofill_field, client(),
                                          on_suggestions_returned)) {
      return;
    }
    if (form_structure && autofill_field &&
        client().GetPaymentsAutofillClient()->GetIbanManager() &&
        client()
            .GetPaymentsAutofillClient()
            ->GetIbanManager()
            ->OnGetSingleFieldSuggestions(*form_structure, field, *autofill_field, client(),
                                          on_suggestions_returned)) {
      return;
    }
    if (client().GetAutocompleteHistoryManager()->OnGetSingleFieldSuggestions(
            field, client(), on_suggestions_returned)) {
      return;
    }

    client().GetAutocompleteHistoryManager()->CancelPendingQueries();
    std::move(on_suggestions_returned).Run(field.global_id(), {});
  } else {
    std::move(on_single_field_suggestions_callback)
        .Run(/*single_field_suggestions=*/{});
  }
}

void BrowserAutofillManager::
    OnGeneratedPlusAddressAndSingleFieldFillSuggestions(
        AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
        PasswordFormClassification::Type password_form_type,
        const FormData& form,
        const FormFieldData& field,
        bool should_offer_single_field_form_fill,
        OnGenerateSuggestionsCallback callback,
        std::vector<Suggestion> plus_address_suggestions,
        std::vector<Suggestion> single_field_suggestions) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(plus_address_suggestions.size() +
                      single_field_suggestions.size());
  // Prioritize plus address over single field form fill suggestions.
  suggestions.insert(suggestions.cend(),
                     std::make_move_iterator(plus_address_suggestions.begin()),
                     std::make_move_iterator(plus_address_suggestions.end()));
  suggestions.insert(suggestions.cend(),
                     std::make_move_iterator(single_field_suggestions.begin()),
                     std::make_move_iterator(single_field_suggestions.end()));

  if (suggestions.empty()) {
    // Note the check below is the same done for regular autocomplete
    // suggestions.
    // TODO(crbug.com/381994105): Consider adding
    // `should_offer_autofill_on_typing()` to `FormFieldData`.
    if (should_offer_single_field_form_fill && field.should_autocomplete() &&
        base::FeatureList::IsEnabled(
            features::kAutofillAddressSuggestionsOnTyping)) {
      // Try to build `Suggestion::kAddressEntryOnTyping` suggestions.
      // Note that these suggestions are always displayed on their own.
      std::move(callback).Run(
          /*show_suggestions=*/true,
          GetSuggestionsOnTypingForProfile(
              client().GetPersonalDataManager().address_data_manager(),
              field.value()),
          std::nullopt);
    } else {
      std::move(callback).Run(/*show_suggestions=*/true, {}, std::nullopt);
    }
    return;
  }

  if (!plus_address_suggestions.empty()) {
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
  bool form_and_field_cached = GetCachedFormAndField(
      form.global_id(), field.global_id(), &form_structure, &autofill_field);
  if (trigger_source ==
          AutofillSuggestionTriggerSource::kFormControlElementClicked &&
      form_and_field_cached) {
    autofill_field->AppendLogEventIfNotRepeated(AskForValuesToFillFieldLogEvent{
        .has_suggestion = ToOptionalBoolean(!suggestions.empty()),
        .suggestion_is_shown = ToOptionalBoolean(show_suggestions),
    });
  }

  // When a user interacts with the credit card form on the merchant checkout
  // pages, `this` checks `amount_extraction_manager_` if amount extraction
  // should happen, and if so, triggers amount extraction.
  if (autofill_field) {
    const DenseSet<AmountExtractionManager::EligibleFeature> eligible_features =
        amount_extraction_manager_->GetEligibleFeatures(
            context,
            ShouldSuppressSuggestions(context.suppress_reason, log_manager()),
            !suggestions.empty(), autofill_field->Type().GetStorableType());

    if (!eligible_features.empty()) {
      for (AmountExtractionManager::EligibleFeature eligible_feature :
           eligible_features) {
        switch (eligible_feature) {
          case AmountExtractionManager::EligibleFeature::kBnpl:
            GetPaymentsBnplManager()->NotifyOfSuggestionGeneration(
                trigger_source);
            continue;
        }
        NOTREACHED();
      }
      amount_extraction_manager_->TriggerCheckoutAmountExtraction();
    }
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

void BrowserAutofillManager::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FieldGlobalId& field_id,
    const FillingPayload& filling_payload,
    AutofillTriggerSource trigger_source) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form.global_id(), field_id, &form_structure,
                             &autofill_field)) {
    return;
  }
  std::visit(base::Overloaded{
                 [&](const AutofillProfile*) {
                   form_filler_->FillOrPreviewForm(
                       action_persistence, form, filling_payload,
                       CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field),
                       trigger_source);
                 },
                 [&](const CreditCard* credit_card) {
                   // We still need to take care of authentication flows,
                   // which is why we do not forward right away to
                   // FormFiller.
                   FillOrPreviewCreditCardForm(action_persistence, form,
                                               CHECK_DEREF(form_structure),
                                               CHECK_DEREF(autofill_field),
                                               *credit_card, trigger_source);
                 },
                 [&](const EntityInstance*) {
                   form_filler_->FillOrPreviewForm(
                       action_persistence, form, filling_payload,
                       CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field),
                       trigger_source);
                 },
                 [&](const VerifiedProfile*) {
                   form_filler_->FillOrPreviewForm(
                       action_persistence, form, filling_payload,
                       CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field),
                       trigger_source);
                 }},
             filling_payload);
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
  std::ignore = GetCachedFormAndField(form.global_id(), field.global_id(),
                                      &form_structure, &autofill_field);
  const FillingProduct filling_product =
      GetFillingProductFromSuggestionType(type);
  form_filler_->FillOrPreviewField(action_persistence, action_type, field,
                                   autofill_field, value, filling_product,
                                   field_type_used);
  if (action_persistence == mojom::ActionPersistence::kFill &&
      type == SuggestionType::kAddressFieldByFieldFilling) {
    metrics_->address_form_event_logger.OnFilledByFieldByFieldFilling(type);
  }
}

void BrowserAutofillManager::OnDidFillAddressFormFillingSuggestion(
    const AutofillProfile& profile,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    AutofillTriggerSource trigger_source) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form_id, field_id, &form_structure,
                             &autofill_field)) {
    return;
  }
  metrics_->address_form_event_logger.OnDidFillFormFillingSuggestion(
      profile, *form_structure, *autofill_field, trigger_source);
}

void BrowserAutofillManager::OnDidFillAddressOnTypingSuggestion(
    const FieldGlobalId& field_id,
    const std::u16string& value,
    FieldType field_type_used_to_build_suggestion,
    const std::string& profile_used_guid) {
  metrics_->address_form_event_logger.OnDidAcceptAutofillOnTyping(
      field_id, value, field_type_used_to_build_suggestion, profile_used_guid);
}

void BrowserAutofillManager::UndoAutofill(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& trigger_field) {
  FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }
  const AutofillField* autofill_trigger_field =
      form_structure->GetFieldById(trigger_field.global_id());
  if (!autofill_trigger_field) {
    return;
  }

  FillingProduct filling_product = autofill_trigger_field->filling_product();
  form_filler_->UndoAutofill(action_persistence, form, *form_structure,
                             trigger_field, filling_product);

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
    const FormStructure& form_structure,
    const AutofillField& autofill_field,
    const CreditCard& credit_card,
    AutofillTriggerSource trigger_source) {
  bool require_card_fetching = [&] {
    if (action_persistence == mojom::ActionPersistence::kPreview) {
      return false;
    }
    switch (trigger_source) {
      case AutofillTriggerSource::kPopup:
      case AutofillTriggerSource::kKeyboardAccessory:
      case AutofillTriggerSource::kTouchToFillCreditCard:
        return ShouldFetchCreditCard(form, form_structure, autofill_field,
                                     credit_card);
      case AutofillTriggerSource::kScanCreditCard:
      case AutofillTriggerSource::kDevtools:
      case AutofillTriggerSource::kFastCheckout:
        return false;
      case AutofillTriggerSource::kFormsSeen:
      case AutofillTriggerSource::kSelectOptionsChanged:
      case AutofillTriggerSource::kJavaScriptChangedAutofilledValue:
      case AutofillTriggerSource::kManualFallback:
      case AutofillTriggerSource::kAutofillAi:
      case AutofillTriggerSource::kNone:
        NOTREACHED();
    }
  }();
  CHECK(action_persistence != mojom::ActionPersistence::kPreview ||
        !require_card_fetching);

  // Called either synchronously (if the card doesn't have to be fetched) or
  // asynchronously (when the card has been successfully fetched).
  auto fill_or_preview = [](BrowserAutofillManager& self,
                            mojom::ActionPersistence action_persistence,
                            const FormData& form, const FieldGlobalId& field_id,
                            const CreditCard& credit_card,
                            AutofillTriggerSource trigger_source) {
    const FormFieldData* const field = form.FindFieldByGlobalId(field_id);
    FormStructure* form_structure = nullptr;
    AutofillField* autofill_field = nullptr;
    if (!IsValidFormData(form) || !field || !IsValidFormFieldData(*field) ||
        !self.GetCachedFormAndField(form.global_id(), field_id, &form_structure,
                                    &autofill_field)) {
      return;
    }
    self.form_filler_->FillOrPreviewForm(
        action_persistence, form, &credit_card, CHECK_DEREF(form_structure),
        CHECK_DEREF(autofill_field), trigger_source);
  };

  // Callback when the credit was feched asynchronously.
  // Ultimately fills the form by calling `fill_or_preview`.
  auto on_fetched = [](base::WeakPtr<BrowserAutofillManager> self,
                       decltype(fill_or_preview) fill_or_preview,
                       const FormData& form, const FieldGlobalId& field_id,
                       AutofillTriggerSource fetched_credit_card_trigger_source,
                       const CreditCard& credit_card) {
    if (!self) {
      return;
    }

    self->last_unlocked_credit_card_cvc_ = credit_card.cvc();
    // If the synced down card is a virtual card or a server card enrolled in
    // runtime retrieval, let the client know so that it can show the UI to help
    // user to manually fill the form, if needed. Masked server card was set to
    // kFullServerCard before filling as the filling process sets its full card
    // number which converts it to a full server card.
    if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard ||
        (credit_card.record_type() == CreditCard::RecordType::kFullServerCard &&
         credit_card.card_info_retrieval_enrollment_state() ==
             CreditCard::CardInfoRetrievalEnrollmentState::
                 kRetrievalEnrolled)) {
      DCHECK(!credit_card.cvc().empty());
      self->client().GetFormDataImporter()->CacheFetchedVirtualCard(
          credit_card.LastFourDigits());

      FilledCardInformationBubbleOptions options;
      options.masked_card_name = credit_card.CardNameForAutofillDisplay();
      options.masked_card_number_last_four =
          credit_card.ObfuscatedNumberWithVisibleLastFourDigits();
      options.filled_card = credit_card;
      // TODO(crbug.com/40927041): Remove CVC from
      // FilledCardInformationBubbleOptions.
      options.cvc = credit_card.cvc();
      options.card_image = self->GetCardImage(credit_card);
      self->client().GetPaymentsAutofillClient()->OnCardDataAvailable(options);
    }

    // After a server card is fetched, save its instrument id.
    self->client().GetFormDataImporter()->SetFetchedCardInstrumentId(
        credit_card.instrument_id());

    if (credit_card.record_type() == CreditCard::RecordType::kFullServerCard ||
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard) {
      self->GetCreditCardAccessManager().CacheUnmaskedCardInfo(
          credit_card, credit_card.cvc());
    }

    fill_or_preview(*self, mojom::ActionPersistence::kFill, form, field_id,
                    credit_card, fetched_credit_card_trigger_source);
  };

  if (action_persistence == mojom::ActionPersistence::kFill) {
    metrics_->credit_card_form_event_logger.OnDidSelectCardSuggestion(
        credit_card, form_structure, metrics_->signin_state_for_metrics);
  }

  // Represents cases where credit cards are fetched independently of the
  // typical card unmasking flow. In these cases, the cards already went through
  // an authentication process, but still require all of the functionality of
  // `on_fetched`.
  // TODO(crbug.com/401566102): Move `on_fetched` and fetching logic out of
  // FillOrPreviewCreditCardForm() and pass it in as a param, so that the moment
  // FillOrPreviewCreditCardForm() is called, the card is just filled without
  // side effects, and `on_fetched` logic will be triggered after if present.
  bool fetched_independently = credit_card.is_bnpl_card();

  if (require_card_fetching) {
    GetCreditCardAccessManager().FetchCreditCard(
        &credit_card,
        base::BindOnce(on_fetched, weak_ptr_factory_.GetWeakPtr(),
                       fill_or_preview, form, autofill_field.global_id(),
                       trigger_source));
  } else if (fetched_independently) {
    // Cards fetched independently, such as for BNPL, have all of their data on
    // creation and do not need further fetching.
    on_fetched(weak_ptr_factory_.GetWeakPtr(), fill_or_preview, form,
               autofill_field.global_id(), trigger_source, credit_card);
  } else {
    fill_or_preview(*this, action_persistence, form, autofill_field.global_id(),
                    credit_card, trigger_source);
  }
}

void BrowserAutofillManager::OnFocusOnNonFormFieldImpl() {
  // TODO(crbug.com/349982907): This function is not called on iOS.

  ProcessPendingFormForUpload();

  if (external_delegate_->HasActiveScreenReader()) {
    external_delegate_->OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
  }
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

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form.global_id(), field_id, &form_structure,
                             &autofill_field)) {
    return;
  }
  autofill_field->set_was_focused(true);

  // Notify installed screen readers if the focus is on a field for which there
  // are suggestions to present. Ignore if a screen reader is not present.
  if (!external_delegate_->HasActiveScreenReader()) {
    return;
  }

  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
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
  std::vector<Suggestion> suggestions = GetAvailableSuggestions(
      form, form_structure, field, autofill_field,
      AutofillSuggestionTriggerSource::kUnspecified,
      /*plus_address_email_override=*/std::nullopt, context, ranking_context);
  external_delegate_->OnAutofillAvailabilityEvent(
      (context.suppress_reason == SuppressReason::kNotSuppressed &&
       !suggestions.empty())
          ? mojom::AutofillSuggestionAvailability::kAutofillAvailable
          : mojom::AutofillSuggestionAvailability::kNoSuggestions);
}

void BrowserAutofillManager::OnSelectControlSelectionChangedImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillRecordCorrectionOfSelectElements)) {
    return;
  }
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form.global_id(), field_id, &form_structure,
                             &autofill_field)) {
    return;
  }

  UpdatePendingForm(form);

  auto* logger = GetEventFormLogger(*autofill_field);
  if (autofill_field->is_autofilled()) {
    autofill_field->set_is_autofilled(false);
    autofill_field->set_previously_autofilled(true);
    if (logger) {
      logger->OnEditedAutofilledField(autofill_field->global_id());
    }
    if (AutofillAiDelegate* delegate = client().GetAutofillAiDelegate();
        delegate &&
        autofill_field->filling_product() == FillingProduct::kAutofillAi) {
      delegate->OnEditedAutofilledField(*form_structure, *autofill_field,
                                        driver().GetPageUkmSourceId());
    }
  }
  // Note that compared to `BAM::OnTextFieldValueChangedImpl()` this function
  // differs in that we do not call `logger->OnEditedNonFilledField()` if the
  // edited select element was not autofilled at the time of the edit. Reason is
  // that this would only make a difference in the following two scenarios:
  // 1) The user modifies a select field in a form while leaving all other
  //    fields untouched. This case is probably uninteresting for Autofill.
  // 2) JavaScript edits a select field in a form that the user didn't interact
  //    with at all (But maybe after the user clicked on the page somewhere so
  //    that the edited frame would have transient activation). This case should
  //    not be included as it doesn't qualify as an edit.
  // Should the metrics start recording the number of edits or anything related
  // to the volume of edits, the decision to not call this function should be
  // revisited, for now we only care about whether the form was edited or not.
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
    base::span<const Suggestion> suggestions,
    const FormData& form,
    const FieldGlobalId& field_id,
    AutofillExternalDelegate::UpdateSuggestionsCallback
        update_suggestions_callback) {
  NotifyObservers(&Observer::OnSuggestionsShown);

  const DenseSet<SuggestionType> shown_suggestion_types(suggestions,
                                                        &Suggestion::type);

  if (shown_suggestion_types.contains(
          SuggestionType::kCreateNewPlusAddressInline)) {
    if (auto* plus_address_delegate = client().GetPlusAddressDelegate()) {
      plus_address_delegate->OnShowedInlineSuggestion(
          client().GetLastCommittedPrimaryMainFrameOrigin(), suggestions,
          update_suggestions_callback);
    }
  }

  if (std::ranges::any_of(suggestions, [](const Suggestion& suggestion) {
        const Suggestion::AutofillProfilePayload* profile_payload =
            std::get_if<Suggestion::AutofillProfilePayload>(
                &suggestion.payload);
        return profile_payload && !profile_payload->email_override.empty();
      })) {
    base::RecordAction(
        base::UserMetricsAction("PlusAddresses.AddressFillSuggestionShown"));
  }

  if (shown_suggestion_types.contains(SuggestionType::kIbanEntry) &&
      client().GetPaymentsAutofillClient()->GetIbanManager()) {
    client()
        .GetPaymentsAutofillClient()
        ->GetIbanManager()
        ->OnIbanSuggestionsShown(field_id);
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  const bool has_cached_form_and_field = GetCachedFormAndField(
      form.global_id(), field_id, &form_structure, &autofill_field);

  if (AutofillAiDelegate* autofill_ai_delegate =
          client().GetAutofillAiDelegate();
      autofill_ai_delegate && has_cached_form_and_field &&
      std::ranges::any_of(shown_suggestion_types,
                          [](const SuggestionType& type) {
                            return GetFillingProductFromSuggestionType(type) ==
                                   FillingProduct::kAutofillAi;
                          })) {
    autofill_ai_delegate->OnSuggestionsShown(CHECK_DEREF(form_structure),
                                             CHECK_DEREF(autofill_field),
                                             driver().GetPageUkmSourceId());
  }

  // Notify the BNPL manager about suggestion shown if the current shown
  // suggestion list contains a credit card entry.

  if (payments::BnplManager* bnpl_manager = GetPaymentsBnplManager();
      bnpl_manager &&
      shown_suggestion_types.contains(SuggestionType::kCreditCardEntry)) {
    bnpl_manager->OnSuggestionsShown(suggestions, update_suggestions_callback);
  }

  if (shown_suggestion_types.contains(SuggestionType::kDevtoolsTestAddresses)) {
    autofill_metrics::OnDevtoolsTestAddressesShown();
  }

  // `SuggestionType::kAddressEntryOnTyping` suggestions do not depend on
  // Autofill types. Because they can be displayed on any fields and are never
  // mixed with other suggestions, emit its possible logging first and return
  // early.
  if (std::ranges::any_of(shown_suggestion_types, [](SuggestionType type) {
        return type == SuggestionType::kAddressEntryOnTyping;
      })) {
    // Assert that only the expected suggestion types exist. Note that despite
    // `SuggestionType::kDatalistEntry` is optionally added by
    // `AutofillExternalDelegate`, therefore checking for it is also required.
    CHECK(DenseSet<SuggestionType>({SuggestionType::kAddressEntryOnTyping,
                                    SuggestionType::kDatalistEntry,
                                    SuggestionType::kSeparator,
                                    SuggestionType::kManageAddress})
              .contains_all(shown_suggestion_types));
    FieldTypeSet field_types_used;
    std::map<std::string, base::TimeDelta> profile_last_used_time_per_guid;
    const base::Time now = base::Time::Now();
    for (const Suggestion& suggestion : client().GetAutofillSuggestions()) {
      if (suggestion.type != SuggestionType::kAddressEntryOnTyping) {
        continue;
      }
      const Suggestion::AutofillProfilePayload& profile_used_payload =
          std::get<Suggestion::AutofillProfilePayload>(suggestion.payload);
      const AutofillProfile* profile_used =
          client()
              .GetPersonalDataManager()
              .address_data_manager()
              .GetProfileByGUID(profile_used_payload.guid.value());

      profile_last_used_time_per_guid[profile_used_payload.guid.value()] =
          now - profile_used->usage_history().use_date();
      field_types_used.insert(*suggestion.field_by_field_filling_type_used);
    }
    metrics_->address_form_event_logger.OnDidShownAutofillOnTyping(
        field_id, field_types_used, profile_last_used_time_per_guid);
    return;
  }

  if (base::Contains(shown_suggestion_types, FillingProduct::kCreditCard,
                     GetFillingProductFromSuggestionType) &&
      IsCreditCardFidoAuthenticationEnabled()) {
    GetCreditCardAccessManager().PrepareToFetchCreditCard();
  }

  if (shown_suggestion_types.contains(SuggestionType::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
  }

  if (std::ranges::none_of(
          shown_suggestion_types,
          AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId)) {
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
  }
}

void BrowserAutofillManager::OnHidePopupImpl() {
  client().GetSingleFieldFillRouter().CancelPendingQueries();
  client().HideAutofillSuggestions(SuggestionHidingReason::kRendererEvent);
  client().HideAutofillFieldIph();
  if (fast_checkout_delegate_) {
    fast_checkout_delegate_->HideFastCheckout(/*allow_further_runs=*/false);
  }
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->HideTouchToFill();
  }
}

void BrowserAutofillManager::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id) {
  client().GetSingleFieldFillRouter().OnSingleFieldSuggestionSelected(
      suggestion);

  AutofillField* autofill_trigger_field = GetAutofillField(form_id, field_id);
  if (!autofill_trigger_field) {
    return;
  }
  if (IsSingleFieldFillerFillingProduct(
          GetFillingProductFromSuggestionType(suggestion.type))) {
    autofill_trigger_field->AppendLogEventIfNotRepeated(
        TriggerFillFieldLogEvent{
            .data_type =
                GetEventTypeFromSingleFieldSuggestionType(suggestion.type),
            .associated_country_code = "",
            .timestamp = AutofillClock::Now()});
  }
}

bool BrowserAutofillManager::ShouldClearPreviewedForm() {
  return GetCreditCardAccessManager().ShouldClearPreviewedForm();
}

void BrowserAutofillManager::OnSelectFieldOptionsDidChangeImpl(
    const FormData& form) {
  raw_ptr<FormStructure, VectorExperimental> form_structure =
      FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }
  form_filler_->MaybeTriggerRefill(
      form, *form_structure, RefillTriggerReason::kSelectOptionsChanged,
      AutofillTriggerSource::kSelectOptionsChanged);
}

void BrowserAutofillManager::OnJavaScriptChangedAutofilledValueImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
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
  if (!GetCachedFormAndField(form.global_id(), field.global_id(),
                             &form_structure, &autofill_field)) {
    return;
  }
  AnalyzeJavaScriptChangedAutofilledValue(*form_structure, *autofill_field,
                                          field.value().empty());
  form_filler_->MaybeTriggerRefill(
      form, *form_structure, RefillTriggerReason::kExpirationDateFormatted,
      AutofillTriggerSource::kJavaScriptChangedAutofilledValue, field,
      old_value);
}

void BrowserAutofillManager::OnLoadedServerPredictionsImpl(
    base::span<const raw_ptr<FormStructure, VectorExperimental>> forms) {
  const AutofillAiModelCache* const model_cache =
      client().GetAutofillAiModelCache();

  if (!model_cache) {
    return;
  }

  for (raw_ptr<FormStructure, VectorExperimental> form : forms) {
    if (!form) {
      continue;
    }

    if (model_cache->Contains(form->form_signature())) {
      if (MayPerformAutofillAiAction(
              client(),
              AutofillAiAction::kUseCachedServerClassificationModelResults)) {
        AddCachedAutofillAiPredictions(*model_cache, *form);
      }
      continue;
    }

    if (!form->may_run_autofill_ai_model() &&
        !base::FeatureList::IsEnabled(
            features::kAutofillAiAlwaysTriggerServerModel)) {
      continue;
    }
    AutofillAiModelExecutor* model_executor =
        client().GetAutofillAiModelExecutor();
    if (!model_executor) {
      LOG_AF(log_manager())
          << LoggingScope::kAutofillAi
          << "Form for model run detected, but the model is unavailable."
          << Br{} << *form;
      continue;
    }

    if (!MayPerformAutofillAiAction(
            client(), AutofillAiAction::kServerClassificationModel)) {
      LOG_AF(log_manager())
          << LoggingScope::kAutofillAi
          << "Form for model run detected, but the model may not be run."
          << Br{} << *form;
      continue;
    }

    auto deferred_add_cached_autofill_ai_predictions =
        [](base::WeakPtr<AutofillManager> self, const FormGlobalId& form_id) {
          if (!self) {
            return;
          }
          AutofillAiModelCache* model_cache =
              self->client().GetAutofillAiModelCache();
          if (!model_cache) {
            return;
          }
          FormStructure* form = self->FindCachedFormById(form_id);
          if (!form) {
            return;
          }
          AddCachedAutofillAiPredictions(*model_cache, *form);
          auto* self_as_bam = static_cast<BrowserAutofillManager*>(self.get());
          form->RationalizeAndAssignSections(self_as_bam->log_manager());
          self_as_bam->LogCurrentFieldTypes(*form);
          self->NotifyObservers(&Observer::OnFieldTypesDetermined,
                                form->global_id(),
                                Observer::FieldTypeSource::kAutofillAiModel);
        };
    if (features::kAutofillAiServerModelSendPageContent.Get()) {
      LOG_AF(log_manager())
          << LoggingScope::kAutofillAi
          << "Requesting page page content for model run for form." << Br{}
          << *form;
      client().GetAiPageContent(base::BindOnce(
          &AutofillAiModelExecutor::GetPredictions,
          model_executor->GetWeakPtr(), form->ToFormData(),
          base::BindOnce(deferred_add_cached_autofill_ai_predictions,
                         GetWeakPtr())));
    } else {
      LOG_AF(log_manager())
          << LoggingScope::kAutofillAi << "Requesting model run for form."
          << Br{} << *form;
      model_executor->GetPredictions(
          form->ToFormData(),
          base::BindOnce(deferred_add_cached_autofill_ai_predictions,
                         GetWeakPtr()),
          std::nullopt);
    }
  }
}

void BrowserAutofillManager::AnalyzeJavaScriptChangedAutofilledValue(
    const FormStructure& form,
    AutofillField& field,
    bool cleared_value) {
  // We are interested in reporting the events where JavaScript resets an
  // autofilled value immediately after filling. For a reset, the value
  // needs to be empty.
  if (!cleared_value) {
    return;
  }
  std::optional<base::TimeTicks> original_fill_time =
      form.last_filling_timestamp();
  if (!original_fill_time) {
    return;
  }
  base::TimeDelta delta = base::TimeTicks::Now() - *original_fill_time;
  // If the filling happened too long ago, maybe this is just an effect of
  // the user pressing a "reset form" button.
  if (delta >= form_filler_->get_limit_before_refill()) {
    return;
  }
  if (auto* logger = GetEventFormLogger(field)) {
    logger->OnAutofilledFieldWasClearedByJavaScriptShortlyAfterFill(form);
  }
}

void BrowserAutofillManager::OnDidEndTextFieldEditingImpl() {
  external_delegate_->DidEndTextFieldEditing();
  // Should not hide the Touch To Fill surface, since it is an overlay UI
  // which ends editing.
}

const FormData& BrowserAutofillManager::last_query_form() const {
  return external_delegate_->query_form();
}

bool BrowserAutofillManager::ShouldUploadForm(const FormStructure& form) {
  return client().IsAutofillEnabled() && !client().IsOffTheRecord() &&
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

const gfx::Image& BrowserAutofillManager::GetCardImage(
    const CreditCard& credit_card) {
  const gfx::Image* const card_art_image =
      client()
          .GetPersonalDataManager()
          .payments_data_manager()
          .GetCachedCardArtImageForUrl(credit_card.card_art_url());
  return card_art_image
             ? *card_art_image
             : ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                   CreditCard::IconResourceId(credit_card.network()));
}

// Some members are intentionally not recreated or reset here:
// - Used for asynchronous form upload:
//   - `vote_upload_task_runner_`
//   - `weak_ptr_factory_`
// - No need to reset or recreate:
//   - external_delegate_
//   - fast_checkout_delegate_
//   - consider_form_as_secure_for_testing_
void BrowserAutofillManager::Reset() {
  // Process log events and record into UKM when the FormStructure is destroyed.
  for (const auto& [form_id, form_structure] : form_structures()) {
    ProcessFieldLogEventsInForm(*form_structure);
  }
  ProcessPendingFormForUpload();
  DCHECK(!pending_form_data_);

  four_digit_combinations_in_dom_.clear();
  last_unlocked_credit_card_cvc_.clear();
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->Reset();
  }
  form_filler_->Reset();

  // The order below is relevant:
  // `credit_card_access_manager_` has a reference to `metrics_`.
  credit_card_access_manager_.reset();
  metrics_.reset();
  AutofillManager::Reset();
  metrics_.emplace(this);
}

void BrowserAutofillManager::UpdateLoggersReadinessData() {
  if (!client().IsAutofillEnabled()) {
    return;
  }
  GetCreditCardAccessManager().UpdateCreditCardFormEventLogger();
  metrics_->address_form_event_logger.UpdateProfileAvailabilityForReadiness(
      client().GetPersonalDataManager().address_data_manager().GetProfiles());
}

void BrowserAutofillManager::OnDidFillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    FormStructure& form_structure,
    AutofillField& trigger_autofill_field,
    base::span<const FormFieldData*> safe_filled_fields,
    base::span<const AutofillField*> safe_filled_autofill_fields,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const base::flat_set<FieldGlobalId>& safe_field_ids,
    const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
        skip_reasons,
    const FillingPayload& filling_payload,
    AutofillTriggerSource trigger_source,
    std::optional<RefillTriggerReason> refill_trigger_reason) {
  NotifyObservers(&Observer::OnFillOrPreviewDataModelForm,
                  form_structure.global_id(), action_persistence,
                  safe_filled_fields, filling_payload);
  if (action_persistence == mojom::ActionPersistence::kPreview) {
    return;
  }
  CHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);

  autofill_metrics::LogNumberOfFieldsModifiedByAutofill(safe_filled_fields,
                                                        filling_payload);
  if (refill_trigger_reason) {
    autofill_metrics::LogNumberOfFieldsModifiedByRefill(
        *refill_trigger_reason, safe_filled_fields.size());
  }
  AppendFillLogEvents(form, form_structure, trigger_autofill_field,
                      safe_field_ids, skip_reasons, filling_payload,
                      refill_trigger_reason.has_value());
  client().DidFillForm(trigger_source, refill_trigger_reason.has_value());

  std::visit(
      base::Overloaded{[&](const AutofillProfile* profile) {
                         LogAndRecordProfileFill(
                             form_structure, trigger_autofill_field,
                             safe_filled_fields, safe_filled_autofill_fields,
                             *profile, trigger_source,
                             refill_trigger_reason.has_value());
                         MaybeShowPlusAddressEmailOverrideNotification(
                             safe_filled_autofill_fields, safe_filled_fields,
                             *profile, form_structure);
                       },
                       [&](const CreditCard* credit_card) {
                         LogAndRecordCreditCardFill(
                             form_structure, trigger_autofill_field,
                             safe_filled_fields, safe_filled_autofill_fields,
                             filled_field_ids, safe_field_ids, *credit_card,
                             trigger_source, refill_trigger_reason.has_value());
                       },
                       [&](const EntityInstance* entity) {
                         if (AutofillAiDelegate* delegate =
                                 client().GetAutofillAiDelegate()) {
                           delegate->OnDidFillSuggestion(
                               entity->guid(), form_structure,
                               trigger_autofill_field,
                               safe_filled_autofill_fields,
                               driver().GetPageUkmSourceId());
                         }
                       },
                       [&](const VerifiedProfile*) {
                         // TODO(crbug.com/380367784): consider moving the
                         // notification to the delegate here.
                       }},
      filling_payload);
}

void BrowserAutofillManager::AppendFillLogEvents(
    const FormData& form,
    FormStructure& form_structure,
    AutofillField& trigger_autofill_field,
    const base::flat_set<FieldGlobalId>& safe_field_ids,
    const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
        skip_reasons,
    const FillingPayload& filling_payload,
    bool is_refill) {
  std::string country_code;
  if (const AutofillProfile* const* address =
          std::get_if<const AutofillProfile*>(&filling_payload)) {
    country_code =
        base::UTF16ToUTF8((*address)->GetRawInfo(ADDRESS_HOME_COUNTRY));
  }
  TriggerFillFieldLogEvent trigger_fill_field_log_event =
      TriggerFillFieldLogEvent{
          .data_type = GetFillDataTypeFromFillingPayload(filling_payload),
          .associated_country_code = country_code,
          .timestamp = base::Time::Now()};
  trigger_autofill_field.AppendLogEventIfNotRepeated(
      trigger_fill_field_log_event);
  FillEventId fill_event_id = trigger_fill_field_log_event.fill_event_id;

  for (auto [form_field, field] :
       base::zip(form.fields(), form_structure.fields())) {
    const FieldGlobalId field_id = field->global_id();
    const bool has_value_before = !form_field.value().empty();
    const FieldFillingSkipReason skip_reason =
        skip_reasons.at(field_id).empty() ? FieldFillingSkipReason::kNotSkipped
                                          : *skip_reasons.at(field_id).begin();
    if (!IsCheckable(field->check_status())) {
      if (skip_reason == FieldFillingSkipReason::kNotSkipped) {
        field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
            .fill_event_id = fill_event_id,
            .had_value_before_filling = ToOptionalBoolean(has_value_before),
            .autofill_skipped_status = skip_reason,
            .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
            .had_value_after_filling =
                ToOptionalBoolean(safe_field_ids.contains(field_id)),
            .filling_prevented_by_iframe_security_policy =
                safe_field_ids.contains(field_id) ? OptionalBoolean::kFalse
                                                  : OptionalBoolean::kTrue,
            .was_refill = ToOptionalBoolean(is_refill),
        });
      } else {
        field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
            .fill_event_id = fill_event_id,
            .had_value_before_filling = ToOptionalBoolean(has_value_before),
            .autofill_skipped_status = skip_reason,
            .was_autofilled_before_security_policy = OptionalBoolean::kFalse,
            .had_value_after_filling = ToOptionalBoolean(has_value_before),
            .was_refill = ToOptionalBoolean(is_refill),
        });
      }
    }
  }
}

void BrowserAutofillManager::LogAndRecordCreditCardFill(
    FormStructure& form_structure,
    AutofillField& trigger_autofill_field,
    base::span<const FormFieldData*> safe_filled_fields,
    base::span<const AutofillField*> safe_filled_autofill_fields,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const base::flat_set<FieldGlobalId>& safe_field_ids,
    const CreditCard& card,
    AutofillTriggerSource trigger_source,
    bool is_refill) {
  if (is_refill) {
    metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
        metrics_->signin_state_for_metrics);
    metrics_->credit_card_form_event_logger.OnDidRefill(form_structure);
  } else {
    CreditCard card_copy = card;
    if (card.record_type() == CreditCard::RecordType::kFullServerCard) {
      // Create a masked version of the card since the metrics function is
      // interested in the CC suggestion that was accepted, and this card was
      // not a kFullServerCard one.
      card_copy.set_record_type(CreditCard::RecordType::kMaskedServerCard);
      card_copy.SetNumber(card_copy.LastFourDigits());
    }
    metrics_->credit_card_form_event_logger.OnDidFillFormFillingSuggestion(
        card_copy, form_structure, trigger_autofill_field, filled_field_ids,
        safe_field_ids, metrics_->signin_state_for_metrics, trigger_source);

    client().GetPersonalDataManager().payments_data_manager().RecordUseOfCard(
        card);
  }
}

void BrowserAutofillManager::LogAndRecordProfileFill(
    FormStructure& form_structure,
    AutofillField& trigger_autofill_field,
    base::span<const FormFieldData*> safe_filled_fields,
    base::span<const AutofillField*> safe_filled_autofill_fields,
    const AutofillProfile& filled_profile,
    AutofillTriggerSource trigger_source,
    bool is_refill) {
  if (!trigger_autofill_field.ShouldSuppressSuggestionsAndFillingByDefault()) {
    if (is_refill) {
      metrics_->address_form_event_logger.OnDidRefill(form_structure);
    } else {
      metrics_->address_form_event_logger.OnDidFillFormFillingSuggestion(
          filled_profile, form_structure, trigger_autofill_field,
          trigger_source);
    }
  }
  if (!is_refill) {
    client().GetPersonalDataManager().address_data_manager().RecordUseOf(
        filled_profile);
  }
}

void BrowserAutofillManager::MaybeShowPlusAddressEmailOverrideNotification(
    base::span<const AutofillField*> safe_filled_autofill_fields,
    base::span<const FormFieldData*> safe_filled_fields,
    const AutofillProfile& filled_profile,
    const FormStructure& form_structure) {
  // `filled_profile` might have had its email overridden, which is what makes
  // it different from `original_profile`.
  const AutofillProfile* original_profile =
      client().GetPersonalDataManager().address_data_manager().GetProfileByGUID(
          filled_profile.guid());
  if (!original_profile) {
    return;
  }

  const AutofillField* email_autofill_field = nullptr;
  if (auto it = std::ranges::find(safe_filled_autofill_fields, EMAIL_ADDRESS,
                                  [](const AutofillField* field) {
                                    return field->Type().GetStorableType();
                                  });
      it != safe_filled_autofill_fields.end()) {
    email_autofill_field = *it;
  } else {
    return;
  }

  const std::u16string original_email =
      original_profile->GetRawInfo(EMAIL_ADDRESS);
  // Note that the filled `profile` could have been updated with a plus
  // address email override.
  const std::u16string potential_email_override =
      filled_profile.GetRawInfo(EMAIL_ADDRESS);
  // If the user has selected a plus address email override, show a
  // notification.
  if (client().GetPlusAddressDelegate() &&
      client().GetPlusAddressDelegate()->IsPlusAddress(
          base::UTF16ToUTF8(potential_email_override)) &&
      original_email != potential_email_override) {
    client().GetPlusAddressDelegate()->DidFillPlusAddress();
    base::RecordAction(
        base::UserMetricsAction("PlusAddresses.FillAddressSuggestionAccepted"));
    // TODO(crbug.com/324557053): Filter out notifications for suggestion type
    // `SuggestionType::kFillFullEmail`.
    client().ShowPlusAddressEmailOverrideNotification(
        base::UTF16ToUTF8(original_email),
        base::BindOnce(&BrowserAutofillManager::OnEmailOverrideUndone,
                       weak_ptr_factory_.GetWeakPtr(), original_email,
                       form_structure.global_id(),
                       email_autofill_field->global_id()));
  }
}

void BrowserAutofillManager::OnEmailOverrideUndone(
    const std::u16string& original_email,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id) {
  base::RecordAction(
      base::UserMetricsAction("PlusAddresses.FillAddressSuggestionUndone"));
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form_id, field_id, &form_structure,
                             &autofill_field)) {
    return;
  }

  if (autofill_field->Type().GetStorableType() != EMAIL_ADDRESS) {
    return;
  }

  // Fill the address profile's original email.
  form_filler_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      *autofill_field, autofill_field, original_email, FillingProduct::kAddress,
      EMAIL_ADDRESS);
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
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id) const {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form_id, field_id, &form_structure,
                             &autofill_field)) {
    return nullptr;
  }
  return autofill_field;
}

autofill_metrics::CreditCardFormEventLogger&
BrowserAutofillManager::GetCreditCardFormEventLogger() {
  return metrics_->credit_card_form_event_logger;
}

std::vector<Suggestion> BrowserAutofillManager::GetProfileSuggestions(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& trigger_field,
    const AutofillField& trigger_autofill_field,
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<std::string> plus_address_email_override) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  bool should_suppress =
      client()
          .GetPersonalDataManager()
          .address_data_manager()
          .AreAddressSuggestionsBlocked(
              CalculateFormSignature(form),
              CalculateFieldSignatureForField(trigger_field), form.url());
  base::UmaHistogramBoolean("Autofill.Suggestion.StrikeSuppression.Address",
                            should_suppress);
  if (should_suppress &&
      !base::FeatureList::IsEnabled(
          features::test::kAutofillDisableSuggestionStrikeDatabase)) {
    LOG_AF(log_manager()) << LoggingScope::kFilling
                          << LogMessage::kSuggestionSuppressed
                          << " Reason: strike limit reached.";
    // If the user already reached the strike limit on this particular field,
    // address suggestions are suppressed.
    return {};
  }
#endif
  metrics_->address_form_event_logger.OnDidPollSuggestions(
      trigger_field.global_id());

  // If the user triggers suggestions on an autofilled field, field-by-field
  // filling suggestions should be shown so that the user could easily correct
  // values to something present in different stored addresses.
  SuggestionType current_suggestion_type =
      trigger_field.is_autofilled()
          ? SuggestionType::kAddressFieldByFieldFilling
          : SuggestionType::kAddressEntry;

  FieldTypeSet field_types = [&]() -> FieldTypeSet {
    if (current_suggestion_type ==
        SuggestionType::kAddressFieldByFieldFilling) {
      return {trigger_autofill_field.Type().GetStorableType()};
    }
    // If the FormData and FormStructure do not have the same size, we assume
    // as a fallback that all fields are fillable.
    base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>
        skip_reasons;
    if (form.fields().size() == form_structure.field_count()) {
      skip_reasons = form_filler_->GetFieldFillingSkipReasons(
          form.fields(), form_structure, trigger_autofill_field,
          /*type_groups_originally_filled=*/std::nullopt,
          FillingProduct::kAddress, /*is_refill=*/false);
    }
    FieldTypeSet field_types;
    for (size_t i = 0; i < form_structure.field_count(); ++i) {
      if (auto it = skip_reasons.find(form_structure.field(i)->global_id());
          it == skip_reasons.end() || it->second.empty()) {
        field_types.insert(form_structure.field(i)->Type().GetStorableType());
      }
    }
    return field_types;
  }();

  return GetSuggestionsForProfiles(
      client(), field_types, trigger_field,
      trigger_autofill_field.Type().GetStorableType(), current_suggestion_type,
      std::move(plus_address_email_override));
}

std::vector<Suggestion> BrowserAutofillManager::GetCreditCardSuggestions(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& trigger_field,
    const AutofillField& autofill_trigger_field,
    AutofillSuggestionTriggerSource trigger_source,
    autofill_metrics::SuggestionRankingContext& ranking_context) {
  metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
      metrics_->signin_state_for_metrics);
  metrics_->credit_card_form_event_logger.OnDidPollSuggestions(
      trigger_field.global_id());

  std::u16string card_number_field_value = u"";
  bool is_card_number_autofilled = false;

  // Preprocess the form to extract info about card number field.
  for (const FormFieldData& field : form.fields()) {
    if (const AutofillField* autofill_field =
            form_structure.GetFieldById(field.global_id());
        autofill_field &&
        autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER) {
      card_number_field_value += SanitizeCreditCardFieldValue(field.value());
      is_card_number_autofilled |= field.is_autofilled();
    }
  }

  // Offer suggestion for expiration date field if the card number field is
  // empty or the card number field is autofilled.
  auto ShouldOfferSuggestionsForExpirationTypeField = [&] {
    return SanitizedFieldIsEmpty(card_number_field_value) ||
           is_card_number_autofilled;
  };

  if (data_util::IsCreditCardExpirationType(
          autofill_trigger_field.Type().GetStorableType()) &&
      !ShouldOfferSuggestionsForExpirationTypeField()) {
    return {};
  }

  if (IsInAutofillSuggestionsDisabledExperiment()) {
    return {};
  }

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      client(), trigger_field, autofill_trigger_field.Type().GetStorableType(),
      summary,
      form_structure.IsCompleteCreditCardForm(
          FormStructure::CreditCardFormCompleteness::
              kCompleteCreditCardFormIncludingCvcAndName),
      ShouldShowScanCreditCard(form, trigger_field),
      four_digit_combinations_in_dom_,
      /*autofilled_last_four_digits_in_form_for_filtering=*/
      is_card_number_autofilled && card_number_field_value.size() >= 4
          ? card_number_field_value.substr(card_number_field_value.size() - 4)
          : u"");
  bool is_virtual_card_standalone_cvc_field =
      std::ranges::any_of(suggestions, [](Suggestion suggestion) {
        return suggestion.type == SuggestionType::kVirtualCreditCardEntry;
      });
  if (!is_virtual_card_standalone_cvc_field) {
    ranking_context = std::move(summary.ranking_context);
  }

  metrics_->credit_card_form_event_logger.OnDidFetchSuggestion(
      suggestions, summary.with_offer, summary.with_cvc,
      summary.with_card_info_retrieval_enrolled,
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
                                           .payments_data_manager()
                                           .GetPaymentsSigninStateForMetrics();
}

void BrowserAutofillManager::OnFormProcessed(
    const FormData& form,
    const FormStructure& form_structure) {
  // If a standalone cvc field is found in the form, query the DOM for last four
  // combinations. Used to search for the virtual card last four for a virtual
  // card saved on file of a merchant webpage.
  auto contains_standalone_cvc_field =
      std::ranges::any_of(form_structure.fields(), [](const auto& field) {
        return field->Type().GetStorableType() ==
               CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
      });
  if (contains_standalone_cvc_field) {
    FetchPotentialCardLastFourDigitsCombinationFromDOM();
  }
  if (data_util::ContainsPhone(data_util::DetermineGroups(form_structure))) {
    metrics_->has_observed_phone_number_field = true;
  }
  // TODO(crbug.com/41405154): avoid logging developer engagement multiple
  // times for a given form if it or other forms on the page are dynamic.
  LogDeveloperEngagementUkm(client().GetUkmRecorder(),
                            driver().GetPageUkmSourceId(), form_structure);

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
    // database.
    autofill_optimization_guide->OnDidParseForm(
        form_structure,
        client().GetPersonalDataManager().payments_data_manager());
  }

  if (AutofillAiDelegate* delegate = client().GetAutofillAiDelegate()) {
    delegate->OnFormSeen(form_structure);
  }

  // If a form with the same FormGlobalId was previously filled, the structure
  // of the form changed, and we might be able to refill the form with other
  // information.
  form_filler_->MaybeTriggerRefill(form, form_structure,
                                   RefillTriggerReason::kFormChanged,
                                   AutofillTriggerSource::kFormsSeen);
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
    AutofillField& autofill_field,
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
  // AblationGroup::kDefault. Note that it is possible (due to implementation
  // details) that this is incorrectly set to kDefault: If the user has typed
  // some characters into a text field, it may look like no suggestions are
  // available, but in practice the suggestions are just filtered out
  // (Autofill only suggests matches that start with the typed prefix). Any
  // consumers of the conditional_ablation_group attribute should monitor it
  // over time. Any transitions of conditional_ablation_group from {kAblation,
  // kControl} to kDefault should just be ignored and the previously reported
  // value should be used. As the ablation experience is stable within period
  // of time, such a transition typically indicates that the user has typed a
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

  if (ablation_group != AblationGroup::kDefault) {
    autofill_field.AppendLogEventIfNotRepeated(
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

std::vector<Suggestion> BrowserAutofillManager::GetAvailableSuggestions(
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
    return {
        Suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_MIXED_FORM),
                   SuggestionType::kMixedFormMessage)};
  }

  if (!context.is_autofill_available ||
      context.do_not_generate_autofill_suggestions) {
    return {};
  }

  if (!form_structure || !autofill_field) {
    return {};
  }

  std::vector<Suggestion> suggestions;
  switch (context.filling_product) {
    case FillingProduct::kAddress:
      suggestions = GetProfileSuggestions(
          form, *form_structure, field, *autofill_field, trigger_source,
          std::move(plus_address_email_override));
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableEmailOrLoyaltyCardsFilling) &&
          autofill_field->Type().GetStorableType() ==
              EMAIL_OR_LOYALTY_MEMBERSHIP_ID) {
        if (ValuablesDataManager* valuables_manager =
                client().GetValuablesDataManager()) {
          if (suggestions.empty()) {
            suggestions = GetLoyaltyCardSuggestions(
                *valuables_manager,
                client().GetLastCommittedPrimaryMainFrameURL());
          } else {
            ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
                suggestions, *valuables_manager,
                client().GetLastCommittedPrimaryMainFrameURL());
          }
        }
      }
      break;
    case FillingProduct::kCreditCard:
      suggestions = GetCreditCardSuggestions(form, *form_structure, field,
                                             *autofill_field, trigger_source,
                                             ranking_context);
      break;
    case FillingProduct::kLoyaltyCard:
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableLoyaltyCardsFilling)) {
        // Only loyalty card numbers filling is supported.
        if (autofill_field->Type().GetStorableType() == LOYALTY_MEMBERSHIP_ID) {
          if (ValuablesDataManager* valuables_manager =
                  client().GetValuablesDataManager()) {
            suggestions = GetLoyaltyCardSuggestions(
                *valuables_manager,
                client().GetLastCommittedPrimaryMainFrameURL());
          }
        }
      }
      break;
    default:
      // Skip other filling products.
      break;
  }

  if (EvaluateAblationStudy(suggestions, CHECK_DEREF(autofill_field),
                            context)) {
    return {};
  }

  if (const IdentityCredentialDelegate* identity_credential_delegate =
          client().GetIdentityCredentialDelegate()) {
    // Only <input autocomplete="email webidentity"> fields are considered.
    if (std::optional<AutocompleteParsingResult> autocomplete =
            ParseAutocompleteAttribute(
                autofill_field->autocomplete_attribute());
        autocomplete && autocomplete->webidentity) {
      std::vector<Suggestion> verified_suggestions =
          identity_credential_delegate->GetVerifiedAutofillSuggestions(
              autofill_field->Type().GetStorableType());
      // Insert verified suggestions above unverified ones.
      // TODO(crbug.com/380367784): figure out what to do when both verified
      // and unverified suggestions point to the same email address.
      suggestions.insert(suggestions.begin(), verified_suggestions.begin(),
                         verified_suggestions.end());
    }
  }

  // Don't provide credit card suggestions for non-secure pages, but do provide
  // them for secure pages with passive mixed content (see implementation of
  // IsContextSecure).
  if (suggestions.empty() ||
      context.filling_product != FillingProduct::kCreditCard ||
      context.is_context_secure) {
    return suggestions;
  }

  // Replace the suggestion content with a warning message explaining why
  // Autofill is disabled for a website. The string is different if the credit
  // card autofill HTTP warning experiment is enabled.
  return {Suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
      SuggestionType::kInsecureContextPaymentDisabledMessage)};
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

  AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
      client().GetUkmRecorder(), driver().GetPageUkmSourceId(),
      phone_collection_metric_state);

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
      autofill_metrics::ShouldRecordUkm() &&
      form_structure.ShouldUploadUkm(/*require_classified_field=*/true);

  for (const auto& autofill_field : form_structure) {
    if (should_upload_ukm) {
      client().GetFormInteractionsUkmLogger().LogAutofillFieldInfoAtFormRemove(
          driver().GetPageUkmSourceId(), form_structure, *autofill_field,
          AutofillMetrics::AutocompleteStateForSubmittedField(*autofill_field));
    }
  }

  // Log FormSummary UKM event.
  if (should_upload_ukm) {
    autofill_metrics::FormInteractionsUkmLogger::FormEventSet form_events;
    form_events.insert_all(metrics_->address_form_event_logger.GetFormEvents(
        form_structure.global_id()));
    form_events.insert_all(
        metrics_->credit_card_form_event_logger.GetFormEvents(
            form_structure.global_id()));
    client().GetFormInteractionsUkmLogger().LogAutofillFormSummaryAtFormRemove(
        driver().GetPageUkmSourceId(), form_structure, form_events,
        metrics_->initial_interaction_timestamp,
        metrics_->form_submitted_timestamp);
    client().GetFormInteractionsUkmLogger().LogFocusedComplexFormAtFormRemove(
        driver().GetPageUkmSourceId(), form_structure, form_events,
        metrics_->initial_interaction_timestamp,
        metrics_->form_submitted_timestamp);
  }

  if (base::FeatureList::IsEnabled(features::kAutofillUKMExperimentalFields) &&
      !metrics_->form_submitted_timestamp.is_null() &&
      form_structure.ShouldUploadUkm(
          /*require_classified_field=*/false)) {
    client()
        .GetFormInteractionsUkmLogger()
        .LogAutofillFormWithExperimentalFieldsCountAtFormRemove(
            driver().GetPageUkmSourceId(), form_structure);
  }

  for (const auto& autofill_field : form_structure) {
    // Clear log events.
    // Not conditioned on kAutofillLogUKMEventsWithSamplingOnSession because
    // there may be other reasons to log events.
    autofill_field->ClearLogEvents();
  }
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
          std::variant_size<AutofillField::FieldLogEventType>() == 10,
          "When adding new variants check that this function does not "
          "need to be updated.");
      if (std::holds_alternative<AskForValuesToFillFieldLogEvent>(log_event)) {
        ++num_ask_for_values_to_fill_event;
      } else if (std::holds_alternative<TriggerFillFieldLogEvent>(log_event)) {
        ++num_trigger_fill_event;
      } else if (std::holds_alternative<FillFieldLogEvent>(log_event)) {
        ++num_fill_event;
      } else if (std::holds_alternative<TypingFieldLogEvent>(log_event)) {
        ++num_typing_event;
      } else if (std::holds_alternative<HeuristicPredictionFieldLogEvent>(
                     log_event)) {
        ++num_heuristic_prediction_event;
      } else if (std::holds_alternative<AutocompleteAttributeFieldLogEvent>(
                     log_event)) {
        ++num_autocomplete_attribute_event;
      } else if (std::holds_alternative<ServerPredictionFieldLogEvent>(
                     log_event)) {
        ++num_server_prediction_event;
      } else if (std::holds_alternative<RationalizationFieldLogEvent>(
                     log_event)) {
        ++num_rationalization_event;
      } else if (std::holds_alternative<AblationFieldLogEvent>(log_event)) {
        ++num_ablation_event;
      } else {
        NOTREACHED();
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
      NOTREACHED();
  }
}

}  // namespace autofill
