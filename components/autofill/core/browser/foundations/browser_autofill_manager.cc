// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "autofill_client.h"
#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/at_memory/at_memory_manager.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#include "components/autofill/core/browser/data_manager/addresses/account_name_email_strike_manager.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/phone_number.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/filling/form_autofill_history.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/filling/payments/field_filling_payments_util.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/loyalty_cards_metrics.h"
#include "components/autofill/core/browser/metrics/per_fill_metrics.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_manager.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/at_memory/at_memory_nudge_generator.h"
#include "components/autofill/core/browser/suggestions/autocomplete_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/autofill_ai/autofill_ai_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/compose/compose_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/one_time_passwords/otp_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/passkeys/passkey_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/iban_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/browser/suggestions/suggestions_context.h"
#include "components/autofill/core/browser/suggestions/valuables/valuable_suggestion_generator.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace autofill {

using mojom::SubmissionSource;
using payments::AmountExtractionManager;

namespace {

ValuePatternsMetric GetValuePattern(std::u16string_view value) {
  if (IsUPIVirtualPaymentAddress(value)) {
    return ValuePatternsMetric::kUpiVpa;
  }
  if (IsInternationalBankAccountNumber(value)) {
    return ValuePatternsMetric::kIban;
  }
  if (IsAchRoutingTransitNumber(value)) {
    return ValuePatternsMetric::kAchRoutingNumber;
  }
  return ValuePatternsMetric::kNoPatternFound;
}

void LogValuePatternsMetric(const FormData& form) {
  for (const FormFieldData& field : form.fields()) {
    if (!field.is_focusable()) {
      continue;
    }
    const std::u16string_view value =
        base::TrimWhitespace(field.value(), base::TRIM_ALL);
    if (value.empty()) {
      continue;
    }
    base::UmaHistogramEnumeration("Autofill.SubmittedValuePatterns",
                                  GetValuePattern(value));
  }
}

// Returns the filling product likely to be used for suggestions given
// `trigger_field_type`. This might not be the definitive product used because
// for example the product could not yield any suggestion and we'd fallback to
// another product.
FillingProduct GetPreferredSuggestionFillingProduct(AutofillType trigger_type) {
  const FieldType field_type = [&] {
    if (FieldType ft = trigger_type.GetCreditCardType(); ft != UNKNOWN_TYPE) {
      return ft;
    }
    if (FieldType ft = trigger_type.GetAddressType(); ft != UNKNOWN_TYPE) {
      return ft;
    }
    if (FieldType ft = trigger_type.GetLoyaltyCardType(); ft != UNKNOWN_TYPE) {
      return ft;
    }
    if (FieldType ft = trigger_type.GetIdentityCredentialType();
        ft != UNKNOWN_TYPE) {
      return ft;
    }
    FieldTypeSet fts = trigger_type.GetTypes();
    return !fts.empty() ? *fts.begin() : UNKNOWN_TYPE;
  }();
  const FillingProduct filling_product =
      GetFillingProductFromFieldTypeGroup(GroupTypeOfFieldType(field_type));
  // Autofill suggestions fallbacks to autocomplete if no product could be
  // inferred from the suggestion context.
  return filling_product == FillingProduct::kNone
             ? FillingProduct::kAutocomplete
             : filling_product;
}

bool IsSingleFieldFillerFillingProduct(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAutocomplete:
    case FillingProduct::kIban:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kLoyaltyCard:
      return true;
    case FillingProduct::kAutofillAi:
    case FillingProduct::kCompose:
    case FillingProduct::kPasskey:
    case FillingProduct::kPassword:
    case FillingProduct::kCreditCard:
    case FillingProduct::kAddress:
    case FillingProduct::kNone:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
    case FillingProduct::kAtMemory:
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
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kPasswordFieldByFieldFilling:
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
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kPersonalContextNotice:
      NOTREACHED();
  }
  NOTREACHED();
}

void LogAutocompletePredictionCollisionTypeMetrics(
    const FormStructure& form_structure) {
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    const AutofillField* field = form_structure.field(i);
    FieldType heuristic_type = field->heuristic_type();
    FieldType server_type = field->server_type();

    AutofillMetrics::PredictionState prediction_state =
        AutofillMetrics::PredictionState::kNone;
    if (IsFillableFieldType(heuristic_type)) {
      prediction_state = IsFillableFieldType(server_type)
                             ? AutofillMetrics::PredictionState::kBoth
                             : AutofillMetrics::PredictionState::kHeuristics;
    } else if (IsFillableFieldType(server_type)) {
      prediction_state = AutofillMetrics::PredictionState::kServer;
    }

    AutofillMetrics::AutocompleteState autocomplete_state =
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

// Checks if the `credit_card` needs to be fetched in order to complete the
// current filling flow.
// TODO(crbug.com/40227496): Only use parsed data.
bool ShouldFetchCreditCard(
    const FormData& form,
    const FormStructure& form_structure,
    const AutofillField& autofill_field,
    const CreditCard& credit_card,
    AutofillTriggerSource trigger_source,
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) {
  if (credit_card.is_bnpl_card()) {
    // This is a BNPL VCN, so fetching is not needed because an authentication
    // already happened.
    return false;
  }
  if (WillFillCreditCardNumberOrCvc(form.fields(), form_structure.fields(),
                                    autofill_field, trigger_source,
                                    /*card_has_cvc=*/!credit_card.cvc().empty(),
                                    ac_unrecognized_behavior)) {
    return true;
  }
  // This happens for web sites which cache all credit card details except for
  // the cvc, which is different every time the virtual credit card is being
  // used.
  return credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
         autofill_field.Type().GetCreditCardType() ==
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
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
    case AutofillSuggestionTriggerSource::kProactivePasswordRecovery:
    case AutofillSuggestionTriggerSource::kGlic:
    case AutofillSuggestionTriggerSource::kAtMemory:
    case AutofillSuggestionTriggerSource::kAtMemoryContextMenu:
    case AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge:
      return false;
  }
}

