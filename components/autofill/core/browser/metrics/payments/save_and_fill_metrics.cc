// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

namespace {
using PaymentsRpcResult =
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult;

std::string_view GetSaveAndFillServerRequestTypeString(
    SaveAndFillServerRequestType type) {
  switch (type) {
    case SaveAndFillServerRequestType::kGetDetailsForCreateCard:
      return "GetDetailsForCreateCard";
    case SaveAndFillServerRequestType::kCreateCard:
      return "CreateCard";
  }
}

std::string_view GetFlowScenarioString(SaveAndFillFlowScenario scenario) {
  switch (scenario) {
    case SaveAndFillFlowScenario::kLocalSaveUploadSaveInfeasible:
      return "LocalSaveUploadSaveInfeasible";
    case SaveAndFillFlowScenario::kLocalSavePreflightCallFailed:
      return "LocalSavePreflightCallFailed";
    case SaveAndFillFlowScenario::kLocalSaveBinRangeNotSupported:
      return "LocalSaveBinRangeNotSupported";
    case SaveAndFillFlowScenario::kLocalSaveUploadSaveFailed:
      return "LocalSaveUploadSaveFailed";
    case SaveAndFillFlowScenario::kUploadSave:
      return "UploadSave";
    case SaveAndFillFlowScenario::kUnknown:
      NOTREACHED();
  }
}
}  // namespace

void LogSaveAndFillFormEvent(SaveAndFillFormEvent event) {
  base::UmaHistogramEnumeration("Autofill.FormEvents.CreditCard.SaveAndFill",
                                event);
}

void LogSaveAndFillSuggestionNotShownReason(
    SaveAndFillSuggestionNotShownReason reason) {
  base::UmaHistogramEnumeration("Autofill.SaveAndFill.SuggestionNotShownReason",
                                reason);
}

void LogSaveAndFillGetDetailsForCreateCardResultAndLatency(
    bool succeeded,
    base::TimeDelta latency) {
  static constexpr std::string_view kHistogramName =
      "Autofill.SaveAndFill.GetDetailsForCreateCard.Latency";
  base::UmaHistogramMediumTimes(kHistogramName, latency);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramName, succeeded ? ".Success" : ".Failure"}),
      latency);
}

void LogSaveAndFillCreateCardResultAndLatency(bool succeeded,
                                              base::TimeDelta latency) {
  static constexpr std::string_view kHistogramName =
      "Autofill.SaveAndFill.CreateCard.Latency";
  base::UmaHistogramMediumTimes(kHistogramName, latency);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramName, succeeded ? ".Success" : ".Failure"}),
      latency);
}

void LogSaveAndFillStrikeDatabaseBlockReason(
    AutofillMetrics::AutofillStrikeDatabaseBlockReason reason) {
  base::UmaHistogramEnumeration(
      "Autofill.StrikeDatabase.SaveAndFillStrikeDatabaseBlockReason", reason);
}

void LogSaveAndFillNumOfStrikesPresentWhenDialogAccepted(int strike_count) {
  base::UmaHistogramCounts100(
      "Autofill.StrikeDatabase.NumOfStrikesPresentWhenSaveAndFillAccepted",
      strike_count);
}

void LogSaveAndFillDialogResult(SaveAndFillDialogResult result) {
  base::UmaHistogramEnumeration("Autofill.SaveAndFill.DialogResult2", result);
}

void LogSaveAndFillDialogShown(bool is_upload) {
  base::UmaHistogramEnumeration(
      "Autofill.SaveAndFill.DialogShown2",
      is_upload ? SaveAndFillDialogShown::kUploadDialogShown
                : SaveAndFillDialogShown::kLocalDialogShown);
}

void LogSaveAndFillFunnelMetrics(bool succeeded,
                                 bool is_for_upload,
                                 SaveAndFillFormEvent event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.SaveAndFill.Funnel",
                    is_for_upload ? ".Upload" : ".Local",
                    succeeded ? ".Success" : ".Failure"}),
      event);
}

void LogSaveAndFillFunnelSucceeded(SaveAndFillFlowScenario scenario,
                                   SaveAndFillFunnelSucceededStage stage) {
  if (scenario == SaveAndFillFlowScenario::kUnknown) {
    return;
  }
  // Aggregate parent histogram.
  base::UmaHistogramEnumeration("Autofill.SaveAndFill.Funnel.Succeeded", stage);
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.SaveAndFill.Funnel.Succeeded.",
                    GetFlowScenarioString(scenario)}),
      stage);
}

void LogSaveAndFillFunnelCanceled(SaveAndFillFlowScenario scenario,
                                  SaveAndFillFunnelCanceledStage stage) {
  if (scenario == SaveAndFillFlowScenario::kUnknown) {
    return;
  }
  if (stage == SaveAndFillFunnelCanceledStage::kSuggestionIgnored) {
    CHECK(scenario == SaveAndFillFlowScenario::kLocalSaveUploadSaveInfeasible ||
          scenario == SaveAndFillFlowScenario::kUploadSave);
  } else if (stage == SaveAndFillFunnelCanceledStage::kDialogCanceled) {
    CHECK(scenario != SaveAndFillFlowScenario::kLocalSaveUploadSaveFailed &&
          scenario != SaveAndFillFlowScenario::kLocalSaveBinRangeNotSupported);
  }

  // Aggregate parent histogram.
  base::UmaHistogramEnumeration("Autofill.SaveAndFill.Funnel.Canceled", stage);
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.SaveAndFill.Funnel.Canceled.",
                    GetFlowScenarioString(scenario)}),
      stage);
}

void LogSaveAndFillPaymentsRequestResult(
    SaveAndFillServerRequestType request_type,
    PaymentsRpcResult result) {
  SaveAndFillPaymentsRequestResult metric_result;
  switch (result) {
    case PaymentsRpcResult::kSuccess:
      metric_result = SaveAndFillPaymentsRequestResult::kSuccess;
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      metric_result = SaveAndFillPaymentsRequestResult::kTimeout;
      break;
    default:
      metric_result = SaveAndFillPaymentsRequestResult::kFailure;
      break;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.SaveAndFill.",
                    GetSaveAndFillServerRequestTypeString(request_type),
                    ".Result"}),
      metric_result);
}

}  // namespace autofill::autofill_metrics
