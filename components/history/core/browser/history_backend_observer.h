// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_OBSERVER_H_

#include "base/macros.h"
#include "components/history/core/browser/history_types.h"

namespace history {

class HistoryBackend;

class HistoryBackendObserver {
 public:
  HistoryBackendObserver() {}
  virtual ~HistoryBackendObserver() {}

  // Called when user visits an URL.
  //
  // The `row` ID will be set to the value that is currently in effect in the
  // main history database. `redirects` is the list of redirects leading up to
  // the URL. If we have a redirect chain A -> B -> C and user is visiting C,
  // then `redirects[0]=B` and `redirects[1]=A`. If there are no redirects,
  // `redirects` is an empty vector.
  virtual void OnURLVisited(HistoryBackend* history_backend,
                            ui::PageTransition transition,
                            const URLRow& row,
                            const RedirectList& redirects,
                            base::Time visit_time) = 0;

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
  virtual void OnURLsDeleted(HistoryBackend* history_backend,
                             bool all_history,
                             bool expired,
                             const URLRows& deleted_rows,
                             const std::set<GURL>& favicon_urls) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryBackendObserver);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_OBSERVER_H_
