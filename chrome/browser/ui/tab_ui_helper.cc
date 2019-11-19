// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_ui_helper.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/invalidate_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

base::string16 FormatUrlToSubdomain(const GURL& url) {
  base::string16 formated_url = url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitTrivialSubdomains,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  return base::UTF8ToUTF16(GURL(formated_url).host());
}

}  // namespace

TabUIHelper::TabUIData::TabUIData(const GURL& url)
    : title(FormatUrlToSubdomain(url)), favicon(favicon::GetDefaultFavicon()) {}

TabUIHelper::TabUIHelper(content::WebContents* contents)
    : WebContentsObserver(contents) {}
TabUIHelper::~TabUIHelper() {}

base::string16 TabUIHelper::GetTitle() const {
  const base::string16& contents_title = web_contents()->GetTitle();
  if (!contents_title.empty())
    return contents_title;

  if (tab_ui_data_)
    return tab_ui_data_->title;

#if defined(OS_MACOSX)
  return l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
#else
  return base::string16();
#endif
}

gfx::Image TabUIHelper::GetFavicon() const {
  if (ShouldUseFaviconFromHistory() && tab_ui_data_)
    return tab_ui_data_->favicon;
  return favicon::TabFaviconFromWebContents(web_contents());
}

bool TabUIHelper::ShouldHideThrobber() const {
  // Hiding throbber and using favicon from history is desired when a new
  // background tab's initial navigation is delayed, so the user has a way to
  // see what the tab is.
  if (ShouldUseFaviconFromHistory())
    return true;

  // We also want to hide a background tab's throbber during page load if it is
  // created by session restore. A restored tab's favicon is already fetched
  // by |SessionRestoreDelegate|.
  if (created_by_session_restore_ && !was_active_at_least_once_)
    return true;

  return false;
}

void TabUIHelper::NotifyInitialNavigationDelayed(bool is_navigation_delayed) {
  DCHECK(web_contents()->GetController().IsInitialNavigation());

  is_navigation_delayed_ = is_navigation_delayed;
  if (!is_navigation_delayed_)
    return;

  tab_ui_data_ = std::make_unique<TabUIData>(web_contents()->GetVisibleURL());
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);

  // When fetching favicon from history, we first try the exact URL, and then
  // fall back to the host.
  FetchFaviconFromHistory(web_contents()->GetVisibleURL(),
                          base::Bind(&TabUIHelper::OnURLFaviconFetched,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void TabUIHelper::DidStopLoading() {
  // Reset the properties after the initial navigation finishes loading, so that
  // latter navigations are not affected.
  is_navigation_delayed_ = false;
  created_by_session_restore_ = false;
  tab_ui_data_.reset();
}

bool TabUIHelper::ShouldUseFaviconFromHistory() const {
  return web_contents()->GetController().IsInitialNavigation() &&
         is_navigation_delayed_ && !was_active_at_least_once_;
}

void TabUIHelper::FetchFaviconFromHistory(
    const GURL& url,
    favicon_base::FaviconImageCallback callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  // |favicon_service| might be null when testing.
  if (favicon_service) {
    favicon_service->GetFaviconImageForPageURL(url, std::move(callback),
                                               &favicon_tracker_);
  }
}

void TabUIHelper::OnURLFaviconFetched(
    const favicon_base::FaviconImageResult& favicon) {
  if (!ShouldUseFaviconFromHistory())
    return;

  if (!favicon.image.IsEmpty()) {
    UpdateFavicon(favicon);
    return;
  }

  FetchFaviconFromHistory(web_contents()->GetVisibleURL().GetWithEmptyPath(),
                          base::Bind(&TabUIHelper::OnHostFaviconFetched,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void TabUIHelper::OnHostFaviconFetched(
    const favicon_base::FaviconImageResult& favicon) {
  if (!ShouldUseFaviconFromHistory())
    return;

  if (!favicon.image.IsEmpty())
    UpdateFavicon(favicon);
}

void TabUIHelper::UpdateFavicon(
    const favicon_base::FaviconImageResult& favicon) {
  if (tab_ui_data_) {
    tab_ui_data_->favicon = favicon.image;
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabUIHelper)
