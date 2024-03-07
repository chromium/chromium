// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/cached_result_provider.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"
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

const char kClientKey[] = "test_key";

// Labels for MultiClassClassifier.
constexpr std::array<const char*, 5> kMultiClassLabels{
    "Vanilla", "Chocolate", "Strawberry", "Mango", "Peach"};

std::unique_ptr<Config> CreateTestConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kClientKey;
  config->segmentation_uma_name = "test_key";
  config->segment_selection_ttl = base::Days(28);
  config->unknown_selection_ttl = base::Days(14);
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  return config;
}

proto::OutputConfig GetTestOutputConfigForMultiClassClassifier() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);

  writer.AddOutputConfigForMultiClassClassifier(
      kMultiClassLabels,
      /*top_k_outputs=*/kMultiClassLabels.size(), /*threshold=*/0.1);

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

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClientResultPrefs> result_prefs_;
  std::unique_ptr<CachedResultProvider> cached_result_provider_;
  std::vector<std::unique_ptr<Config>> configs_;
};

TEST_F(CachedResultProviderTest, CachedResultProviderWithEmptyPrefs) {
  cached_result_provider_ =
      std::make_unique<CachedResultProvider>(result_prefs_.get(), configs_);
  std::optional<proto::PredictionResult> retrieved_prediction_result =
      cached_result_provider_->GetPredictionResultForClient(kClientKey);
  EXPECT_FALSE(retrieved_prediction_result);
}

TEST_F(CachedResultProviderTest,
       GetPredictionResultForClient_WithNonEmptyResult) {
  std::vector<float> model_scores = {0, 0, 1, 0, 0};
  proto::PredictionResult saved_prediction_result =
      metadata_utils::CreatePredictionResult(
          model_scores, GetTestOutputConfigForMultiClassClassifier(),
          /*timestamp=*/base::Time::Now(), /*model_version=*/1);
  result_prefs_->SaveClientResultToPrefs(
      kClientKey, metadata_utils::CreateClientResultFromPredResult(
                      saved_prediction_result,
                      /*timestamp=*/base::Time::Now()));

  cached_result_provider_ =
      std::make_unique<CachedResultProvider>(result_prefs_.get(), configs_);
  std::optional<proto::PredictionResult> retrieved_prediction_result =
      cached_result_provider_->GetPredictionResultForClient(kClientKey);
  EXPECT_TRUE(retrieved_prediction_result.has_value());
  EXPECT_EQ(saved_prediction_result.result_size(),
            retrieved_prediction_result.value().result_size());
  for (int i = 0; i < saved_prediction_result.result_size(); i++) {
    EXPECT_EQ(saved_prediction_result.result(i),
              retrieved_prediction_result.value().result(i));
  }
}

TEST_F(CachedResultProviderTest, GetPredictionResultForClient_WithNoResult) {
  cached_result_provider_ =
      std::make_unique<CachedResultProvider>(result_prefs_.get(), configs_);
  std::optional<proto::PredictionResult> retrieved_prediction_result =
      cached_result_provider_->GetPredictionResultForClient(kClientKey);
  EXPECT_FALSE(retrieved_prediction_result.has_value());
}

}  // namespace segmentation_platform