void LogSuggestionsCount(const SuggestionsContext& context,
                         const std::vector<Suggestion>& suggestions) {
  if (suggestions.empty()) {
    return;
  }

  if (context.filling_product == FillingProduct::kCreditCard) {
    // TODO(crbug.com/41484171): Move to payments_suggestion_generator_util.cc.
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
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (field->autocomplete_attribute() == "off" &&
        field->did_trigger_suggestions() &&
        !field->all_modifiers().contains(FieldModifier::kAutofill)) {
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

// Returns what `FillingProduct`s should be asked for filling given this
// `trigger_source`.
FillingProductSet GetFillingProductsToSuggest(
    AutofillSuggestionTriggerSource trigger_source) {
  using enum AutofillSuggestionTriggerSource;
  switch (trigger_source) {
    case kUnspecified:
    case kTextareaFocusedWithoutClick:
    case kComposeDialogLostFocus:
    case kComposeDelayedProactiveNudge:
    case kContentEditableClicked:
      return {FillingProduct::kCompose};
    case kPasswordManager:
    case kProactivePasswordRecovery:
    case kPasswordManagerProcessedFocusedField:
    case kManualFallbackPasswords:
      return {FillingProduct::kPassword, FillingProduct::kPasskey};
    case kPlusAddressUpdatedInBrowserProcess:
    case kOpenTextDataListChooser:
    case kFormControlElementClicked:
    case kTextFieldValueChanged:
    case kTextFieldDidReceiveKeyDown:
    case kiOS:
      return FillingProductSet::all();
    case kGlic:
      return {FillingProduct::kAddress, FillingProduct::kCreditCard,
              FillingProduct::kPassword};
    case kAtMemory:
    case kAtMemoryContextMenu:
      return {FillingProduct::kAtMemory};
    case kAtMemoryInactivityNudge:
      return {FillingProduct::kNone};
  }
}

// Populates all the fields (except for ablation study related fields) in
// `SuggestionsContext` based on the given params.
SuggestionsContext BuildSuggestionsContext(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    AutofillSuggestionTriggerSource trigger_source,
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) {
  SuggestionsContext context;

  // When Compose suggestions are requested, there is no need to load Autofill
  // suggestions.
  if (IsTriggerSourceOnlyRelevantForCompose(trigger_source)) {
    context.do_not_generate_autofill_suggestions = true;
  }

  // Don't send suggestions or track forms that should not be parsed.
  if (!form_structure || !autofill_field ||
      !ShouldBeParsed(*form_structure, /*log_manager=*/nullptr)) {
    return context;
  }

  context.filling_product =
      GetPreferredSuggestionFillingProduct(autofill_field->Type());

  if (SuppressSuggestionsForAutocompleteUnrecognizedField(
          *autofill_field, ac_unrecognized_behavior)) {
    // If non-Autocomplete suggestions may be shown on some other field of the
    // form, we want to suppress Autocomplete suggestions on this field.
    // Setting `SuggestionsContext::suppress_reason` to
    // `kAutocompleteUnrecognized` achieves that.
    if (!std::ranges::all_of(
            *form_structure, [ac_unrecognized_behavior](
                                 const std::unique_ptr<AutofillField>& field) {
              return field->ShouldSuppressSuggestionsAndFillingByDefault(
                         ac_unrecognized_behavior) ||
                     field->Type().GetTypes().contains(UNKNOWN_TYPE);
            })) {
      context.suppress_reason = SuppressReason::kAutocompleteUnrecognized;
    }
    context.do_not_generate_autofill_suggestions = true;
  }
  return context;
}

// Triggers the possible import of submitted data at submission time.
void MaybeImportFromSubmittedForm(AutofillClient& client,
                                  ukm::SourceId ukm_source_id,
                                  const FormStructure& form_structure) {
  // This intentionally happens prior to `ImportAndProcessFormData()`. See
  // crbug.com/381205586.
  ProfileTokenQuality::SaveObservationsForFilledFormForAllSubmittedProfiles(
      form_structure, client.GetPersonalDataManager().address_data_manager());

  AutofillAiManager* const ai_manager = client.GetAutofillAiManager();
  const bool autofill_ai_shows_bubble =
      ai_manager && ai_manager->OnFormSubmitted(form_structure, ukm_source_id);
  if (!autofill_ai_shows_bubble) {
    // Update Personal Data with the form's submitted data.
    client.GetFormDataImporter()->ImportAndProcessFormData(
        form_structure, client.IsAutofillProfileEnabled(),
        client.GetPaymentsAutofillClient()->IsAutofillPaymentMethodsEnabled(),
        ukm_source_id);
  }

  std::vector<FormFieldData> fields_for_autocomplete = base::ToVector(
      form_structure,
      [&](const std::unique_ptr<AutofillField>& autofill_field) {
        FormFieldData field = *autofill_field;
        if (autofill_field->Type().GetCreditCardType() ==
            CREDIT_CARD_VERIFICATION_CODE) {
          // However, if Autofill has recognized a field as CVC, that shouldn't
          // be saved.
          field.set_should_autocomplete(false);
        }
        if (autofill_field->Type().GetLoyaltyCardType() ==
                LOYALTY_MEMBERSHIP_ID &&
            autofill_field->last_modifier() == FieldModifier::kAutofill) {
          // Only store loyalty cards values in Autocomplete if they were filled
          // manually.
          field.set_should_autocomplete(false);
        }
        return field;
      });

  // TODO crbug.com/40100455 - Eliminate `form_for_autocomplete`.
  FormData form_for_autocomplete = form_structure.ToFormData();
  form_for_autocomplete.set_fields(std::move(fields_for_autocomplete));
  client.GetSingleFieldFillRouter().OnWillSubmitForm(
      form_for_autocomplete, &form_structure, client.IsAutocompleteEnabled());
}

// Generates a compose suggestion for the given `form` and `field` if conditions
// are met, returns `std::nullopt` otherwise.
// TODO(crbug.com/409962888): Remove once new suggestion generator architecture
// is launched.
std::optional<Suggestion> GenerateComposeSuggestion(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    AutofillClient& client) {
  ComposeSuggestionGenerator suggestion_generator(trigger_source);
  std::vector<Suggestion> suggestions;

  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  // Since the `on_suggestions_generated` callback is called synchronously, we
  // can assume that `suggestions` will hold the correct value.
  suggestion_generator.GenerateSuggestions(
      form, field, /*form_structure=*/nullptr,
      /*trigger_autofill_field=*/nullptr, client, on_suggestions_generated);
  if (suggestions.empty()) {
    return std::nullopt;
  }
  CHECK_EQ(suggestions.size(), 1u);
  return suggestions[0];
}

bool ShouldShowWebauthnHybridEntryPoint(const FormFieldData& field) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  const std::optional<AutocompleteParsingResult>& autocomplete =
      field.parsed_autocomplete();
  return autocomplete.has_value() &&  // Assume no autocomplete if not parsed.
         autocomplete->webauthn;      // Field must have "webauthn" annotation.
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

bool IsManagementFooterOption(const Suggestion& suggestion) {
  switch (suggestion.type) {
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kOpenGemini:
      return true;
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kPersonalContextNotice:
      return false;
  }
}

// Finds the footer section with "Manage" suggestions or adds a separator to
// create a new footer section if there is none. Then, it moves the webauthn
// sign-in fallback item to the footer - before any "Manage" suggestion.
void ReorderWebauthnFallbackToFooter(std::vector<Suggestion>& suggestions) {
  const auto old_pos = std::ranges::find(
      suggestions, SuggestionType::kWebauthnSignInWithAnotherDevice,
      &Suggestion::type);
  if (suggestions.size() < 2 || old_pos == suggestions.end()) {
    return;  // Nothing to reorder.
  }

  // Points to the first "Manage" item of the footer block due to `base` adding
  // an offset when converting the reverse iterator to the forward iterator.
  const auto new_pos =
      std::find_if_not(suggestions.rbegin(), suggestions.rend(),
                       &IsManagementFooterOption)
          .base();
  const bool has_other_management_items =
      new_pos != suggestions.end() &&
      new_pos->type != SuggestionType::kWebauthnSignInWithAnotherDevice;

  // Move the webauthn item from `old_pos` to `new_pos`.
  if (old_pos < new_pos) {
    std::rotate(old_pos, old_pos + 1, new_pos);
  } else {
    std::rotate(new_pos, old_pos, old_pos + 1);
  }

  // Without "Manage" suggestions, ensure a separator for the footer exists.
  if (!has_other_management_items &&
      !std::ranges::contains(suggestions, SuggestionType::kSeparator,
                             &Suggestion::type)) {
    suggestions.emplace(new_pos, SuggestionType::kSeparator);
  }
}

}  // namespace

BrowserAutofillManager::MetricsState::MetricsState(
    BrowserAutofillManager* owner)
    : address_form_event_logger(owner),
      credit_card_form_event_logger(owner),
      loyalty_card_form_event_logger(owner),
      otp_form_event_logger(owner) {}

BrowserAutofillManager::MetricsState::~MetricsState() {
  if (has_parsed_forms) {
    base::UmaHistogramBoolean("Autofill.WebOTP.OneTimeCode.FromAutocomplete",
                              has_observed_one_time_code_field);
  }
  credit_card_form_event_logger.OnDestroyed();
  address_form_event_logger.OnDestroyed();
  loyalty_card_form_event_logger.OnDestroyed();
  otp_form_event_logger.OnDestroyed();
}

BrowserAutofillManager::BrowserAutofillManager(AutofillDriver* driver)
    : AutofillManager(driver),
      autofill_ai_access_manager_(
          std::make_unique<AutofillAiAccessManager>(this)),
      otp_manager_(
          new OtpManagerImpl(*this, client().GetOneTimeTokenService())),
      at_memory_manager_(std::make_unique<AtMemoryManager>(this)),
      account_name_email_strike_manager_(
          std::make_unique<AccountNameEmailStrikeManager>(*this)),
      address_on_typing_manager_(client()) {}

BrowserAutofillManager::~BrowserAutofillManager() {
  // Process log events and record into UKM when the FormStructure is destroyed.
  ForEachCachedForm([this](const FormStructure& form_structure) {
    ProcessFieldLogEventsInForm(form_structure);
  });
  client().GetSingleFieldFillRouter().CancelPendingQueries();
}

base::WeakPtr<AutofillManager> BrowserAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CreditCardAccessManager* BrowserAutofillManager::GetCreditCardAccessManager() {
  if (!credit_card_access_manager_) {
    credit_card_access_manager_ =
        std::make_unique<CreditCardAccessManager>(this);
  }
  return credit_card_access_manager_.get();
}

const CreditCardAccessManager*
BrowserAutofillManager::GetCreditCardAccessManager() const {
  return const_cast<BrowserAutofillManager*>(this)
      ->GetCreditCardAccessManager();
}

AtMemoryManager& BrowserAutofillManager::GetAtMemoryManager() {
  return *at_memory_manager_;
}

AutofillAiAccessManager& BrowserAutofillManager::GetAutofillAiAccessManager() {
  return *autofill_ai_access_manager_;
}

void BrowserAutofillManager::TriggerAtMemorySuggestions(
    const FieldGlobalId& field_id) {
  const FormStructure* form_structure = FindCachedFormById(field_id);
  if (!form_structure) {
    return;
  }
  OnAskForValuesToFill(form_structure->ToFormData(), field_id, gfx::Rect(),
                       AutofillSuggestionTriggerSource::kAtMemory,
                       std::nullopt);
}

payments::AmountExtractionManager&
BrowserAutofillManager::GetAmountExtractionManager() {
  if (!amount_extraction_manager_) {
    amount_extraction_manager_ =
        std::make_unique<AmountExtractionManager>(this);
  }

  return *amount_extraction_manager_;
}

payments::BnplManager* BrowserAutofillManager::GetPaymentsBnplManager() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  if (!bnpl_manager_) {
    bnpl_manager_ = std::make_unique<payments::BnplManager>(this);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

  return bnpl_manager_.get();
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
        client().GetPaymentsAutofillClient()->IsAutofillPaymentMethodsEnabled(),
        metrics_->signin_state_for_metrics);
    if (!client()
             .GetPaymentsAutofillClient()
             ->IsAutofillPaymentMethodsEnabled()) {
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

  const base::TimeTicks form_submitted_timestamp = base::TimeTicks::Now();
  auto log_submission =
      [&](const LogMessage& log_message) {
        LOG_AF(log_manager())
            << LoggingScope::kSubmission << log_message << Br{} << "timestamp: "
            << form_submitted_timestamp.since_origin().InMilliseconds() << Br{}
            << "source: " << SubmissionSourceToString(source) << Br{} << form;
      };
  if (base::FeatureList::IsEnabled(features::kAutofillActorSuppressImport) &&
      client().IsTabInActorMode()) {
    log_submission(LogMessage::kFormSubmissionDetectedButIgnoredDueToActorTask);
    return;
  }

  base::UmaHistogramEnumeration("Autofill.FormSubmission.PerProfileType",
                                client().GetProfileType());
  log_submission(LogMessage::kFormSubmissionDetected);

  // Always let the value patterns metric upload data.
  LogValuePatternsMetric(form);

  std::unique_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  CHECK(!client().IsOffTheRecord() || !submitted_form);

  if (!submitted_form) {
    // We always give Autocomplete a chance to save the data.
    client().GetSingleFieldFillRouter().OnWillSubmitForm(
        form, nullptr, client().IsAutocompleteEnabled());
    return;
  }

  submitted_form->set_submission_source(source);
  LogSubmissionMetrics(submitted_form.get(), form_submitted_timestamp);

  MaybeImportFromSubmittedForm(client(), driver().GetPageUkmSourceId(),
                               *submitted_form);
  MaybeAddAddressSuggestionStrikes(client(), *submitted_form);
  client().GetVotesUploader().MaybeStartVoteUploadProcess(
      std::move(submitted_form),
      /*observed_submission=*/true, GetCurrentPageLanguage(),
      metrics_->initial_interaction_timestamp, last_unlocked_credit_card_cvc_,
      driver().GetPageUkmSourceId());

  if (auto* save_and_fill_manager =
          client().GetPaymentsAutofillClient()->GetSaveAndFillManager()) {
    save_and_fill_manager->MaybeAddStrikeForSaveAndFill();
  }
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
    DenseSet<FormType> form_types =
        submitted_form->GetFormTypes(GetAcUnrecognizedBehavior(client()));
    bool card_form = form_types.contains(FormType::kCreditCardForm);
    bool address_form = form_types.contains(FormType::kAddressForm);
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
  if (client().GetPaymentsAutofillClient()->IsAutofillPaymentMethodsEnabled()) {
    metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
        metrics_->signin_state_for_metrics);
    metrics_->credit_card_form_event_logger.OnWillSubmitForm(*submitted_form);
  }
  if (client().IsAutofillEnabled()) {
    metrics_->loyalty_card_form_event_logger.OnWillSubmitForm(*submitted_form);
  }

  if (client().IsAutofillProfileEnabled()) {
    metrics_->address_form_event_logger.OnFormSubmitted(*submitted_form);
    address_on_typing_manager_.LogAddressOnTypingCorrectnessMetrics(
        *submitted_form);
  }
  if (client().GetPaymentsAutofillClient()->IsAutofillPaymentMethodsEnabled()) {
    metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
        metrics_->signin_state_for_metrics);
    metrics_->credit_card_form_event_logger.OnFormSubmitted(*submitted_form);
    if (touch_to_fill_delegate_) {
      touch_to_fill_delegate_->LogMetricsAfterSubmission(*submitted_form);
    }
  }
  if (client().IsAutofillEnabled()) {
    metrics_->loyalty_card_form_event_logger.OnFormSubmitted(*submitted_form);
  }

  metrics_->otp_form_event_logger.OnWillSubmitForm(*submitted_form);
}

void BrowserAutofillManager::OnTextFieldValueChangedImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const base::TimeTicks timestamp) {
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field_id);
  if (!form_structure || !autofill_field) {
    return;
  }

  // Log events when user edits the field.
  // If the user types into the same field multiple times, repeated
  // TypingFieldLogEvents are coalesced.
  autofill_field->AppendLogEventIfNotRepeated(TypingFieldLogEvent{
      .has_value_after_typing =
          ToOptionalBoolean(!autofill_field->value().empty())});

  UpdatePendingForm(form);
  const bool edited_autofilled_field =
      autofill_field->last_modifier() == FieldModifier::kAutofill;

  if (!metrics_->user_did_type || edited_autofilled_field) {
    metrics_->user_did_type = true;
    client().GetFormInteractionsUkmLogger().LogTextFieldValueChanged(
        driver().GetPageUkmSourceId(), *form_structure, *autofill_field);
  }

  autofill_field->AddFieldModifier(FieldModifier::kUser);

  auto* logger = GetEventFormLogger(*autofill_field);
  if (edited_autofilled_field) {
    if (logger) {
      logger->OnEditedAutofilledField(field_id);
    }
    if (AutofillAiManager* ai_manager = client().GetAutofillAiManager();
        ai_manager &&
        autofill_field->filling_product() == FillingProduct::kAutofillAi) {
      ai_manager->OnEditedAutofilledField(*form_structure, *autofill_field,
                                          driver().GetPageUkmSourceId());
    }
  } else {
    if (logger) {
      logger->OnEditedNonFilledField(field_id);
    }
  }
  UpdateInitialInteractionTimestamp(timestamp);
}

void BrowserAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<PasswordSuggestionRequest> password_request) {
  base::TimeTicks suggestion_generation_start_time = base::TimeTicks::Now();

  // In case we cannot fetch the parsed `FormStructure` and `AutofillField`, we
  // still need to offer Autocomplete.
  // TODO(crbug.com/433224307): Consider early returning here when the cache
  // starts storing all forms and fields.
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field_id);

  if (password_request.has_value()) {
    if (PasswordManagerDelegate* password_delegate =
            client().GetPasswordManagerDelegate(field_id)) {
      // This block implements the following behavior: For an <input
      // type="password"> field, do not show a dropdown if the current value is
      // non empty (typically manually typed) or autofilled, and close any
      // previously opened dropdown.
      if (autofill_field &&
          autofill_field->form_control_type() ==
              FormControlType::kInputPassword &&
          !autofill_field->value().empty() &&
          autofill_field->last_modifier() != FieldModifier::kAutofill) {
        client().HideSuggestions(SuggestionHidingReason::kFieldValueChanged,
                                 /*product=*/std::nullopt);
        return;
      }
#if !BUILDFLAG(IS_ANDROID)
      password_delegate->ShowSuggestions(password_request->field);
#else
      password_delegate->ShowKeyboardReplacingSurface(password_request.value());
#endif  // !BUILDFLAG(IS_ANDROID)
      return;
    }
  } else if (IsPasswordsAutofillManuallyTriggered(trigger_source)) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kAutofillDisableFilling)) {
    return;
  }

  UpdateLoggersReadinessData(form_structure);

  if (form_structure && autofill_field) {
    AutofillMetrics::LogParsedFormUntilInteractionTiming(
        base::TimeTicks::Now() - form_structure->form_parsed_timestamp());
    if (AutofillAiManager* ai_manager = client().GetAutofillAiManager()) {
      ai_manager->OnFormInteracted(*form_structure,
                                   driver().GetPageUkmSourceId());
    }
    if (autofill_metrics::FormEventLoggerBase* logger =
            GetEventFormLogger(*autofill_field);
        logger && ShouldBeParsed(*form_structure, /*log_manager=*/nullptr)) {
      if (logger == &metrics_->credit_card_form_event_logger) {
        metrics_->credit_card_form_event_logger.set_signin_state_for_metrics(
            metrics_->signin_state_for_metrics);
      }
      logger->OnDidInteractWithAutofillableForm(*form_structure);
    }

    // TODO(crbug.com/349982907): Until the linked bug is fixed, Chrome on iOS
    // does not forward focus events. The OnAskForValuesToFillImpl() call
    // indicates that a field was focused on iOS. On desktop it's not capturing
    // all focus events (neglecting if the user presses the tab key or a field
    // acquires focus on page load). Therefore, this is a temporary workaround
    // that should be deleted with crbug.com/349982907.
    autofill_field->set_was_focused(true);
  }

  if (trigger_source ==
          AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge &&
      client().GetSessionIdForCurrentAutofillSuggestions().has_value()) {
    return;
  }

  // TODO(crbug.com/519061643): Rely on central atMemory eligibility logic
  // instead.
  if (IsAtMemoryTriggerSource(trigger_source) &&
      client().GetPersonalContextEnablementState() ==
          personal_context::PersonalContextEnablementState::
              kDisabledNotEligible) {
    return;
  }

  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  external_delegate_->OnQuery(form, field, caret_bounds, trigger_source,
                              /*update_datalist=*/true);

  if (IsAtMemoryTriggerSource(trigger_source)) {
    std::vector<Suggestion> suggestions;
    GetAtMemoryManager().MaybeAppendPersonalContextNotice(suggestions);

    // Show suggestions with a search bar to start the flow.
    external_delegate_->OnSuggestionsReturned(field_id, suggestions);
    return;
  }

  // TODO(crbug.com/409962888): Cleanup once the new logic is launched.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillNewSuggestionGeneration)) {
    GenerateSuggestionsAndMaybeShowUIPhase1(form, field, trigger_source,
                                            suggestion_generation_start_time);
    return;
  }

  SuggestionsContext context = BuildSuggestionsContext(
      form, form_structure, field, autofill_field, trigger_source,
      GetAcUnrecognizedBehavior(client()));
  InitializeSuggestionGenerators(trigger_source, form.global_id(),
                                 field.global_id());

  auto barrier_callback =
      base::BarrierCallback<SuggestionGenerator::ReturnedSuggestions>(
          suggestion_generators_.size(),
          base::BindOnce(
              &BrowserAutofillManager::OnIndividualSuggestionsGenerated,
              weak_ptr_factory_.GetWeakPtr(), form, field, trigger_source,
              context, suggestion_generation_start_time));

  for (const std::unique_ptr<SuggestionGenerator>& suggestion_generator :
       suggestion_generators_) {
    suggestion_generator->GenerateSuggestions(form, field, form_structure,
                                              autofill_field, client(),
                                              barrier_callback);
  }
}

void BrowserAutofillManager::OnIndividualSuggestionsGenerated(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    SuggestionsContext context,
    base::TimeTicks suggestion_generation_start_time,
    std::vector<SuggestionGenerator::ReturnedSuggestions>
        returned_suggestions) {
  using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

  // Suggestion generators lifespan should be limited to only when they are
  // needed.
  suggestion_generators_.clear();

  // In case we cannot fetch the parsed `FormStructure` and `AutofillField`, we
  // still need to offer Autocomplete.
  // TODO(crbug.com/433224307): Consider early returning here when the cache
  // starts storing all forms and fields.
  AutofillField* autofill_field =
      GetCachedFormAndField(form.global_id(), field.global_id()).autofill_field;

  base::flat_map<SuggestionDataSource, std::vector<Suggestion>> all_suggestions(
      std::move(returned_suggestions));

  // Clear some of the suggestions based on the ablation study.
  if (autofill_field &&
      all_suggestions.contains(SuggestionDataSource::kAddress) &&
      !all_suggestions[SuggestionDataSource::kAddress].empty() &&
      EvaluateAblationStudy(*autofill_field, FillingProduct::kAddress,
                            /*has_suggestions=*/true)) {
    all_suggestions[SuggestionDataSource::kAddress].clear();
  }
  if (autofill_field &&
      all_suggestions.contains(SuggestionDataSource::kCreditCard) &&
      !all_suggestions[SuggestionDataSource::kCreditCard].empty() &&
      EvaluateAblationStudy(*autofill_field, FillingProduct::kCreditCard,
                            /*has_suggestions=*/true)) {
    all_suggestions[SuggestionDataSource::kCreditCard].clear();
  }

  // Clear some of the suggestions based on priorities:
  // 1. Find the highest priority suggestion data source S that returned data.
  // 2. Keep only data from sources that are mergeable with S, discard the rest.
  std::optional<SuggestionDataSource> highest_priority_source;
  const DenseSet<SuggestionDataSource>* supported_mergeable_sources;
  for (SuggestionDataSource source :
       SuggestionGenerator::kOrderedPrioritizedSources) {
    if (!all_suggestions.contains(source) || all_suggestions[source].empty()) {
      continue;
    }
    if (!highest_priority_source.has_value()) {
      highest_priority_source = source;
      supported_mergeable_sources =
          base::FindOrNull(SuggestionGenerator::kSupportedMerges,
                           highest_priority_source.value());
      continue;
    }
    if (!supported_mergeable_sources ||
        !supported_mergeable_sources->contains(source)) {
      all_suggestions.erase(source);
    }
  }

  std::map<FillingProduct, std::vector<Suggestion>> prioritized_suggestions;
  for (auto& [suggestion_data_source, suggestions] : all_suggestions) {
    if (suggestions.empty()) {
      continue;
    }
    FillingProduct filling_product =
        GetFillingProductFromSuggestionDataSource(suggestion_data_source);
    base::UmaHistogramEnumeration(
        "Autofill.SuggestionGeneration.GeneratedFillingProduct",
        filling_product);
    prioritized_suggestions[filling_product] = std::move(suggestions);
  }

  auto passkey_suggestions =
      prioritized_suggestions.extract(FillingProduct::kPasskey);

  // TODO(crbug.com/409962888): Consider moving the TTF logic to
  // `OnGenerateSuggestionsComplete()` when the old suggestion generation logic
  // is removed.
  auto on_generate_suggestions_complete =
      [&](std::vector<Suggestion> suggestions) {
        if (TryToShowTouchToFillSuggestions(form, field, autofill_field,
                                            suggestions, trigger_source)) {
          OnGenerateSuggestionsComplete(
              form.global_id(), field.global_id(), trigger_source, context,
              suggestion_generation_start_time,
              /*show_suggestions=*/false, suggestions);
          return;
        }

        // Handle passkeys separately, since they can merge with every
        // suggestion.
        if (!passkey_suggestions.empty()) {
          CHECK(passkey_suggestions.mapped().size() == 1);
          MergePasskeysAndExistingSuggestions(
              suggestions, std::move(passkey_suggestions.mapped()[0]));
        }
        OnGenerateSuggestionsComplete(form.global_id(), field.global_id(),
                                      trigger_source, context,
                                      suggestion_generation_start_time,
                                      /*show_suggestions=*/true, suggestions);
      };

  if (prioritized_suggestions.empty()) {
    on_generate_suggestions_complete({});
    return;
  }

  if (prioritized_suggestions.contains(FillingProduct::kAddress)) {
    on_generate_suggestions_complete(
        MergeWithAddressSuggestions(prioritized_suggestions, trigger_source));
    return;
  }

  // This ensures we cover all merge cases above, and avoid any other wrong
  // combination during prioritization.
  CHECK_EQ(prioritized_suggestions.size(), 1u);
  on_generate_suggestions_complete(prioritized_suggestions.begin()->second);
}

bool BrowserAutofillManager::TryToShowTouchToFillSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const AutofillField* trigger_autofill_field,
    const std::vector<Suggestion>& suggestions,
    AutofillSuggestionTriggerSource trigger_source) {
  if (!touch_to_fill_delegate_) {
    return false;
  }

  if (touch_to_fill_delegate_->IsShowingTouchToFill()) {
    return true;
  }

  // Touch to fill is not shown if other address suggestions are available
  // for EMAIL_OR_LOYALTY_MEMBERSHIP_ID fields.
  if (trigger_autofill_field &&
      trigger_autofill_field->Type().GetLoyaltyCardType() ==
          EMAIL_OR_LOYALTY_MEMBERSHIP_ID &&
      std::ranges::any_of(suggestions, [](const Suggestion& suggestion) {
        return GetFillingProductFromSuggestionType(suggestion.type) ==
               FillingProduct::kAddress;
      })) {
    return false;
  }

  return trigger_source ==
             AutofillSuggestionTriggerSource::kFormControlElementClicked &&
         touch_to_fill_delegate_->TryToShowTouchToFill(form, trigger_field);
}

std::vector<Suggestion> BrowserAutofillManager::MergeWithAddressSuggestions(
    std::map<FillingProduct, std::vector<Suggestion>>& suggestions_map,
    AutofillSuggestionTriggerSource trigger_source) {
  auto extract_vector = [&suggestions_map](FillingProduct product) {
    auto node = suggestions_map.extract(product);
    return node.empty() ? std::vector<Suggestion>() : std::move(node.mapped());
  };

  std::vector<Suggestion> address_suggestions =
      extract_vector(FillingProduct::kAddress);
  std::vector<Suggestion> identity_credentials_suggestions =
      extract_vector(FillingProduct::kIdentityCredential);
  std::vector<Suggestion> loyalty_card_suggestions =
      extract_vector(FillingProduct::kLoyaltyCard);

  CHECK(suggestions_map.empty())
      << "Some suggestions not currently supported with addresses were "
         "generated. "
      << FillingProductToString(suggestions_map.begin()->first);

  if (!loyalty_card_suggestions.empty()) {
    MergeLoyaltyCardsAndAddressSuggestions(address_suggestions,
                                           std::move(loyalty_card_suggestions));
  }

  if (!identity_credentials_suggestions.empty()) {
    MergeIdentityCredentialsAndAddressSuggestions(
        address_suggestions, std::move(identity_credentials_suggestions));
  }
  return address_suggestions;
}

void BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase1(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    base::TimeTicks suggestion_generation_start_time) {
  // In case we cannot fetch the parsed `FormStructure` and `AutofillField`, we
  // still need to offer Autocomplete.
  // TODO(crbug.com/433224307): Consider early returning here when the cache
  // starts storing all forms and fields.
  AutofillField* autofill_field =
      GetCachedFormAndField(form.global_id(), field.global_id()).autofill_field;

  auto generate_suggestions_and_maybe_show_ui_phase2 = base::BindOnce(
      &BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase2,
      weak_ptr_factory_.GetWeakPtr(), form, field, trigger_source,
      suggestion_generation_start_time);

  // `otp_manager_` may not be instantiated on all platforms. If a focused field
  // is not classified, `autofill_field` is null but the field may be filled by
  // autocomplete.
  if (otp_manager_ && autofill_field &&
      autofill_field->Type().GetTypes().contains(ONE_TIME_CODE)) {
    otp_manager_->GetOtpSuggestions(
        std::move(generate_suggestions_and_maybe_show_ui_phase2));
    return;
  }

  std::move(generate_suggestions_and_maybe_show_ui_phase2)
      .Run(/*otp_suggestions=*/{});
}

void BrowserAutofillManager::GenerateSuggestionsAndMaybeShowUIPhase2(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    base::TimeTicks suggestion_generation_start_time,
    std::vector<std::string> one_time_passwords) {
  // In case we cannot fetch the parsed `FormStructure` and `AutofillField`, we
  // still need to offer Autocomplete.
  // TODO(crbug.com/433224307): Consider early returning here when the cache
  // starts storing all forms and fields.
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field.global_id());
  SuggestionsContext context = BuildSuggestionsContext(
      form, form_structure, field, autofill_field, trigger_source,
      GetAcUnrecognizedBehavior(client()));

  OnGenerateSuggestionsCallback callback = base::BindOnce(
      &BrowserAutofillManager::GenerateFooter, weak_ptr_factory_.GetWeakPtr(),
      form, field, trigger_source, context, suggestion_generation_start_time);

  // If this is a mixed content form, we show a warning message and don't offer
  // autofill. The warning is shown even if there are no autofill suggestions
  // available.
  if (IsFormMixedContent(client(), form) &&
      client().GetPrefs()->FindPreference(
          ::prefs::kMixedFormsWarningsEnabled) &&
      client().GetPrefs()->GetBoolean(::prefs::kMixedFormsWarningsEnabled)) {
    LOG_AF(log_manager()) << LoggingScope::kFilling
                          << LogMessage::kSuggestionSuppressed
                          << " Reason: Insecure form";
    // If the user begins typing, we interpret that as dismissing the warning.
    // No suggestions are allowed, but the warning is no longer shown.
    std::vector<Suggestion> suggestions;
    if (!(field.properties_mask() & kUserTyped)) {
      suggestions.emplace_back(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_MIXED_FORM),
          SuggestionType::kMixedFormMessage);
    }
    std::move(callback).Run(/*show_suggestions=*/true, suggestions);
    return;
  }

  std::vector<Suggestion> suggestions =
      GetAvailableSuggestions(form, form_structure, field, autofill_field,
                              trigger_source, one_time_passwords, context);

  if (ShouldSuppressSuggestions(context.suppress_reason, log_manager())) {
    if (context.suppress_reason == SuppressReason::kAblation) {
      CHECK(suggestions.empty());
      client().GetSingleFieldFillRouter().CancelPendingQueries();
    }
    std::move(callback).Run(/*show_suggestions=*/true, /*suggestions=*/{});
    return;
  }

  AutofillAiManager* ai_manager = client().GetAutofillAiManager();
  if (form_structure && autofill_field && ai_manager &&
      !context.do_not_generate_autofill_suggestions &&
      GetFieldsFillableByAutofillAi(*form_structure, client())
          .contains(field.global_id())) {
    std::move(callback).Run(
        /*show_suggestions=*/true,
        ai_manager->GetSuggestions(*form_structure, field));
    return;
  } else if (suggestions.empty() && ai_manager && form_structure &&
             ai_manager->ShouldDisplayIph(*form_structure, field.global_id()) &&
             client().ShowAutofillFieldIphForFeature(
                 field, AutofillClient::IphFeature::kAutofillAi)) {
    std::move(callback).Run(/*show_suggestions=*/false, /*suggestions=*/{});
    return;
  }

  if (TryToShowTouchToFillSuggestions(form, field, autofill_field, suggestions,
                                      trigger_source)) {
    std::move(callback).Run(/*show_suggestions=*/false, std::move(suggestions));
    return;
  }

  if (!suggestions.empty()) {
    // Show the list of `suggestions` if not empty. These may include address or
    // credit card suggestions. Additionally, warnings about mixed content might
    // be present.
    std::move(callback).Run(/*show_suggestions=*/true, std::move(suggestions));
    return;
  }

  if (field.form_control_type() == FormControlType::kTextArea ||
      field.form_control_type() == FormControlType::kContentEditable) {
    std::optional<Suggestion> maybe_compose_suggestion =
        GenerateComposeSuggestion(form, field, trigger_source, client());
    if (maybe_compose_suggestion) {
      std::move(callback).Run(/*show_suggestions=*/true,
                              {*std::move(maybe_compose_suggestion)});
      return;
    }
  } else if (IsTriggerSourceOnlyRelevantForCompose(trigger_source)) {
    std::move(callback).Run(/*show_suggestions=*/true,
                            /*suggestions=*/{});
    return;
  }

  auto on_single_field_suggestions_callback = base::BindOnce(
      &BrowserAutofillManager::OnGeneratedSingleFieldFillSuggestions,
      weak_ptr_factory_.GetWeakPtr(), form, field, std::move(callback));

  // Generating single field suggestions.
  base::OnceCallback<void(FieldGlobalId, std::vector<Suggestion>)>
      on_suggestions_returned = base::IgnoreArgs<FieldGlobalId>(
          std::move(on_single_field_suggestions_callback));
  if (form_structure && autofill_field &&
      client().GetPaymentsAutofillClient()->GetMerchantPromoCodeManager() &&
      client()
          .GetPaymentsAutofillClient()
          ->GetMerchantPromoCodeManager()
          ->OnGetSingleFieldSuggestions(*form_structure, field, *autofill_field,
                                        client(), on_suggestions_returned)) {
    return;
  }
  if (form_structure && autofill_field &&
      client().GetPaymentsAutofillClient()->GetIbanManager() &&
      client()
          .GetPaymentsAutofillClient()
          ->GetIbanManager()
          ->OnGetSingleFieldSuggestions(*form_structure, field, *autofill_field,
                                        client(), on_suggestions_returned)) {
    return;
  }
  // Autocomplete suggestions have to be generated last since they have to
  // take the ownership of `on_suggestions_returned`.
  // Even if no autocomplete suggestions are generated,
  // `on_suggestions_returned` is still called with an empty list of
  // suggestions.
  client().GetAutocompleteHistoryManager()->OnGetSingleFieldSuggestions(
      form, form_structure, field, autofill_field, client(),
      std::move(on_suggestions_returned));
}

std::optional<Suggestion>
BrowserAutofillManager::CreatePasskeySuggestionForMerge(
    const FormFieldData& field) {
  if (!ShouldShowWebauthnHybridEntryPoint(field)) {
    return std::nullopt;
  }
  PasswordManagerDelegate* password_delegate =
      client().GetPasswordManagerDelegate(field.global_id());
  if (!password_delegate) {
    return std::nullopt;
  }

  // If any field **on the page** allows starting the hybrid passkey flow,
  // this suggestion becomes available.
  std::optional<Suggestion> passkey_suggestion =
      password_delegate->GetWebauthnSignInWithAnotherDeviceSuggestion();
  if (!passkey_suggestion) {
    return std::nullopt;
  }
  return {passkey_suggestion.value()};
}

void BrowserAutofillManager::MergePasskeysAndExistingSuggestions(
    std::vector<Suggestion>& suggestions,
    Suggestion passkey_suggestion) {
  suggestions.push_back(passkey_suggestion);
}

void BrowserAutofillManager::GenerateFooter(
    const FormData& form,
    const FormFieldData& field,
    AutofillSuggestionTriggerSource trigger_source,
    const SuggestionsContext& context,
    base::TimeTicks suggestion_generation_start_time,
    bool show_suggestions,
    std::vector<Suggestion> suggestions) {
  std::optional<Suggestion> passkey_suggestion =
      CreatePasskeySuggestionForMerge(field);
  if (passkey_suggestion.has_value()) {
    MergePasskeysAndExistingSuggestions(suggestions,
                                        std::move(passkey_suggestion.value()));
  }

  OnGenerateSuggestionsComplete(form.global_id(), field.global_id(),
                                trigger_source, context,
                                suggestion_generation_start_time,
                                show_suggestions, std::move(suggestions));
}

void BrowserAutofillManager::OnGeneratedSingleFieldFillSuggestions(
    const FormData& form,
    const FormFieldData& field,
    OnGenerateSuggestionsCallback callback,
    std::vector<Suggestion> single_field_suggestions) {
  if (!single_field_suggestions.empty()) {
    std::move(callback).Run(/*show_suggestions=*/true,
                            std::move(single_field_suggestions));
    return;
  }

  // Note the check below is the same done for regular autocomplete suggestions.
  // TODO(crbug.com/381994105): Consider adding
  // `should_offer_autofill_on_typing()` to `FormFieldData`.
  if (field.should_autocomplete() &&
      base::FeatureList::IsEnabled(
          features::kAutofillAddressSuggestionsOnTyping)) {
    // Try to build `Suggestion::kAddressEntryOnTyping` suggestions.
    // Note that these suggestions are always displayed on their own.
    std::move(callback).Run(
        /*show_suggestions=*/true,
        GetSuggestionsOnTypingForProfile(client(), form, field));
    return;
  }

  std::move(callback).Run(/*show_suggestions=*/true, {});
}

void BrowserAutofillManager::OnGenerateSuggestionsComplete(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source,
    const SuggestionsContext& context,
    base::TimeTicks suggestion_generation_start_time,
    bool show_suggestions,
    std::vector<Suggestion> suggestions) {
  ReorderWebauthnFallbackToFooter(suggestions);

  if (!suggestions.empty()) {
    base::UmaHistogramTimes(
        "Autofill.Timing.SuggestionGeneration2",
        base::TimeTicks::Now() - suggestion_generation_start_time);
  }

  LogSuggestionsCount(context, suggestions);
  // When focusing on a field, log whether there is a suggestion for the user
  // and whether the suggestion is shown.
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form_id, field_id);
  if (form_structure &&
      context.filling_product == FillingProduct::kCreditCard) {
    AutofillMetrics::LogIsQueriedCreditCardFormSecure(
        !IsFormOrClientNonSecure(client(), *form_structure));
  }
  if (trigger_source ==
          AutofillSuggestionTriggerSource::kFormControlElementClicked &&
      autofill_field) {
    autofill_field->AppendLogEventIfNotRepeated(AskForValuesToFillFieldLogEvent{
        .has_suggestion = ToOptionalBoolean(!suggestions.empty()),
        .suggestion_is_shown = ToOptionalBoolean(show_suggestions),
    });
  }

  // When a user interacts with the credit card form on the merchant checkout
  // pages, trigger amount extraction if any suggested feature requires it.
  if (autofill_field) {
    const DenseSet<AmountExtractionManager::EligibleFeature> eligible_features =
        GetAmountExtractionManager().GetEligibleFeatures(
            client()
                .GetPaymentsAutofillClient()
                ->IsAutofillPaymentMethodsEnabled(),
            ShouldSuppressSuggestions(context.suppress_reason, log_manager()),
            suggestions, context.filling_product,
            autofill_field->Type().GetCreditCardType());

    for (AmountExtractionManager::EligibleFeature eligible_feature :
         eligible_features) {
      switch (eligible_feature) {
        case AmountExtractionManager::EligibleFeature::kBnpl:
          const bool ai_amount_extraction_disabled =
              !base::FeatureList::IsEnabled(
                  features::kAutofillEnableAiBasedAmountExtraction);
          const bool pay_later_tabs_enabled = base::FeatureList::IsEnabled(
              features::kAutofillEnablePayNowPayLaterTabs);

          // Notifying BNPL manager of suggestion generation is only necessary
          // for flows that do an update of the suggestions dropdown. In the
          // regex-based amount extraction approach, the dropdown is updated to
          // add the BNPL chip so suggestion generation notification is
          // necessary. In the AI-based amount extraction case, no suggestion
          // dropdown updates happen so it's not necessary. For pay later tabs,
          // once again suggestions are updated (switching out progress
          // suggestion with regular BNPL suggestions), so suggestion generation
          // notification is necessary again.
          if (ai_amount_extraction_disabled || pay_later_tabs_enabled) {
            GetPaymentsBnplManager()->NotifyOfSuggestionGeneration(
                trigger_source);
          }

          // This is the entry point for regex-based amount extraction. If
          // AI-based amount extraction is enabled, this should not be
          // triggered.
          if (ai_amount_extraction_disabled) {
            GetAmountExtractionManager().TriggerCheckoutAmountExtraction();
          }
      }
    }
  }

  if (show_suggestions) {
    // Send Autofill suggestions (could be an empty list).
    external_delegate_->OnSuggestionsReturned(field_id, suggestions);
  }
}

