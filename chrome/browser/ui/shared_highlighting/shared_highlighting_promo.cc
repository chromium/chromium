// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shared_highlighting/shared_highlighting_promo.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

void OnGetExistingSelectorsComplete(
    base::WeakPtr<user_education::FeaturePromoController>
        feature_promo_controller,
    const std::vector<std::string>& selectors) {
  if (feature_promo_controller && selectors.size() > 0) {
    feature_promo_controller->MaybeShowPromo(
        feature_engagement::kIPHDesktopSharedHighlightingFeature);
  }
}

}  // namespace

SharedHighlightingPromo::SharedHighlightingPromo(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SharedHighlightingPromo>(*web_contents) {}

SharedHighlightingPromo::~SharedHighlightingPromo() = default;

void SharedHighlightingPromo::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopSharedHighlightingFeature) ||
      !render_frame_host->GetOutermostMainFrame()) {
    return;
  }

  if (HasTextFragment(validated_url.spec()))
    CheckExistingSelectors(render_frame_host);
}

void SharedHighlightingPromo::CheckExistingSelectors(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host->GetOutermostMainFrame());
  if (!remote_.is_bound()) {
    render_frame_host->GetRemoteInterfaces()->GetInterface(
        remote_.BindNewPipeAndPassReceiver());
  }

  // Make sure that all the relevant things still exist - and then still use a
  // weak pointer to ensure we don't tear down the browser and its promo
  // controller before the callback returns.
  auto* const window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents());
  if (window) {
    auto* const controller = window->GetFeaturePromoController();
    if (controller) {
      remote_->GetExistingSelectors(base::BindOnce(
          &OnGetExistingSelectorsComplete, controller->GetAsWeakPtr()));
    }
  }
}

bool SharedHighlightingPromo::HasTextFragment(std::string url) {
  if (url.empty())
    return false;

  return url.find(":~:text=") != std::string::npos;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedHighlightingPromo);
