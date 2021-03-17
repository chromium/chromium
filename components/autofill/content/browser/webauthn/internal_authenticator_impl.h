// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WEBAUTHN_INTERNAL_AUTHENTICATOR_IMPL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WEBAUTHN_INTERNAL_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "content/browser/webauth/authenticator_common.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;

// Implementation of the public InternalAuthenticator interface.
// This class is meant only for trusted and internal components of Chrome to
// use.
class InternalAuthenticatorImpl : public autofill::InternalAuthenticator,
                                  public WebContentsObserver {
 public:
  explicit InternalAuthenticatorImpl(RenderFrameHost* render_frame_host);

  ~InternalAuthenticatorImpl() override;

  // InternalAuthenticator:
  void SetEffectiveOrigin(const url::Origin& origin) override;
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) override;
  void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::Authenticator::GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override;
  void Cancel() override;
  content::RenderFrameHost* GetRenderFrameHost() override;

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

 private:
  friend class InternalAuthenticatorImplTest;

  AuthenticatorCommon* get_authenticator_common_for_testing() {
    return authenticator_common_.get();
  }

  RenderFrameHost* const render_frame_host_;
  url::Origin effective_origin_;
  std::unique_ptr<AuthenticatorCommon> authenticator_common_;

  base::WeakPtrFactory<InternalAuthenticatorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InternalAuthenticatorImpl);
};

}  // namespace content

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WEBAUTHN_INTERNAL_AUTHENTICATOR_IMPL_H_
