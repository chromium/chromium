// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shared_highlighting/shared_highlighting_promo.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

void OnGetExistingSelectorsComplete(
    base::WeakPtr<content::WebContents> web_contents,
    const std::vector<std::string>& selectors) {
  if (web_contents && selectors.size() > 0) {
    BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
        web_contents.get())
        ->MaybeShowFeaturePromo(
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
  remote_->GetExistingSelectors(base::BindOnce(&OnGetExistingSelectorsComplete,
                                               web_contents()->GetWeakPtr()));
}

bool SharedHighlightingPromo::HasTextFragment(std::string url) {
  if (url.empty())
    return false;

  return url.find(":~:text=") != std::string::npos;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedHighlightingPromo);
