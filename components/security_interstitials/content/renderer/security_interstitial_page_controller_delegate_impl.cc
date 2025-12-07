// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/renderer/security_interstitial_page_controller_delegate_impl.h"

#include "content/public/renderer/render_frame.h"

namespace security_interstitials {

SecurityInterstitialPageControllerDelegateImpl::
    SecurityInterstitialPageControllerDelegateImpl(
        content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<
          SecurityInterstitialPageControllerDelegateImpl>(render_frame) {}

SecurityInterstitialPageControllerDelegateImpl::
    ~SecurityInterstitialPageControllerDelegateImpl() = default;

void SecurityInterstitialPageControllerDelegateImpl::PrepareForErrorPage() {
  pending_error_ = true;
}

void SecurityInterstitialPageControllerDelegateImpl::OnDestruct() {
  delete this;
}

void SecurityInterstitialPageControllerDelegateImpl::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  committed_error_ = pending_error_;
  pending_error_ = false;
}

void SecurityInterstitialPageControllerDelegateImpl::DidFinishLoad() {
  if (committed_error_) {
    security_interstitials::SecurityInterstitialPageController::Install(
        render_frame());
  }
}

}  // namespace security_interstitials
