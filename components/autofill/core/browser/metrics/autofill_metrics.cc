// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/popup_interaction.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/language/core/browser/language_usage_metrics.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

using mojom::SubmissionSource;

namespace {

using PaymentsRpcCardType =
    payments::PaymentsAutofillClient::PaymentsRpcCardType;
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

constexpr char kUserActionRootPopupShown[] =
    "Autofill_PopupInteraction_PopupLevel_0_SuggestionShown";
constexpr char kUserActionSecondLevelPopupShown[] =
    "Autofill_PopupInteraction_PopupLevel_1_SuggestionShown";
constexpr char kUserActionThirdLevelPopupShown[] =
    "Autofill_PopupInteraction_PopupLevel_2_SuggestionShown";
constexpr char kUserActionRootPopupSuggestionSelected[] =
    "Autofill_PopupInteraction_PopupLevel_0_SuggestionSelected";
constexpr char kUserActionSecondLevelPopupSuggestionSelected[] =
    "Autofill_PopupInteraction_PopupLevel_1_SuggestionSelected";
constexpr char kUserActionThirdLevelPopupSuggestionSelected[] =
    "Autofill_PopupInteraction_PopupLevel_2_SuggestionSelected";
constexpr char kUserActionRootPopupSuggestionAccepted[] =
    "Autofill_PopupInteraction_PopupLevel_0_SuggestionAccepted";
constexpr char kUserActionSecondLevelPopupSuggestionAccepted[] =
    "Autofill_PopupInteraction_PopupLevel_1_SuggestionAccepted";
constexpr char kUserActionThirdLevelPopupSuggestionAccepted[] =
    "Autofill_PopupInteraction_PopupLevel_2_SuggestionAccepted";

// Translates structured name types into simple names that are used for
// naming histograms.
constexpr auto kStructuredNameTypeToNameMap =
    base::MakeFixedFlatMap<FieldType, std::string_view>(
        {{NAME_FULL, "Full"},
         {NAME_FIRST, "First"},
         {NAME_MIDDLE, "Middle"},
         {NAME_LAST, "Last"},
         {NAME_LAST_FIRST, "FirstLast"},
         {NAME_LAST_SECOND, "SecondLast"}});

// Translates structured address types into simple names that are used for
// naming histograms.
constexpr auto kStructuredAddressTypeToNameMap =
    base::MakeFixedFlatMap<FieldType, std::string_view>(
        {{ADDRESS_HOME_STREET_ADDRESS, "StreetAddress"},
         {ADDRESS_HOME_STREET_NAME, "StreetName"},
         {ADDRESS_HOME_HOUSE_NUMBER, "HouseNumber"},
         {ADDRESS_HOME_FLOOR, "FloorNumber"},
         {ADDRESS_HOME_APT_NUM, "ApartmentNumber"},
         {ADDRESS_HOME_SUBPREMISE, "SubPremise"}});

}  // namespace

// This function encodes the integer value of a |FieldType| and the
// metric value of an |AutofilledFieldUserEditingStatus| into a 16 bit integer.
// The lower four bits are used to encode the editing status and the higher
// 12 bits are used to encode the field type.
int GetFieldTypeUserEditStatusMetric(
    FieldType server_type,
    AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric) {
  static_assert(FieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 4),
                "Autofill::ServerTypes value needs more than 12 bits.");

  static_assert(
      static_cast<int>(
          AutofillMetrics::AutofilledFieldUserEditingStatusMetric::kMaxValue) <=
          (UINT16_MAX >> 12),
      "AutofillMetrics::AutofilledFieldUserEditingStatusMetric value needs "
      "more than 4 bits");

  return (server_type << 4) | static_cast<int>(metric);
}

const int kMaxBucketsCount = 50;

// static
AutofillMetrics::AutocompleteState
AutofillMetrics::AutocompleteStateForSubmittedField(
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

// static
void AutofillMetrics::LogSubmittedCardStateMetric(
    SubmittedCardStateMetric metric) {
  DCHECK_LT(metric, NUM_SUBMITTED_CARD_STATE_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.SubmittedCardState", metric,
                            NUM_SUBMITTED_CARD_STATE_METRICS);
}

// static
void AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
    SubmittedServerCardExpirationStatusMetric metric) {
  DCHECK_LT(metric, NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.SubmittedServerCardExpirationStatus", metric,
      NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
}

// static
void AutofillMetrics::LogUploadOfferedCardOriginMetric(
    UploadOfferedCardOriginMetric metric) {
  DCHECK_LT(metric, NUM_UPLOAD_OFFERED_CARD_ORIGIN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UploadOfferedCardOrigin", metric,
                            NUM_UPLOAD_OFFERED_CARD_ORIGIN_METRICS);
}

// static
void AutofillMetrics::LogUploadAcceptedCardOriginMetric(
    UploadAcceptedCardOriginMetric metric) {
  DCHECK_LT(metric, NUM_UPLOAD_ACCEPTED_CARD_ORIGIN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UploadAcceptedCardOrigin", metric,
                            NUM_UPLOAD_ACCEPTED_CARD_ORIGIN_METRICS);
}

// static
void AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogResultMetric(
    CardUnmaskAuthenticationSelectionDialogResultMetric metric) {
  DCHECK_LE(metric,
            CardUnmaskAuthenticationSelectionDialogResultMetric::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result", metric);
}

// static
void AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogShown(
    size_t number_of_challenge_options) {
  static_assert(static_cast<int>(CardUnmaskChallengeOptionType::kMaxValue) <
                10);
  DCHECK_GE(number_of_challenge_options, 0U);
  // We are using an exact linear histogram, with a max of 10. This is a
  // reasonable max so that the histogram is not sparse, as we do not foresee
  // ever having more than 10 challenge options at the same time on a dialog to
  // authenticate a virtual card.
  base::UmaHistogramExactLinear(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown2",
      number_of_challenge_options, /*exclusive_max=*/10);
}

// static
void AutofillMetrics::LogCreditCardInfoBarMetric(
    InfoBarMetric metric,
    bool is_uploading,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);

  std::string destination = is_uploading ? ".Server" : ".Local";
  base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination,
                                metric, NUM_INFO_BAR_METRICS);
  if (options.should_request_name_from_user) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".RequestingCardholderName",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".RequestingExpirationDate",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.has_multiple_legal_lines) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardInfoBar" + destination + ".WithMultipleLegalLines",
        metric, NUM_INFO_BAR_METRICS);
  }

  if (options.has_same_last_four_as_server_card_but_different_expiration_date) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".WithSameLastFourButDifferentExpiration",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.card_save_type ==
      payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc) {
    base::UmaHistogramEnumeration(base::StrCat({"Autofill.CreditCardInfoBar",
                                                destination, ".SavingWithCvc"}),
                                  metric, NUM_INFO_BAR_METRICS);
  }
}

