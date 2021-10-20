// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"

namespace {

// A flag to track whether the last call to `TestSRPAvailability()` returned
// successfully. Currently only called once per browser session.
bool g_is_side_panel_srp_available = false;

}  // namespace

SideSearchTabContentsHelper::~SideSearchTabContentsHelper() = default;

void SideSearchTabContentsHelper::NavigateInTabContents(
    const content::OpenURLParams& params) {
  web_contents()->GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
}

void SideSearchTabContentsHelper::LastSearchURLUpdated(const GURL& url) {
  DCHECK(google_util::IsGoogleSearchUrl(url));
  last_search_url_ = url;
}

bool SideSearchTabContentsHelper::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return delegate_ ? delegate_->HandleKeyboardEvent(source, event) : false;
}

content::WebContents* SideSearchTabContentsHelper::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return delegate_ ? delegate_->OpenURLFromTab(source, params) : nullptr;
}

void SideSearchTabContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  const auto& url = navigation_handle->GetURL();

  if (google_util::IsGoogleSearchUrl(url)) {
    returned_to_previous_srp_ = navigation_handle->GetPageTransition() &
                                ui::PAGE_TRANSITION_FORWARD_BACK;

    // Capture the URL here in case the side contents is closed before the
    // navigation completes.
    last_search_url_ = url;

    if (!g_is_side_panel_srp_available)
      TestSRPAvailability();

    if (side_panel_contents_)
      UpdateSideContentsNavigation();
  }
}

void SideSearchTabContentsHelper::SidePanelProcessGone() {
  ClearSidePanelContents();
  // For state-per-tab we want to toggle the helper closed to ensure its toggled
  // state is updated correctly in the case the renderer crashes but it is not
  // currently being hosted in the side panel.
  toggled_open_ = false;
  if (delegate_)
    delegate_->SidePanelAvailabilityChanged(true);
}

content::WebContents* SideSearchTabContentsHelper::GetSidePanelContents() {
  if (!side_panel_contents_)
    CreateSidePanelContents();

  DCHECK(side_panel_contents_);
  UpdateSideContentsNavigation();
  return side_panel_contents_.get();
}

void SideSearchTabContentsHelper::ClearSidePanelContents() {
  // It is safe to reset this here as any views::WebViews hosting this
  // WebContents will clear their reference to this away during its destruction.
  side_panel_contents_.reset();
}

bool SideSearchTabContentsHelper::CanShowSidePanelForCommittedNavigation() {
  const GURL& url = web_contents()->GetLastCommittedURL();
  return last_search_url_ && g_is_side_panel_srp_available &&
         !google_util::IsGoogleSearchUrl(url) &&
         !google_util::IsGoogleHomePageUrl(url) &&
         url.spec() != chrome::kChromeUINewTabURL;
}

void SideSearchTabContentsHelper::SetDelegate(
    base::WeakPtr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void SideSearchTabContentsHelper::SetSidePanelContentsForTesting(
    std::unique_ptr<content::WebContents> side_panel_contents) {
  side_panel_contents_ = std::move(side_panel_contents);
  SideSearchSideContentsHelper::CreateForWebContents(
      side_panel_contents_.get());
  GetSideContentsHelper()->SetDelegate(this);
}

void SideSearchTabContentsHelper::SetIsSidePanelSRPAvailableForTesting(
    bool is_side_panel_srp_available) {
  g_is_side_panel_srp_available = is_side_panel_srp_available;
}

SideSearchTabContentsHelper::SideSearchTabContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

SideSearchSideContentsHelper*
SideSearchTabContentsHelper::GetSideContentsHelper() {
  DCHECK(side_panel_contents_);
  return SideSearchSideContentsHelper::FromWebContents(
      side_panel_contents_.get());
}

void SideSearchTabContentsHelper::CreateSidePanelContents() {
  DCHECK(!side_panel_contents_);
  side_panel_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext(), nullptr));
  task_manager::WebContentsTags::CreateForTabContents(
      side_panel_contents_.get());
  PrefsTabHelper::CreateForWebContents(side_panel_contents_.get());
  SideSearchSideContentsHelper::CreateForWebContents(
      side_panel_contents_.get());
  GetSideContentsHelper()->SetDelegate(this);
}

void SideSearchTabContentsHelper::UpdateSideContentsNavigation() {
  DCHECK(side_panel_contents_);
  // Only update the side panel contents with the latest `last_search_url_` if
  // present
  if (last_search_url_ && g_is_side_panel_srp_available)
    GetSideContentsHelper()->LoadURL(last_search_url_.value());
}

void SideSearchTabContentsHelper::TestSRPAvailability() {
  if (g_is_side_panel_srp_available)
    return;
  // TODO(tluk): Add rate limiting to the SRP test to permanently disable the
  // feature for a given session if the availability check fails enough times.
  DCHECK(last_search_url_.has_value());
  DCHECK(google_util::IsGoogleSearchUrl(last_search_url_.value()));
  auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("side_search_availability_test", R"(
        semantics {
          sender: "Side Search Tab Helper"
          description: "Pings for the Side Search Google SRP page to check if "
            "the page is available."
          trigger:
            "After the user has successfully committed a navigation to a Google"
            "SRP in a tab contents."
          data:
            "No data sent except for the additional sidesearch URL parameter. "
            "Data does not contain PII."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "None"
          chrome_policy {
            SideSearchEnabled {
              SideSearchEnabled: false
            }
          }
        })");
  auto url_loader_factory = web_contents()
                                ->GetBrowserContext()
                                ->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = last_search_url_.value();
  // Make a HEAD request to avoid generating an actual SRP page when checking
  // for availability of the side panel SRP.
  request->method = net::HttpRequestHeaders::kHeadMethod;
  simple_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  simple_loader_->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::BindOnce(&SideSearchTabContentsHelper::OnResponseLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SideSearchTabContentsHelper::OnResponseLoaded(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  g_is_side_panel_srp_available = simple_loader_->NetError() == net::OK;

  // The test for availability is performed async so alert `delegate_` that the
  // side panel SRP is available to give it the opportunity to update
  // appropriately.
  if (delegate_)
    delegate_->SidePanelAvailabilityChanged(false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideSearchTabContentsHelper);
