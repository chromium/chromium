// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;

namespace history {

// PrepopulatedPage stores information for prepopulated pages for the initial
// run.
struct PrepopulatedPage {
  PrepopulatedPage();
  PrepopulatedPage(const GURL& url,
                   const base::string16& title,
                   int favicon_id,
                   SkColor color);

  MostVisitedURL most_visited;  // The prepopulated page URL and title.
  int favicon_id;               // The raw data resource for the favicon.
  SkColor color;                // The best color to highlight the page, should
                                // roughly match the favicon.
};

typedef std::vector<PrepopulatedPage> PrepopulatedPageList;

// Interface for TopSites, which stores the data for the top "most visited"
// sites. This includes a cache of the most visited data from history.
//
// Some methods should only be called from the UI thread (see method
// descriptions below). All others are assumed to be threadsafe.
class TopSites : public RefcountedKeyedService {
 public:
  TopSites();

  typedef base::Callback<void(const MostVisitedURLList&)>
      GetMostVisitedURLsCallback;

  // Returns a list of most visited URLs via a callback. This may be invoked on
  // any thread. NOTE: The callback is called immediately if we have the data
  // cached. If data is not available yet, callback will later be posted to the
  // thread that called this function.
  virtual void GetMostVisitedURLs(
      const GetMostVisitedURLsCallback& callback) = 0;

  // Asks TopSites to refresh what it thinks the top sites are. This may do
  // nothing. Should be called from the UI thread.
  virtual void SyncWithHistory() = 0;

  // Blacklisted URLs

  // Returns true if there is at least one item in the blacklist.
  virtual bool HasBlacklistedItems() const = 0;

  // Add a URL to the blacklist. Should be called from the UI thread.
  virtual void AddBlacklistedURL(const GURL& url) = 0;

  // Removes a URL from the blacklist. Should be called from the UI thread.
  virtual void RemoveBlacklistedURL(const GURL& url) = 0;

  // Returns true if the URL is blacklisted. Should be called from the UI
  // thread.
  virtual bool IsBlacklisted(const GURL& url) = 0;

  // Clear the blacklist. Should be called from the UI thread.
  virtual void ClearBlacklistedURLs() = 0;

  // Returns true if the top sites list is full (i.e. we already have the
  // maximum number of top sites).  This function also returns false if TopSites
  // isn't loaded yet.
  virtual bool IsFull() = 0;

  virtual bool loaded() const = 0;

  // Returns the set of prepopulated pages.
  virtual PrepopulatedPageList GetPrepopulatedPages() = 0;

  // Called when user has navigated to |url|.
  virtual void OnNavigationCommitted(const GURL& url) = 0;

  // Add Observer to the list.
  void AddObserver(TopSitesObserver* observer);

  // Remove Observer from the list.
  void RemoveObserver(TopSitesObserver* observer);

 protected:
  void NotifyTopSitesLoaded();
  void NotifyTopSitesChanged(const TopSitesObserver::ChangeReason reason);
  ~TopSites() override;

 private:
  friend class base::RefCountedThreadSafe<TopSites>;

  base::ObserverList<TopSitesObserver, true>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(TopSites);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_H_
