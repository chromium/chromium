// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OTP_UNMASK_RESULT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OTP_UNMASK_RESULT_H_

namespace autofill {

// Used by Payments Autofill components to represent the response result when
// unmasking a card using OTP.
enum class OtpUnmaskResult {
  // Default value, should never be used.
  kUnknownType = 0,
  // OTP successfully verified.
  kSuccess = 1,
  // OTP code expired.
  kOtpExpired = 2,
  // Incorrect OTP entered, user must enter the correct OTP.
  kOtpMismatch = 3,
  // Unretriable failure.
  kPermanentFailure = 4,
  // Max value, must be updated every time a new enum is added.
  kMaxValue = kPermanentFailure,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OTP_UNMASK_RESULT_H_
