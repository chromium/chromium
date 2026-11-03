// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace one_time_tokens {

class GmailOtpBackend;
class OneTimeTokenLogSink;

// Service to manage Gmail one-time passwords (OTP). One instance per profile.
class GmailOtpService : public KeyedService {
 public:
  ~GmailOtpService() override = default;

  // Returns the underlying `GmailOtpBackend`.
  virtual GmailOtpBackend* GetBackend() = 0;

  // Returns the log sink.
  virtual OneTimeTokenLogSink* GetLogSink() = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_SERVICE_H_
