// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/about_this_site_side_panel_coordinator.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/about_this_site_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

namespace {
content::OpenURLParams CreateOpenUrlParams(const GURL& url) {
  return content::OpenURLParams(
      net::AppendOrReplaceQueryParameter(
          url, page_info::AboutThisSiteRenderModeParameterName,
          page_info::AboutThisSiteRenderModeParameterValue),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
}
}  // namespace

void ShowAboutThisSiteSidePanel(content::WebContents* web_contents,
                                const GURL& more_about_url) {
  // Create PanelCoordinator if it doesn't exist yet.
  AboutThisSideSidePanelCoordinator::CreateForWebContents(web_contents);
  AboutThisSideSidePanelCoordinator::FromWebContents(web_contents)
      ->RegisterEntryAndShow(more_about_url);
}

void RegisterAboutThisSiteSidePanel(content::WebContents* web_contents,
                                    const GURL& more_about_url) {
  // Create PanelCoordinator if it doesn't exist yet.
  AboutThisSideSidePanelCoordinator::CreateForWebContents(web_contents);
  AboutThisSideSidePanelCoordinator::FromWebContents(web_contents)
      ->RegisterEntry(more_about_url);
}

AboutThisSideSidePanelCoordinator::AboutThisSideSidePanelCoordinator(
    content::WebContents* web_contents)
    : content::WebContentsUserData<AboutThisSideSidePanelCoordinator>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

AboutThisSideSidePanelCoordinator::~AboutThisSideSidePanelCoordinator() =
    default;

void AboutThisSideSidePanelCoordinator::RegisterEntry(
    const GURL& more_about_url) {
  auto* browser_view = GetBrowserView();
  if (!browser_view)
    return;

  auto* registry = SidePanelRegistry::Get(web_contents());
  last_url_info_ = {web_contents()->GetLastCommittedURL(), more_about_url,
                    CreateOpenUrlParams(more_about_url)};
  registered_but_not_shown_ = true;

  // Check if the view is already registered.
  if (!registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite))) {
    const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
        ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kAboutThisSite,
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE),
        ui::ImageModel::FromVectorIcon(views::kInfoIcon, ui::kColorIcon,
                                       icon_size),
        base::BindRepeating(
            &AboutThisSideSidePanelCoordinator::CreateAboutThisSiteWebView,
            base::Unretained(this)),
        base::BindRepeating(
            &AboutThisSideSidePanelCoordinator::GetOpenInNewTabUrl,
            base::Unretained(this)));
    registry->Register(std::move(entry));
  }
}

void AboutThisSideSidePanelCoordinator::RegisterEntryAndShow(
    const GURL& more_about_url) {
  auto* browser_view = GetBrowserView();
  if (!browser_view)
    return;

  RegisterEntry(more_about_url);
  registered_but_not_shown_ = false;

  if (about_this_site_side_panel_view_) {
    // Load params in view if view still exists.
    about_this_site_side_panel_view_->OpenUrl(last_url_info_->url_params);
  }

  auto* side_panel_coordinator = browser_view->side_panel_coordinator();
  if (side_panel_coordinator->GetCurrentEntryId() !=
      SidePanelEntry::Id::kAboutThisSite) {
    side_panel_coordinator->Show(SidePanelEntry::Id::kAboutThisSite);
  }
}

void AboutThisSideSidePanelCoordinator::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  auto* browser_view = GetBrowserView();
  if (!browser_view)
    return;

  // If the side panel is open and shows the AboutThisSide panel, close it.
  auto* side_panel_coordinator = browser_view->side_panel_coordinator();
  if (side_panel_coordinator->GetCurrentEntryId() ==
      SidePanelEntry::Id::kAboutThisSite) {
    side_panel_coordinator->Close();
  }

  // If the user navigates to a different page than the one we have data for
  // we need to remove the SidePanel registration. We might already have data
  // from cacao for the current pageload if it was locally cached.
  auto* registry = SidePanelRegistry::Get(web_contents());
  SidePanelEntry::Key key(SidePanelEntry::Id::kAboutThisSite);
  if (registry->GetEntryForKey(key) &&
      web_contents()->GetLastCommittedURL() != last_url_info_->context_url) {
    registry->Deregister(key);
    last_url_info_.reset();
  }

  // If the view is cached, we will remove it since it shows the wrong page.
  if (about_this_site_side_panel_view_) {
    auto* entry = registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));
    DCHECK(entry);
    entry->ClearCachedView();
  }
}

std::unique_ptr<views::View>
AboutThisSideSidePanelCoordinator::CreateAboutThisSiteWebView() {
  DCHECK(GetBrowserView());
  DCHECK(last_url_info_);
  if (registered_but_not_shown_) {
    page_info::AboutThisSiteService::OnOpenedDirectlyFromSidePanel();
    registered_but_not_shown_ = false;
  }

  auto side_panel_view_ =
      std::make_unique<AboutThisSiteSidePanelView>(GetBrowserView());
  side_panel_view_->OpenUrl(last_url_info_->url_params);
  about_this_site_side_panel_view_ = side_panel_view_->AsWeakPtr();
  return side_panel_view_;
}

BrowserView* AboutThisSideSidePanelCoordinator::GetBrowserView() const {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
  return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
}

GURL AboutThisSideSidePanelCoordinator::GetOpenInNewTabUrl() {
  DCHECK(last_url_info_.has_value());
  DCHECK(!base::Contains(last_url_info_.value().new_tab_url.query_piece(),
                         page_info::AboutThisSiteRenderModeParameterName));
  return last_url_info_.value().new_tab_url;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AboutThisSideSidePanelCoordinator);
