// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_webauthn_delegate.h"
#include "chrome/browser/webauthn/shared_types.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/webauthn/android/webauthn_client_android.h"
#include "content/public/browser/global_routing_id.h"

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

class PasswordCredentialFetcher;
class TouchToFillController;

// Helper class for connecting the autofill implementation to the WebAuthn
// request handling for Conditional UI on Android. This is attached to a
// WebContents via SetUserData. It caches a callback that will complete the
// WebAuthn 'get' request when a user selects a credential.
class WebAuthnRequestDelegateAndroid
    : public base::SupportsUserData::Data,
      public TouchToFillControllerWebAuthnDelegate::CredentialReceiver {
 public:
  explicit WebAuthnRequestDelegateAndroid(content::WebContents* web_contents);

  WebAuthnRequestDelegateAndroid(const WebAuthnRequestDelegateAndroid&) =
      delete;
  WebAuthnRequestDelegateAndroid& operator=(
      const WebAuthnRequestDelegateAndroid&) = delete;

  ~WebAuthnRequestDelegateAndroid() override;

  // Called when a Web Authentication GetAssertion request is received. This
  // provides password and passkey callbacks that will complete the request if
  // and when a user selects a credential from a touch to fill sheet.
  // `hybrid_closure` is invoked if the user selects the option to trigger the
  // hybrid transport for passkeys.
  // `non_credential_callback` is invoked if the sheet if the request will be
  // completed for any other reason.
  void OnWebAuthnRequestPending(
      content::RenderFrameHost* frame_host,
      std::vector<device::DiscoverableCredentialMetadata> credentials,
      webauthn::AssertionMediationType mediation_type,
      base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
          passkey_callback,
      base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
          password_callback,
      base::RepeatingClosure hybrid_closure,
      base::RepeatingCallback<void(webauthn::NonCredentialReturnReason)>
          non_credential_callback);

  // Called when an outstanding request is ended, either because it was aborted
  // by the RP, or because it completed successfully. Its main purpose is to
  // clean up conditional UI state.
  void CleanupWebAuthnRequest(content::RenderFrameHost* frame_host);

  // TouchToFillControllerWebAuthnDelegate::CredentialReceiver:
  void OnWebAuthnAccountSelected(const std::vector<uint8_t>& id) override;
  void OnPasswordCredentialSelected(
      const PasswordCredentialPair& password_credential) override;
  void OnCredentialSelectionDeclined() override;
  void OnHybridSignInSelected() override;
  content::WebContents* web_contents() override;

  // Returns a delegate associated with the |web_contents|. It creates one if
  // one does not already exist.
  // The delegate is destroyed along with the WebContents and so should not be
  // cached.
  static WebAuthnRequestDelegateAndroid* GetRequestDelegate(
      content::WebContents* web_contents);

 private:
  // This takes the RenderFrameHost's GlobalID rather than a pointer, so that
  // it can be called asynchronously without having to worry about lifetimes.
  void MaybeShowTouchToFillSheet(
      content::GlobalRenderFrameHostId render_frame_host_id,
      bool isImmediate,
      std::vector<password_manager::PasskeyCredential> passkey_credentials,
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          password_credentials);

  // Completion callbacks for the request. These can be null if
  // `CleanupWebAuthnRequest` has been called, probably due to the credential
  // request being cancelled.
  base::RepeatingCallback<void(const std::vector<uint8_t>& id)>
      passkey_callback_;
  base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
      password_callback_;
  base::RepeatingClosure hybrid_closure_;
  base::RepeatingCallback<void(webauthn::NonCredentialReturnReason)>
      non_credential_callback_;

  // Controller for using the Touch To Fill bottom sheet for non-conditional
  // requests.
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;

  std::unique_ptr<
      password_manager::KeyboardReplacingSurfaceVisibilityController>
      visibility_controller_;

  std::unique_ptr<PasswordCredentialFetcher> password_fetcher_;

  // The WebContents that has this object in its userdata.
  raw_ptr<content::WebContents> web_contents_;

  bool conditional_request_in_progress_ = false;

  base::WeakPtrFactory<WebAuthnRequestDelegateAndroid> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_WEBAUTHN_REQUEST_DELEGATE_ANDROID_H_
