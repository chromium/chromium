// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OTP_UNMASK_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OTP_UNMASK_DELEGATE_H_

#include <string>

namespace autofill {

class OtpUnmaskDelegate {
 public:
  // Called when the user has attempted a verification. Prompt is still
  // open at this point.
  virtual void OnUnmaskPromptAccepted(const std::u16string& otp) = 0;

  // Called when the unmask prompt is closed (e.g., cancelled).
  // |user_closed_dialog| indicates whether the closure was triggered by
  // user cancellation.
  virtual void OnUnmaskPromptClosed(bool user_closed_dialog) = 0;

  // Called when the user requested a new OTP.
  virtual void OnNewOtpRequested() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OTP_UNMASK_DELEGATE_H_
