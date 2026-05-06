// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_navigation_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"
#include "chrome/browser/ui/search_promotion/search_promotion_manager_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

DEFINE_USER_DATA(SearchPromotionNavigationObserver);

SearchPromotionNavigationObserver::SearchPromotionNavigationObserver(
    tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

SearchPromotionNavigationObserver::~SearchPromotionNavigationObserver() =
    default;

// static
SearchPromotionNavigationObserver* SearchPromotionNavigationObserver::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

void SearchPromotionNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only operate on successful, committed, non-same-document navigations in the
  // main frame.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsErrorPage() || navigation_handle->IsSameDocument()) {
    return;
  }

  // Only trigger promotional logic for Google Search URLs.
  if (!google_util::IsGoogleSearchUrl(navigation_handle->GetURL())) {
    return;
  }

  SearchPromotionManager* manager = GetManager();
  if (manager) {
    manager->OnTargetURLVisited(navigation_handle->GetURL());
  }
}

SearchPromotionManager* SearchPromotionNavigationObserver::GetManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return SearchPromotionManagerFactory::GetForProfile(profile);
}
