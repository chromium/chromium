// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_OBSERVER_H_

#include "components/history/core/browser/history_types.h"

namespace history {

class HistoryBackend;

// Used by internal History components to observe `HistoryBackend` and process
// those notifications on the backend task runner.
//
// Classes external to History that wish to observe History should instead use
// `HistoryServiceObserver`, which operates on the main thread.
//
// These notifications are kept roughly in sync with `HistoryServiceObserver`,
// but there's already not an exact 1-to-1 correspondence.
class HistoryBackendObserver {
 public:
  HistoryBackendObserver() = default;

  HistoryBackendObserver(const HistoryBackendObserver&) = delete;
  HistoryBackendObserver& operator=(const HistoryBackendObserver&) = delete;

  virtual ~HistoryBackendObserver() = default;

  // Called when the user visits an URL.
  //
  // The row IDs will be set to the values that are currently in effect in the
  // main history database.
  virtual void OnURLVisited(HistoryBackend* history_backend,
                            const URLRow& url_row,
                            const VisitRow& visit_row) = 0;

  // Called when a URL has been added or modified.
  //
  // `changed_urls` lists the information for each of the URLs affected. The
  // rows will have the IDs that are currently in effect in the main history
  // database. `is_from_expiration` is true if the modification is caused by
  // automatic history expiration (the visit count got reduced by expiring some
  // of the visits); it is false if the modification is caused by user action.
  virtual void OnURLsModified(HistoryBackend* history_backend,
                              const URLRows& changed_urls,
                              bool is_from_expiration) = 0;

  // Called when one or more of URLs are deleted.
  //
  // `all_history` is set to true, if all the URLs are deleted.
  //               When set to true, `deleted_rows` and `favicon_urls` are
  //               undefined.
  // `expired` is set to true, if the URL deletion is due to expiration.
  // `deleted_rows` list of the deleted URLs.
  // `favicon_urls` list of favicon URLs that correspond to the deleted URLs.
  virtual void OnHistoryDeletions(HistoryBackend* history_backend,
                                  bool all_history,
                                  bool expired,
                                  const URLRows& deleted_rows,
                                  const std::set<GURL>& favicon_urls) = 0;

  // Called when a visit, or some of its annotations, are updated. `reason`
  // specifies what specifically was updated.
  virtual void OnVisitUpdated(const VisitRow& visit,
                              VisitUpdateReason reason) = 0;

  // Called when a visit is deleted - usually either due to expiry, or because
  // the user explicitly deleted it.
  virtual void OnVisitDeleted(const VisitRow& visit) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_OBSERVER_H_