// static
void AutofillMetrics::LogScanCreditCardPromptMetric(
    ScanCreditCardPromptMetric metric) {
  DCHECK_LT(metric, NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ScanCreditCardPrompt", metric,
                            NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardCompleted(base::TimeDelta duration,
                                                 bool completed) {
  std::string suffix = completed ? "Completed" : "Cancelled";
  base::UmaHistogramLongTimes("Autofill.ScanCreditCard.Duration_" + suffix,
                              duration);
  UMA_HISTOGRAM_BOOLEAN("Autofill.ScanCreditCard.Completed", completed);
}

// static
void AutofillMetrics::LogProgressDialogResultMetric(
    bool is_canceled_by_user,
    AutofillProgressDialogType autofill_progress_dialog_type) {
  base::UmaHistogramBoolean(base::StrCat({"Autofill.ProgressDialog.",
                                          GetDialogTypeStringForLogging(
                                              autofill_progress_dialog_type),
                                          ".Result"}),
                            is_canceled_by_user);
}

void AutofillMetrics::LogProgressDialogShown(
    AutofillProgressDialogType autofill_progress_dialog_type) {
  base::UmaHistogramBoolean(base::StrCat({"Autofill.ProgressDialog.",
                                          GetDialogTypeStringForLogging(
                                              autofill_progress_dialog_type),
                                          ".Shown"}),
                            true);
}

std::string_view AutofillMetrics::GetDialogTypeStringForLogging(
    AutofillProgressDialogType autofill_progress_dialog_type) {
  switch (autofill_progress_dialog_type) {
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      return "VirtualCardUnmask";
    case AutofillProgressDialogType::kServerCardUnmaskProgressDialog:
      return "ServerCardUnmask";
    case AutofillProgressDialogType::kServerIbanUnmaskProgressDialog:
      return "ServerIbanUnmask";
    case AutofillProgressDialogType::k3dsFetchVcnProgressDialog:
      return "3dsFetchVirtualCard";
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
  }
}

// static
void AutofillMetrics::LogUnmaskPromptEvent(UnmaskPromptEvent event,
                                           bool has_valid_nickname,
                                           CreditCard::RecordType card_type) {
  base::UmaHistogramEnumeration("Autofill.UnmaskPrompt" +
                                    GetHistogramStringForCardType(card_type) +
                                    ".Events",
                                event, NUM_UNMASK_PROMPT_EVENTS);
  if (has_valid_nickname) {
    base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.Events.WithNickname",
                                  event, NUM_UNMASK_PROMPT_EVENTS);
  }
}

// static
void AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
    CardholderNameFixFlowPromptEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.CardholderNameFixFlowPrompt.Events",
                            event, NUM_CARDHOLDER_NAME_FIXFLOW_PROMPT_EVENTS);
}

// static
void AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
    ExpirationDateFixFlowPromptEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ExpirationDateFixFlowPrompt.Events",
                            event);
}

// static
void AutofillMetrics::LogExpirationDateFixFlowPromptShown() {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ExpirationDateFixFlowPromptShown", true);
}

// static
void AutofillMetrics::LogUnmaskPromptEventDuration(
    base::TimeDelta duration,
    UnmaskPromptEvent close_event,
    bool has_valid_nickname) {
  std::string suffix;
  switch (close_event) {
    case UNMASK_PROMPT_CLOSED_NO_ATTEMPTS:
      suffix = ".NoAttempts";
      break;
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE:
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE:
      suffix = ".Failure";
      break;
    case UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING:
      suffix = ".AbandonUnmasking";
      break;
    case UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT:
    case UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS:
      suffix = ".Success";
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration", duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration" + suffix,
                              duration);

  if (has_valid_nickname) {
    base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration.WithNickname",
                                duration);
    base::UmaHistogramLongTimes(
        "Autofill.UnmaskPrompt.Duration" + suffix + ".WithNickname", duration);
  }
}

// static
void AutofillMetrics::LogTimeBeforeAbandonUnmasking(base::TimeDelta duration,
                                                    bool has_valid_nickname) {
  base::UmaHistogramLongTimes(
      "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking", duration);
  if (has_valid_nickname) {
    base::UmaHistogramLongTimes(
        "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking.WithNickname",
        duration);
  }
}

// static
void AutofillMetrics::LogRealPanResult(PaymentsRpcResult result,
                                       PaymentsRpcCardType card_type) {
  PaymentsRpcMetricResult metric_result;
  switch (result) {
    case PaymentsRpcResult::kSuccess:
      metric_result = PAYMENTS_RESULT_SUCCESS;
      break;
    case PaymentsRpcResult::kTryAgainFailure:
      metric_result = PAYMENTS_RESULT_TRY_AGAIN_FAILURE;
      break;
    case PaymentsRpcResult::kPermanentFailure:
      metric_result = PAYMENTS_RESULT_PERMANENT_FAILURE;
      break;
    case PaymentsRpcResult::kNetworkError:
      metric_result = PAYMENTS_RESULT_NETWORK_ERROR;
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_TRY_AGAIN_FAILURE;
      break;
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE;
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      metric_result = PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT;
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED();
  }

  std::string card_type_suffix;
  switch (card_type) {
    case PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case PaymentsRpcCardType::kUnknown:
      NOTREACHED();
  }

  base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.GetRealPanResult",
                                metric_result);

  base::UmaHistogramEnumeration(
      "Autofill.UnmaskPrompt.GetRealPanResult." + card_type_suffix,
      metric_result);
}

