// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites.h"

#include "base/observer_list.h"
#include "components/history/core/browser/top_sites_observer.h"

namespace history {

PrepopulatedPage::PrepopulatedPage() : favicon_id(-1), color() {}

PrepopulatedPage::PrepopulatedPage(const GURL& url,
                                   const std::u16string& title,
                                   int favicon_id,
                                   SkColor color)
    : most_visited(url, title), favicon_id(favicon_id), color(color) {}

TopSites::TopSites() = default;

TopSites::~TopSites() = default;

void TopSites::AddObserver(TopSitesObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TopSites::RemoveObserver(TopSitesObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TopSites::NotifyTopSitesLoaded() {
  for (TopSitesObserver& observer : observer_list_)
    observer.TopSitesLoaded(this);
}

void TopSites::NotifyTopSitesChanged(
    const TopSitesObserver::ChangeReason reason) {
  for (TopSitesObserver& observer : observer_list_)
    observer.TopSitesChanged(this, reason);
}

}  // namespace history
