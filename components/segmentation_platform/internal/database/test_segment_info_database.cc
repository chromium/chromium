// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/test_segment_info_database.h"

#include "base/containers/contains.h"
#include "base/metrics/metrics_hashes.h"
#include "base/ranges/algorithm.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform::test {

TestSegmentInfoDatabase::TestSegmentInfoDatabase()
    : SegmentInfoDatabase(nullptr, nullptr) {}

TestSegmentInfoDatabase::~TestSegmentInfoDatabase() = default;

void TestSegmentInfoDatabase::Initialize(SuccessCallback callback) {
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::GetSegmentInfoForSegments(
    const base::flat_set<SegmentId>& segment_ids,
    MultipleSegmentInfoCallback callback) {
  auto result = std::make_unique<SegmentInfoDatabase::SegmentInfoList>();
  for (const auto& pair : segment_infos_) {
    if (base::Contains(segment_ids, pair.first))
      result->emplace_back(pair);
  }
  std::move(callback).Run(std::move(result));
}

void TestSegmentInfoDatabase::GetSegmentInfo(SegmentId segment_id,
                                             SegmentInfoCallback callback) {
  auto result =
      base::ranges::find(segment_infos_, segment_id,
                         &std::pair<SegmentId, proto::SegmentInfo>::first);

  std::move(callback).Run(result == segment_infos_.end()
                              ? absl::nullopt
                              : absl::make_optional(result->second));
}

void TestSegmentInfoDatabase::UpdateSegment(
    SegmentId segment_id,
    absl::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  if (segment_info.has_value()) {
    proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
    info->CopyFrom(segment_info.value());
  } else {
    // Delete the segment.
    auto new_end = std::remove_if(
        segment_infos_.begin(), segment_infos_.end(),
        [segment_id](const std::pair<SegmentId, proto::SegmentInfo>& pair) {
          return pair.first == segment_id;
        });
    segment_infos_.erase(new_end, segment_infos_.end());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::SaveSegmentResult(
    SegmentId segment_id,
    absl::optional<proto::PredictionResult> result,
    SuccessCallback callback) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  if (!result.has_value()) {
    info->clear_prediction_result();
  } else {
    info->mutable_prediction_result()->Swap(&result.value());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::SaveTrainingData(SegmentId segment_id,
                                               const proto::TrainingData& data,
                                               SuccessCallback callback) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  info->add_training_data()->CopyFrom(data);
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::GetTrainingData(SegmentId segment_id,
                                              TrainingRequestId request_id,
                                              bool delete_from_db,
                                              TrainingDataCallback callback) {
  auto segment_info =
      base::ranges::find(segment_infos_, segment_id,
                         &std::pair<SegmentId, proto::SegmentInfo>::first);

  absl::optional<proto::TrainingData> result;
  if (segment_info != segment_infos_.end()) {
    for (int i = 0; i < segment_info->second.training_data_size(); i++) {
      if (segment_info->second.training_data(i).request_id() ==
          request_id.GetUnsafeValue()) {
        result = segment_info->second.training_data(i);
        if (delete_from_db) {
          segment_info->second.mutable_training_data()->DeleteSubrange(i, 1);
        }
        break;
      }
    }
  }
  std::move(callback).Run(result);
}

void TestSegmentInfoDatabase::AddUserActionFeature(
    SegmentId segment_id,
    const std::string& name,
    uint64_t bucket_count,
    uint64_t tensor_length,
    proto::Aggregation aggregation) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  MetadataWriter writer(info->mutable_model_metadata());
  MetadataWriter::UMAFeature feature{
      .signal_type = proto::SignalType::USER_ACTION,
      .name = name.c_str(),
      .bucket_count = bucket_count,
      .tensor_length = tensor_length,
      .aggregation = aggregation,
      .accepted_enum_ids = nullptr};
  MetadataWriter::UMAFeature features[] = {feature};
  writer.AddUmaFeatures(features, 1);
}

void TestSegmentInfoDatabase::AddHistogramValueFeature(
    SegmentId segment_id,
    const std::string& name,
    uint64_t bucket_count,
    uint64_t tensor_length,
    proto::Aggregation aggregation) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  MetadataWriter writer(info->mutable_model_metadata());
  MetadataWriter::UMAFeature feature{
      .signal_type = proto::SignalType::HISTOGRAM_VALUE,
      .name = name.c_str(),
      .bucket_count = bucket_count,
      .tensor_length = tensor_length,
      .aggregation = aggregation,
      .accepted_enum_ids = nullptr};
  MetadataWriter::UMAFeature features[] = {feature};
  writer.AddUmaFeatures(features, 1);
}

void TestSegmentInfoDatabase::AddHistogramEnumFeature(
    SegmentId segment_id,
    const std::string& name,
    uint64_t bucket_count,
    uint64_t tensor_length,
    proto::Aggregation aggregation,
    const std::vector<int32_t>& accepted_enum_ids) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  MetadataWriter writer(info->mutable_model_metadata());
  MetadataWriter::UMAFeature feature{
      .signal_type = proto::SignalType::HISTOGRAM_ENUM,
      .name = name.c_str(),
      .bucket_count = bucket_count,
      .tensor_length = tensor_length,
      .aggregation = aggregation,
      .enum_ids_size = accepted_enum_ids.size(),
      .accepted_enum_ids = accepted_enum_ids.data()};
  MetadataWriter::UMAFeature features[] = {feature};
  writer.AddUmaFeatures(features, 1);
}

void TestSegmentInfoDatabase::AddSqlFeature(
    SegmentId segment_id,
    const MetadataWriter::SqlFeature& feature) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  MetadataWriter writer(info->mutable_model_metadata());
  MetadataWriter::SqlFeature features[] = {feature};
  writer.AddSqlFeatures(features, 1);
}

void TestSegmentInfoDatabase::AddPredictionResult(SegmentId segment_id,
                                                  float score,
                                                  base::Time timestamp) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  proto::PredictionResult* result = info->mutable_prediction_result();
  result->clear_result();
  result->add_result(score);
  result->set_timestamp_us(
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void TestSegmentInfoDatabase::AddDiscreteMapping(
    SegmentId segment_id,
    const float mappings[][2],
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

void TestSegmentInfoDatabase::SetBucketDuration(SegmentId segment_id,
                                                uint64_t bucket_duration,
                                                proto::TimeUnit time_unit) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  info->mutable_model_metadata()->set_bucket_duration(bucket_duration);
  info->mutable_model_metadata()->set_time_unit(time_unit);
}

proto::SegmentInfo* TestSegmentInfoDatabase::FindOrCreateSegment(
    SegmentId segment_id) {
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
