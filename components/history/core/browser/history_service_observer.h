// Copyright 2014 The Chromium Authors. All rights reserved.
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

  // Called when a new visit is added to History. This happens in two scenarios:
  //  1. User makes a new visit on the local device.
  //  2. Sync brings a visit from a different device onto the local device.
  //     Notably, this is called for each visit brought over.
  //
  // The values in `row` are set to what is cuurrently in the history database.
  //
  // TODO(crbug.com/1345274): We should either add a parameter to this method,
  // or wholly merge this with `OnURLsModified` before enabling Full History
  // Sync, as this will start to get called way more often.
  virtual void OnURLVisited(HistoryService* history_service,
                            ui::PageTransition transition,
                            const URLRow& row,
                            base::Time visit_time) {}

  // Called when a `URLRow` is modified without necessarily having a new visit.
  // This happens in these scenarios:
  //  1. When the Page Title is updated shortly after the page loads.
  //  2. When `TypedURLSyncBridge` updates the `URLRow` data. This often happens
  //     in addition to adding new visits, so `OnURLVisited` will be called too.
  //  3. When History expiration expires some, but not all visits related to
  //     a URL. In that case, the URL's metadata is updated.
  //
  // `changed_urls` lists the information for each of the URLs affected. The
  // rows will have the IDs that are currently in effect in the main history
  // database.
  //
  // TODO(crbug.com/1345274): The differences between this and `OnURLVisited`
  // are pretty deep into the implementation of History, and we may be forcing
  // observers to think too hard. Many observers simply map both calls to the
  // same on-changed code. Consider merging this with `OnURLVisited`.
  virtual void OnURLsModified(HistoryService* history_service,
                              const URLRows& changed_urls) {}

  // Called when one or more URLs are deleted.
  //
  // `deletion_info` describes the urls that have been removed from history.
  virtual void OnURLsDeleted(HistoryService* history_service,
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

  // Called when content model annotation is modified for a url.
  // `url_id` is the id of the url row.
  virtual void OnContentModelAnnotationModified(
      HistoryService* history_service,
      const URLRow& row,
      const VisitContentModelAnnotations& model_annotations) {}
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_
