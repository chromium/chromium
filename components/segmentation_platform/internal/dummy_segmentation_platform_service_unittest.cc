// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/dummy_segmentation_platform_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class DummySegmentationPlatformServiceTest : public testing::Test {
 public:
  DummySegmentationPlatformServiceTest() = default;
  ~DummySegmentationPlatformServiceTest() override = default;

  void SetUp() override {
    segmentation_platform_service_ =
        std::make_unique<DummySegmentationPlatformService>();
  }

  void OnGetSelectedSegment(base::RepeatingClosure closure,
                            const SegmentSelectionResult& expected,
                            const SegmentSelectionResult& actual) {
    ASSERT_EQ(expected, actual);
    std::move(closure).Run();
  }

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 const ClassificationResult& result) {
    ASSERT_EQ(result.status, PredictionStatus::kFailed);
    EXPECT_TRUE(result.ordered_labels.empty());
    std::move(closure).Run();
  }

  void OnGetAnnotatedNumericResult(base::RepeatingClosure closure,
                                   const AnnotatedNumericResult& result) {
    ASSERT_EQ(result.status, PredictionStatus::kFailed);
    EXPECT_TRUE(result.result.result().empty());
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DummySegmentationPlatformService>
      segmentation_platform_service_;
};

TEST_F(DummySegmentationPlatformServiceTest, GetSelectedSegment) {
  SegmentSelectionResult expected;
  base::RunLoop loop;
  segmentation_platform_service_->GetSelectedSegment(
      "test_key",
      base::BindOnce(
          &DummySegmentationPlatformServiceTest::OnGetSelectedSegment,
          base::Unretained(this), loop.QuitClosure(), expected));
  loop.Run();
  ASSERT_EQ(expected,
            segmentation_platform_service_->GetCachedSegmentResult("test_key"));
}

TEST_F(DummySegmentationPlatformServiceTest, GetClassificationResult) {
  base::RunLoop loop;
  PredictionOptions options;
  segmentation_platform_service_->GetClassificationResult(
      "test_key", options, nullptr,
      base::BindOnce(
          &DummySegmentationPlatformServiceTest::OnGetClassificationResult,
          base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(DummySegmentationPlatformServiceTest, GetAnnotatedNumericResult) {
  base::RunLoop loop;
  PredictionOptions options;
  segmentation_platform_service_->GetAnnotatedNumericResult(
      "test_key", options, nullptr,
      base::BindOnce(
          &DummySegmentationPlatformServiceTest::OnGetAnnotatedNumericResult,
          base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

}  // namespace segmentation_platform
