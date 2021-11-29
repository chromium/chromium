// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shared_highlighting/shared_highlighting_promo.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

SharedHighlightingPromo::SharedHighlightingPromo(
    content::WebContents* web_contents,
    Browser* browser)
    : content::WebContentsObserver(web_contents) {
  feature_promo_controller_ = browser->window()->GetFeaturePromoController();
}

SharedHighlightingPromo::~SharedHighlightingPromo() = default;

void SharedHighlightingPromo::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopSharedHighlightingFeature)) {
    return;
  }

  if (HasTextFragment(validated_url.spec()))
    CheckExistingSelectors(render_frame_host);
}

void SharedHighlightingPromo::OnGetExistingSelectorsComplete(
    const std::vector<std::string>& selectors) {
  if (selectors.size() > 0) {
    feature_promo_controller_->MaybeShowPromo(
        feature_engagement::kIPHDesktopSharedHighlightingFeature);
  }
}

void SharedHighlightingPromo::CheckExistingSelectors(
    content::RenderFrameHost* render_frame_host) {
  if (!remote_.is_bound()) {
    render_frame_host->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
        remote_.BindNewPipeAndPassReceiver());
  }

  remote_->GetExistingSelectors(
      base::BindOnce(&SharedHighlightingPromo::OnGetExistingSelectorsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool SharedHighlightingPromo::HasTextFragment(std::string url) {
  if (url.empty())
    return false;

  return url.find(":~:text=") != std::string::npos;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedHighlightingPromo);
