// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/test_segment_info_database.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/metrics/metrics_hashes.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform::test {

namespace {
void AddFeature(proto::SegmentInfo* segment_info,
                proto::SignalType signal_type,
                const std::string& name,
                uint64_t bucket_count,
                uint64_t tensor_length,
                proto::Aggregation aggregation,
                const std::vector<int32_t>& accepted_enum_ids) {
  proto::SegmentationModelMetadata* metadata =
      segment_info->mutable_model_metadata();
  proto::InputFeature* input = metadata->add_input_features();
  proto::UMAFeature* feature = input->mutable_uma_feature();
  feature->set_type(signal_type);
  feature->set_name(name);
  feature->set_name_hash(base::HashMetricName(name));
  feature->set_bucket_count(bucket_count);
  feature->set_tensor_length(tensor_length);
  feature->set_aggregation(aggregation);

  for (int32_t accepted_enum_id : accepted_enum_ids)
    feature->add_enum_ids(accepted_enum_id);
}
}  // namespace

TestSegmentInfoDatabase::TestSegmentInfoDatabase()
    : SegmentInfoDatabase(nullptr) {}

TestSegmentInfoDatabase::~TestSegmentInfoDatabase() = default;

void TestSegmentInfoDatabase::Initialize(SuccessCallback callback) {
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::GetAllSegmentInfo(
    MultipleSegmentInfoCallback callback) {
  std::move(callback).Run(
      std::make_unique<SegmentInfoDatabase::SegmentInfoList>(segment_infos_));
}

void TestSegmentInfoDatabase::GetSegmentInfoForSegments(
    const std::vector<OptimizationTarget>& segment_ids,
    MultipleSegmentInfoCallback callback) {
  auto result = std::make_unique<SegmentInfoDatabase::SegmentInfoList>();
  for (const auto& pair : segment_infos_) {
    if (base::Contains(segment_ids, pair.first))
      result->emplace_back(pair);
  }
  std::move(callback).Run(std::move(result));
}

void TestSegmentInfoDatabase::GetSegmentInfo(OptimizationTarget segment_id,
                                             SegmentInfoCallback callback) {
  auto result = std::find_if(
      segment_infos_.begin(), segment_infos_.end(),
      [segment_id](std::pair<OptimizationTarget, proto::SegmentInfo> pair) {
        return pair.first == segment_id;
      });

  std::move(callback).Run(result == segment_infos_.end()
                              ? absl::nullopt
                              : absl::make_optional(result->second));
}

void TestSegmentInfoDatabase::UpdateSegment(
    OptimizationTarget segment_id,
    absl::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  if (segment_info.has_value()) {
    proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
    info->CopyFrom(segment_info.value());
  } else {
    // Delete the segment.
    auto new_end = std::remove_if(
        segment_infos_.begin(), segment_infos_.end(),
        [segment_id](
            const std::pair<OptimizationTarget, proto::SegmentInfo>& pair) {
          return pair.first == segment_id;
        });
    segment_infos_.erase(new_end, segment_infos_.end());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::SaveSegmentResult(
    OptimizationTarget segment_id,
    absl::optional<proto::PredictionResult> result,
    SuccessCallback callback) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  if (!result.has_value()) {
    info->clear_prediction_result();
  } else {
    auto* mutable_result = info->mutable_prediction_result();
    mutable_result->set_result(result->result());
    mutable_result->set_timestamp_us(result->timestamp_us());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::AddUserActionFeature(
    OptimizationTarget segment_id,
    const std::string& name,
    uint64_t bucket_count,
    uint64_t tensor_length,
    proto::Aggregation aggregation) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  AddFeature(info, proto::SignalType::USER_ACTION, name, bucket_count,
             tensor_length, aggregation, {});
}

void TestSegmentInfoDatabase::AddHistogramValueFeature(
    OptimizationTarget segment_id,
    const std::string& name,
    uint64_t bucket_count,
    uint64_t tensor_length,
    proto::Aggregation aggregation) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  AddFeature(info, proto::SignalType::HISTOGRAM_VALUE, name, bucket_count,
             tensor_length, aggregation, {});
}

void TestSegmentInfoDatabase::AddHistogramEnumFeature(
    OptimizationTarget segment_id,
    const std::string& name,
    uint64_t bucket_count,
    uint64_t tensor_length,
    proto::Aggregation aggregation,
    const std::vector<int32_t>& accepted_enum_ids) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  AddFeature(info, proto::SignalType::HISTOGRAM_ENUM, name, bucket_count,
             tensor_length, aggregation, accepted_enum_ids);
}

void TestSegmentInfoDatabase::AddSqlFeature(OptimizationTarget segment_id,
                                            const std::string& sql,
                                            const UkmConfig& event_config) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  auto* metadata = info->mutable_model_metadata();
  proto::SqlFeature* feature =
      metadata->add_input_features()->mutable_sql_feature();
  feature->set_sql(sql);
  for (const auto& event_it : event_config.metrics_for_event_for_testing()) {
    auto* ukm_event = feature->mutable_signal_filter()->add_ukm_events();
    const UkmEventHash event_hash = event_it.first;
    ukm_event->set_event_hash(event_hash.GetUnsafeValue());
    const base::flat_set<UkmMetricHash>& metrics = event_it.second;
    for (const auto& metric : metrics)
      ukm_event->mutable_metric_hash_filter()->Add(metric.GetUnsafeValue());
  }
}

void TestSegmentInfoDatabase::AddPredictionResult(OptimizationTarget segment_id,
                                                  float score,
                                                  base::Time timestamp) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  proto::PredictionResult* result = info->mutable_prediction_result();
  result->set_result(score);
  result->set_timestamp_us(
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void TestSegmentInfoDatabase::AddDiscreteMapping(
    OptimizationTarget segment_id,
    float mappings[][2],
    int num_pairs,
    const std::string& discrete_mapping_key) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  auto* discrete_mappings_map =
      info->mutable_model_metadata()->mutable_discrete_mappings();
  auto& discrete_mappings = (*discrete_mappings_map)[discrete_mapping_key];
  for (int i = 0; i < num_pairs; i++) {
    auto* pair = mappings[i];
    auto* entry = discrete_mappings.add_entries();
    entry->set_min_result(pair[0]);
    entry->set_rank(pair[1]);
  }
}

void TestSegmentInfoDatabase::SetBucketDuration(OptimizationTarget segment_id,
                                                uint64_t bucket_duration,
                                                proto::TimeUnit time_unit) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  info->mutable_model_metadata()->set_bucket_duration(bucket_duration);
  info->mutable_model_metadata()->set_time_unit(time_unit);
}

proto::SegmentInfo* TestSegmentInfoDatabase::FindOrCreateSegment(
    OptimizationTarget segment_id) {
  proto::SegmentInfo* info = nullptr;
  for (auto& pair : segment_infos_) {
    if (pair.first == segment_id) {
      info = &pair.second;
      break;
    }
  }

  if (info == nullptr) {
    segment_infos_.emplace_back(segment_id, proto::SegmentInfo());
    info = &segment_infos_.back().second;
    info->set_segment_id(segment_id);
  }

  return info;
}

}  // namespace segmentation_platform::test