void BrowserAutofillManager::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FieldGlobalId& field_id,
    const FillingPayload& filling_payload,
    AutofillTriggerSource trigger_source,
    const base::flat_set<FieldGlobalId>& blocked_fields) {
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field_id);
  if (!form_structure || !autofill_field) {
    return;
  }
  std::visit(absl::Overload{
                 [&](const AutofillProfile*) {
                   form_filler_->FillOrPreviewForm(
                       action_persistence, form, filling_payload,
                       CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field),
                       trigger_source, blocked_fields, FillId::Create(),
                       /*forced_fill_values=*/{},
                       FormFiller::RefillOptions::NotRefill());
                 },
                 [&](const CreditCard* credit_card) {
                   // We still need to take care of authentication
                   // flows, which is why we do not forward right
                   // away to FormFiller.
                   FillOrPreviewCreditCardForm(
                       action_persistence, form, CHECK_DEREF(form_structure),
                       CHECK_DEREF(autofill_field), *credit_card,
                       trigger_source, blocked_fields);
                 },
                 [&](const EntityInstance*) {
                   form_filler_->FillOrPreviewForm(
                       action_persistence, form, filling_payload,
                       CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field),
                       trigger_source, blocked_fields, FillId::Create(),
                       /*forced_fill_values=*/{},
                       FormFiller::RefillOptions::NotRefill());
                 },
                 [&](const VerifiedProfile*) {
                   form_filler_->FillOrPreviewForm(
                       action_persistence, form, filling_payload,
                       CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field),
                       trigger_source, blocked_fields, FillId::Create(),
                       /*forced_fill_values=*/{},
                       FormFiller::RefillOptions::NotRefill());
                 },
                 [&](const OtpFillData*) {
                   form_filler_->FillOrPreviewForm(
                       action_persistence, form, filling_payload,
                       CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field),
                       trigger_source, blocked_fields, FillId::Create(),
                       /*forced_fill_values=*/{},
                       FormFiller::RefillOptions::NotRefill());
                 }},
             filling_payload);
}
void BrowserAutofillManager::FillOrPreviewField(
    mojom::ActionPersistence action_persistence,
    mojom::FieldActionType action_type,
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& value,
    FillingProduct filling_product,
    std::optional<FieldType> field_type_used) {
  // In case we cannot fetch the parsed `FormStructure` and `AutofillField`, we
  // still need to offer Autocomplete. Therefore, the code below, including
  // called functions, must handle `form_structure == nullptr` and
  // `autofill_field == nullptr`.
  // TODO(crbug.com/433224307): Consider early returning here when the cache
  // starts storing all forms and fields.
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field.global_id());
  form_filler_->FillOrPreviewField(action_persistence, action_type, field,
                                   autofill_field, value, filling_product,
                                   field_type_used);
  // Notify observers of the single-field filling event.
  NotifyObservers(&Observer::OnFillOrPreviewField, form.global_id(),
                  field.global_id(), action_persistence, value,
                  field_type_used);
  if (action_persistence != mojom::ActionPersistence::kFill) {
    return;
  }
  if (autofill_field && autofill_field->Type().GetLoyaltyCardType() ==
                            EMAIL_OR_LOYALTY_MEMBERSHIP_ID) {
    if (field_type_used == LOYALTY_MEMBERSHIP_ID) {
      LogEmailOrLoyaltyCardSuggestionAccepted(
          autofill_metrics::AutofillEmailOrLoyaltyCardAcceptanceMetricValue::
              kLoyaltyCardSelected);
    } else if (field_type_used == EMAIL_ADDRESS &&
               client().GetValuablesDataManager() &&
               !client().GetValuablesDataManager()->GetLoyaltyCards().empty()) {
      LogEmailOrLoyaltyCardSuggestionAccepted(
          autofill_metrics::AutofillEmailOrLoyaltyCardAcceptanceMetricValue::
              kEmailSelected);
    }
  }
}

void BrowserAutofillManager::OnDidFillAddressFormFillingSuggestion(
    const AutofillProfile& profile,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    AutofillTriggerSource trigger_source) {
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form_id, field_id);
  if (!form_structure || !autofill_field) {
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
  address_on_typing_manager_.OnDidAcceptAddressOnTyping(
      field_id, value, field_type_used_to_build_suggestion, profile_used_guid);
}

void BrowserAutofillManager::UndoAutofill(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormFieldData& trigger_field) {
  FormStructure* form_structure =
      FindCachedFormById(form.global_id(), /*pass_key=*/{});
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

void BrowserAutofillManager::DelegateSelectToPasswordManager(
    const Suggestion& suggestion,
    const FormFieldData& trigger_field) {
  if (PasswordManagerDelegate* password_delegate =
          client().GetPasswordManagerDelegate(trigger_field.global_id())) {
    password_delegate->SelectSuggestion(suggestion);
  }
}

void BrowserAutofillManager::DelegateAcceptToPasswordManager(
    const Suggestion& suggestion,
    const AutofillSuggestionDelegate::SuggestionMetadata& metadata,
    const FormFieldData& trigger_field) {
  if (PasswordManagerDelegate* password_delegate =
          client().GetPasswordManagerDelegate(trigger_field.global_id())) {
    password_delegate->AcceptSuggestion(suggestion, metadata);
  }
}

void BrowserAutofillManager::FillOrPreviewCreditCardForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FormStructure& form_structure,
    const AutofillField& autofill_field,
    const CreditCard& credit_card,
    AutofillTriggerSource trigger_source,
    const base::flat_set<FieldGlobalId>& blocked_fields) {
  bool require_card_fetching = [&] {
    if (action_persistence == mojom::ActionPersistence::kPreview) {
      return false;
    }
    switch (trigger_source) {
      case AutofillTriggerSource::kPopup:
      case AutofillTriggerSource::kKeyboardAccessoryOrBottomSheet:
      case AutofillTriggerSource::kGlic:
        return ShouldFetchCreditCard(form, form_structure, autofill_field,
                                     credit_card, trigger_source,
                                     GetAcUnrecognizedBehavior(client()));
      case AutofillTriggerSource::kScanCreditCard:
      case AutofillTriggerSource::kDevtools:
      case AutofillTriggerSource::kCreditCardSaveAndFill:
        return false;
      case AutofillTriggerSource::kFormsSeen:
      case AutofillTriggerSource::kSelectOptionsChanged:
      case AutofillTriggerSource::kJavaScriptChangedAutofilledValue:
      case AutofillTriggerSource::kManualFallback:
      case AutofillTriggerSource::kNone:
      case AutofillTriggerSource::kProactivePasswordRecovery:
      case AutofillTriggerSource::kProgrammaticRefill:
        NOTREACHED();
    }
  }();
  CHECK(action_persistence != mojom::ActionPersistence::kPreview ||
        !require_card_fetching);

  // Called either synchronously (if the card doesn't have to be fetched) or
  // asynchronously (when the card has been successfully fetched).
  auto fill_or_preview =
      [](BrowserAutofillManager& self,
         mojom::ActionPersistence action_persistence, const FormData& form,
         const FieldGlobalId& field_id, const CreditCard& credit_card,
         AutofillTriggerSource trigger_source,
         const base::flat_set<FieldGlobalId>& blocked_fields) {
        const FormFieldData* const field = form.FindFieldByGlobalId(field_id);
        auto [cached_form_structure, cached_autofill_field] =
            self.GetCachedFormAndField(form.global_id(), field_id);
        if (!field || !cached_form_structure || !cached_autofill_field) {
          return;
        }
        self.form_filler_->FillOrPreviewForm(
            action_persistence, form, &credit_card,
            CHECK_DEREF(cached_form_structure),
            CHECK_DEREF(cached_autofill_field), trigger_source, blocked_fields,
            FillId::Create(), /*forced_fill_values=*/{},
            FormFiller::RefillOptions::NotRefill());
      };

  // Called when fetching the credit card was successful.
  // Ultimately fills the form by calling `fill_or_preview`.
  auto on_fetched = [](base::WeakPtr<BrowserAutofillManager> self,
                       decltype(fill_or_preview) fill_or_preview,
                       const FormData& form, const FieldGlobalId& field_id,
                       const url::Origin& origin,
                       AutofillTriggerSource fetched_credit_card_trigger_source,
                       const base::flat_set<FieldGlobalId>& blocked_fields,
                       const CreditCard& credit_card) {
    if (!self) {
      return;
    }

    if (credit_card.record_type() == CreditCard::RecordType::kFullServerCard ||
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard) {
      self->GetCreditCardAccessManager()->CacheUnmaskedCardInfo(
          credit_card, credit_card.cvc());
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
      self->client()
          .GetFormDataImporter()
          ->GetPaymentsFormDataImporter()
          .CacheFetchedVirtualCard(credit_card.LastFourDigits());

      FilledCardInformationBubbleOptions options;
      options.masked_card_name = credit_card.CardNameForAutofillDisplay();
      options.masked_card_number_last_four =
          credit_card.ObfuscatedNumberWithVisibleLastFourDigits();
      options.filled_card = credit_card;
      // TODO(crbug.com/40927041): Remove CVC from
      // FilledCardInformationBubbleOptions.
      options.cvc = credit_card.cvc();
      options.card_image = self->GetCardImage(credit_card);
      self->client().GetPaymentsAutofillClient()->OnCardDataAvailable(options,
                                                                      origin);
    }

    fill_or_preview(*self, mojom::ActionPersistence::kFill, form, field_id,
                    credit_card, fetched_credit_card_trigger_source,
                    blocked_fields);
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
    GetCreditCardAccessManager()->FetchCreditCard(
        &credit_card,
        base::BindOnce(on_fetched, weak_ptr_factory_.GetWeakPtr(),
                       fill_or_preview, form, autofill_field.global_id(),
                       autofill_field.origin(), trigger_source,
                       blocked_fields));
  } else if (fetched_independently) {
    // Cards fetched independently, such as for BNPL, have all of their data on
    // creation and do not need further fetching.
    on_fetched(weak_ptr_factory_.GetWeakPtr(), fill_or_preview, form,
               autofill_field.global_id(), autofill_field.origin(),
               trigger_source, blocked_fields, credit_card);
  } else {
    fill_or_preview(*this, action_persistence, form, autofill_field.global_id(),
                    credit_card, trigger_source, blocked_fields);
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

  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field_id);
  if (!form_structure || !autofill_field) {
    return;
  }
  autofill_field->set_was_focused(true);

  for (const FieldType type : autofill_field->Type().GetTypes()) {
    static constexpr std::string_view kMetricName =
        "Autofill.FocusedField.FieldType";
    const bool same_origin =
        autofill_field->origin() == form.main_frame_origin();
    base::UmaHistogramExactLinear(base::StrCat({kMetricName}), type,
                                  MAX_VALID_FIELD_TYPE);
    base::UmaHistogramExactLinear(
        base::StrCat(
            {kMetricName, ".", same_origin ? "Same" : "Cross", "Origin"}),
        type, MAX_VALID_FIELD_TYPE);
  }

  // This code path checks if suggestions to be announced to a screen reader are
  // available when the focus on a form field changes. This cannot happen in
  // `OnAskForValuesToFillImpl()`, since the `AutofillSuggestionAvailability` is
  // a sticky flag and needs to be reset when a non-autofillable field is
  // focused. The suggestion trigger source doesn't influence the set of
  // suggestions generated, but only the way suggestions behave when they are
  // accepted. For this reason, checking whether suggestions are available can
  // be done with the `kUnspecified` suggestion trigger source.
  if (external_delegate_->HasActiveScreenReader() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillDoNotUpdateAutofillAvailabilityOnFocusEvents)) {
    const FormFieldData& field =
        CHECK_DEREF(form.FindFieldByGlobalId(field_id));
    SuggestionsContext context =
        BuildSuggestionsContext(form, form_structure, field, autofill_field,
                                AutofillSuggestionTriggerSource::kUnspecified,
                                GetAcUnrecognizedBehavior(client()));
    std::vector<Suggestion> suggestions =
        GetAvailableSuggestions(form, form_structure, field, autofill_field,
                                AutofillSuggestionTriggerSource::kUnspecified,
                                /*one_time_passwords=*/{}, context);
    // Notify installed screen readers if the focus is on a field for which
    // there are suggestions to present.
    external_delegate_->OnAutofillAvailabilityEvent(
        (context.suppress_reason == SuppressReason::kNotSuppressed &&
         !suggestions.empty())
            ? mojom::AutofillSuggestionAvailability::kAutofillAvailable
            : mojom::AutofillSuggestionAvailability::kNoSuggestions);
  }
}

