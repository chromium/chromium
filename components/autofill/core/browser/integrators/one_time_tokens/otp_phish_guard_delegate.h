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

  // Checks if the given URLs are safe from phishing or other threats.
  // `callback` is run with `is_phishing` (true if phishing/unsafe, false if
  // safe).
  virtual void StartOtpPhishGuardCheck(
      const GURL& main_frame_url,
      const GURL& frame_to_fill_url,
      base::OnceCallback<void(bool is_phishing)> callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_PHISH_GUARD_DELEGATE_H_
