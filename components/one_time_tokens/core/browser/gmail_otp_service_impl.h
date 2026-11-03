// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_IMPL_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_IMPL_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/one_time_tokens/core/browser/gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace one_time_tokens {

class GmailOtpBackend;

class GmailOtpServiceImpl : public GmailOtpService {
 public:
  // `backend` and `affiliation_service` must be non-null and must outlive
  // `this` (both are `KeyedService`s declared as dependencies in
  // `GmailOtpServiceFactory`). `affiliation_service` is accepted here so
  // `GmailOtpServiceFactory` and callers are wired up for sender domain
  // matching in a follow-up change.
  GmailOtpServiceImpl(
      GmailOtpBackend* backend,
      affiliations::AffiliationService* affiliation_service,
      std::optional<OneTimeTokenLogSink::LogHandler> log_handler);

  GmailOtpServiceImpl(const GmailOtpServiceImpl&) = delete;
  GmailOtpServiceImpl& operator=(const GmailOtpServiceImpl&) = delete;
  ~GmailOtpServiceImpl() override;

  // GmailOtpService:
  GmailOtpBackend* GetBackend() override;
  OneTimeTokenLogSink* GetLogSink() override;

 private:
  raw_ptr<GmailOtpBackend> backend_;
  base::CallbackListSubscription log_subscription_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_IMPL_H_