// static
void AutofillMetrics::LogRealPanDuration(base::TimeDelta duration,
                                         PaymentsRpcResult result,
                                         PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case PaymentsRpcCardType::kUnknown:
      // Unknown card types imply UnmaskCardRequest::ParseResponse() was never
      // called, due to bad internet connection or otherwise. Log anyway so that
      // we have a rough idea of the magnitude of this problem.
      card_type_suffix = "UnknownCard";
      break;
  }

  switch (result) {
    case PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      result_suffix = "ClientSideTimeout";
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED();
  }

  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration",
                              duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration." +
                                  card_type_suffix + "." + result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogUnmaskingDuration(base::TimeDelta duration,
                                           PaymentsRpcResult result,
                                           PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case PaymentsRpcCardType::kUnknown:
      NOTREACHED();
  }

  switch (result) {
    case PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      result_suffix = "ClientSideTimeout";
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED();
  }
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration",
                              duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration." +
                                  card_type_suffix + "." + result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogDeveloperEngagementMetric(
    DeveloperEngagementMetric metric) {
  DCHECK_LT(metric, NUM_DEVELOPER_ENGAGEMENT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.DeveloperEngagement", metric,
                            NUM_DEVELOPER_ENGAGEMENT_METRICS);
}

// static
void AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field) {
  AutofilledFieldUserEditingStatusMetric editing_metric =
      field.previously_autofilled()
          ? AutofilledFieldUserEditingStatusMetric::AUTOFILLED_FIELD_WAS_EDITED
          : AutofilledFieldUserEditingStatusMetric::
                AUTOFILLED_FIELD_WAS_NOT_EDITED;

  // Record the aggregated UMA statistics.
  base::UmaHistogramEnumeration(
      "Autofill.EditedAutofilledFieldAtSubmission2.Aggregate", editing_metric);

  // Record the type specific UMA statistics.
  base::UmaHistogramSparse(
      "Autofill.EditedAutofilledFieldAtSubmission2.ByFieldType",
      GetFieldTypeUserEditStatusMetric(field.Type().GetStorableType(),
                                       editing_metric));

  // Record the metric for FormsAI specific fields.
  if (field.filling_product() == FillingProduct::kPredictionImprovements) {
    base::UmaHistogramEnumeration(
        "Autofill.FormsAI.EditedAutofilledFieldAtSubmission", editing_metric);
  }

  // Record the UMA statistics spliced by the autocomplete attribute value.
  FormType form_type = FieldTypeGroupToFormType(field.Type().group());
  if (form_type == FormType::kAddressForm ||
      form_type == FormType::kCreditCardForm) {
    bool autocomplete_off = field.autocomplete_attribute() == "off";
    const std::string autocomplete_histogram = base::StrCat(
        {"Autofill.Autocomplete.", autocomplete_off ? "Off" : "NotOff",
         ".EditedAutofilledFieldAtSubmission2.",
         form_type == FormType::kAddressForm ? "Address" : "CreditCard"});
    base::UmaHistogramEnumeration(autocomplete_histogram, editing_metric);
  }

  // If the field was edited, record the event to UKM.
  if (editing_metric ==
      AutofilledFieldUserEditingStatusMetric::AUTOFILLED_FIELD_WAS_EDITED) {
    form_interactions_ukm_logger->LogEditedAutofilledFieldAtSubmission(form,
                                                                       field);
  }
}

// static
void AutofillMetrics::LogServerQueryMetric(ServerQueryMetric metric) {
  DCHECK_LT(metric, NUM_SERVER_QUERY_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    base::TimeDelta duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithAutofill", duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    base::TimeDelta duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithoutAutofill",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadForOneTimeCode(
    base::TimeDelta duration) {
  LogFormFillDuration("Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromInteraction(
    const DenseSet<FormType>& form_types,
    bool used_autofill,
    base::TimeDelta duration) {
  std::string parent_metric;
  if (used_autofill) {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithAutofill";
  } else {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithoutAutofill";
  }
  LogFormFillDuration(parent_metric, duration);
  if (base::Contains(form_types, FormType::kCreditCardForm)) {
    LogFormFillDuration(parent_metric + ".CreditCard", duration);
  }
  if (base::Contains(form_types, FormType::kAddressForm)) {
    LogFormFillDuration(parent_metric + ".Address", duration);
  }
  if (base::Contains(form_types, FormType::kPasswordForm)) {
    LogFormFillDuration(parent_metric + ".Password", duration);
  }
  if (base::Contains(form_types, FormType::kUnknownFormType)) {
    LogFormFillDuration(parent_metric + ".Unknown", duration);
  }
}

// static
void AutofillMetrics::LogFormFillDurationFromInteractionForOneTimeCode(
    base::TimeDelta duration) {
  LogFormFillDuration(
      "Autofill.WebOTP.OneTimeCode.FillDuration.FromInteraction", duration);
}

// static
void AutofillMetrics::LogFormFillDuration(const std::string& metric,
                                          base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(metric, duration, base::Milliseconds(100),
                                base::Minutes(10), 50);
}

// static
void AutofillMetrics::LogStoredCreditCardMetrics(
    const std::vector<std::unique_ptr<CreditCard>>& local_cards,
    const std::vector<std::unique_ptr<CreditCard>>& server_cards,
    size_t server_card_count_with_card_art_image,
    base::TimeDelta disused_data_threshold) {
  size_t num_local_cards = 0;
  size_t num_local_cards_with_nickname = 0;
  size_t num_local_cards_with_invalid_number = 0;
  size_t num_server_cards = 0;
  size_t num_server_cards_with_nickname = 0;
  size_t num_disused_local_cards = 0;
  size_t num_disused_server_cards = 0;

  // Concatenate the local and server cards into one big collection of raw
  // CreditCard pointers.
  std::vector<const CreditCard*> credit_cards;
  credit_cards.reserve(local_cards.size() + server_cards.size());
  for (const auto* collection : {&local_cards, &server_cards}) {
    for (const auto& card : *collection) {
      credit_cards.push_back(card.get());
    }
  }

  // Iterate over all of the cards and gather metrics.
  const base::Time now = AutofillClock::Now();
  for (const CreditCard* card : credit_cards) {
    const base::TimeDelta time_since_last_use = now - card->use_date();
    const int days_since_last_use = time_since_last_use.InDays();
    const int disused_delta =
        (time_since_last_use > disused_data_threshold) ? 1 : 0;
    UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.StoredCreditCard",
                              days_since_last_use);
    switch (card->record_type()) {
      case CreditCard::RecordType::kLocalCard:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Local",
            days_since_last_use);
        num_local_cards += 1;
        num_disused_local_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_local_cards_with_nickname += 1;
        if (!card->HasValidCardNumber()) {
          num_local_cards_with_invalid_number += 1;
        }
        break;
      case CreditCard::RecordType::kMaskedServerCard:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        num_server_cards += 1;
        num_disused_server_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_server_cards_with_nickname += 1;
        break;
      // These card types are not persisted in Chrome.
      case CreditCard::RecordType::kFullServerCard:
      case CreditCard::RecordType::kVirtualCard:
        NOTREACHED();
    }
  }

  // Calculate some summary info.
  const size_t num_cards = num_local_cards + num_server_cards;
  const size_t num_disused_cards =
      num_disused_local_cards + num_disused_server_cards;

  // Log the overall counts.
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount", num_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local",
                            num_local_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local.WithNickname",
                            num_local_cards_with_nickname);
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.StoredCreditCardCount.Local.WithInvalidCardNumber",
      num_local_cards_with_invalid_number);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server",
                            num_server_cards);
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.StoredCreditCardCount.Server.WithNickname",
      num_server_cards_with_nickname);

  // For card types held by the user, log how many are disused.
  if (num_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount",
                              num_disused_cards);
  }
  if (num_local_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Local",
                              num_disused_local_cards);
  }
  if (num_server_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Server",
                              num_disused_server_cards);
  }

  // Log the number of server cards that are enrolled with virtual cards.
  size_t virtual_card_enabled_card_count = base::ranges::count_if(
      server_cards, [](const std::unique_ptr<CreditCard>& card) {
        return card->virtual_card_enrollment_state() ==
               CreditCard::VirtualCardEnrollmentState::kEnrolled;
      });
  base::UmaHistogramCounts1000(
      "Autofill.StoredCreditCardCount.Server.WithVirtualCardMetadata",
      virtual_card_enabled_card_count);

  // Log the number of server cards that have valid customized art images.
  base::UmaHistogramCounts1000(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage",
      server_card_count_with_card_art_image);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
    size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsSuppressedForDisuse",
                            num_cards);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsDeletedForDisuse(size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsDeletedForDisuse", num_cards);
}

