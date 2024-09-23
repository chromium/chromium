// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITED_URL_RANKING_DESKTOP_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_
#define CHROME_BROWSER_VISITED_URL_RANKING_DESKTOP_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"

class Profile;

namespace visited_url_ranking {

struct FetchOptions;

class DesktopTabModelURLVisitDataFetcher : public URLVisitDataFetcher {
 public:
  explicit DesktopTabModelURLVisitDataFetcher(Profile* profile);
  ~DesktopTabModelURLVisitDataFetcher() override;

  // Disallow copy/assign.
  DesktopTabModelURLVisitDataFetcher(
      const DesktopTabModelURLVisitDataFetcher&) = delete;
  DesktopTabModelURLVisitDataFetcher& operator=(
      const DesktopTabModelURLVisitDataFetcher&) = delete;

  // URLVisitDataFetcher:
  void FetchURLVisitData(const FetchOptions& options,
                         const FetcherConfig& config,
                         FetchResultCallback callback) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace visited_url_ranking

#endif  // CHROME_BROWSER_VISITED_URL_RANKING_DESKTOP_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_
