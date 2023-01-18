// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/cached_result_provider.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/selection/client_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {

namespace proto {
class SegmentInfo;
}  // namespace proto

namespace {

// Labels for BinaryClassifier.
const char kNotShowShare[] = "Not Show Share";
const char kShowShare[] = "Show Share";

const char kClientKey[] = "test_key";

std::unique_ptr<Config> CreateTestConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kClientKey;
  config->segmentation_uma_name = "test_key";
  config->segment_selection_ttl = base::Days(28);
  config->unknown_selection_ttl = base::Days(14);
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  return config;
}

proto::OutputConfig GetTestOutputConfigForBinaryClassifier() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);

  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5, /*positive_label=*/kShowShare,
      /*negative_label=*/kNotShowShare);

  return model_metadata.output_config();
}

}  // namespace

class CachedResultProviderTest : public testing::Test {
 public:
  CachedResultProviderTest() = default;
  ~CachedResultProviderTest() override = default;

  void SetUp() override {
    result_prefs_ = std::make_unique<ClientResultPrefs>(&pref_service_);
    pref_service_.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                                 std::string());

    configs_.push_back(CreateTestConfig());
  }

  proto::ClientResult CreateClientResult(proto::PredictionResult pred_result) {
    proto::ClientResult client_result;
    client_result.mutable_client_result()->CopyFrom(pred_result);
    client_result.set_timestamp_us(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
    return client_result;
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClientResultPrefs> result_prefs_;
  std::unique_ptr<CachedResultProvider> cached_result_provider_;
  std::vector<std::unique_ptr<Config>> configs_;
};

TEST_F(CachedResultProviderTest, CachedResultProviderWithNonEmptyPredResult) {
  result_prefs_->SaveClientResultToPrefs(
      kClientKey,
      CreateClientResult(metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.8}, GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now())));
  cached_result_provider_ = std::make_unique<CachedResultProvider>(
      std::move(result_prefs_), configs_);
  ClassificationResult classification_result =
      cached_result_provider_->GetCachedResultForClient(kClientKey);
  EXPECT_THAT(classification_result.status, PredictionStatus::kSucceeded);
  EXPECT_THAT(classification_result.ordered_labels,
              testing::ElementsAre(kShowShare));
}

TEST_F(CachedResultProviderTest, CachedResultProviderWithEmptyPredResult) {
  proto::PredictionResult pred_result;
  result_prefs_->SaveClientResultToPrefs(kClientKey,
                                         CreateClientResult(pred_result));
  cached_result_provider_ = std::make_unique<CachedResultProvider>(
      std::move(result_prefs_), configs_);
  ClassificationResult classification_result =
      cached_result_provider_->GetCachedResultForClient(kClientKey);
  EXPECT_THAT(classification_result.status, PredictionStatus::kFailed);
  EXPECT_TRUE(classification_result.ordered_labels.empty());
}

}  // namespace segmentation_platform