// static
void AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1M(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", num_profiles);
}

// static
void AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.AddressesSuppressedForDisuse",
                            num_profiles);
}

// static
void AutofillMetrics::LogAutofillSuggestionHidingReason(
    FillingProduct filling_product,
    SuggestionHidingReason reason) {
  base::UmaHistogramEnumeration("Autofill.PopupHidingReason", reason);
  base::UmaHistogramEnumeration(base::StrCat({
                                    "Autofill.PopupHidingReason.",
                                    FillingProductToString(filling_product),
                                }),
                                reason);
}

void AutofillMetrics::LogPopupInteraction(FillingProduct filling_product,
                                          int popup_level,
                                          PopupInteraction action) {
  // Three level popups are the most we can currently have.
  CHECK_GE(popup_level, 0);
  CHECK_LE(popup_level, 2);
  const std::string histogram_metric_name =
      base::StrCat({"Autofill.PopupInteraction.PopupLevel.",
                    base::NumberToString(popup_level)});
  base::UmaHistogramEnumeration(histogram_metric_name, action);
  base::UmaHistogramEnumeration(
      base::StrCat({histogram_metric_name, ".",
                    FillingProductToString(filling_product)}),
      action);

  // Only emit user actions for address suggestions.
  if (filling_product != FillingProduct::kAddress) {
    return;
  }

  if (popup_level == 0) {
    switch (action) {
      case PopupInteraction::kPopupShown:
        base::RecordAction(base::UserMetricsAction(kUserActionRootPopupShown));
        break;
      case PopupInteraction::kSuggestionSelected:
        base::RecordAction(
            base::UserMetricsAction(kUserActionRootPopupSuggestionSelected));
        break;
      case PopupInteraction::kSuggestionAccepted:
        base::RecordAction(
            base::UserMetricsAction(kUserActionRootPopupSuggestionAccepted));
        break;
    }
  } else if (popup_level == 1) {
    switch (action) {
      case PopupInteraction::kPopupShown:
        base::RecordAction(
            base::UserMetricsAction(kUserActionSecondLevelPopupShown));
        break;
      case PopupInteraction::kSuggestionSelected:
        base::RecordAction(base::UserMetricsAction(
            kUserActionSecondLevelPopupSuggestionSelected));
        break;
      case PopupInteraction::kSuggestionAccepted:
        base::RecordAction(base::UserMetricsAction(
            kUserActionSecondLevelPopupSuggestionAccepted));
        break;
    }
  } else {
    switch (action) {
      case PopupInteraction::kPopupShown:
        base::RecordAction(
            base::UserMetricsAction(kUserActionThirdLevelPopupShown));
        break;
      case PopupInteraction::kSuggestionSelected:
        base::RecordAction(base::UserMetricsAction(
            kUserActionThirdLevelPopupSuggestionSelected));
        break;
      case PopupInteraction::kSuggestionAccepted:
        base::RecordAction(base::UserMetricsAction(
            kUserActionThirdLevelPopupSuggestionAccepted));
        break;
    }
  }
}

// static
void AutofillMetrics::LogServerResponseHasDataForForm(bool has_data) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ServerResponseHasDataForForm", has_data);
}

// static
void AutofillMetrics::LogAutofillPerfectFilling(bool is_address,
                                                bool perfect_filling) {
  if (is_address) {
    UMA_HISTOGRAM_BOOLEAN("Autofill.PerfectFilling.Addresses", perfect_filling);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Autofill.PerfectFilling.CreditCards",
                          perfect_filling);
  }
}

