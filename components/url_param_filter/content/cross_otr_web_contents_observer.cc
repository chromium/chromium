// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/content/cross_otr_web_contents_observer.h"

#include <memory>

#include "base/strings/string_util.h"
#include "components/url_param_filter/core/cross_otr_observer.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_response_headers.h"
#include "ui/base/page_transition_types.h"

namespace url_param_filter {
namespace {
constexpr char kInternalRedirectHeaderStatusLine[] =
    "HTTP/1.1 307 Internal Redirect";

bool IsInternalRedirect(const net::HttpResponseHeaders* headers) {
  return base::EqualsCaseInsensitiveASCII(headers->GetStatusLine(),
                                          kInternalRedirectHeaderStatusLine);
}
}  // anonymous namespace

void CrossOtrWebContentsObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    bool privacy_sensitivity_is_cross_otr,
    bool started_from_context_menu,
    ui::PageTransition transition) {
  if (privacy_sensitivity_is_cross_otr && started_from_context_menu &&
      !ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    // Inherited from WebContentsUserData and checks for an already-attached
    // instance internally.
    CrossOtrWebContentsObserver::CreateForWebContents(web_contents);
  }
}

CrossOtrWebContentsObserver::CrossOtrWebContentsObserver(
    content::WebContents* web_contents)
    : CrossOtrObserver(ObserverType::kContent),
      content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CrossOtrWebContentsObserver>(*web_contents),
      weak_factory_(this) {}

CrossOtrWebContentsObserver::~CrossOtrWebContentsObserver() = default;

void CrossOtrWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnNavigationStart(
      /*is_primary_frame=*/navigation_handle->IsInPrimaryMainFrame(),
      /*user_activated=*/absl::nullopt,
      /*is_client_redirect=*/
      (navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      /*init_cross_otr=*/absl::nullopt);
}

void CrossOtrWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool should_detach = OnNavigationFinish(
      /*is_primary_frame=*/navigation_handle->IsInPrimaryMainFrame(),
      /*is_same_document=*/navigation_handle->IsSameDocument(),
      /*headers=*/navigation_handle->GetResponseHeaders(),
      /*is_reload=*/navigation_handle->GetReloadType() !=
          content::ReloadType::NONE,
      /*has_committed=*/navigation_handle->HasCommitted());
  if (should_detach) {
    Detach();
    // DO NOT add code past this point. `this` is destroyed.
  }
}

void CrossOtrWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  OnNavigationRedirect(
      /*is_primary_frame=*/navigation_handle->IsInPrimaryMainFrame(),
      /*is_same_document=*/navigation_handle->IsSameDocument(), headers,
      /*is_internal_redirect=*/headers ? IsInternalRedirect(headers) : false);
}
void CrossOtrWebContentsObserver::WebContentsDestroyed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The user has closed the tab or otherwise destroyed the web contents. Flush
  // metrics and cease observation.
  Detach();
  // DO NOT add code past this point. `this` is destroyed.
}

void CrossOtrWebContentsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Anytime the user activates a frame in the web contents, we cease to
  // consider the case cross-OTR.
  ExitCrossOtr();
}

void CrossOtrWebContentsObserver::Detach() {
  // Metrics will not be collected for non intervened navigation chains and
  // navigations occurring prior to params filtering.
  if (did_filter_params()) {
    WriteRefreshMetric();
  }
  web_contents()->RemoveUserData(CrossOtrWebContentsObserver::UserDataKey());
  // DO NOT add code past this point. `this` is destroyed.
}

base::WeakPtr<CrossOtrWebContentsObserver>
CrossOtrWebContentsObserver::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CrossOtrWebContentsObserver);

}  // namespace url_param_filter
