// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

namespace autofill {

// Test API for `OtpManagerImpl`.
class OtpManagerImplTestApi {
 public:
  explicit OtpManagerImplTestApi(OtpManagerImpl& manager) : manager_(manager) {}

  void SetReceivedOtps(std::vector<one_time_tokens::OneTimeToken> otps) {
    manager_->received_otps_ = std::move(otps);
  }

 private:
  raw_ref<OtpManagerImpl> manager_;
};

inline OtpManagerImplTestApi test_api(OtpManagerImpl& manager) {
  return OtpManagerImplTestApi(manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_TEST_API_H_
