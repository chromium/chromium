// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

namespace {

std::string_view GetFlowTypeSuffix(
    payments::RecordLegalReminderAcknowledgmentRequestDetails::FlowType
        flow_type) {
  switch (flow_type) {
    case payments::RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
        kChromeDownstream:
      return "ChromeDownstream";
    case payments::RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
        kWalletPass:
      return "WalletPass";
    case payments::RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
        kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace

void LogWalletReminderNoticeInteraction(
    WalletReminderNoticeInteraction interaction) {
  base::UmaHistogramEnumeration("Autofill.WalletReminderNotice.Interaction",
                                interaction);
}

void LogWalletReminderNoticeShowResult(
    WalletReminderNoticeShowResult show_result,
    payments::RecordLegalReminderAcknowledgmentRequestDetails::FlowType
        flow_type) {
  base::UmaHistogramEnumeration("Autofill.WalletReminderNotice.ShowResult",
                                show_result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.WalletReminderNotice.ShowResult.",
                    GetFlowTypeSuffix(flow_type)}),
      show_result);
}

}  // namespace autofill::autofill_metrics
