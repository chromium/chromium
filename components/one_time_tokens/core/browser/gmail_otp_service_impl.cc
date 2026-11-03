// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_service_impl.h"

#include <utility>

#include "base/check.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"

namespace one_time_tokens {

GmailOtpServiceImpl::GmailOtpServiceImpl(
    GmailOtpBackend* backend,
    affiliations::AffiliationService* affiliation_service,
    std::optional<OneTimeTokenLogSink::LogHandler> log_handler)
    : backend_(backend) {
  CHECK(backend);
  CHECK(affiliation_service);
  if (log_handler && backend_->GetLogSink()) {
    log_subscription_ =
        backend_->GetLogSink()->AddLogHandler(std::move(*log_handler));
  }
}

GmailOtpServiceImpl::~GmailOtpServiceImpl() = default;

GmailOtpBackend* GmailOtpServiceImpl::GetBackend() {
  return backend_;
}

OneTimeTokenLogSink* GmailOtpServiceImpl::GetLogSink() {
  return backend_->GetLogSink();
}

}  // namespace one_time_tokens
