// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/fake_gmail_otp_service.h"

#include <utility>

namespace one_time_tokens {

FakeGmailOtpService::FakeGmailOtpService() = default;

FakeGmailOtpService::~FakeGmailOtpService() = default;

GmailOtpBackend* FakeGmailOtpService::GetBackend() {
  return &backend_;
}

OneTimeTokenLogSink* FakeGmailOtpService::GetLogSink() {
  return backend_.GetLogSink();
}

void FakeGmailOtpService::ProcessCallbacks(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> reply) {
  backend_.ProcessCallbacks(std::move(reply));
}

void FakeGmailOtpService::ProcessCallbacks(OneTimeToken token) {
  ProcessCallbacks(base::ok(std::move(token)));
}

void FakeGmailOtpService::SetHasPendingRequests(bool has_pending_requests) {
  backend_.SetHasPendingRequests(has_pending_requests);
}

void FakeGmailOtpService::ClearCache() {
  backend_.ClearCache();
}

size_t FakeGmailOtpService::num_callbacks() const {
  return backend_.num_callbacks();
}

}  // namespace one_time_tokens
