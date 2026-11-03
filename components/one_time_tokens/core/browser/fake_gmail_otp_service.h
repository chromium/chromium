// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_SERVICE_H_

#include <utility>

#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace one_time_tokens {

class FakeGmailOtpService : public GmailOtpService {
 public:
  FakeGmailOtpService();
  FakeGmailOtpService(const FakeGmailOtpService&) = delete;
  FakeGmailOtpService& operator=(const FakeGmailOtpService&) = delete;
  ~FakeGmailOtpService() override;

  // GmailOtpService:
  GmailOtpBackend* GetBackend() override;
  OneTimeTokenLogSink* GetLogSink() override;

  // Simulates reception of a token or error from backend.
  void ProcessCallbacks(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply);
  void ProcessCallbacks(OneTimeToken token);

  void SetHasPendingRequests(bool has_pending_requests);
  void ClearCache();

  size_t num_callbacks() const;

 private:
  FakeGmailOtpBackend backend_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_FAKE_GMAIL_OTP_SERVICE_H_
