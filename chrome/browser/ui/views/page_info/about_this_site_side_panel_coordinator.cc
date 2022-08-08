// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/about_this_site_side_panel_coordinator.h"

#include "base/callback.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/about_this_site_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

void ShowAboutThisSiteSidePanel(content::WebContents* web_contents,
                                const content::OpenURLParams& params) {
  // Create PanelCoordinator if it doesn't exist yet.
  AboutThisSideSidePanelCoordinator::CreateForWebContents(web_contents);
  AboutThisSideSidePanelCoordinator::FromWebContents(web_contents)
      ->RegisterEntryAndShow(params);
}

AboutThisSideSidePanelCoordinator::AboutThisSideSidePanelCoordinator(
    content::WebContents* web_contents)
    : content::WebContentsUserData<AboutThisSideSidePanelCoordinator>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

AboutThisSideSidePanelCoordinator::~AboutThisSideSidePanelCoordinator() =
    default;

void AboutThisSideSidePanelCoordinator::RegisterEntryAndShow(
    const content::OpenURLParams& params) {
  auto* browser_view = GetBrowserView();
  if (!browser_view)
    return;

  auto* side_panel_coordinator = browser_view->side_panel_coordinator();
  auto* registry = SidePanelRegistry::Get(web_contents());

  last_url_params_ = params;

  // Check if the view is already registered.
  if (!registry->GetEntryForId(SidePanelEntry::Id::kAboutThisSite)) {
    const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
        ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kAboutThisSite,
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE),
        ui::ImageModel::FromVectorIcon(vector_icons::kGoogleColorIcon,
                                       ui::kColorIcon, icon_size),
        base::BindRepeating(
            &AboutThisSideSidePanelCoordinator::CreateAboutThisSiteWebView,
            base::Unretained(this)));
    registry->Register(std::move(entry));
  }

  if (about_this_site_side_panel_view_) {
    // Load params in view if view still exists.
    about_this_site_side_panel_view_->OpenUrl(params);
  }

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
  // Remove SidePanel entry when user navigates to a different page.
  SidePanelRegistry::Get(web_contents())
      ->Deregister(SidePanelEntry::Id::kAboutThisSite);
  about_this_site_side_panel_view_ = nullptr;
  last_url_params_.reset();
}

std::unique_ptr<views::View>
AboutThisSideSidePanelCoordinator::CreateAboutThisSiteWebView() {
  DCHECK(GetBrowserView());
  DCHECK(last_url_params_);
  auto side_panel_view_ =
      std::make_unique<AboutThisSiteSidePanelView>(GetBrowserView());
  side_panel_view_->OpenUrl(*last_url_params_);
  about_this_site_side_panel_view_ = side_panel_view_->AsWeakPtr();
  return side_panel_view_;
}

BrowserView* AboutThisSideSidePanelCoordinator::GetBrowserView() const {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
  return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AboutThisSideSidePanelCoordinator);
