// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/lom_recorder.h"

#include <limits>
#include <utility>

#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"

namespace metrics::private_metrics {

LomRecorder::LomRecorder() = default;

LomRecorder::~LomRecorder() = default;

// static
LomRecorder* LomRecorder::Get() {
  static base::NoDestructor<LomRecorder> recorder;
  return recorder.get();
}

void LomRecorder::RecordBoolean(PumaType puma_type,
                                std::string_view name,
                                bool sample,
                                std::optional<uint64_t> profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // kRc is currently the only valid PUMA type as all other types are
  // deprecated. Additionally, Private UMA (PUMA) for RC (Regulatory Compliance)
  // is being renamed to LOM (Legal Obligation Metrics).
  if (puma_type != PumaType::kRc) {
    return;
  }
  RecordSample(name, sample ? 1 : 0, 1, 2, profile_id);
}

void LomRecorder::RecordExactLinear(PumaType puma_type,
                                    std::string_view name,
                                    int sample,
                                    int exclusive_max,
                                    std::optional<uint64_t> profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // kRc is currently the only valid PUMA type as all other types are
  // deprecated.
  // Additionally, Private UMA (PUMA) for RC (Regulatory Compliance) is being
  // renamed to LOM (Legal Obligation Metrics).
  if (puma_type != PumaType::kRc) {
    return;
  }
  RecordSample(name, sample, 1, exclusive_max, profile_id);
}

std::vector<::private_metrics::ProfileKeyedHistogramEvent>
LomRecorder::TakeHistogramEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  absl::flat_hash_map<uint64_t, ::private_metrics::ProfileKeyedHistogramEvent>
      profile_events_map;

  for (auto& [_, profile_map] : histograms_) {
    for (auto& [profile_id, histogram_event] : profile_map) {
      auto [it, inserted] = profile_events_map.try_emplace(profile_id);
      if (inserted) {
        it->second.set_profile_id(profile_id);
      }
      *it->second.add_histogram_events() = std::move(histogram_event);
    }
  }
  histograms_.clear();

  std::vector<::private_metrics::ProfileKeyedHistogramEvent> result;
  result.reserve(profile_events_map.size());
  for (auto& [_, profile_event] : profile_events_map) {
    result.push_back(std::move(profile_event));
  }
  return result;
}

void LomRecorder::RecordSample(std::string_view name,
                               int64_t sample,
                               int64_t min,
                               int64_t max,
                               std::optional<uint64_t> profile_id) {
  uint64_t histogram_hash = base::HashMetricName(name);
  uint64_t profile_id_key = profile_id.value_or(0u);

  auto [it, inserted] = histograms_[histogram_hash].try_emplace(profile_id_key);
  metrics::HistogramEventProto& histogram_event = it->second;

  if (inserted) {
    histogram_event.set_name_hash(histogram_hash);
  }

  histogram_event.set_sum(histogram_event.sum() + sample);

  int64_t bucket_min = sample;
  int64_t bucket_max = sample + 1;
  if (sample < min) {
    bucket_min = std::numeric_limits<int64_t>::min();
    bucket_max = min;
  } else if (sample >= max) {
    bucket_min = max;
    bucket_max = std::numeric_limits<int64_t>::max();
  }

  metrics::HistogramEventProto::Bucket* target_bucket = nullptr;
  for (int i = 0; i < histogram_event.bucket_size(); ++i) {
    auto* bucket = histogram_event.mutable_bucket(i);
    if (bucket->min() == bucket_min && bucket->max() == bucket_max) {
      target_bucket = bucket;
      break;
    }
  }

  if (target_bucket) {
    target_bucket->set_count(target_bucket->count() + 1);
  } else {
    target_bucket = histogram_event.add_bucket();
    target_bucket->set_min(bucket_min);
    target_bucket->set_max(bucket_max);
    target_bucket->set_count(1);
  }
}

}  // namespace metrics::private_metrics
