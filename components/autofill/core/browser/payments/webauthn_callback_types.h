// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WEBAUTHN_CALLBACK_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WEBAUTHN_CALLBACK_TYPES_H_

namespace autofill {

// The callback that should be invoked on user decision.
enum class WebauthnDialogCallbackType {
  // Invoked when the OK button in the Webauthn offer dialog is clicked.
  kOfferAccepted = 0,
  // Invoked when the cancel button in the Webauthn offer dialog is clicked.
  kOfferCancelled = 1,
  // Invoked when the cancel button in the Webauthn verify pending dialog
  // is clicked.
  kVerificationCancelled = 2,
  kMaxValue = kVerificationCancelled,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WEBAUTHN_CALLBACK_TYPES_H_