AutofillMetrics::CreditCardSeamlessness::CreditCardSeamlessness(
    const FieldTypeSet& filled_types)
    : name_(filled_types.contains(CREDIT_CARD_NAME_FULL) ||
            (filled_types.contains(CREDIT_CARD_NAME_FIRST) &&
             filled_types.contains(CREDIT_CARD_NAME_LAST))),
      number_(filled_types.contains(CREDIT_CARD_NUMBER)),
      exp_(filled_types.contains(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR) ||
           filled_types.contains(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) ||
           (filled_types.contains(CREDIT_CARD_EXP_MONTH) &&
            (filled_types.contains(CREDIT_CARD_EXP_2_DIGIT_YEAR) ||
             filled_types.contains(CREDIT_CARD_EXP_4_DIGIT_YEAR)))),
      cvc_(filled_types.contains(CREDIT_CARD_VERIFICATION_CODE)) {}

AutofillMetrics::CreditCardSeamlessness::Metric
AutofillMetrics::CreditCardSeamlessness::QualitativeMetric() const {
  DCHECK(is_valid());
  if (name_ && number_ && exp_ && cvc_) {
    return Metric::kFullFill;
  } else if (!name_ && number_ && exp_ && cvc_) {
    return Metric::kOptionalNameMissing;
  } else if (name_ && number_ && exp_ && !cvc_) {
    return Metric::kOptionalCvcMissing;
  } else if (!name_ && number_ && exp_ && !cvc_) {
    return Metric::kOptionalNameAndCvcMissing;
  } else if (name_ && number_ && !exp_ && cvc_) {
    return Metric::kFullFillButExpDateMissing;
  } else {
    return Metric::kPartialFill;
  }
}

autofill_metrics::FormEvent
AutofillMetrics::CreditCardSeamlessness::QualitativeFillableFormEvent() const {
  DCHECK(is_valid());
  switch (QualitativeMetric()) {
    case Metric::kFullFill:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_FULL_FILL;
    case Metric::kOptionalNameMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_NAME_MISSING;
    case Metric::kFullFillButExpDateMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_FULL_FILL_BUT_EXPDATE_MISSING;
    case Metric::kOptionalNameAndCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_NAME_AND_CVC_MISSING;
    case Metric::kOptionalCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_CVC_MISSING;
    case Metric::kPartialFill:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_PARTIAL_FILL;
  }
  NOTREACHED();
}

autofill_metrics::FormEvent
AutofillMetrics::CreditCardSeamlessness::QualitativeFillFormEvent() const {
  DCHECK(is_valid());
  switch (QualitativeMetric()) {
    case Metric::kFullFill:
      return autofill_metrics::FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_FULL_FILL;
    case Metric::kOptionalNameMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_NAME_MISSING;
    case Metric::kFullFillButExpDateMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_FULL_FILL_BUT_EXPDATE_MISSING;
    case Metric::kOptionalNameAndCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_NAME_AND_CVC_MISSING;
    case Metric::kOptionalCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_CVC_MISSING;
    case Metric::kPartialFill:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_PARTIAL_FILL;
  }
  NOTREACHED();
}

uint8_t AutofillMetrics::CreditCardSeamlessness::BitmaskMetric() const {
  DCHECK(is_valid());
  uint32_t bitmask = (name_ << 3) | (number_ << 2) | (exp_ << 1) | (cvc_ << 0);
  DCHECK_GE(bitmask, 1u);
  DCHECK_LE(bitmask, BitmaskExclusiveMax());
  return bitmask;
}

