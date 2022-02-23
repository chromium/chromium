// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_

#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"

namespace segmentation_platform {

class UkmConfig;

namespace test {

// A fake database with sample entries that can be used for tests.
class TestSegmentInfoDatabase : public SegmentInfoDatabase {
 public:
  TestSegmentInfoDatabase();
  ~TestSegmentInfoDatabase() override;

  // SegmentInfoDatabase overrides.
  void Initialize(SuccessCallback callback) override;
  void GetAllSegmentInfo(MultipleSegmentInfoCallback callback) override;
  void GetSegmentInfoForSegments(
      const std::vector<OptimizationTarget>& segment_ids,
      MultipleSegmentInfoCallback callback) override;
  void GetSegmentInfo(OptimizationTarget segment_id,
                      SegmentInfoCallback callback) override;
  void UpdateSegment(OptimizationTarget segment_id,
                     absl::optional<proto::SegmentInfo> segment_info,
                     SuccessCallback callback) override;
  void SaveSegmentResult(OptimizationTarget segment_id,
                         absl::optional<proto::PredictionResult> result,
                         SuccessCallback callback) override;

  // Test helper methods.
  void AddUserActionFeature(OptimizationTarget segment_id,
                            const std::string& user_action,
                            uint64_t bucket_count,
                            uint64_t tensor_length,
                            proto::Aggregation aggregation);
  void AddHistogramValueFeature(OptimizationTarget segment_id,
                                const std::string& histogram,
                                uint64_t bucket_count,
                                uint64_t tensor_length,
                                proto::Aggregation aggregation);
  void AddHistogramEnumFeature(OptimizationTarget segment_id,
                               const std::string& histogram_name,
                               uint64_t bucket_count,
                               uint64_t tensor_length,
                               proto::Aggregation aggregation,
                               const std::vector<int32_t>& accepted_enum_ids);
  void AddSqlFeature(OptimizationTarget segment_id,
                     const std::string& sql,
                     const UkmConfig& ukm_config);
  void AddPredictionResult(OptimizationTarget segment_id,
                           float score,
                           base::Time timestamp);
  void AddDiscreteMapping(OptimizationTarget segment_id,
                          float mappings[][2],
                          int num_pairs,
                          const std::string& discrete_mapping_key);
  void SetBucketDuration(OptimizationTarget segment_id,
                         uint64_t bucket_duration,
                         proto::TimeUnit time_unit);

  // Finds a segment with given |segment_id|. Creates one if it doesn't exist.
  proto::SegmentInfo* FindOrCreateSegment(OptimizationTarget segment_id);

 private:
  std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>> segment_infos_;
};

}  // namespace test

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_
