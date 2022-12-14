// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace device {
class DiscoverableCredentialMetadata;
}

class TouchToFillController;

// Helper class for connecting the autofill implementation to the WebAuthn
// request handling for Conditional UI on Android. This is attached to a
// WebContents via SetUserData. It caches a callback that will complete the
// WebAuthn 'get' request when a user selects a credential.
class WebAuthnRequestDelegateAndroid : public base::SupportsUserData::Data {
 public:
  explicit WebAuthnRequestDelegateAndroid(content::WebContents* web_contents);

  WebAuthnRequestDelegateAndroid(const WebAuthnRequestDelegateAndroid&) =
      delete;
  WebAuthnRequestDelegateAndroid& operator=(
      const WebAuthnRequestDelegateAndroid&) = delete;

  ~WebAuthnRequestDelegateAndroid() override;

  // Called when a Web Authentication Conditional UI request is received. This
  // provides the callback that will complete the request if and when a user
  // selects a credential from a form autofill dialog.
  void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      bool is_conditional_request,
      base::OnceCallback<void(const std::vector<uint8_t>& id)> callback);

  // Called when an outstanding request is aborted. This triggers the cached
  // callback with an empty credential.
  void CancelWebAuthnRequest(content::RenderFrameHost* frame_host);

  // Tells the driver that the user has selected a Web Authentication
  // credential from a dialog, and provides the credential ID for the selected
  // credential.
  virtual void OnWebAuthnAccountSelected(const std::vector<uint8_t>& id);

  // Returns the WebContents that owns this object.
  content::WebContents* web_contents();

  // Returns a delegate associated with the |web_contents|. It creates one if
  // one does not already exist.
  // The delegate is destroyed along with the WebContents and so should not be
  // cached.
  static WebAuthnRequestDelegateAndroid* GetRequestDelegate(
      content::WebContents* web_contents);

 private:
  base::OnceCallback<void(const std::vector<uint8_t>& user_id)>
      webauthn_account_selection_callback_;

  // Controller for using the Touch To Fill bottom sheet for non-conditional
  // requests.
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;

  // The WebContents that has this object in its userdata.
  raw_ptr<content::WebContents> web_contents_;

  bool conditional_request_in_progress_ = false;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_
