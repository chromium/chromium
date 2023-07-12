// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace device {
class DiscoverableCredentialMetadata;
}

namespace password_manager {
class KeyboardReplacingSurfaceVisibilityController;
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
  // provides a callback that will complete the request if and when a user
  // selects a credential from a form autofill dialog, and also a closure that
  // is invoked if the user starts a hybrid authentication.
  void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      bool is_conditional_request,
      base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
          get_assertion_callback,
      base::RepeatingClosure hybrid_callback);

  // Called when an outstanding request is ended, either because it was aborted
  // by the RP, or because it completed successfully. Its main purpose is to
  // clean up conditional UI state.
  void CleanupWebAuthnRequest(content::RenderFrameHost* frame_host);

  // Tells the WebAuthn Java implementation that the user has selected a Web
  // Authentication credential from a dialog, and provides the credential ID
  // for the selected credential.
  virtual void OnWebAuthnAccountSelected(const std::vector<uint8_t>& id);

  // Tells the WebAuthn Java implementation the the user has selected the
  // option for hybrid sign-in, which should be handled by the platform.
  virtual void ShowHybridSignIn();

  // Returns the WebContents that owns this object.
  content::WebContents* web_contents();

  // Returns a delegate associated with the |web_contents|. It creates one if
  // one does not already exist.
  // The delegate is destroyed along with the WebContents and so should not be
  // cached.
  static WebAuthnRequestDelegateAndroid* GetRequestDelegate(
      content::WebContents* web_contents);

 private:
  base::RepeatingCallback<void(const std::vector<uint8_t>& user_id)>
      get_assertion_callback_;
  base::RepeatingClosure hybrid_callback_;

  // Controller for using the Touch To Fill bottom sheet for non-conditional
  // requests.
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;

  std::unique_ptr<
      password_manager::KeyboardReplacingSurfaceVisibilityController>
      visibility_controller_;

  // The WebContents that has this object in its userdata.
  raw_ptr<content::WebContents> web_contents_;

  bool conditional_request_in_progress_ = false;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_
