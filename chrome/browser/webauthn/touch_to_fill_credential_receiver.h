// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_TOUCH_TO_FILL_CREDENTIAL_RECEIVER_H_
#define CHROME_BROWSER_WEBAUTHN_TOUCH_TO_FILL_CREDENTIAL_RECEIVER_H_

#include <cstdint>
#include <vector>

#include "chrome/browser/webauthn/shared_types.h"

namespace content {
class WebContents;
}

// Pure-virtual interface for objects that receive the outcome of a Touch To
// Fill credential selection sheet.
class TouchToFillCredentialReceiver {
 public:
  // Tells the WebAuthn Java implementation that the user has selected a Web
  // Authentication credential from a dialog, and provides the credential ID
  // for the selected credential.
  virtual void OnWebAuthnAccountSelected(const std::vector<uint8_t>& id) = 0;

  // Provides a password credential that the user has selected.
  virtual void OnPasswordCredentialSelected(
      const PasswordCredentialPair& password_credential) = 0;

  // Called when the user dismisses the sheet in immediate mode without
  // having selected a credential.
  virtual void OnCredentialSelectionDeclined() = 0;

  // Tells the WebAuthn Java implementation the the user has selected the
  // option for hybrid sign-in, which should be handled by the platform.
  virtual void OnHybridSignInSelected() = 0;

  // Return the WebContents associated with the current WebAuthn request.
  virtual content::WebContents* web_contents() = 0;

 protected:
  ~TouchToFillCredentialReceiver() = default;
};

#endif  // CHROME_BROWSER_WEBAUTHN_TOUCH_TO_FILL_CREDENTIAL_RECEIVER_H_