// static
void AutofillMetrics::LogCreditCardSeamlessnessAtFillTime(
    const LogCreditCardSeamlessnessParam& p) {
  auto GetSeamlessness = [&p](bool only_newly_filled_fields,
                              bool only_after_security_policy,
                              bool only_visible_fields) {
    FieldTypeSet autofilled_types;
    for (const auto& field : *p.form) {
      FieldGlobalId id = field->global_id();
      if (only_newly_filled_fields && !p.newly_filled_fields->contains(id))
        continue;
      if (only_after_security_policy && !p.safe_fields->contains(id))
        continue;
      if (only_visible_fields && !field->is_visible()) {
        continue;
      }
      autofilled_types.insert(field->Type().GetStorableType());
    }
    return CreditCardSeamlessness(autofilled_types);
  };

  auto RecordUma = [](std::string_view infix, CreditCardSeamlessness s) {
    std::string prefix = base::StrCat({"Autofill.CreditCard.Seamless", infix});
    base::UmaHistogramEnumeration(prefix, s.QualitativeMetric());
    base::UmaHistogramExactLinear(prefix + ".Bitmask", s.BitmaskMetric(),
                                  s.BitmaskExclusiveMax());
  };

  // Note that `GetSeamlessness(false, true, *)` are not called because the
  // Fillable.AtFillingAfterSecurityPolicy variants are not recorded, since the
  // available information are not enough to infer these metrics.
  // This is because we do not know which fields could have been filled after
  // the security policy, because the security policy is only checked against
  // the fields that were actually filled).
  if (auto s = GetSeamlessness(false, false, false)) {
    RecordUma("Fillable.AtFillTimeBeforeSecurityPolicy", s);
    p.builder->SetFillable_BeforeSecurity_Bitmask(s.BitmaskMetric());
    p.builder->SetFillable_BeforeSecurity_Qualitative(
        s.QualitativeMetricAsInt());
    p.event_logger->Log(s.QualitativeFillableFormEvent(), *p.form);
  }
  if (auto s = GetSeamlessness(true, false, false)) {
    RecordUma("Fills.AtFillTimeBeforeSecurityPolicy", s);
    p.builder->SetFilled_BeforeSecurity_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_BeforeSecurity_Qualitative(s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(true, true, false)) {
    RecordUma("Fills.AtFillTimeAfterSecurityPolicy", s);
    p.builder->SetFilled_AfterSecurity_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_AfterSecurity_Qualitative(s.QualitativeMetricAsInt());
    p.event_logger->Log(s.QualitativeFillFormEvent(), *p.form);
  }
  if (auto s = GetSeamlessness(false, false, true)) {
    RecordUma("Fillable.AtFillTimeBeforeSecurityPolicy.Visible", s);
    p.builder->SetFillable_BeforeSecurity_Visible_Bitmask(s.BitmaskMetric());
    p.builder->SetFillable_BeforeSecurity_Visible_Qualitative(
        s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(true, false, true)) {
    RecordUma("Fills.AtFillTimeBeforeSecurityPolicy.Visible", s);
    p.builder->SetFilled_BeforeSecurity_Visible_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_BeforeSecurity_Visible_Qualitative(
        s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(true, true, true)) {
    RecordUma("Fills.AtFillTimeAfterSecurityPolicy.Visible", s);
    p.builder->SetFilled_AfterSecurity_Visible_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_AfterSecurity_Visible_Qualitative(
        s.QualitativeMetricAsInt());
  }

  // In a multi-frame form, a cross-origin field is filled only if
  // shared-autofill is enabled in the field's frame. Here, we log whether
  // shared-autofill did or would improve the fill seamlessness.
  //
  // This is referring to the actual fill, not the hypothetical scenarios
  // assuming that the card on file is complete or that there's no security
  // policy.
  //
  // See FormForest::GetRendererFormsOfBrowserForm() for details when a field
  // requires shared-autofill in order to be autofilled.
  //
  // Shared-autofill is a policy-controlled feature. As such, a parent frame
  // can enable it in a child frame with in the iframe's "allow" attribute:
  // <iframe allow="shared-autofill">. Whether it's enabled in the main frame is
  // controller by an HTTP header; by default, it is.
  auto RequiresSharedAutofill = [&](const AutofillField& field) {
    auto IsSensitiveFieldType = [](FieldType field_type) {
      switch (field_type) {
        case CREDIT_CARD_TYPE:
        case CREDIT_CARD_NAME_FULL:
        case CREDIT_CARD_NAME_FIRST:
        case CREDIT_CARD_NAME_LAST:
          return false;
        default:
          return true;
      }
    };
    const url::Origin& main_origin = p.form->main_frame_origin();
    const url::Origin& triggered_origin = p.field->origin();
    return field.origin() != triggered_origin &&
           (field.origin() != main_origin ||
            IsSensitiveFieldType(field.Type().GetStorableType())) &&
           triggered_origin == main_origin;
  };

  bool some_field_needs_shared_autofill = false;
  bool some_field_has_shared_autofill = false;
  for (const auto& field : *p.form) {
    if (RequiresSharedAutofill(*field) &&
        p.newly_filled_fields->contains(field->global_id())) {
      if (!p.safe_fields->contains(field->global_id())) {
        some_field_needs_shared_autofill = true;
      } else {
        some_field_has_shared_autofill = true;
      }
    }
  }

  enum SharedAutofillMetric : uint64_t {
    kSharedAutofillIsIrrelevant = 0,
    kSharedAutofillWouldHelp = 1,
    kSharedAutofillDidHelp = 2,
  };
  if (some_field_needs_shared_autofill) {
    p.builder->SetSharedAutofill(kSharedAutofillWouldHelp);
    p.event_logger->Log(
        autofill_metrics::FORM_EVENT_CREDIT_CARD_MISSING_SHARED_AUTOFILL,
        *p.form);
  } else if (some_field_has_shared_autofill) {
    p.builder->SetSharedAutofill(kSharedAutofillDidHelp);
  } else {
    p.builder->SetSharedAutofill(kSharedAutofillIsIrrelevant);
  }
}

// static
void AutofillMetrics::LogCreditCardSeamlessnessAtSubmissionTime(
    const FieldTypeSet& autofilled_types) {
  CreditCardSeamlessness seamlessness(autofilled_types);
  if (seamlessness.is_valid()) {
    base::UmaHistogramExactLinear(
        "Autofill.CreditCard.SeamlessFills.AtSubmissionTime.Bitmask",
        seamlessness.BitmaskMetric(), seamlessness.BitmaskExclusiveMax());
    base::UmaHistogramEnumeration(
        "Autofill.CreditCard.SeamlessFills.AtSubmissionTime",
        seamlessness.QualitativeMetric());
  }
}

// static
void AutofillMetrics::LogParseFormTiming(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("Autofill.Timing.ParseForm", duration);
}

// static
void AutofillMetrics::LogParsedFormUntilInteractionTiming(
    base::TimeDelta duration) {
  base::UmaHistogramLongTimes("Autofill.Timing.ParseFormUntilInteraction2",
                              duration);
}

// static
void AutofillMetrics::LogIsQueriedCreditCardFormSecure(bool is_secure) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.QueriedCreditCardFormIsSecure", is_secure);
}

// static
void AutofillMetrics::LogShowedHttpNotSecureExplanation() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedHttpNotSecureExplanation"));
}

// static
void AutofillMetrics::LogAutocompleteDaysSinceLastUse(size_t days) {
  UMA_HISTOGRAM_COUNTS_1000("Autocomplete.DaysSinceLastUse", days);
}

// static
void AutofillMetrics::OnAutocompleteSuggestionsShown() {
  AutofillMetrics::LogAutocompleteEvent(
      AutocompleteEvent::AUTOCOMPLETE_SUGGESTIONS_SHOWN);
}

// static
void AutofillMetrics::OnAutocompleteSuggestionDeleted(
    SingleEntryRemovalMethod removal_method) {
  AutofillMetrics::LogAutocompleteEvent(
      AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED);
  base::UmaHistogramEnumeration(
      "Autofill.Autocomplete.SingleEntryRemovalMethod", removal_method);
}

// static
void AutofillMetrics::LogAutocompleteEvent(AutocompleteEvent event) {
  DCHECK_LT(event, AutocompleteEvent::NUM_AUTOCOMPLETE_EVENTS);
  base::UmaHistogramEnumeration("Autocomplete.Events2", event,
                                NUM_AUTOCOMPLETE_EVENTS);
}

// static
void AutofillMetrics::LogAutofillPopupVisibleDuration(
    FillingProduct filling_product,
    base::TimeDelta duration) {
  base::UmaHistogramTimes("Autofill.Popup.VisibleDuration", duration);
  base::UmaHistogramTimes(
      base::StrCat({"Autofill.Popup.VisibleDuration.",
                    FillingProductToString(filling_product)}),
      duration);
}

// static
const char* AutofillMetrics::SubmissionSourceToUploadEventMetric(
    SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return "Autofill.UploadEvent.None";
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return "Autofill.UploadEvent.SameDocumentNavigation";
    case SubmissionSource::XHR_SUCCEEDED:
      return "Autofill.UploadEvent.XhrSucceeded";
    case SubmissionSource::FRAME_DETACHED:
      return "Autofill.UploadEvent.FrameDetached";
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return "Autofill.UploadEvent.ProbablyFormSubmitted";
    case SubmissionSource::FORM_SUBMISSION:
      return "Autofill.UploadEvent.FormSubmission";
    case SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      return "Autofill.UploadEvent.DomMutationAfterAutofill";
  }
  // Unit tests exercise this path, so do not put NOTREACHED() here.
  return "Autofill.UploadEvent.Unknown";
}

