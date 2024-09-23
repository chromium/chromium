// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VISITED_URL_RANKING_ANDROID_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_
#define CHROME_BROWSER_VISITED_URL_RANKING_ANDROID_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"

class Profile;

namespace visited_url_ranking {

struct FetchOptions;

class AndroidTabModelURLVisitDataFetcher : public URLVisitDataFetcher {
 public:
  explicit AndroidTabModelURLVisitDataFetcher(Profile* profile);
  ~AndroidTabModelURLVisitDataFetcher() override;

  // Disallow copy/assign.
  AndroidTabModelURLVisitDataFetcher(
      const AndroidTabModelURLVisitDataFetcher&) = delete;
  AndroidTabModelURLVisitDataFetcher& operator=(
      const AndroidTabModelURLVisitDataFetcher&) = delete;

  // URLVisitDataFetcher:
  void FetchURLVisitData(const FetchOptions& options,
                         const FetcherConfig& config,
                         FetchResultCallback callback) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace visited_url_ranking

#endif  // CHROME_BROWSER_VISITED_URL_RANKING_ANDROID_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_
