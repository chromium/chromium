// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include <cstdint>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/metrics_hashes.h"
#include "base/observer_list.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"

namespace segmentation_platform {

HistogramSignalHandler::HistogramSignalHandler(const std::string& profile_id,
                                               SignalDatabase* signal_database,
                                               UkmDatabase* ukm_db)
    : profile_id_(profile_id),
      db_(signal_database),
      ukm_db_(ukm_db),
      metrics_enabled_(false) {}

HistogramSignalHandler::~HistogramSignalHandler() {
  DCHECK(observers_.empty());
}

void HistogramSignalHandler::SetRelevantHistograms(
    const RelevantHistograms& histograms) {
  auto it = histogram_observers_.begin();
  while (it != histogram_observers_.end()) {
    if (!base::Contains(histograms, it->first)) {
      it = histogram_observers_.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto& pair : histograms) {
    if (base::Contains(histogram_observers_, pair)) {
      continue;
    }
    const auto& histogram_name = pair.first;
    proto::SignalType signal_type = pair.second;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindRepeating(&HistogramSignalHandler::OnHistogramSample,
                            weak_ptr_factory_.GetWeakPtr(), signal_type));
    histogram_observers_[pair] = std::move(histogram_observer);
  }
}

void HistogramSignalHandler::EnableMetrics(bool enable_metrics) {
  if (metrics_enabled_ == enable_metrics)
    return;

  metrics_enabled_ = enable_metrics;
}

void HistogramSignalHandler::OnHistogramSample(
    proto::SignalType signal_type,
    const char* histogram_name,
    uint64_t name_hash,
    base::HistogramBase::Sample sample) {
  if (!metrics_enabled_)
    return;

  db_->WriteSample(signal_type, name_hash, sample,
                   base::BindOnce(&HistogramSignalHandler::OnSampleWritten,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::string(histogram_name), sample));
  if (ukm_db_) {
    ukm_db_->AddUmaMetric(profile_id_, UmaMetricEntry{.type = signal_type,
                                                      .name_hash = name_hash,
                                                      .time = base::Time::Now(),
                                                      .value = sample});
  }
}

void HistogramSignalHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HistogramSignalHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HistogramSignalHandler::OnSampleWritten(const std::string& histogram_name,
                                             base::HistogramBase::Sample sample,
                                             bool success) {
  if (!success)
    return;

  for (Observer& ob : observers_)
    ob.OnHistogramSignalUpdated(histogram_name, sample);
}

}  // namespace segmentation_platform
