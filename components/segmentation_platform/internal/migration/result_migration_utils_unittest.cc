// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/result_migration_utils.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/migration/migration_test_utils.h"
#include "components/segmentation_platform/public/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ResultMigrationUtilsTest : public testing::Test {
 public:
  ~ResultMigrationUtilsTest() override = default;
};

TEST_F(ResultMigrationUtilsTest, CreateClientResultForAdaptiveToolbar) {
  std::unique_ptr<Config> config =
      migration_test_utils::GetTestConfigForAdaptiveToolbar();
  SelectedSegment result(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE, 1);
  result.selection_time = base::Time::Now();

  proto::ClientResult client_result =
      pref_migration_utils::CreateClientResultFromOldResult(config.get(),
                                                            result);

  proto::OutputConfig expected_output_config =
      migration_test_utils::GetTestOutputConfigForAdaptiveToolbar();

  proto::PredictionResult pred_result = client_result.client_result();
  EXPECT_EQ(expected_output_config.SerializeAsString(),
            pred_result.output_config().SerializeAsString());
  EXPECT_THAT(pred_result.result(), testing::ElementsAre(0, 0, 1, 0, 0));
}

TEST_F(ResultMigrationUtilsTest, CreateClientResultForBinaryClassifier) {
  std::unique_ptr<Config> config =
      migration_test_utils::GetTestConfigForBinaryClassifier(
          kShoppingUserSegmentationKey, kShoppingUserUmaName,
          SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER);
  SelectedSegment result(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER, 1);
  result.selection_time = base::Time::Now();

  proto::ClientResult client_result =
      pref_migration_utils::CreateClientResultFromOldResult(config.get(),
                                                            result);

  proto::OutputConfig expected_output_config =
      migration_test_utils::GetTestOutputConfigForBinaryClassifier(
          SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER);

  proto::PredictionResult pred_result = client_result.client_result();
  EXPECT_EQ(expected_output_config.SerializeAsString(),
            pred_result.output_config().SerializeAsString());
  EXPECT_THAT(pred_result.result(), testing::ElementsAre(1));
}

}  // namespace segmentation_platform
