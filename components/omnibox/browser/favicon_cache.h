// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAVICON_CACHE_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAVICON_CACHE_H_

#include <list>
#include <map>

#include "base/callback_list.h"
#include "base/containers/lru_cache.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"

namespace favicon {
class FaviconService;
}

namespace gfx {
class Image;
}

class GURL;

typedef base::OnceCallback<void(const gfx::Image& favicon)>
    FaviconFetchedCallback;

// This caches favicons by both page URL and icon URL. We cache a small number
// of them so we can synchronously deliver them to the UI to prevent flicker as
// the user types.
//
// This class also observes the HistoryService, and invalidates cached favicons
// and null responses when matching favicons are updated.
class FaviconCache : public history::HistoryServiceObserver {
 public:
  FaviconCache(favicon::FaviconService* favicon_service,
               history::HistoryService* history_service);
  ~FaviconCache() override;
  FaviconCache(const FaviconCache&) = delete;
  FaviconCache& operator=(const FaviconCache&) = delete;

  // These methods fetch favicons by the |page_url| or |icon_url| respectively.
  // If the correct favicon is already cached, these methods return the image
  // synchronously.
  //
  // If the correct favicon is not cached, we return an empty gfx::Image and
  // forward the request to FaviconService. |on_favicon_fetched| is stored in a
  // pending callback list, and subsequent identical requests are added to the
  // same pending list without issuing duplicate calls to FaviconService.
  //
  // If FaviconService responds with a non-empty image, we fulfill all the
  // matching |on_favicon_fetched| callbacks in the pending list, and cache the
  // result so that future matching requests can be fulfilled synchronously.
  //
  // If FaviconService responds with an empty image (because the correct favicon
  // isn't in our database), we simply erase all the pending callbacks, and also
  // cache the result.
  //
  // Therefore, |on_favicon_fetched| may or may not be called asynchronously
  // later, but will never be called with an empty result. It will also never
  // be called synchronously.
  //
  // Note that GetFaviconForPageUrl and GetLargestFaviconForPageUrl should not
  // be used interchangeably. These methods use the same |page_url| key for
  // caching favicons and as a result may return favicons with the wrong size if
  // called with the same |page_url|.
  gfx::Image GetFaviconForPageUrl(const GURL& page_url,
                                  FaviconFetchedCallback on_favicon_fetched);
  gfx::Image GetLargestFaviconForPageUrl(
      const GURL& page_url,
      FaviconFetchedCallback on_favicon_fetched);
  gfx::Image GetFaviconForIconUrl(const GURL& icon_url,
                                  FaviconFetchedCallback on_favicon_fetched);

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ClearIconsWithHistoryDeletions);
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ExpireNullFaviconsByHistory);
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ObserveFaviconsChanged);

  enum class RequestType {
    BY_PAGE_URL,
    BY_ICON_URL,
    RAW_BY_PAGE_URL,
  };

  struct Request {
    RequestType type;
    GURL url;

    // This operator is defined to support using Request as a key of std::map.
    bool operator<(const Request& rhs) const;
  };

  // Internal method backing GetFaviconForPageUrl and GetFaviconForIconUrl.
  gfx::Image GetFaviconInternal(const Request& request,
                                FaviconFetchedCallback on_favicon_fetched);

  // These are the callbacks passed to the underlying FaviconService. When these
  // are called, all the pending requests that match |request| will be called.
  void OnFaviconFetched(const Request& request,
                        const favicon_base::FaviconImageResult& result);
  void OnFaviconRawBitmapFetched(
      const Request& request,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Invokes all the pending requests that match |request| with |image|.
  void InvokeRequestCallbackWithFavicon(const Request& request,
                                        const gfx::Image& image);

  // Removes cached favicons and null responses that match |request| from the
  // cache. Subsequent matching requests pull fresh data from FaviconService.
  void InvalidateCachedRequests(const Request& request);

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void OnFaviconsChanged(const std::set<GURL>& page_urls, const GURL& icon_url);

  // Non-owning pointer to a KeyedService.
  raw_ptr<favicon::FaviconService> favicon_service_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};

  base::CancelableTaskTracker task_tracker_;
  std::map<Request, std::list<FaviconFetchedCallback>> pending_requests_;

  base::LRUCache<Request, gfx::Image> lru_cache_;

  // Keep responses with empty favicons in a separate list, to prevent a
  // response with an empty favicon from ever evicting an existing favicon.
  // The value is always set to true and has no meaning.
  base::LRUCache<Request, bool> responses_without_favicons_;

  // Subscription for notifications of changes to favicons.
  base::CallbackListSubscription favicons_changed_subscription_;

  base::WeakPtrFactory<FaviconCache> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAVICON_CACHE_H_
