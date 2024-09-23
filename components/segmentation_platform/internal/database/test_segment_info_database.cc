// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/segmentation_platform/internal/database/test_segment_info_database.h"

#include <optional>

#include "base/containers/contains.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

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
    if (pair.second.model_source() != ModelSource::DEFAULT_MODEL_SOURCE &&
        base::Contains(segment_ids, pair.first)) {
      result->emplace_back(pair.first, &pair.second);
    }
  }
  std::move(callback).Run(std::move(result));
}

std::unique_ptr<SegmentInfoDatabase::SegmentInfoList>
TestSegmentInfoDatabase::GetSegmentInfoForBothModels(
    const base::flat_set<SegmentId>& segment_ids) {
  auto result = std::make_unique<SegmentInfoDatabase::SegmentInfoList>();
  for (const auto& pair : segment_infos_) {
    if (base::Contains(segment_ids, pair.first)) {
      result->emplace_back(pair.first, &pair.second);
    }
  }
  return result;
}

const SegmentInfo* TestSegmentInfoDatabase::GetCachedSegmentInfo(
    SegmentId segment_id,
    ModelSource model_source) {
  for (const auto& pair : segment_infos_) {
    if (segment_id == pair.first &&
        model_source == pair.second.model_source()) {
      return &pair.second;
    }
  }
  return nullptr;
}

void TestSegmentInfoDatabase::UpdateSegment(
    SegmentId segment_id,
    ModelSource model_source,
    std::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  if (segment_info.has_value()) {
    proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
    info->CopyFrom(segment_info.value());
  } else {
    // Delete the segment.
    auto new_end = std::remove_if(
        segment_infos_.begin(), segment_infos_.end(),
        [segment_id,
         model_source](const std::pair<SegmentId, proto::SegmentInfo>& pair) {
          return (pair.first == segment_id &&
                  pair.second.model_source() == model_source);
        });
    segment_infos_.erase(new_end, segment_infos_.end());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::SaveSegmentResult(
    SegmentId segment_id,
    ModelSource model_source,
    std::optional<proto::PredictionResult> result,
    SuccessCallback callback) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
  if (!result.has_value()) {
    info->clear_prediction_result();
  } else {
    info->mutable_prediction_result()->Swap(&result.value());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::SaveTrainingData(SegmentId segment_id,
                                               ModelSource model_source,
                                               const proto::TrainingData& data,
                                               SuccessCallback callback) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
  info->add_training_data()->CopyFrom(data);
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::GetTrainingData(SegmentId segment_id,
                                              ModelSource model_source,
                                              TrainingRequestId request_id,
                                              bool delete_from_db,
                                              TrainingDataCallback callback) {
  // TODO(ritikagup) : Try replacing it with GetCachedSegmentInfo.
  proto::SegmentInfo* segment_info = nullptr;
  for (auto& pair : segment_infos_) {
    if (pair.first == segment_id &&
        pair.second.model_source() == model_source) {
      segment_info = &pair.second;
      break;
    }
  }

  std::optional<proto::TrainingData> result;
  if (segment_info == nullptr) {
    std::move(callback).Run(result);
    return;
  }
  for (int i = 0; i < segment_info->training_data_size(); i++) {
    if (segment_info->training_data(i).request_id() ==
        request_id.GetUnsafeValue()) {
      result = segment_info->training_data(i);
      if (delete_from_db) {
        segment_info->mutable_training_data()->DeleteSubrange(i, 1);
      }
      break;
    }
  }
  std::move(callback).Run(result);
}

void TestSegmentInfoDatabase::AddUserActionFeature(
    SegmentId segment_id,
    const std::string& name,
    uint64_t bucket_count,
    uint64_t tensor_length,
    proto::Aggregation aggregation,
    ModelSource model_source) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
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
    proto::Aggregation aggregation,
    ModelSource model_source) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
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
    const std::vector<int32_t>& accepted_enum_ids,
    ModelSource model_source) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
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
    const MetadataWriter::SqlFeature& feature,
    ModelSource model_source) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
  MetadataWriter writer(info->mutable_model_metadata());
  writer.AddSqlFeature(feature);
}

void TestSegmentInfoDatabase::AddPredictionResult(SegmentId segment_id,
                                                  float score,
                                                  base::Time timestamp,
                                                  ModelSource model_source) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
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
    const std::string& discrete_mapping_key,
    ModelSource model_source) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
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
                                                proto::TimeUnit time_unit,
                                                ModelSource model_source) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id, model_source);
  info->mutable_model_metadata()->set_bucket_duration(bucket_duration);
  info->mutable_model_metadata()->set_time_unit(time_unit);
}

proto::SegmentInfo* TestSegmentInfoDatabase::FindOrCreateSegment(
    SegmentId segment_id,
    ModelSource model_source) {
  // TODO(ritikagup) : Try replacing it with GetCachedSegmentInfo.
  proto::SegmentInfo* info = nullptr;
  for (auto& pair : segment_infos_) {
    if (pair.first == segment_id &&
        pair.second.model_source() == model_source) {
      info = &pair.second;
      break;
    }
  }

  if (info == nullptr) {
    segment_infos_.emplace_back(segment_id, proto::SegmentInfo());
    info = &segment_infos_.back().second;
    info->set_segment_id(segment_id);
    info->set_model_source(model_source);
  }

  return info;
}

}  // namespace segmentation_platform::test
