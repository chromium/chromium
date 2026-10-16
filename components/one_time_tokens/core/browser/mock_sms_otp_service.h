// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_MOCK_SMS_OTP_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_MOCK_SMS_OTP_SERVICE_H_

#include <vector>

#include "components/one_time_tokens/core/browser/sms_otp_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace one_time_tokens {

class MockSmsOtpService : public SmsOtpService {
 public:
  MockSmsOtpService();
  ~MockSmsOtpService() override;
  MOCK_METHOD(OneTimeTokenLogSink*, log_sink, (), (override));
  MOCK_METHOD(void, GetRecentOneTimeTokens, (Callback callback), (override));
  MOCK_METHOD(std::vector<OneTimeToken>,
              GetCachedOneTimeTokens,
              (),
              (const override));
  MOCK_METHOD(ExpiringSubscription,
              Subscribe,
              (base::Time expiration,
               Callback callback,
               base::OnceClosure expiration_callback),
              (override));
  MOCK_METHOD(void,
              RequestOneTimeToken,
              (base::TimeDelta timeout,
               base::OnceCallback<void(std::optional<OneTimeToken>)> callback),
              (override));
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_MOCK_SMS_OTP_SERVICE_H_
