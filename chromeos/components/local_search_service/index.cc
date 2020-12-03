// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/index.h"

#include "base/metrics/histogram_functions.h"
#include "base/optional.h"

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

void OnSearchPerformedDone() {
  // TODO(thanhdng): add a histogram to log this.
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

void Index::MaybeLogSearchResultsStats(ResponseStatus status,
                                       size_t num_results,
                                       base::TimeDelta latency) {
  if (reporter_remote_.is_bound())
    reporter_remote_->OnSearchPerformed(index_id_,
                                        base::BindOnce(&OnSearchPerformedDone));

  base::UmaHistogramEnumeration(histogram_prefix_ + ".ResponseStatus", status);
  if (status == ResponseStatus::kSuccess) {
    // Only logs number of results and latency if search is a success.
    base::UmaHistogramCounts100(histogram_prefix_ + ".NumberResults",
                                num_results);
    base::UmaHistogramTimes(histogram_prefix_ + ".SearchLatency", latency);
  }
}

void Index::MaybeLogIndexSize(uint64_t index_size) {
  if (index_size != 0u) {
    base::UmaHistogramCounts10000(histogram_prefix_ + ".NumberDocuments",
                                  index_size);
  }
}

}  // namespace local_search_service
}  // namespace chromeos
