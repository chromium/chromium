// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CONDITIONAL_UI_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CONDITIONAL_UI_DELEGATE_ANDROID_H_

#include <vector>

#include "base/callback.h"
#include "base/supports_user_data.h"

namespace content {
class WebContents;
}

namespace device {
class DiscoverableCredentialMetadata;
}

// Helper class for connecting the autofill implementation to the WebAuthn
// request handling for Conditional UI on Android. This is attached to a
// WebContents via SetUserData. It caches callbacks in both directions to
// eliminate races between the WebAuthn 'get' request and the autofill request
// for form suggestions.
class ConditionalUiDelegateAndroid : public base::SupportsUserData::Data {
 public:
  ConditionalUiDelegateAndroid();

  ConditionalUiDelegateAndroid(const ConditionalUiDelegateAndroid&) = delete;
  ConditionalUiDelegateAndroid& operator=(const ConditionalUiDelegateAndroid&) =
      delete;

  ~ConditionalUiDelegateAndroid() override;

  // Called when a Web Authentication Conditional UI request is received. This
  // provides the callback that will complete the request if and when a user
  // selects a credential from a form autofill dialog.
  void OnWebAuthnRequestPending(
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      base::OnceCallback<void(const std::vector<uint8_t>& id)> callback);

  // Tells the driver that the user has selected a Web Authentication
  // credential from a dialog, and provides the credential ID for the selected
  // credential.
  void OnWebAuthnAccountSelected(const std::vector<uint8_t>& id);

  // Retrieves a list of Web Authentication credentials that can be displayed
  // as suggestions in an autofill dialog.
  void RetrieveWebAuthnCredentials(
      base::OnceCallback<
          void(const std::vector<device::DiscoverableCredentialMetadata>&)>);

  // Returns a delegate associated with the |web_contents|. It creates one if
  // one does not already exist.
  // The delegate is destroyed along with the WebContents and so should not be
  // cached.
  static ConditionalUiDelegateAndroid* GetConditionalUiDelegate(
      content::WebContents* web_contents);

 private:
  std::vector<device::DiscoverableCredentialMetadata>
      webauthn_account_suggestions_;

  base::OnceCallback<void(const std::vector<uint8_t>& user_id)>
      webauthn_account_selection_callback_;
  base::OnceCallback<void(
      const std::vector<device::DiscoverableCredentialMetadata>&)>
      retrieve_credentials_callback_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_CONDITIONAL_UI_DELEGATE_ANDROID_H_