// static
void AutofillMetrics::LogUploadEvent(SubmissionSource submission_source,
                                     bool was_sent) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.UploadEvent", was_sent);
  base::UmaHistogramEnumeration(
      SubmissionSourceToUploadEventMetric(submission_source),
      was_sent ? UploadEventStatus::kSent : UploadEventStatus::kNotSent);
}

// static
void AutofillMetrics::LogDeveloperEngagementUkm(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    const GURL& url,
    bool is_for_credit_card,
    DenseSet<FormTypeNameForLogging> form_types,
    int developer_engagement_metrics,
    FormSignature form_signature) {
  DCHECK(developer_engagement_metrics);
  DCHECK_LT(developer_engagement_metrics,
            1 << NUM_DEVELOPER_ENGAGEMENT_METRICS);
  if (!url.is_valid())
    return;

  ukm::builders::Autofill_DeveloperEngagement(source_id)
      .SetDeveloperEngagement(developer_engagement_metrics)
      .SetIsForCreditCard(is_for_credit_card)
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetFormSignature(HashFormSignature(form_signature))
      .Record(ukm_recorder);
}

// static
void AutofillMetrics::LogAutofillFieldInfoAfterSubmission(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    const FormStructure& form,
    base::TimeTicks form_submitted_timestamp) {
  for (const auto& field : form) {
    // The possible field submitted types determined by comparing the submitted
    // value in the field with the data stored in the Autofill server. We will
    // have at most three possible field submitted types.
    FieldType submitted_type1 = UNKNOWN_TYPE;

    ukm::builders::Autofill2_FieldInfoAfterSubmission builder(source_id);
    builder
        .SetFormSessionIdentifier(
            AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id()))
        .SetFieldSessionIdentifier(
            AutofillMetrics::FieldGlobalIdToHash64Bit(field->global_id()));

    const FieldTypeSet& type_set = field->possible_types();
    if (!type_set.empty()) {
      auto type = type_set.begin();
      submitted_type1 = *type;
      if (type_set.size() >= 2) {
        builder.SetSubmittedType2(*++type);
      }
      if (type_set.size() >= 3) {
        builder.SetSubmittedType3(*++type);
      }
    }

    builder.SetSubmittedType1(submitted_type1)
        .SetSubmissionSource(static_cast<int>(form.submission_source()))
        .SetMillisecondsFromFormParsedUntilSubmission(
            autofill_metrics::GetSemanticBucketMinForAutofillDurationTiming(
                (form_submitted_timestamp - form.form_parsed_timestamp())
                    .InMilliseconds()))
        .Record(ukm_recorder);
  }
}

int64_t AutofillMetrics::FormTypesToBitVector(
    const DenseSet<FormTypeNameForLogging>& form_types) {
  int64_t form_type_bv = 0;
  for (FormTypeNameForLogging form_type : form_types) {
    DCHECK_LT(static_cast<int64_t>(form_type), 63);
    form_type_bv |= 1LL << static_cast<int64_t>(form_type);
  }
  return form_type_bv;
}

void AutofillMetrics::LogServerCardLinkClicked(PaymentsSigninState sync_state) {
  base::UmaHistogramEnumeration("Autofill.ServerCardLinkClicked", sync_state);
}

// static
const char* AutofillMetrics::GetMetricsSyncStateSuffix(
    PaymentsSigninState sync_state) {
  switch (sync_state) {
    case PaymentsSigninState::kSignedOut:
      return ".SignedOut";
    case PaymentsSigninState::kSignedIn:
      return ".SignedIn";
    case PaymentsSigninState::kSignedInAndWalletSyncTransportEnabled:
      return ".SignedInAndWalletSyncTransportEnabled";
    case PaymentsSigninState::kSignedInAndSyncFeatureEnabled:
      return ".SignedInAndSyncFeatureEnabled";
    case PaymentsSigninState::kSyncPaused:
      return ".SyncPaused";
    case PaymentsSigninState::kUnknown:
      return ".Unknown";
  }
}

// static
void AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
    ukm::UkmRecorder* recorder,
    ukm::SourceId source_id,
    uint32_t phone_collection_metric_state) {
  // UKM recording is not supported for WebViews.
  if (!recorder || source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::WebOTPImpact builder(source_id);
  builder.SetPhoneCollection(phone_collection_metric_state);
  builder.Record(recorder);
}

void AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(
    const AutofillProfile& profile) {
  constexpr std::string_view base_histogram_name =
      "Autofill.NameTokenVerificationStatusAtProfileUsage.";

  for (const auto& [type, name] : kStructuredNameTypeToNameMap) {
    // Do not record the status for empty values.
    if (profile.GetRawInfo(type).empty()) {
      continue;
    }

    VerificationStatus status = profile.GetVerificationStatus(type);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, name}),
                                  status);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, "Any"}),
                                  status);
  }
}

void AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(
    const AutofillProfile& profile) {
  constexpr std::string_view base_histogram_name =
      "Autofill.AddressTokenVerificationStatusAtProfileUsage.";

  for (const auto& [type, name] : kStructuredAddressTypeToNameMap) {
    // Do not record the status for empty values.
    if (profile.GetRawInfo(type).empty()) {
      continue;
    }

    VerificationStatus status = profile.GetVerificationStatus(type);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, name}),
                                  status);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, "Any"}),
                                  status);
  }
}

