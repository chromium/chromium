// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/merchant_trust_side_panel_coordinator.h"

#include "base/functional/bind.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_info/merchant_trust_side_panel.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/page_info/web_view_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

constexpr char kStaticLoadingScreenURL[] =
    "https://www.gstatic.com/diner/chrome/atp_loading.html";

namespace {
content::OpenURLParams CreateOpenUrlParams(const GURL& url) {
  return content::OpenURLParams(
      net::AppendOrReplaceQueryParameter(url,
                                         kMerchantTrustContextParameterName,
                                         kMerchantTrustContextParameterValue),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
}
}  // namespace

void ShowMerchantTrustSidePanel(content::WebContents* web_contents,
                                const GURL& merchant_reviews_url) {
  // Create PanelCoordinator if it doesn't exist yet.
  MerchantTrustSidePanelCoordinator::CreateForWebContents(web_contents);
  MerchantTrustSidePanelCoordinator::FromWebContents(web_contents)
      ->RegisterEntryAndShow(merchant_reviews_url);
}

MerchantTrustSidePanelCoordinator::MerchantTrustSidePanelCoordinator(
    content::WebContents* web_contents)
    : content::WebContentsUserData<MerchantTrustSidePanelCoordinator>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

MerchantTrustSidePanelCoordinator::~MerchantTrustSidePanelCoordinator() =
    default;

void MerchantTrustSidePanelCoordinator::RegisterEntry(
    const GURL& merchant_reviews_url) {
  SidePanelUI* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui) {
    return;
  }

  auto* registry = SidePanelRegistry::GetDeprecated(web_contents());
  last_url_info_ = {web_contents()->GetLastCommittedURL(), merchant_reviews_url,
                    CreateOpenUrlParams(merchant_reviews_url)};
  registered_but_not_shown_ = true;

  // Check if the view is already registered.
  if (!registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kMerchantTrust))) {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kMerchantTrust,
        base::BindRepeating(
            &MerchantTrustSidePanelCoordinator::CreateMerchantTrustWebView,
            base::Unretained(this)),
        base::BindRepeating(
            &MerchantTrustSidePanelCoordinator::GetOpenInNewTabUrl,
            base::Unretained(this)));
    registry->Register(std::move(entry));
  }
}

void MerchantTrustSidePanelCoordinator::RegisterEntryAndShow(
    const GURL& more_about_url) {
  SidePanelUI* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui) {
    return;
  }

  RegisterEntry(more_about_url);
  registered_but_not_shown_ = false;

  if (web_view_side_panel_view_) {
    // Load params in view if view still exists.
    web_view_side_panel_view_->OpenUrl(last_url_info_->url_params);
  }

  if (side_panel_ui->GetCurrentEntryId() !=
      SidePanelEntry::Id::kMerchantTrust) {
    side_panel_ui->Show(SidePanelEntry::Id::kMerchantTrust);
  }
}

std::unique_ptr<views::View>
MerchantTrustSidePanelCoordinator::CreateMerchantTrustWebView(
    SidePanelEntryScope& scope) {
  DCHECK(GetBrowserView());
  DCHECK(last_url_info_);
  if (registered_but_not_shown_) {
    // TODO(crbug.com/378818867): Add metrics for when the view is opened
    // directly from the side panel (the side panel is currently being shown)
    registered_but_not_shown_ = false;
  }

  auto side_panel_view_ = std::make_unique<WebViewSidePanelView>(
      web_contents(), /*loading_screen_url=*/kStaticLoadingScreenURL,
      kMerchantTrustContextParameterName);
  side_panel_view_->OpenUrl(last_url_info_->url_params);
  web_view_side_panel_view_ = side_panel_view_->AsWeakPtr();
  return side_panel_view_;
}

BrowserView* MerchantTrustSidePanelCoordinator::GetBrowserView() const {
  auto* browser = chrome::FindBrowserWithTab(web_contents());
  return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
}

SidePanelUI* MerchantTrustSidePanelCoordinator::GetSidePanelUI() {
  auto* browser = chrome::FindBrowserWithTab(web_contents());
  return browser ? browser->GetFeatures().side_panel_ui() : nullptr;
}

GURL MerchantTrustSidePanelCoordinator::GetOpenInNewTabUrl() {
  // TODO(crbug.com/378818867): TBD, need to figure out how to build the correct
  // URL here.
  return last_url_info_.value().new_tab_url;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MerchantTrustSidePanelCoordinator);
