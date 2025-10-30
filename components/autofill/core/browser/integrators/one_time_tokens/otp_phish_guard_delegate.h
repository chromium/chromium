// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_DELEGATE_H_

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace autofill {

// A delegate for checking if a given URL is a phishing site before unmasking
// an OTP.
class OtpPhishGuardDelegate {
 public:
  virtual ~OtpPhishGuardDelegate() = default;

  // Checks if the given `url` is a phishing site. `callback` is run with the
  // result.
  virtual void StartOtpPhishGuardCheck(
      const GURL& url,
      base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_DELEGATE_H_