void BrowserAutofillManager::OnSelectControlSelectionChangedImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field_id);
  if (!form_structure || !autofill_field) {
    return;
  }

  UpdatePendingForm(form);

  const bool edited_autofilled_field =
      autofill_field->last_modifier() == FieldModifier::kAutofill;
  autofill_field->AddFieldModifier(FieldModifier::kUser);

  auto* logger = GetEventFormLogger(*autofill_field);
  if (edited_autofilled_field) {
    if (logger) {
      logger->OnEditedAutofilledField(field_id);
    }
    if (AutofillAiManager* ai_manager = client().GetAutofillAiManager();
        ai_manager &&
        autofill_field->filling_product() == FillingProduct::kAutofillAi) {
      ai_manager->OnEditedAutofilledField(*form_structure, *autofill_field,
                                          driver().GetPageUkmSourceId());
    }
  } else {
    if (logger && base::FeatureList::IsEnabled(
                      features::kAutofillTrackSelectFieldEdits)) {
      logger->OnEditedNonFilledField(field_id);
    }
  }

  // Note that it is possible that we capture a select field edit while the user
  // didn't interact with the form at all. This is because the select field
  // could have been edited by JavaScript.
  // TODO(crbug.com/480908722): Revert the comment if
  // kAutofillTrackSelectFieldEdits doesn't launch.
}

void BrowserAutofillManager::OnDidAutofillFormImpl(const FormData& form) {
  UpdatePendingForm(form);
  UpdateInitialInteractionTimestamp(base::TimeTicks::Now());
}

void BrowserAutofillManager::SuppressAutomaticRefillsImpl(
    const FillId& fill_id) {
  form_filler_->SuppressAutomaticRefills(fill_id);
}

void BrowserAutofillManager::RequestRefillImpl(const FillId& fill_id) {
  form_filler_->MaybeScheduleProgrammaticRefill(fill_id);
}

void BrowserAutofillManager::DidShowSuggestions(
    base::span<const Suggestion> suggestions,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    AutofillExternalDelegate::UpdateSuggestionsCallback
        update_suggestions_callback,
    AutofillSuggestionTriggerSource trigger_source) {
  NotifyObservers(&Observer::OnSuggestionsShown, suggestions);

  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form_id, field_id);

  const bool is_context_secure =
      form_structure ? !IsFormOrClientNonSecure(client(), *form_structure)
                     : !IsFormOrClientNonSecure(client(), last_query_form());
  GetAtMemoryManager().OnPopupShown(trigger_source, is_context_secure,
                                    update_suggestions_callback);

  const DenseSet<SuggestionType> shown_suggestion_types(suggestions,
                                                        &Suggestion::type);

  if (shown_suggestion_types.contains(SuggestionType::kIbanEntry) &&
      client().GetPaymentsAutofillClient()->GetIbanManager()) {
    client()
        .GetPaymentsAutofillClient()
        ->GetIbanManager()
        ->OnIbanSuggestionsShown(field_id);
  }

  if (AutofillAiManager* ai_manager = client().GetAutofillAiManager();
      ai_manager && form_structure && autofill_field &&
      std::ranges::any_of(shown_suggestion_types,
                          [](const SuggestionType& type) {
                            return GetFillingProductFromSuggestionType(type) ==
                                   FillingProduct::kAutofillAi;
                          })) {
    ai_manager->OnAutofillAiSuggestionsShown(
        CHECK_DEREF(form_structure), CHECK_DEREF(autofill_field), suggestions,
        driver().GetPageUkmSourceId());
  }

  // Notify the BNPL manager about suggestion shown if the current shown
  // suggestion list contains a credit card entry.
  if (payments::BnplManager* bnpl_manager = GetPaymentsBnplManager();
      bnpl_manager &&
      shown_suggestion_types.contains(SuggestionType::kCreditCardEntry)) {
    bnpl_manager->OnCreditCardSuggestionsShown(suggestions,
                                               update_suggestions_callback);
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
    // TODO: crbug.com/468793696 - Remove crashkey after investigation.
    SCOPED_CRASH_KEY_STRING64(
        "autofill", "suggestions-shown-on-typing",
        base::JoinString(base::ToVector(shown_suggestion_types,
                                        [](SuggestionType type) {
                                          return base::NumberToString(
                                              std::to_underlying(type));
                                        }),
                         ","));

    // Assert that only the expected suggestion types exist. Note that despite
    // `SuggestionType::kDatalistEntry` is optionally added by
    // `AutofillExternalDelegate`, therefore checking for it is also required.
    CHECK(DenseSet<SuggestionType>(
              {SuggestionType::kAddressEntryOnTyping,
               SuggestionType::kDatalistEntry, SuggestionType::kSeparator,
               SuggestionType::kWebauthnSignInWithAnotherDevice,
               SuggestionType::kManageAddress})
              .contains_all(shown_suggestion_types));
    address_on_typing_manager_.OnDidShowAddressOnTyping(field_id,
                                                        autofill_field);
    return;
  }

  if (std::ranges::contains(shown_suggestion_types, FillingProduct::kCreditCard,
                            GetFillingProductFromSuggestionType) &&
      IsCreditCardFidoAuthenticationEnabled()) {
    GetCreditCardAccessManager()->PrepareToFetchCreditCard();
  }

  if (shown_suggestion_types.contains(SuggestionType::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
  }

  if (shown_suggestion_types.contains(
          SuggestionType::kSaveAndFillCreditCardEntry)) {
    metrics_->credit_card_form_event_logger.OnSaveAndFillSuggestionShown();

    if (auto* save_and_fill_manager =
            client().GetPaymentsAutofillClient()->GetSaveAndFillManager()) {
      save_and_fill_manager->OnSuggestionOffered();
    }
  }

  if (std::ranges::none_of(
          shown_suggestion_types,
          AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId)) {
    return;
  }

  if (!form_structure || !autofill_field) {
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
                                 client().IsOffTheRecord(), suggestions);
  }
}

void BrowserAutofillManager::OnHidePopupImpl() {
  client().GetSingleFieldFillRouter().CancelPendingQueries();
  client().HideSuggestions(SuggestionHidingReason::kRendererEvent,
                           /*product=*/std::nullopt);
  client().HideAutofillFieldIph();
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

  FormStructure* form_structure = FindCachedFormById(form_id, /*pass_key=*/{});
  if (!form_structure) {
    return;
  }
  AutofillField* autofill_trigger_field =
      form_structure->GetFieldById(field_id);
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
  return GetCreditCardAccessManager()->ShouldClearPreviewedForm();
}

void BrowserAutofillManager::OnSelectFieldOptionsDidChangeImpl(
    const FormData& form,
    const FieldGlobalId& field_id) {
  const FormStructure* form_structure = FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }
  form_filler_->MaybeScheduleAutomaticRefill(
      form, *form_structure, RefillTriggerReason::kSelectOptionsChanged,
      AutofillTriggerSource::kSelectOptionsChanged,
      form_structure->GetFieldById(field_id));
}

