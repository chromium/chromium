// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_RENDERER_SECURITY_INTERSTITIAL_PAGE_CONTROLLER_DELEGATE_IMPL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_RENDERER_SECURITY_INTERSTITIAL_PAGE_CONTROLLER_DELEGATE_IMPL_H_

#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace security_interstitials {

class SecurityInterstitialPageControllerDelegateImpl
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<
          SecurityInterstitialPageControllerDelegateImpl> {
 public:
  explicit SecurityInterstitialPageControllerDelegateImpl(
      content::RenderFrame* render_frame);

  // Disallow copy and assign
  SecurityInterstitialPageControllerDelegateImpl(
      const SecurityInterstitialPageControllerDelegateImpl&) = delete;
  SecurityInterstitialPageControllerDelegateImpl& operator=(
      const SecurityInterstitialPageControllerDelegateImpl&) = delete;

  ~SecurityInterstitialPageControllerDelegateImpl() override;

  // Notifies us that a navigation error has occurred and will be committed
  void PrepareForErrorPage();

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishLoad() override;

 private:
  // Whether there is an error page pending to be committed.
  bool pending_error_ = false;

  // Whether the committed page is an error page.
  bool committed_error_ = false;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_RENDERER_SECURITY_INTERSTITIAL_PAGE_CONTROLLER_DELEGATE_IMPL_H_
