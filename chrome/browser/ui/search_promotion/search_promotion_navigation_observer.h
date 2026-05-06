// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_NAVIGATION_OBSERVER_H_

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class SearchPromotionManager;

namespace tabs {
class TabInterface;
}

class SearchPromotionNavigationObserver
    : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(SearchPromotionNavigationObserver);

  explicit SearchPromotionNavigationObserver(tabs::TabInterface& tab);
  SearchPromotionNavigationObserver(const SearchPromotionNavigationObserver&) =
      delete;
  SearchPromotionNavigationObserver& operator=(
      const SearchPromotionNavigationObserver&) = delete;

  ~SearchPromotionNavigationObserver() override;

  static SearchPromotionNavigationObserver* From(tabs::TabInterface* tab);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  SearchPromotionManager* GetManager();

  ui::ScopedUnownedUserData<SearchPromotionNavigationObserver>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_NAVIGATION_OBSERVER_H_
