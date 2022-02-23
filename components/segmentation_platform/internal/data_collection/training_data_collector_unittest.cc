// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include "components/segmentation_platform/internal/execution/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/signals/mock_histogram_signal_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

class TrainingDataCollectorTest : public ::testing::Test {
 public:
  TrainingDataCollectorTest() = default;
  ~TrainingDataCollectorTest() override = default;

  void SetUp() override {
    collector_ = std::make_unique<TrainingDataCollector>(
        &feature_list_processor_, &histogram_signal_handler_);
  }

 protected:
  TrainingDataCollector* collector() { return collector_.get(); }

 private:
  MockFeatureListQueryProcessor feature_list_processor_;
  MockHistogramSignalHandler histogram_signal_handler_;
  std::unique_ptr<TrainingDataCollector> collector_;
};

// Place holder test case that will be replaced to test real implementation
// logic.
TEST_F(TrainingDataCollectorTest, Construction) {
  // TODO(xingliu): Remove this once read test cases are added.
  EXPECT_NE(nullptr, collector());
}

}  // namespace
}  // namespace segmentation_platform
