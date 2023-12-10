// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/index.h"

#include <optional>

#include "base/metrics/histogram_functions.h"

namespace ash::local_search_service {

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
    case IndexId::kHelpAppLauncher:
      return prefix + "HelpAppLauncher";
    case IndexId::kPersonalization:
      return prefix + "Personalization";
    case IndexId::kShortcutsApp:
      return prefix + "ShortcutsApp";
  }
}

void OnSearchPerformedDone(const std::string& histogram_string) {
  base::UmaHistogramBoolean(histogram_string + ".NumberSearchPerformedDone",
                            true);
}

}  // namespace

Index::Index(IndexId index_id, Backend backend) : index_id_(index_id) {
  histogram_prefix_ = IndexIdBasedHistogramPrefix(index_id);
  DCHECK(!histogram_prefix_.empty());
  LogIndexIdAndBackendType(histogram_prefix_, backend);
}

Index::~Index() = default;

void Index::BindReceiver(mojo::PendingReceiver<mojom::Index> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void Index::SetReporterRemote(
    mojo::PendingRemote<mojom::SearchMetricsReporter> reporter_remote) {
  DCHECK(!reporter_remote_.is_bound());
  reporter_remote_.Bind(std::move(reporter_remote));
}

void Index::SetSearchParams(const SearchParams& search_params,
                            SetSearchParamsCallback callback) {
  search_params_ = search_params;
  std::move(callback).Run();
}

void Index::MaybeLogSearchResultsStats(ResponseStatus status,
                                       size_t num_results,
                                       base::TimeDelta latency) {
  if (reporter_remote_.is_bound())
    reporter_remote_->OnSearchPerformed(
        index_id_, base::BindOnce(&OnSearchPerformedDone, histogram_prefix_));

  base::UmaHistogramEnumeration(histogram_prefix_ + ".ResponseStatus", status);
  if (status == ResponseStatus::kSuccess) {
    // Only logs number of results and latency if search is a success.
    base::UmaHistogramCounts100(histogram_prefix_ + ".NumberResults",
                                num_results);
    base::UmaHistogramTimes(histogram_prefix_ + ".SearchLatency", latency);
  }
}

void Index::MaybeLogIndexSize() {
  const uint32_t index_size = GetIndexSize();
  if (index_size != 0u) {
    base::UmaHistogramCounts10000(histogram_prefix_ + ".NumberDocuments",
                                  index_size);
  }
}

void Index::AddOrUpdateCallbackWithTime(AddOrUpdateCallback callback,
                                        const base::Time start_time) {
  const auto time_diff = base::Time::Now() - start_time;
  MaybeLogIndexSize();
  base::UmaHistogramTimes(histogram_prefix_ + ".AddOrUpdateLatency", time_diff);
  std::move(callback).Run();
}

void Index::DeleteCallbackWithTime(DeleteCallback callback,
                                   const base::Time start_time,
                                   const uint32_t num_deleted) {
  const auto time_diff = base::Time::Now() - start_time;
  base::UmaHistogramTimes(histogram_prefix_ + ".DeleteLatency", time_diff);
  MaybeLogIndexSize();
  std::move(callback).Run(num_deleted);
}

void Index::UpdateDocumentsCallbackWithTime(UpdateDocumentsCallback callback,
                                            const base::Time start_time,
                                            const uint32_t num_deleted) {
  const auto time_diff = base::Time::Now() - start_time;
  base::UmaHistogramTimes(histogram_prefix_ + ".UpdateDocumentsLatency",
                          time_diff);
  MaybeLogIndexSize();
  std::move(callback).Run(num_deleted);
}

void Index::ClearIndexCallbackWithTime(ClearIndexCallback callback,
                                       const base::Time start_time) {
  const auto time_diff = base::Time::Now() - start_time;
  base::UmaHistogramTimes(histogram_prefix_ + ".ClearIndexLatency", time_diff);
  std::move(callback).Run();
}

}  // namespace ash::local_search_service
