// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_

#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_id.h"

namespace history {

class HistoryService;

// Used by components external to History to observe `HistoryService` and
// process tasks on the main thread.
//
// The notifications roughly correspond to the ones in `HistoryBackendObserver`,
// although there are some differences.
class HistoryServiceObserver {
 public:
  HistoryServiceObserver() = default;

  HistoryServiceObserver(const HistoryServiceObserver&) = delete;
  HistoryServiceObserver& operator=(const HistoryServiceObserver&) = delete;

  virtual ~HistoryServiceObserver() = default;

  // Called when a `new_visit` is added to History. This happens in two
  // scenarios:
  //  1. User makes a new visit on the local device.
  //  2. Sync brings a visit from a different device onto the local device.
  //     Notably, this is called for each visit brought over.
  //
  // The values in `url_row` and `new_visit` are set to what is currently in the
  // history database.
  virtual void OnURLVisited(HistoryService* history_service,
                            const URLRow& url_row,
                            const VisitRow& new_visit) {}

  // Same as above, but including the navigation_id from the underlying
  // `content::NavigationHandle`. Observers only need to override `OnURLVisited`
  // or `OnNavigationURLVisited`, but not both.
  virtual void OnURLVisitedWithNavigationId(
      HistoryService* history_service,
      const URLRow& url_row,
      const VisitRow& new_visit,
      std::optional<int64_t> local_navigation_id) {}

  // Called when a URL has a metadata-only update. In situations where a URL has
  // a metadata-only update AND new visits, both `OnURLsModified` and
  // `OnURLVisited` will be called. Therefore observers that only care about new
  // visits should only override `OnURLVisited`.
  //
  // These metadata-only updates happen in these scenarios:
  //  1. When the Page Title is updated shortly after the page loads.
  //  2. When History expiration expires some, but not all visits related to
  //     a URL. In that case, the URL's metadata is updated.
  //
  // `changed_urls` lists the information for each of the URLs affected. The
  // rows will have the IDs that are currently in effect in the main history
  // database.
  virtual void OnURLsModified(HistoryService* history_service,
                              const URLRows& changed_urls) {}

  // Called when one or more URLs and/or Visits are deleted.
  // `deletion_info` describes all the deletions that have occurred.
  virtual void OnHistoryDeletions(HistoryService* history_service,
                                  const DeletionInfo& deletion_info) {}

  // Is called to notify when `history_service` has finished loading.
  virtual void OnHistoryServiceLoaded(HistoryService* history_service) {}

  // Is called to notify when `history_service` is being deleted.
  virtual void HistoryServiceBeingDeleted(HistoryService* history_service) {}

  // Sent when a keyword search term is updated.
  //
  // `row` contains the URL information for search `term`.
  // `keyword_id` associated with a URL and search term.
  virtual void OnKeywordSearchTermUpdated(HistoryService* history_service,
                                          const URLRow& row,
                                          KeywordID keyword_id,
                                          const std::u16string& term) {}

  // Sent when a keyword search term is deleted.
  // `url_id` is the id of the url row.
  virtual void OnKeywordSearchTermDeleted(HistoryService* history_service,
                                          URLID url_id) {}
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_
