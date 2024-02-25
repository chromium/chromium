// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_

#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::test {

// A fake database with sample entries that can be used for tests.
// TODO(b/285912101) : Remove this class and migrate its callers to used mock
// version of SegmentInfoDatabase.
class TestSegmentInfoDatabase : public SegmentInfoDatabase {
 public:
  TestSegmentInfoDatabase();
  ~TestSegmentInfoDatabase() override;

  // SegmentInfoDatabase overrides.
  void Initialize(SuccessCallback callback) override;
  void GetSegmentInfoForSegments(const base::flat_set<SegmentId>& segment_ids,
                                 MultipleSegmentInfoCallback callback) override;
  std::unique_ptr<SegmentInfoDatabase::SegmentInfoList>
  GetSegmentInfoForBothModels(
      const base::flat_set<SegmentId>& segment_ids) override;
  const SegmentInfo* GetCachedSegmentInfo(SegmentId segment_id,
                                          ModelSource model_source) override;
  void UpdateSegment(SegmentId segment_id,
                     ModelSource model_score,
                     std::optional<proto::SegmentInfo> segment_info,
                     SuccessCallback callback) override;
  void SaveSegmentResult(SegmentId segment_id,
                         ModelSource model_source,
                         std::optional<proto::PredictionResult> result,
                         SuccessCallback callback) override;
  void SaveTrainingData(SegmentId segment_id,
                        ModelSource model_source,
                        const proto::TrainingData& data,
                        SuccessCallback callback) override;
  void GetTrainingData(SegmentId segment_id,
                       ModelSource model_source,
                       TrainingRequestId request_id,
                       bool delete_from_db,
                       TrainingDataCallback callback) override;

  // Test helper methods.
  void AddUserActionFeature(
      SegmentId segment_id,
      const std::string& user_action,
      uint64_t bucket_count,
      uint64_t tensor_length,
      proto::Aggregation aggregation,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);
  void AddHistogramValueFeature(
      SegmentId segment_id,
      const std::string& histogram,
      uint64_t bucket_count,
      uint64_t tensor_length,
      proto::Aggregation aggregation,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);
  void AddHistogramEnumFeature(
      SegmentId segment_id,
      const std::string& histogram_name,
      uint64_t bucket_count,
      uint64_t tensor_length,
      proto::Aggregation aggregation,
      const std::vector<int32_t>& accepted_enum_ids,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);
  void AddSqlFeature(
      SegmentId segment_id,
      const MetadataWriter::SqlFeature& feature,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);
  void AddPredictionResult(
      SegmentId segment_id,
      float score,
      base::Time timestamp,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);
  void AddDiscreteMapping(
      SegmentId segment_id,
      const float mappings[][2],
      int num_pairs,
      const std::string& discrete_mapping_key,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);
  void SetBucketDuration(
      SegmentId segment_id,
      uint64_t bucket_duration,
      proto::TimeUnit time_unit,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);

  // Finds a segment with given |segment_id| and |model_source|. Creates one if
  // it doesn't exists. By default the |model_source| corresponds to server
  // model.
  proto::SegmentInfo* FindOrCreateSegment(
      SegmentId segment_id,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE);

 private:
  std::vector<std::pair<SegmentId, proto::SegmentInfo>> segment_infos_;
};

}  // namespace segmentation_platform::test

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_
