// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_LEGACY_IMPL_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_LEGACY_IMPL_TEST_API_H_

#include <utility>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_legacy_impl.h"

namespace autofill {

// Test API for `OtpManagerLegacyImpl`.
class OtpManagerLegacyImplTestApi {
 public:
  static constexpr base::TimeDelta kSmsOtpSubscriptionDuration =
      OtpManagerLegacyImpl::kSmsOtpSubscriptionDuration;

  explicit OtpManagerLegacyImplTestApi(OtpManagerLegacyImpl& manager)
      : manager_(manager) {}

  bool has_log_subscription() const { return !!manager_->log_subscription_; }

  void OnOneTimeTokenReceived(
      one_time_tokens::OneTimeTokenSource source,
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>
          token_or_error) {
    manager_->OnOneTimeTokenReceived(source, std::move(token_or_error));
  }

 private:
  raw_ref<OtpManagerLegacyImpl> manager_;
};

inline OtpManagerLegacyImplTestApi test_api(OtpManagerLegacyImpl& manager) {
  return OtpManagerLegacyImplTestApi(manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_LEGACY_IMPL_TEST_API_H_
