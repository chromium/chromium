// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SENDER_DOMAIN_METRICS_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SENDER_DOMAIN_METRICS_H_

#include "components/one_time_tokens/core/browser/gmail_otp_flow.h"
#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_matcher.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace one_time_tokens {

// Records the match type of a token whose sender domain was accepted for an
// OTP form in `flow`. `is_cached` is true for tokens from the backend cache and
// false for tokens received through a subscription. Also records to UKM against
// `ukm_source_id`, unless it is `ukm::kInvalidSourceId`.
void RecordSenderDomainMatchAcceptedMatchType(
    GmailOtpFlow flow,
    GmailOtpSenderDomainMatchType match_type,
    bool is_cached,
    ukm::SourceId ukm_source_id);

// Like `RecordSenderDomainMatchAcceptedMatchType()`, but for a token whose
// sender domain was rejected.
void RecordSenderDomainMatchRejectionReason(
    GmailOtpFlow flow,
    GmailOtpSenderDomainMatchType match_type,
    bool is_cached,
    ukm::SourceId ukm_source_id);

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SENDER_DOMAIN_METRICS_H_
