// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_BACKEND_FOR_SYNC_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_BACKEND_FOR_SYNC_H_

#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"

namespace base {
class Time;
}

namespace history {

class HistoryBackendObserver;

// Interface that defines the subset of HistoryBackend that is required by
// HistorySyncBridge. This is a separate interface mainly for ease of testing.
// Look at HistoryBackend for comments about the individual methods.
class HistoryBackendForSync {
 public:
  virtual bool IsExpiredVisitTime(const base::Time& time) const = 0;

  virtual bool GetURLByID(URLID url_id, URLRow* url_row) = 0;
  virtual bool GetLastVisitByTime(base::Time visit_time,
                                  VisitRow* visit_row) = 0;
  virtual bool GetMostRecentVisitsForURL(URLID id,
                                         int max_visits,
                                         VisitVector* visits) = 0;
  virtual VisitVector GetRedirectChain(VisitRow visit) = 0;

  virtual bool GetForeignVisit(const std::string& originator_cache_guid,
                               VisitID originator_visit_id,
                               VisitRow* visit_row) = 0;

  virtual VisitID AddSyncedVisit(const GURL& url,
                                 const std::u16string& title,
                                 bool hidden,
                                 const VisitRow& visit) = 0;
  virtual VisitID UpdateSyncedVisit(const VisitRow& visit) = 0;
  virtual bool UpdateVisitReferrerOpenerIDs(VisitID visit_id,
                                            VisitID referrer_id,
                                            VisitID opener_id) = 0;

  virtual void AddObserver(HistoryBackendObserver* observer) = 0;
  virtual void RemoveObserver(HistoryBackendObserver* observer) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_BACKEND_FOR_SYNC_H_
