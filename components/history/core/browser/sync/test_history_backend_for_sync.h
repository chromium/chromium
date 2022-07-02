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

  URLID AddURL(URLRow row);
  VisitID AddVisit(VisitRow row);

  const std::vector<URLRow>& GetURLs() const;
  const std::vector<VisitRow>& GetVisits() const;

  // HistoryBackendForSync implementation.
  bool IsExpiredVisitTime(const base::Time& time) const override;
  VisitID AddSyncedVisit(const GURL& url,
                         const std::u16string& title,
                         bool hidden,
                         const VisitRow& visit) override;
  bool UpdateSyncedVisit(const VisitRow& visit) override;

 private:
  const URLRow& FindOrAddURL(const GURL& url,
                             const std::u16string& title,
                             bool hidden);

  std::vector<URLRow> urls_;
  URLID next_url_id_ = 1;
  std::vector<VisitRow> visits_;
  VisitID next_visit_id_ = 1;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TEST_HISTORY_BACKEND_FOR_SYNC_H_
