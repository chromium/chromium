// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_

#include "components/segmentation_platform/internal/database/segment_info_database.h"

namespace segmentation_platform {

namespace test {

// A fake database with sample entries that can be used for tests.
class TestSegmentInfoDatabase : public SegmentInfoDatabase {
 public:
  TestSegmentInfoDatabase();
  ~TestSegmentInfoDatabase() override;

  // SegmentInfoDatabase overrides.
  void GetAllSegmentInfo(AllSegmentInfoCallback callback) override;
  void SaveSegmentResult(OptimizationTarget segment_id,
                         proto::PredictionResult* result,
                         SuccessCallback callback) override;

  // Test helper methods.
  void AddUserAction(OptimizationTarget segment_id,
                     const std::string& user_action);
  void AddPredictionResult(OptimizationTarget segment_id,
                           float score,
                           base::Time timestamp);
  void AddDiscreteMapping(OptimizationTarget segment_id,
                          float mappings[][2],
                          int num_pairs);

  // Finds a segment with given |segment_id|. Creates one if it doesn't exist.
  proto::SegmentInfo* FindOrCreateSegment(OptimizationTarget segment_id);

 private:
  std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>> segment_infos_;
};

}  // namespace test

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_TEST_SEGMENT_INFO_DATABASE_H_
