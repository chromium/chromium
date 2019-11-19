// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/webauthn/internal_authenticator_impl.h"

#include <string>
#include <utility>

#include "base/timer/timer.h"
#include "content/browser/webauth/authenticator_common.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace content {

InternalAuthenticatorImpl::InternalAuthenticatorImpl(
    RenderFrameHost* render_frame_host,
    url::Origin effective_origin)
    : InternalAuthenticatorImpl(render_frame_host,
                                std::move(effective_origin),
                                std::make_unique<AuthenticatorCommon>(
                                    render_frame_host,
                                    nullptr /* connector */,
                                    std::make_unique<base::OneShotTimer>())) {}

InternalAuthenticatorImpl::InternalAuthenticatorImpl(
    RenderFrameHost* render_frame_host,
    url::Origin effective_origin,
    std::unique_ptr<AuthenticatorCommon> authenticator_common)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      effective_origin_(std::move(effective_origin)),
      authenticator_common_(std::move(authenticator_common)) {
  DCHECK(render_frame_host_);
  DCHECK(authenticator_common_);
  DCHECK(!effective_origin.opaque());
}

InternalAuthenticatorImpl::~InternalAuthenticatorImpl() {
  // This call exists to assert that |render_frame_host_| outlives this object.
  // If this is violated, ASAN should notice.
  render_frame_host_->GetRoutingID();
}

void InternalAuthenticatorImpl::Bind(
    mojo::PendingReceiver<blink::mojom::InternalAuthenticator> receiver) {
  // If |render_frame_host_| is being unloaded then binding receivers are
  // rejected.
  if (!render_frame_host_->IsCurrent()) {
    return;
  }

  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

// mojom::InternalAuthenticator
void InternalAuthenticatorImpl::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakeCredentialCallback callback) {
  authenticator_common_->MakeCredential(effective_origin_, std::move(options),
                                        std::move(callback));
}

// mojom::InternalAuthenticator
void InternalAuthenticatorImpl::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    GetAssertionCallback callback) {
  authenticator_common_->GetAssertion(effective_origin_, std::move(options),
                                      std::move(callback));
}

// mojom::InternalAuthenticator
void InternalAuthenticatorImpl::IsUserVerifyingPlatformAuthenticatorAvailable(
    IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  authenticator_common_->IsUserVerifyingPlatformAuthenticatorAvailable(
      std::move(callback));
}

void InternalAuthenticatorImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the RenderFrameHost itself is navigated then this function will cause
  // request state to be cleaned up. It's also possible for a navigation in the
  // same frame to use a fresh RenderFrameHost. In this case,
  // |render_frame_host_->IsCurrent()| will start returning false, causing all
  // focus checks to fail if any Mojo requests are made in that state.
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      navigation_handle->GetRenderFrameHost() != render_frame_host_) {
    return;
  }

  receiver_.reset();
  authenticator_common_->Cleanup();
}

}  // namespace content
