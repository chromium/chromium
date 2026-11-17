// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// A delegate for checking if a given URL is a phishing site before unmasking
// an OTP.
class OtpPhishGuardDelegate {
 public:
  using VerdictCallback =
      base::OnceCallback<void(OneTimeTokensPhishGuardVerdict)>;

  virtual ~OtpPhishGuardDelegate() = default;

  // Checks if the target frame to fill is safe from phishing or other
  // threats. `callback` is never run with `kUnknown` or `kTimedOut`, which
  // callers use when there is no delegate or the check times out.
  virtual void StartOtpPhishGuardCheck(LocalFrameToken frame_to_fill,
                                       VerdictCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_DELEGATE_H_
