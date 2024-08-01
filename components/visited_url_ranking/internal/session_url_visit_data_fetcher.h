// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_SESSION_URL_VISIT_DATA_FETCHER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_SESSION_URL_VISIT_DATA_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"

namespace visited_url_ranking {

// Fetches and computes aggregated URL visit data from the synced sessions
// service.
class SessionURLVisitDataFetcher : public URLVisitDataFetcher {
 public:
  explicit SessionURLVisitDataFetcher(
      sync_sessions::SessionSyncService* session_sync_service);
  SessionURLVisitDataFetcher(const SessionURLVisitDataFetcher&) = delete;
  ~SessionURLVisitDataFetcher() override;

  // URLVisitDataFetcher:
  void FetchURLVisitData(const FetchOptions& options,
                         const FetcherConfig& config,
                         FetchResultCallback callback) override;

 private:
  const raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_SESSION_URL_VISIT_DATA_FETCHER_H_
