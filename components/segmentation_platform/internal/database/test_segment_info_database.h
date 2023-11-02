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
class TestSegmentInfoDatabase : public SegmentInfoDatabase {
 public:
  TestSegmentInfoDatabase();
  ~TestSegmentInfoDatabase() override;

  // SegmentInfoDatabase overrides.
  void Initialize(SuccessCallback callback) override;
  void GetSegmentInfoForSegments(const base::flat_set<SegmentId>& segment_ids,
                                 MultipleSegmentInfoCallback callback) override;
  void GetSegmentInfo(SegmentId segment_id,
                      SegmentInfoCallback callback) override;
  void UpdateSegment(SegmentId segment_id,
                     absl::optional<proto::SegmentInfo> segment_info,
                     SuccessCallback callback) override;
  void SaveSegmentResult(SegmentId segment_id,
                         absl::optional<proto::PredictionResult> result,
                         SuccessCallback callback) override;

  // Test helper methods.
  void AddUserActionFeature(SegmentId segment_id,
                            const std::string& user_action,
                            uint64_t bucket_count,
                            uint64_t tensor_length,
                            proto::Aggregation aggregation);
  void AddHistogramValueFeature(SegmentId segment_id,
                                const std::string& histogram,
                                uint64_t bucket_count,
                                uint64_t tensor_length,
                                proto::Aggregation aggregation);
  void AddHistogramEnumFeature(SegmentId segment_id,
                               const std::string& histogram_name,
                               uint64_t bucket_count,
                               uint64_t tensor_length,
                               proto::Aggregation aggregation,
                               const std::vector<int32_t>& accepted_enum_ids);
  void AddSqlFeature(SegmentId segment_id,
                     const MetadataWriter::SqlFeature& feature);
  void AddPredictionResult(SegmentId segment_id,
                           float score,
                           base::Time timestamp);
  void AddDiscreteMapping(SegmentId segment_id,
                          const float mappings[][2],
                          int num_pairs,
                          const std::string& discrete_mapping_key);
  void SetBucketDuration(SegmentId segment_id,
                         uint64_t bucket_duration,
                         proto::TimeUnit time_unit);

  // Finds a segment with given |segment_id|. Creates one if it doesn't exist.
  proto::SegmentInfo* FindOrCreateSegment(SegmentId segment_id);

 private:
  std::vector<std::pair<SegmentId, proto::SegmentInfo>> segment_infos_;
};

}  // namespace segmentation_platform::test

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_
