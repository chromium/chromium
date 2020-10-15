// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/index_sync.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace local_search_service {

namespace {

void LogIndexIdAndBackendType(const std::string& histogram_prefix,
                              Backend backend) {
  base::UmaHistogramEnumeration(histogram_prefix + ".Backend", backend);
}

std::string IndexIdBasedHistogramPrefix(IndexId index_id) {
  const std::string prefix = "LocalSearchService.";
  switch (index_id) {
    case IndexId::kCrosSettings:
      return prefix + "CrosSettings";
    case IndexId::kHelpApp:
      return prefix + "HelpApp";
  }
}

}  // namespace
IndexSync::IndexSync(IndexId index_id,
                     Backend backend,
                     PrefService* local_state) {
  histogram_prefix_ = IndexIdBasedHistogramPrefix(index_id);
  DCHECK(!histogram_prefix_.empty());
  LogIndexIdAndBackendType(histogram_prefix_, backend);

  // TODO(jiameng): consider enforcing this to be non-nullable.
  if (!local_state) {
    return;
  }

  reporter_ = std::make_unique<SearchMetricsReporterSync>(local_state);
  DCHECK(reporter_);
  reporter_->SetIndexId(index_id);
}

IndexSync::~IndexSync() = default;

void IndexSync::MaybeLogSearchResultsStats(ResponseStatus status,
                                           size_t num_results,
                                           base::TimeDelta latency) {
  if (reporter_)
    reporter_->OnSearchPerformed();

  base::UmaHistogramEnumeration(histogram_prefix_ + ".ResponseStatus", status);
  if (status == ResponseStatus::kSuccess) {
    // Only logs number of results and latency if search is a success.
    base::UmaHistogramCounts100(histogram_prefix_ + ".NumberResults",
                                num_results);
    base::UmaHistogramTimes(histogram_prefix_ + ".SearchLatency", latency);
  }
}

void IndexSync::MaybeLogIndexSize() {
  const uint64_t index_size = GetSizeSync();
  if (index_size != 0u) {
    base::UmaHistogramCounts10000(histogram_prefix_ + ".NumberDocuments",
                                  index_size);
  }
}

void IndexSync::SetSearchParams(const SearchParams& search_params) {
  search_params_ = search_params;
}

SearchParams IndexSync::GetSearchParamsForTesting() {
  return search_params_;
}

}  // namespace local_search_service
}  // namespace chromeos
