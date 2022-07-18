// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TEST_HISTORY_BACKEND_FOR_SYNC_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TEST_HISTORY_BACKEND_FOR_SYNC_H_

#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/sync/history_backend_for_sync.h"
#include "components/history/core/browser/url_row.h"

namespace history {

// A simple in-memory implementation of HistoryBackendForSync for use in tests.
class TestHistoryBackendForSync : public HistoryBackendForSync {
 public:
  static constexpr base::TimeDelta kExpiryThreshold = base::Days(7);

  TestHistoryBackendForSync();
  ~TestHistoryBackendForSync();

  // Methods to manipulate the contents. These do *not* notify the observers.
  URLID AddURL(URLRow row);
  VisitID AddVisit(VisitRow row);
  bool UpdateVisit(VisitRow row);

  void RemoveURLAndVisits(URLID url_id);
  void Clear();

  // Direct access to the contents.
  const std::vector<URLRow>& GetURLs() const;
  const std::vector<VisitRow>& GetVisits() const;
  const URLRow* FindURLRow(const GURL& url) const;

  // HistoryBackendForSync implementation.
  bool IsExpiredVisitTime(const base::Time& time) const override;
  bool GetURLByID(URLID url_id, URLRow* url_row) override;
  bool GetLastVisitByTime(base::Time visit_time, VisitRow* visit_row) override;
  bool GetMostRecentVisitsForURL(URLID id,
                                 int max_visits,
                                 VisitVector* visits) override;
  VisitVector GetRedirectChain(VisitRow visit) override;
  bool GetForeignVisit(const std::string& originator_cache_guid,
                       VisitID originator_visit_id,
                       VisitRow* visit_row) override;
  VisitID AddSyncedVisit(const GURL& url,
                         const std::u16string& title,
                         bool hidden,
                         const VisitRow& visit) override;
  VisitID UpdateSyncedVisit(const VisitRow& visit) override;
  bool UpdateVisitReferrerOpenerIDs(VisitID visit_id,
                                    VisitID referrer_id,
                                    VisitID opener_id) override;
  void AddObserver(HistoryBackendObserver* observer) override;
  void RemoveObserver(HistoryBackendObserver* observer) override;

  int get_foreign_visit_call_count() const {
    return get_foreign_visit_call_count_;
  }

 private:
  bool FindVisit(VisitID id, VisitRow* result);

  const URLRow& FindOrAddURL(const GURL& url,
                             const std::u16string& title,
                             bool hidden);

  std::vector<URLRow> urls_;
  URLID next_url_id_ = 1;
  std::vector<VisitRow> visits_;
  VisitID next_visit_id_ = 1;

  int get_foreign_visit_call_count_ = 0;

  base::ObserverList<HistoryBackendObserver, true>::Unchecked observers_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TEST_HISTORY_BACKEND_FOR_SYNC_H_
