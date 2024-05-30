// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"

#include "build/build_config.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace {

class SideSearchContentsThrottle : public content::NavigationThrottle {
 public:
  explicit SideSearchContentsThrottle(
      content::NavigationHandle* navigation_handle)
      : NavigationThrottle(navigation_handle) {}

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override {
    return HandleSidePanelRequest();
  }
  ThrottleCheckResult WillRedirectRequest() override {
    return HandleSidePanelRequest();
  }
  const char* GetNameForLogging() override {
    return "SideSearchContentsThrottle";
  }

 private:
  ThrottleCheckResult HandleSidePanelRequest() {
    const auto& url = navigation_handle()->GetURL();

    // Allow Google search navigations to proceed in the side panel.
    auto* config = SideSearchConfig::Get(
        navigation_handle()->GetWebContents()->GetBrowserContext());
    DCHECK(config);
    if (config->ShouldNavigateInSidePanel(url)) {
      return NavigationThrottle::PROCEED;
    }

    // Route all non-Google search URLs to the tab contents associated with this
    // side contents. This throttle will only be applied to WebContents objects
    // that are hosted in the side panel. Hence all intercepted navigations will
    // have a SideSearchContentsHelper.
    auto* side_contents_helper = SideSearchSideContentsHelper::FromWebContents(
        navigation_handle()->GetWebContents());
    DCHECK(side_contents_helper);
    side_contents_helper->NavigateInTabContents(
        content::OpenURLParams::FromNavigationHandle(navigation_handle()));
    return content::NavigationThrottle::CANCEL;
  }
};

}  // namespace

void SideSearchSideContentsHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  DCHECK(delegate_);
  delegate_->CarryOverSideSearchStateToNewTab(web_contents()->GetVisibleURL(),
                                              new_contents);
}

bool SideSearchSideContentsHelper::Delegate::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

content::WebContents* SideSearchSideContentsHelper::Delegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return nullptr;
}

SideSearchSideContentsHelper::~SideSearchSideContentsHelper() {
  if (web_contents())
    web_contents()->SetDelegate(nullptr);
  MaybeRecordMetricsPerJourney();
}

std::unique_ptr<content::NavigationThrottle>
SideSearchSideContentsHelper::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  // Only add a SideSearchContentsThrottle for side contentses.
  auto* helper =
      SideSearchSideContentsHelper::FromWebContents(handle->GetWebContents());
  if (!helper)
    return nullptr;
  return std::make_unique<SideSearchContentsThrottle>(handle);
}

void SideSearchSideContentsHelper::PrimaryPageChanged(content::Page& page) {
  const auto& url = page.GetMainDocument().GetLastCommittedURL();
  DCHECK(GetConfig()->ShouldNavigateInSidePanel(url));
  DCHECK(delegate_);
  delegate_->LastSearchURLUpdated(url);

  RecordSideSearchNavigation(
      SideSearchNavigationType::kNavigationCommittedWithinSideSearch);
  ++navigation_within_side_search_count_;
}

void SideSearchSideContentsHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  DCHECK(delegate_);
  return delegate_->SidePanelProcessGone();
}

bool SideSearchSideContentsHelper::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::DragOperationsMask operations_allowed) {
  return false;
}

bool SideSearchSideContentsHelper::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  DCHECK(delegate_);
  return delegate_->HandleKeyboardEvent(source, event);
}

content::WebContents* SideSearchSideContentsHelper::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  DCHECK(delegate_);
  return delegate_->OpenURLFromTab(source, params,
                                   std::move(navigation_handle_callback));
}

void SideSearchSideContentsHelper::NavigateInTabContents(
    const content::OpenURLParams& params) {
  DCHECK(delegate_);
  delegate_->NavigateInTabContents(params);
  RecordSideSearchNavigation(SideSearchNavigationType::kRedirectionToTab);
  ++redirection_to_tab_count_;
}

void SideSearchSideContentsHelper::LoadURL(const GURL& url) {
  DCHECK(GetConfig()->ShouldNavigateInSidePanel(url));

  // Do not reload the side contents if already navigated to `url`.
  if (web_contents()->GetLastCommittedURL() == url)
    return;

  const GURL side_search_url = GetConfig()->GenerateSideSearchURL(url);
  content::NavigationController::LoadURLParams load_url_params(side_search_url);

  // Fake the user agent on non ChromeOS systems to allow for development and
  // testing. This is needed as the side search SRP is only served to ChromeOS
  // user agents.
#if !BUILDFLAG(IS_CHROMEOS)
  load_url_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  load_url_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_TRUE;
#endif  // !BUILDFLAG(IS_CHROMEOS)

  web_contents()->GetController().LoadURLWithParams(load_url_params);
}

content::WebContents* SideSearchSideContentsHelper::GetTabWebContents() {
  return delegate_->GetTabWebContents();
}

void SideSearchSideContentsHelper::SetDelegate(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

SideSearchSideContentsHelper::SideSearchSideContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SideSearchSideContentsHelper>(*web_contents),
      webui_load_timer_(web_contents,
                        "SideSearch.LoadDocumentTime",
                        "SideSearch.LoadCompletedTime") {
  Observe(web_contents);

  web_contents->SetDelegate(this);
}

void SideSearchSideContentsHelper::MaybeRecordMetricsPerJourney() {
  RecordNavigationCommittedWithinSideSearchCountPerJourney(
      is_created_from_menu_option_, navigation_within_side_search_count_,
      auto_triggered_);
  RecordRedirectionToTabCountPerJourney(
      is_created_from_menu_option_, redirection_to_tab_count_, auto_triggered_);
}

SideSearchConfig* SideSearchSideContentsHelper::GetConfig() {
  return SideSearchConfig::Get(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideSearchSideContentsHelper);