// static
void AutofillMetrics::LogVirtualCardMetadataSynced(bool existing_card) {
  base::UmaHistogramBoolean("Autofill.VirtualCard.MetadataSynced",
                            existing_card);
}

// static
void AutofillMetrics::LogImageFetchResult(bool succeeded) {
  base::UmaHistogramBoolean("Autofill.ImageFetcher.Result", succeeded);
}

// static
void AutofillMetrics::LogImageFetcherRequestLatency(base::TimeDelta duration) {
  base::UmaHistogramLongTimes("Autofill.ImageFetcher.RequestLatency", duration);
}

// static
void AutofillMetrics::LogAutocompletePredictionCollisionState(
    PredictionState prediction_state,
    AutocompleteState autocomplete_state) {
  if (prediction_state == PredictionState::kNone &&
      autocomplete_state == AutocompleteState::kNone) {
    return;
  }
  // The buckets are calculated by using the least significant two bits to
  // encode the `autocomplete_state`, and the next two bits to encode the
  // `prediction_state`.
  int bucket = (static_cast<int>(prediction_state) << 2) |
               static_cast<int>(autocomplete_state);
  // Without (kNone, kNone), 4*4 - 1 = 15 possible pairs remain. Log the bucket
  // 0-based, in order to interpret the metric as an enum.
  DCHECK(1 <= bucket && bucket <= 15);
  UMA_HISTOGRAM_ENUMERATION("Autofill.Autocomplete.PredictionCollisionState",
                            bucket - 1, 15);
}

// static
void AutofillMetrics::LogAutocompletePredictionCollisionTypes(
    AutocompleteState autocomplete_state,
    FieldType server_type,
    FieldType heuristic_type) {
  // Convert `autocomplete_state` to a string for the metric's name.
  std::string autocomplete_suffix;
  switch (autocomplete_state) {
    case AutocompleteState::kNone:
      autocomplete_suffix = "None";
      break;
    case AutocompleteState::kValid:
      autocomplete_suffix = "Valid";
      break;
    case AutocompleteState::kGarbage:
      autocomplete_suffix = "Garbage";
      break;
    case AutocompleteState::kOff:
      autocomplete_suffix = "Off";
      break;
    case AutocompleteState::kPassword:
      autocomplete_suffix = "Password";
      break;
    default:
      NOTREACHED();
  }

  // Log the metric for heuristic and server type.
  std::string kHistogramName =
      "Autofill.Autocomplete.PredictionCollisionType2.";
  if (server_type != NO_SERVER_DATA) {
    base::UmaHistogramEnumeration(
        kHistogramName + "Server." + autocomplete_suffix, server_type,
        FieldType::MAX_VALID_FIELD_TYPE);
  }
  base::UmaHistogramEnumeration(
      kHistogramName + "Heuristics." + autocomplete_suffix, heuristic_type,
      FieldType::MAX_VALID_FIELD_TYPE);
  base::UmaHistogramEnumeration(
      kHistogramName + "ServerOrHeuristics." + autocomplete_suffix,
      server_type != NO_SERVER_DATA ? server_type : heuristic_type,
      FieldType::MAX_VALID_FIELD_TYPE);
}

const std::string PaymentsRpcResultToMetricsSuffix(PaymentsRpcResult result) {
  std::string result_suffix;

  switch (result) {
    case PaymentsRpcResult::kSuccess:
      result_suffix = ".Success";
      break;
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
      result_suffix = ".Failure";
      break;
    case PaymentsRpcResult::kNetworkError:
      result_suffix = ".NetworkError";
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      result_suffix = ".VcnRetrievalFailure";
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      result_suffix = ".ClientSideTimeout";
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED();
  }

  return result_suffix;
}

// static
std::string AutofillMetrics::GetHistogramStringForCardType(
    absl::variant<PaymentsRpcCardType, CreditCard::RecordType> card_type) {
  if (absl::holds_alternative<PaymentsRpcCardType>(card_type)) {
    switch (absl::get<PaymentsRpcCardType>(card_type)) {
      case PaymentsRpcCardType::kServerCard:
        return ".ServerCard";
      case PaymentsRpcCardType::kVirtualCard:
        return ".VirtualCard";
      case PaymentsRpcCardType::kUnknown:
        DUMP_WILL_BE_NOTREACHED();
        break;
    }
  } else if (absl::holds_alternative<CreditCard::RecordType>(card_type)) {
    switch (absl::get<CreditCard::RecordType>(card_type)) {
      case CreditCard::RecordType::kFullServerCard:
      case CreditCard::RecordType::kMaskedServerCard:
        return ".ServerCard";
      case CreditCard::RecordType::kVirtualCard:
        return ".VirtualCard";
      case CreditCard::RecordType::kLocalCard:
        return ".LocalCard";
    }
  }

  return "";
}

// static
void AutofillMetrics::LogDeleteAddressProfileFromPopup() {
  // Only the "confirmed" bucket can be recorded, as the user cannot cancel this
  // type of deletion.
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.Popup",
                            /*delete_confirmed=*/true);
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.Any",
                            /*delete_confirmed=*/true);
}

// static
void AutofillMetrics::LogDeleteAddressProfileFromKeyboardAccessory() {
  // Only the "confirmed" bucket is recorded here, as the cancellation can only
  // be recorded from Java.
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.KeyboardAccessory",
                            /*delete_confirmed=*/true);
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.Any",
                            /*delete_confirmed=*/true);
}

// static
uint64_t AutofillMetrics::FormGlobalIdToHash64Bit(
    const FormGlobalId& form_global_id) {
  return StrToHash64Bit(
      base::NumberToString(form_global_id.renderer_id.value()) +
      form_global_id.frame_token.ToString());
}

// static
uint64_t AutofillMetrics::FieldGlobalIdToHash64Bit(
    const FieldGlobalId& field_global_id) {
  return StrToHash64Bit(
      base::NumberToString(field_global_id.renderer_id.value()) +
      field_global_id.frame_token.ToString());
}

}  // namespace autofill
