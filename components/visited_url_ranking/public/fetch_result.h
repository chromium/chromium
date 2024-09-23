// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_RESULT_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_RESULT_H_

#include <map>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

// A URL based identifier derived from a merging and deduplication strategy
// which helps match related URL visit data.
using URLMergeKey = std::string;

struct FetchResult {
  enum class Status { kError = 0, kSuccess = 1 };

  using URLVisitVariant = URLVisitAggregate::URLVisitVariant;

  FetchResult(Status status_arg,
              std::map<URLMergeKey, URLVisitVariant> data_arg);
  FetchResult(const FetchResult&) = delete;
  FetchResult(FetchResult&& other);
  FetchResult& operator=(FetchResult&& other);
  ~FetchResult();

  // Captures the fetch operation's outcome, being either success or failure.
  Status status;

  // URL visit data tied to a specific URL key and used for computing `URLVisit`
  // entries.
  std::map<URLMergeKey, URLVisitVariant> data;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_RESULT_H_
