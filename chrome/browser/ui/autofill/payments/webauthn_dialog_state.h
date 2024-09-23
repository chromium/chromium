// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_STATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_STATE_H_

namespace autofill {

// The type of save card bubble to show.
enum class WebauthnDialogState {
  kUnknown,
  // The dialog is about to be closed automatically. This happens only after
  // authentication challenge is successfully fetched.
  kInactive,
  // The option of using platform authenticator is being offered.
  kOffer,
  // Offer was accepted, fetching authentication challenge.
  kOfferPending,
  // Fetching authentication challenge failed.
  kOfferError,
  // Indicating the card verification is in progress. Shown only for opted-in
  // users.
  kVerifyPending,
  // TODO(crbug.com/40639086): Add an extra state for case when the cancel
  // button in the verify pending dialog should be disabled.
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_STATE_H_
