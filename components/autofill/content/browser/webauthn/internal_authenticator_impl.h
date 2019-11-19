// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WEBAUTHN_INTERNAL_AUTHENTICATOR_IMPL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WEBAUTHN_INTERNAL_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/webauth/authenticator_common.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;

// Implementation of the public InternalAuthenticator interface.
// This class is meant only for trusted and internal components of Chrome to
// use.
class InternalAuthenticatorImpl : public blink::mojom::InternalAuthenticator,
                                  public WebContentsObserver {
 public:
  InternalAuthenticatorImpl(RenderFrameHost* render_frame_host,
                            url::Origin effective_origin);

  ~InternalAuthenticatorImpl() override;

  // Creates a binding between this implementation and |receiver|.
  //
  // Note that one InternalAuthenticatorImpl instance can be bound to
  // exactly one interface connection at a time, and disconnected when the frame
  // navigates to a new active document.
  void Bind(
      mojo::PendingReceiver<blink::mojom::InternalAuthenticator> receiver);

 private:
  friend class InternalAuthenticatorImplTest;

  // By being able to set AuthenticatorCommon, this constructor permits setting
  // the connector and timer for testing. Using this constructor will also empty
  // out the protocol set, since no device discovery will take place during
  // tests.
  InternalAuthenticatorImpl(
      RenderFrameHost* render_frame_host,
      url::Origin effective_origin,
      std::unique_ptr<AuthenticatorCommon> authenticator_common);

  AuthenticatorCommon* get_authenticator_common_for_testing() {
    return authenticator_common_.get();
  }

  // mojom::InternalAuthenticator
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      MakeCredentialCallback callback) override;
  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
                    GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) override;

  // WebContentsObserver
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  RenderFrameHost* const render_frame_host_;
  const url::Origin effective_origin_;
  std::unique_ptr<AuthenticatorCommon> authenticator_common_;

  // Owns pipes to this Authenticator from |render_frame_host_|.
  mojo::Receiver<blink::mojom::InternalAuthenticator> receiver_{this};

  base::WeakPtrFactory<InternalAuthenticatorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InternalAuthenticatorImpl);
};

}  // namespace content

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WEBAUTHN_INTERNAL_AUTHENTICATOR_IMPL_H_
