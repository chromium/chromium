// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_metrics.h"

#include <string_view>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace one_time_tokens {

namespace {

// LINT.IfChange(GmailOtpFlowType)
std::string_view GetFlowHistogramInfix(GmailOtpFlow flow) {
  switch (flow) {
    case GmailOtpFlow::kAutofill:
      return "GmailAutofill";
    case GmailOtpFlow::kActor:
      return "GmailActor";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/histograms.xml:GmailOtpFlowType)

std::string_view GetTokenSourceHistogramSuffix(bool is_cached) {
  return is_cached ? "Cached" : "Received";
}

}  // namespace

void RecordSenderDomainMatchAcceptedMatchType(
    GmailOtpFlow flow,
    GmailOtpSenderDomainMatchType match_type,
    bool is_cached,
    ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OneTimeTokens.", GetFlowHistogramInfix(flow),
                    ".SenderDomainMatchAcceptedMatchType.",
                    GetTokenSourceHistogramSuffix(is_cached)}),
      match_type);
  if (ukm_source_id == ukm::kInvalidSourceId) {
    return;
  }
  ukm::builders::OneTimeTokens builder(ukm_source_id);
  if (is_cached) {
    builder.SetGmail_SenderDomainMatchAcceptedMatchType_Cached(
        std::to_underlying(match_type));
  } else {
    builder.SetGmail_SenderDomainMatchAcceptedMatchType_Received(
        std::to_underlying(match_type));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void RecordSenderDomainMatchRejectionReason(
    GmailOtpFlow flow,
    GmailOtpSenderDomainMatchType match_type,
    bool is_cached,
    ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OneTimeTokens.", GetFlowHistogramInfix(flow),
                    ".SenderDomainMatchRejectionReason.",
                    GetTokenSourceHistogramSuffix(is_cached)}),
      match_type);
  if (ukm_source_id == ukm::kInvalidSourceId) {
    return;
  }
  ukm::builders::OneTimeTokens builder(ukm_source_id);
  if (is_cached) {
    builder.SetGmail_SenderDomainMatchRejectionReason_Cached(
        std::to_underlying(match_type));
  } else {
    builder.SetGmail_SenderDomainMatchRejectionReason_Received(
        std::to_underlying(match_type));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace one_time_tokens
