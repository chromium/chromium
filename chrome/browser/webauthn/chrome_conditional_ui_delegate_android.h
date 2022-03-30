// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_CONDITIONAL_UI_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_CONDITIONAL_UI_DELEGATE_ANDROID_H_

#include <vector>

#include "base/callback.h"
#include "base/supports_user_data.h"
#include "device/fido/discoverable_credential_metadata.h"

namespace content {
class WebContents;
}

// Chrome implementation of ConditionalUiDelegateAndroid.
class ChromeConditionalUiDelegateAndroid : public base::SupportsUserData::Data {
 public:
  ChromeConditionalUiDelegateAndroid();

  ChromeConditionalUiDelegateAndroid(
      const ChromeConditionalUiDelegateAndroid&) = delete;
  ChromeConditionalUiDelegateAndroid& operator=(
      const ChromeConditionalUiDelegateAndroid&) = delete;

  ~ChromeConditionalUiDelegateAndroid() override;

  // TODO(kenrb): This will override a method in a content delegate interface
  // in a subsequent change, so that it can be invoked from RenderFrameHost.
  void OnWebAuthnRequestPending(
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      base::OnceCallback<void(const std::vector<uint8_t>& user_id)> callback);

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
  static ChromeConditionalUiDelegateAndroid* GetConditionalUiDelegate(
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

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_CONDITIONAL_UI_DELEGATE_ANDROID_H_
