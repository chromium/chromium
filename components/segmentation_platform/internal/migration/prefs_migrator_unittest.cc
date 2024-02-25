// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/prefs_migrator.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/migration/migration_test_utils.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {

class PrefsMigratorTest : public testing::Test {
 public:
  PrefsMigratorTest() = default;
  ~PrefsMigratorTest() override = default;

  void SetUp() override {
    old_result_prefs_ =
        std::make_unique<SegmentationResultPrefs>(&pref_service_);
    new_result_prefs_ = std::make_unique<ClientResultPrefs>(&pref_service_);
    pref_service_.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                                 std::string());
    pref_service_.registry()->RegisterDictionaryPref(kSegmentationResultPref);
    configs_.push_back(migration_test_utils::GetTestConfigForBinaryClassifier(
        kShoppingUserSegmentationKey, kShoppingUserUmaName,
        proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER));
    configs_.push_back(migration_test_utils::GetTestConfigForAdaptiveToolbar());
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClientResultPrefs> new_result_prefs_;
  std::unique_ptr<SegmentationResultPrefs> old_result_prefs_;
  std::unique_ptr<PrefsMigrator> prefs_migrator_;
  std::vector<std::unique_ptr<Config>> configs_;
};

TEST_F(PrefsMigratorTest, PrefsMigratorForBinaryClassifier) {
  // Model with binary classifier when `segment_id` is selected.
  SelectedSegment result(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER, 1);
  result.selection_time = base::Time::Now() - base::Seconds(1);
  old_result_prefs_->SaveSegmentationResultToPref(kShoppingUserSegmentationKey,
                                                  result);
  prefs_migrator_ = std::make_unique<PrefsMigrator>(
      &pref_service_, new_result_prefs_.get(), configs_);

  prefs_migrator_->MigrateOldPrefsToNewPrefs();

  auto expected_output_config =
      migration_test_utils::GetTestOutputConfigForBinaryClassifier(
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER);
  const auto* result_in_new_prefs =
      new_result_prefs_->ReadClientResultFromPrefs(
          kShoppingUserSegmentationKey);

  EXPECT_TRUE(result_in_new_prefs);
  proto::PredictionResult client_result = result_in_new_prefs->client_result();
  EXPECT_EQ(expected_output_config.SerializeAsString(),
            client_result.output_config().SerializeAsString());
  EXPECT_THAT(client_result.result(), testing::ElementsAre(1));

  new_result_prefs_->SaveClientResultToPrefs(kShoppingUserSegmentationKey,
                                             *result_in_new_prefs);

  // Models with binary classifier when `segment_id` is not selected.
  SelectedSegment result1(SegmentId::OPTIMIZATION_TARGET_UNKNOWN, 0);
  result1.selection_time = base::Time::Now();
  old_result_prefs_->SaveSegmentationResultToPref(kShoppingUserSegmentationKey,
                                                  result1);
  prefs_migrator_ = std::make_unique<PrefsMigrator>(
      &pref_service_, new_result_prefs_.get(), configs_);
  prefs_migrator_->MigrateOldPrefsToNewPrefs();

  result_in_new_prefs = new_result_prefs_->ReadClientResultFromPrefs(
      kShoppingUserSegmentationKey);
  EXPECT_TRUE(result_in_new_prefs);
  client_result = result_in_new_prefs->client_result();
  EXPECT_EQ(expected_output_config.SerializeAsString(),
            client_result.output_config().SerializeAsString());
  EXPECT_THAT(client_result.result(), testing::ElementsAre(0));
}

TEST_F(PrefsMigratorTest, PrefsMigratorForAdaptiveToolbar) {
  // AdpativeToolbar model when new tab is selected.
  SelectedSegment result(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
                         1);
  result.selection_time = base::Time::Now();
  old_result_prefs_->SaveSegmentationResultToPref(
      kAdaptiveToolbarSegmentationKey, result);
  prefs_migrator_ = std::make_unique<PrefsMigrator>(
      &pref_service_, new_result_prefs_.get(), configs_);

  prefs_migrator_->MigrateOldPrefsToNewPrefs();

  auto expected_output_config =
      migration_test_utils::GetTestOutputConfigForAdaptiveToolbar();
  const auto* result_in_new_prefs =
      new_result_prefs_->ReadClientResultFromPrefs(
          kAdaptiveToolbarSegmentationKey);

  EXPECT_TRUE(result_in_new_prefs);
  proto::PredictionResult client_result = result_in_new_prefs->client_result();
  EXPECT_EQ(expected_output_config.SerializeAsString(),
            client_result.output_config().SerializeAsString());
  EXPECT_THAT(client_result.result(), testing::ElementsAre(1, 0, 0, 0, 0));

  new_result_prefs_->SaveClientResultToPrefs(kAdaptiveToolbarSegmentationKey,
                                             *result_in_new_prefs);

  // AdpativeToolbar model when share is selected.
  SelectedSegment result1(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, 1);
  result1.selection_time = base::Time::Now();
  old_result_prefs_->SaveSegmentationResultToPref(
      kAdaptiveToolbarSegmentationKey, result1);
  prefs_migrator_ = std::make_unique<PrefsMigrator>(
      &pref_service_, new_result_prefs_.get(), configs_);

  prefs_migrator_->MigrateOldPrefsToNewPrefs();

  result_in_new_prefs = new_result_prefs_->ReadClientResultFromPrefs(
      kAdaptiveToolbarSegmentationKey);

  EXPECT_TRUE(result_in_new_prefs);
  client_result = result_in_new_prefs->client_result();
  EXPECT_EQ(expected_output_config.SerializeAsString(),
            client_result.output_config().SerializeAsString());
  EXPECT_THAT(client_result.result(), testing::ElementsAre(0, 1, 0, 0, 0));
}

TEST_F(PrefsMigratorTest, PrefsMigratorForOtherConfig) {
  SelectedSegment result(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER, 1);
  result.selection_time = base::Time::Now();
  old_result_prefs_->SaveSegmentationResultToPref(kSearchUserKey, result);
  prefs_migrator_ = std::make_unique<PrefsMigrator>(
      &pref_service_, new_result_prefs_.get(), configs_);

  prefs_migrator_->MigrateOldPrefsToNewPrefs();

  const auto* result_in_new_prefs =
      new_result_prefs_->ReadClientResultFromPrefs(kSearchUserKey);
  EXPECT_FALSE(result_in_new_prefs);
}

}  // namespace segmentation_platform