void BrowserAutofillManager::OnJavaScriptChangedAutofilledValueImpl(
    const FormData& form,
    const FieldGlobalId& field_id,
    const std::u16string& old_value) {
  // Log to chrome://autofill-internals that a field's value was set by
  // JavaScript.
  auto StructureOfString = [](std::u16string str) {
    for (char16_t& c : str) {
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

  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form.global_id(), field_id);
  if (!form_structure || !autofill_field) {
    return;
  }
  AnalyzeJavaScriptChangedAutofilledValue(*form_structure, *autofill_field);
  form_filler_->MaybeScheduleAutomaticRefill(
      form, *form_structure, RefillTriggerReason::kExpirationDateFormatted,
      AutofillTriggerSource::kJavaScriptChangedAutofilledValue, *autofill_field,
      old_value);
}

void BrowserAutofillManager::OnLoadedServerPredictionsImpl(
    base::span<const raw_ref<FormStructure>> forms) {
  for (const raw_ref<FormStructure>& form : forms) {
    OnDidIdentifyFormForMetrics(
        *form, autofill_metrics::FormEventLoggerBase::FormIdentificationTime::
                   kAfterServerPredictions);
  }
  HandleLoadedServerPredictionsForAutofillAi(forms);
}

void BrowserAutofillManager::HandleLoadedServerPredictionsForAutofillAi(
    base::span<const raw_ref<FormStructure>> forms) {
  const AutofillAiModelCache* const model_cache =
      client().GetAutofillAiModelCache();

  if (!model_cache) {
    return;
  }

  for (const raw_ref<FormStructure>& form : forms) {
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
          if (!model_cache ||
              !MayPerformAutofillAiAction(
                  self->client(),
                  AutofillAiAction::
                      kUseCachedServerClassificationModelResults)) {
            return;
          }
          FormStructure* form =
              self->FindCachedFormById(form_id, /*pass_key=*/{});
          if (!form) {
            return;
          }
          auto* self_as_bam = static_cast<BrowserAutofillManager*>(self.get());
          self_as_bam->AddCachedAutofillAiPredictions(*model_cache, *form);
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

void BrowserAutofillManager::AddCachedAutofillAiPredictions(
    const AutofillAiModelCache& cache,
    FormStructure& form) {
  // Mixing Autofill AI model predictions (which come from the online LLM) and
  // Autofill AI server predictions (which come from the Autofill crowdsourcing
  // server) may lead to too many false positives. We therefore favor server
  // predictions over model predictions. (There's no specific reason for this
  // precedence -- preferring model predictions may work just as well.)
  if (std::ranges::any_of(form.fields(),
                          [](const std::unique_ptr<AutofillField>& field) {
                            return field->Type().GetGroups().contains(
                                FieldTypeGroup::kAutofillAi);
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
    if (prediction.format_string) {
      field->set_format_string_unless_overruled(
          *prediction.format_string, AutofillFormatStringSource::kModelResult);
    }
  }
  form.RationalizeAndAssignSections(client().GetVariationConfigCountryCode(),
                                    GetCurrentPageLanguage(), log_manager());
  LogCurrentFieldTypes(&form);
  NotifyObservers(&Observer::OnFieldTypesDetermined, form.global_id(),
                  Observer::FieldTypeSource::kAutofillAiModel,
                  /*small_forms_were_parsed=*/client().IsTabInActorMode());
}

void BrowserAutofillManager::AnalyzeJavaScriptChangedAutofilledValue(
    const FormStructure& form,
    AutofillField& field) {
  // We are interested in reporting the events where JavaScript resets an
  // autofilled value immediately after filling. For a reset, the value
  // needs to be empty.
  if (!field.value().empty()) {
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
  if (delta >= form_filler_->limit_before_automatic_refill()) {
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
         ShouldBeUploaded(form);
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
void BrowserAutofillManager::Reset() {
  // Process log events and record into UKM when the FormStructure is destroyed.
  ForEachCachedForm([this](const FormStructure& form_structure) {
    ProcessFieldLogEventsInForm(form_structure);
  });
  ProcessPendingFormForUpload();
  DCHECK(!pending_form_data_);

  suggestion_generators_.clear();
  four_digit_combinations_in_dom_.clear();
  last_unlocked_credit_card_cvc_.clear();
  if (touch_to_fill_delegate_) {
    touch_to_fill_delegate_->Reset();
  }
  form_filler_->Reset();
  amount_extraction_manager_.reset();
  bnpl_manager_.reset();

  credit_card_access_manager_.reset();
  autofill_ai_access_manager_->Reset();
  // Forget stored data (e.g. active subscriptions and pending callbacks) after
  // a navigation.
  otp_manager_ = std::make_unique<OtpManagerImpl>(
      *this, client().GetOneTimeTokenService());
  at_memory_manager_ = std::make_unique<AtMemoryManager>(this);
  account_name_email_strike_manager_ =
      std::make_unique<AccountNameEmailStrikeManager>(*this);
  metrics_.reset();
  AutofillManager::Reset();
  metrics_.emplace(this);
}

void BrowserAutofillManager::UpdateLoggersReadinessData(
    const FormStructure* form_structure) {
  if (!client().IsAutofillEnabled()) {
    return;
  }
  GetCreditCardAccessManager()->UpdateCreditCardFormEventLogger();
  metrics_->address_form_event_logger.UpdateProfileAvailabilityForReadiness(
      client().GetPersonalDataManager().address_data_manager().GetProfiles());
  if (ValuablesDataManager* valuables_manager =
          client().GetValuablesDataManager()) {
    metrics_->loyalty_card_form_event_logger
        .UpdateLoyaltyCardsAvailabilityForReadiness(
            valuables_manager->GetLoyaltyCards(),
            client().GetLastCommittedPrimaryMainFrameURL());
  }
  if (AutofillAiManager* ai_manager = client().GetAutofillAiManager();
      ai_manager && form_structure) {
    ai_manager->UpdateLoggerReadinessData(*form_structure);
  }
}

void BrowserAutofillManager::OnDidFillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormStructure& form,
    const AutofillField& trigger_field,
    base::span<const AutofillField* const> safe_filled_fields,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload& filling_payload,
    AutofillTriggerSource trigger_source,
    std::optional<RefillTriggerReason> refill_trigger_reason) {
  const auto safe_filled_field_ids = base::MakeFlatSet<FieldGlobalId>(
      safe_filled_fields, /*comp=*/{}, &FormFieldData::global_id);
  NotifyObservers(&Observer::OnFillOrPreviewForm, form.global_id(),
                  trigger_field.global_id(), action_persistence,
                  safe_filled_field_ids, filling_payload);
  if (action_persistence == mojom::ActionPersistence::kPreview) {
    return;
  }
  CHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);

  autofill_metrics::LogNumberOfFieldsModifiedByAutofill(
      safe_filled_fields.size(), filling_payload);
  if (refill_trigger_reason) {
    autofill_metrics::LogNumberOfFieldsModifiedByRefill(
        *refill_trigger_reason, safe_filled_fields.size());
  }

  std::visit(
      absl::Overload{[&](const AutofillProfile* profile) {
                       LogAndRecordProfileFill(
                           form, trigger_field, *profile, trigger_source,
                           refill_trigger_reason.has_value());
                     },
                     [&](const CreditCard* credit_card) {
                       LogAndRecordCreditCardFill(
                           form, trigger_field, filled_field_ids,
                           safe_filled_field_ids, *credit_card, trigger_source,
                           refill_trigger_reason.has_value());
                     },
                     [&](const EntityInstance* entity) {
                       if (AutofillAiManager* ai_manager =
                               client().GetAutofillAiManager()) {
                         ai_manager->OnDidFillSuggestion(
                             *entity, form, trigger_field, safe_filled_fields,
                             driver().GetPageUkmSourceId());
                       }
                     },
                     [&](const VerifiedProfile*) {
                       // TODO(crbug.com/380367784): consider moving the
                       // notification to the delegate here.
                     },
                     [&](const OtpFillData*) {
                       metrics_->otp_form_event_logger.OnDidFillOtpSuggestion(
                           form, trigger_field);
                     }},
      filling_payload);
}

void BrowserAutofillManager::OnDidFillOrPreviewField(
    mojom::ActionPersistence action_persistence,
    std::optional<FieldType> field_type_used) {
  if (action_persistence == mojom::ActionPersistence::kPreview) {
    return;
  }
  CHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);
  if (field_type_used == IBAN_VALUE) {
    // If the fill is triggered by the IBAN flow. We log form filling
    // specifically for IBAN field as the it is single form field filling
    // and not handled by FormEventLogger.
    IbanManager* iban_manager =
        client().GetPaymentsAutofillClient()->GetIbanManager();
    CHECK(iban_manager);
    iban_manager->LogIbanFormFilled();
  }
}

void BrowserAutofillManager::LogAndRecordCreditCardFill(
    const FormStructure& form_structure,
    const AutofillField& trigger_field,
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
        card_copy, form_structure, trigger_field, filled_field_ids,
        safe_field_ids, metrics_->signin_state_for_metrics, trigger_source);

    client().GetPersonalDataManager().payments_data_manager().RecordUseOfCard(
        card);
  }
}

void BrowserAutofillManager::LogAndRecordProfileFill(
    const FormStructure& form_structure,
    const AutofillField& trigger_field,
    const AutofillProfile& filled_profile,
    AutofillTriggerSource trigger_source,
    bool is_refill) {
  if (is_refill) {
    metrics_->address_form_event_logger.OnDidRefill(form_structure);
    return;
  }
  metrics_->address_form_event_logger.OnDidFillFormFillingSuggestion(
      filled_profile, form_structure, trigger_field, trigger_source);
  client().GetPersonalDataManager().address_data_manager().RecordUseOf(
      filled_profile);
}

void BrowserAutofillManager::LogAndRecordLoyaltyCardFill(
    const LoyaltyCard& loyalty_card,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id) {
  auto [form_structure, autofill_field] =
      GetCachedFormAndField(form_id, field_id);
  if (!form_structure || !autofill_field) {
    return;
  }
  client().GetValuablesDataManager()->RecordLoyaltyCardUsed(loyalty_card.id(),
                                                            base::Time::Now());
  // TODO(crbug.com/422366498): Move the Onfill event to `FillOrPreviewField`.
  metrics_->loyalty_card_form_event_logger.OnDidFillSuggestion(
      *form_structure, *autofill_field, loyalty_card,
      client().GetLastCommittedPrimaryMainFrameURL());
}

std::unique_ptr<FormStructure> BrowserAutofillManager::ValidateSubmittedForm(
    const FormData& form) {
  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  const FormStructure* cached_submitted_form =
      FindCachedFormById(form.global_id());
  if (!cached_submitted_form || !ShouldUploadForm(*cached_submitted_form)) {
    return nullptr;
  }

  // TODO(crbug.com/40232021): Optimize this after improving the relationship
  // between `FormStructure` and `FormData`.
  std::unique_ptr<FormStructure> submitted_form =
      FormStructure::Clone(*cached_submitted_form, /*pass_key=*/{});
  submitted_form->UpdateFormData(form, base::PassKey<BrowserAutofillManager>());

  // The form signature should match between query and upload requests to the
  // server. On many websites, form elements are dynamically added, removed, or
  // rearranged via JavaScript between page load and form submission, so we
  // copy over the |form_signature_field_names_| corresponding to the query
  // request.
  submitted_form->set_form_signature(cached_submitted_form->form_signature());

  return submitted_form;
}

BrowserAutofillManager::FormAndField
BrowserAutofillManager::GetCachedFormAndField(const FormGlobalId& form_id,
                                              const FieldGlobalId& field_id) {
  FormStructure* cached_form = FindCachedFormById(form_id, /*pass_key=*/{});
  if (!cached_form) {
    return {};
  }
  return FormAndField{cached_form, cached_form->GetFieldById(field_id)};
}

autofill_metrics::CreditCardFormEventLogger&
BrowserAutofillManager::GetCreditCardFormEventLogger() {
  return metrics_->credit_card_form_event_logger;
}

// TODO(crbug.com/409962888): Remove once the new suggestion generation logic is
// launched.
std::vector<Suggestion> BrowserAutofillManager::GetProfileSuggestions(
    const FormData& form,
    const FormStructure& form_structure,
    const FormFieldData& trigger_field,
    const AutofillField& trigger_autofill_field,
    AutofillSuggestionTriggerSource trigger_source) {
  std::vector<Suggestion> suggestions;
  AddressSuggestionGenerator address_suggestion_generator(trigger_source);

  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  address_suggestion_generator.GenerateSuggestions(
      form, trigger_field, &form_structure, &trigger_autofill_field, client(),
      on_suggestions_generated);
  return suggestions;
}

std::vector<Suggestion> BrowserAutofillManager::GetLoyaltyCardSuggestions(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field) {
  ValuablesDataManager* valuables_manager = client().GetValuablesDataManager();
  if (!valuables_manager) {
    return {};
  }
  const PasswordFormClassification password_form_classification =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableNonAffiliatedLoyaltyCardsFilling)
          ? client().ClassifyAsPasswordForm(*this, form_structure->global_id(),
                                            autofill_field->global_id())
          : PasswordFormClassification();

  return GetSuggestionsForLoyaltyCards(form, form_structure, field,
                                       autofill_field,
                                       password_form_classification, client());
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
  const bool contains_standalone_cvc_field = std::ranges::any_of(
      form_structure.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->Type().GetCreditCardType() ==
               CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
      });
  if (contains_standalone_cvc_field) {
    FetchPotentialCardLastFourDigitsCombinationFromDOM();
  }
  if (data_util::ContainsPhone(data_util::DetermineGroups(form_structure))) {
    metrics_->has_observed_phone_number_field = true;
  }

  for (const std::unique_ptr<AutofillField>& field : form_structure) {
    if (field->html_type() == HtmlFieldType::kOneTimeCode) {
      metrics_->has_observed_one_time_code_field = true;
      break;
    }
  }
  OnDidIdentifyFormForMetrics(
      form_structure, autofill_metrics::FormEventLoggerBase::
                          FormIdentificationTime::kAfterLocalHeuristics);
  // `autofill_optimization_guide_decider` is not present on unsupported
  // platforms.
  if (auto* autofill_optimization_guide_decider =
          client().GetAutofillOptimizationGuideDecider()) {
    // Initiate necessary pre-processing based on the forms and fields that are
    // parsed, as well as the information that the user has saved in the web
    // database.
    autofill_optimization_guide_decider->OnDidParseForm(
        form_structure,
        client().GetPersonalDataManager().payments_data_manager());
  }

  if (AutofillAiManager* ai_manager = client().GetAutofillAiManager()) {
    ai_manager->OnFormSeen(form_structure);
  }

  // If a form with the same FormGlobalId was previously filled, the structure
  // of the form changed, and we might be able to refill the form with other
  // information.
  form_filler_->MaybeScheduleAutomaticRefill(form, form_structure,
                                             RefillTriggerReason::kFormChanged,
                                             AutofillTriggerSource::kFormsSeen);
}

void BrowserAutofillManager::OnDidIdentifyFormForMetrics(
    const FormStructure& form_structure,
    autofill_metrics::FormEventLoggerBase::FormIdentificationTime
        identification_time) {
  DenseSet<FormType> form_types =
      form_structure.GetFormTypes(GetAcUnrecognizedBehavior(client()));
  const bool card_form = form_types.contains(FormType::kCreditCardForm) ||
                         form_types.contains(FormType::kStandaloneCvcForm);
  const bool address_form = form_types.contains(FormType::kAddressForm);
  const bool loyalty_card_form =
      form_types.contains(FormType::kLoyaltyCardForm);
  const bool otp_form = form_types.contains(FormType::kOneTimePasswordForm);
  if (card_form) {
    metrics_->credit_card_form_event_logger.OnDidIdentifyForm(
        form_structure, identification_time);
  }
  if (address_form) {
    metrics_->address_form_event_logger.OnDidIdentifyForm(form_structure,
                                                          identification_time);
  }
  if (loyalty_card_form) {
    metrics_->loyalty_card_form_event_logger.OnDidIdentifyForm(
        form_structure, identification_time);
  }
  if (otp_form) {
    metrics_->otp_form_event_logger.OnDidIdentifyForm(form_structure,
                                                      identification_time);
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
    AutofillField& autofill_field,
    FillingProduct filling_product,
    bool has_suggestions) {
  if (filling_product != FillingProduct::kAddress &&
      filling_product != FillingProduct::kCreditCard) {
    // The ablation study only supports addresses and credit cards.
    return false;
  }

  FormTypeForAblationStudy form_type =
      filling_product == FillingProduct::kCreditCard
          ? FormTypeForAblationStudy::kPayment
          : FormTypeForAblationStudy::kAddress;

  // The `ablation_group` indicates if the form filling is under ablation,
  // meaning that autofill popups are suppressed. If ablation_group is
  // AblationGroup::kDefault or AblationGroup::kControl, no ablation happens
  // in the following.
  AblationGroup ablation_group = client().GetAblationStudy().GetAblationGroup(
      client().GetLastCommittedPrimaryMainFrameURL(), form_type,
      client().GetAutofillOptimizationGuideDecider());

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
      has_suggestions ? ablation_group : AblationGroup::kDefault;

  // For both form types (credit card and address forms), we inform the other
  // event logger also about the ablation. This prevents for example that for
  // an encountered address form we log a sample
  // Autofill.Funnel.ParsedAsType.CreditCard = 0 (which would be recorded by
  // the metrics_->credit_card_form_event_logger). For the complementary event
  // logger, the conditional ablation status is logged as kDefault to not
  // imply that data would be filled without ablation.
  if (filling_product == FillingProduct::kCreditCard) {
    metrics_->credit_card_form_event_logger.SetAblationStatus(
        ablation_group, conditional_ablation_group);
    metrics_->address_form_event_logger.SetAblationStatus(
        ablation_group, AblationGroup::kDefault);
  } else if (filling_product == FillingProduct::kAddress) {
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

  if (has_suggestions && ablation_group == AblationGroup::kAblation &&
      !features::kAutofillAblationStudyIsDryRun.Get()) {
    return true;
  }

  return false;
}

void BrowserAutofillManager::MergeIdentityCredentialsAndAddressSuggestions(
    std::vector<Suggestion>& suggestion,
    std::vector<Suggestion> identity_credential_suggestions) {
  // TODO(crbug.com/380367784): figure out what to do when both verified
  // and unverified suggestions point to the same email address.
  suggestion.insert(
      suggestion.begin(),
      std::make_move_iterator(identity_credential_suggestions.begin()),
      std::make_move_iterator(identity_credential_suggestions.end()));
}

std::vector<Suggestion> BrowserAutofillManager::GetAvailableSuggestions(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    AutofillField* autofill_field,
    AutofillSuggestionTriggerSource trigger_source,
    const std::vector<std::string>& one_time_passwords,
    SuggestionsContext& context) {
  if (context.do_not_generate_autofill_suggestions) {
    return {};
  }

  if (!form_structure || !autofill_field) {
    return {};
  }

  // TODO(crbug.com/489659527): This currently overrides Autofill suggestions if
  // suggestions were presented to the user after typing started. Fix that.
  if (trigger_source ==
      AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge) {
    // TODO(crbug.com/489659527): Localize the string.
    return {Suggestion(u"Try searching in Chrome Memory",
                       SuggestionType::kAtMemoryInactivityNudge)};
  }

  std::vector<Suggestion> suggestions;
  switch (context.filling_product) {
    case FillingProduct::kAddress:
      if (client().IsAutofillProfileEnabled()) {
        suggestions = GetProfileSuggestions(form, *form_structure, field,
                                            *autofill_field, trigger_source);
      }
      if (autofill_field->Type().GetLoyaltyCardType() ==
          EMAIL_OR_LOYALTY_MEMBERSHIP_ID) {
        if (ValuablesDataManager* valuables_manager =
                client().GetValuablesDataManager()) {
          if (suggestions.empty()) {
            suggestions = GetLoyaltyCardSuggestions(form, form_structure, field,
                                                    autofill_field);
          } else {
            std::vector<Suggestion> loyalty_cards_suggestions_for_merge =
                CreateLoyaltyCardSuggestionsForMerge(
                    *valuables_manager,
                    client().GetLastCommittedPrimaryMainFrameURL());
            MergeLoyaltyCardsAndAddressSuggestions(
                suggestions, std::move(loyalty_cards_suggestions_for_merge));
          }
        }
      }
      break;
    case FillingProduct::kCreditCard:
      if (client()
              .GetPaymentsAutofillClient()
              ->IsAutofillPaymentMethodsEnabled()) {
        suggestions = GetSuggestionsForCreditCards(
            form, *form_structure, field, *autofill_field, client(),
            four_digit_combinations_in_dom_, &GetAmountExtractionManager(),
            GetPaymentsBnplManager(), metrics_->credit_card_form_event_logger,
            metrics_->signin_state_for_metrics,
            /*exclude_virtual_cards=*/false);
      }
      break;
    case FillingProduct::kLoyaltyCard:
      // Only loyalty card numbers filling is supported.
      if (autofill_field->Type().GetLoyaltyCardType() ==
          LOYALTY_MEMBERSHIP_ID) {
        suggestions = GetLoyaltyCardSuggestions(form, form_structure, field,
                                                autofill_field);
      }
      break;
    case FillingProduct::kOneTimePassword:
      suggestions = BuildOtpSuggestions(one_time_passwords);
      break;
    case FillingProduct::kAtMemory:
      return {};
    default:
      // Skip other filling products.
      break;
  }

  if (EvaluateAblationStudy(CHECK_DEREF(autofill_field),
                            context.filling_product, !suggestions.empty())) {
    // Logic for disabling/ablating autofill.
    context.suppress_reason = SuppressReason::kAblation;
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
              form, form_structure, field, autofill_field, client());
      // Insert verified suggestions above unverified ones.
      MergeIdentityCredentialsAndAddressSuggestions(
          suggestions, std::move(verified_suggestions));
    }
  }

  return suggestions;
}

autofill_metrics::FormEventLoggerBase*
BrowserAutofillManager::GetEventFormLogger(const AutofillField& field) {
  // TODO(crbug.com/432645177): When migrating Loyalty Cards to AutofillType, we
  // need to pick the right logger(s) here.
  const DenseSet<FormType> form_types = field.Type().GetFormTypes();
  switch (!form_types.empty() ? *form_types.begin()
                              : FormType::kUnknownFormType) {
    case FormType::kAddressForm:
      return &metrics_->address_form_event_logger;
    case FormType::kCreditCardForm:
    case FormType::kStandaloneCvcForm:
      return &metrics_->credit_card_form_event_logger;
    case FormType::kLoyaltyCardForm:
      return &metrics_->loyalty_card_form_event_logger;
    case FormType::kOneTimePasswordForm:
      return &metrics_->otp_form_event_logger;
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
      ShouldUploadUkm(form_structure, /*require_classified_field=*/true);

  for (const std::unique_ptr<AutofillField>& autofill_field : form_structure) {
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
    form_events.insert_all(
        metrics_->loyalty_card_form_event_logger.GetFormEvents(
            form_structure.global_id()));
    client().GetFormInteractionsUkmLogger().LogAutofillFormSummaryAtFormRemove(
        driver().GetPageUkmSourceId(), form_structure, form_events,
        metrics_->initial_interaction_timestamp,
        metrics_->form_submitted_timestamp);
    client().GetFormInteractionsUkmLogger().LogFocusedComplexFormAtFormRemove(
        driver().GetPageUkmSourceId(), form_structure, form_events,
        metrics_->initial_interaction_timestamp,
        metrics_->form_submitted_timestamp,
        GetAcUnrecognizedBehavior(client()));
  }

  for (const std::unique_ptr<AutofillField>& autofill_field : form_structure) {
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

  for (const std::unique_ptr<AutofillField>& autofill_field : form_structure) {
    for (const AutofillField::FieldLogEventType& log_event :
         autofill_field->field_log_events()) {
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

void BrowserAutofillManager::InitializeSuggestionGenerators(
    AutofillSuggestionTriggerSource trigger_source,
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  // Suggestion generators lifespan should be limited to only when they are
  // needed.
  suggestion_generators_.clear();
  const FillingProductSet relevant_filling_products =
      GetFillingProductsToSuggest(trigger_source);

  if (trigger_source ==
      AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge) {
    suggestion_generators_.push_back(
        std::make_unique<AtMemoryNudgeGenerator>());
  }

  if (relevant_filling_products.contains(FillingProduct::kAutofillAi)) {
    suggestion_generators_.push_back(
        std::make_unique<AutofillAiSuggestionGenerator>());
  }
  if (relevant_filling_products.contains(FillingProduct::kIban)) {
    suggestion_generators_.push_back(
        std::make_unique<IbanSuggestionGenerator>());
  }
  if (relevant_filling_products.contains(FillingProduct::kMerchantPromoCode)) {
    suggestion_generators_.push_back(
        std::make_unique<MerchantPromoCodeSuggestionGenerator>());
  }
  if (relevant_filling_products.contains(FillingProduct::kAutocomplete) &&
      client().GetAutocompleteHistoryManager()) {
    suggestion_generators_.push_back(
        std::make_unique<AutocompleteSuggestionGenerator>(
            client().GetAutocompleteHistoryManager()->GetProfileDatabase()));
  }
  if (relevant_filling_products.contains(FillingProduct::kLoyaltyCard) &&
      client().GetValuablesDataManager()) {
    const PasswordFormClassification password_form_classification =
        base::FeatureList::IsEnabled(
            features::kAutofillEnableNonAffiliatedLoyaltyCardsFilling)
            ? client().ClassifyAsPasswordForm(*this, form_id, field_id)
            : PasswordFormClassification();

    suggestion_generators_.push_back(
        std::make_unique<LoyaltyCardSuggestionGenerator>(
            password_form_classification));
  }
  if (relevant_filling_products.contains(FillingProduct::kCompose) &&
      client().GetComposeDelegate()) {
    suggestion_generators_.push_back(
        std::make_unique<ComposeSuggestionGenerator>(trigger_source));
  }
  if (relevant_filling_products.contains(FillingProduct::kIdentityCredential)) {
    if (IdentityCredentialDelegate* delegate =
            client().GetIdentityCredentialDelegate()) {
      if (std::unique_ptr<SuggestionGenerator> suggestion_generator =
              delegate->GetIdentityCredentialSuggestionGenerator()) {
        suggestion_generators_.push_back(std::move(suggestion_generator));
      }
    }
  }
  if (relevant_filling_products.contains(FillingProduct::kPasskey)) {
    suggestion_generators_.push_back(
        std::make_unique<PasskeySuggestionGenerator>());
  }
  if (relevant_filling_products.contains(FillingProduct::kOneTimePassword) &&
      otp_manager_) {
    suggestion_generators_.push_back(
        std::make_unique<OtpSuggestionGenerator>(*otp_manager_));
  }
  if (relevant_filling_products.contains(FillingProduct::kAddress)) {
    suggestion_generators_.push_back(
        std::make_unique<AddressSuggestionGenerator>(trigger_source));
  }
  if (relevant_filling_products.contains(FillingProduct::kCreditCard)) {
    suggestion_generators_.push_back(
        std::make_unique<CreditCardSuggestionGenerator>(
            four_digit_combinations_in_dom_, &GetAmountExtractionManager(),
            GetPaymentsBnplManager(), &metrics_->credit_card_form_event_logger,
            metrics_->signin_state_for_metrics,
            /*exclude_virtual_cards=*/false));
  }
}

base::WeakPtr<BrowserAutofillManager>
BrowserAutofillManager::GetBrowserAutofillManagerWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
