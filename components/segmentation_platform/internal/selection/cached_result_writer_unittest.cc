// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/cached_result_writer.h"

#include "base/test/simple_test_clock.h"
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

namespace {

// Labels for BinaryClassifier.
const char kNotShowShare[] = "Not Show Share";
const char kShowShare[] = "Show Share";

// TTL for BinaryClassifier labels.
const int kShowShareTTL = 3;
const int kDefaultTTL = 5;

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

  writer.AddPredictedResultTTLInOutputConfig({{kShowShare, kShowShareTTL}},
                                             kDefaultTTL, proto::TimeUnit::DAY);

  return model_metadata.output_config();
}

}  // namespace

class CachedResultWriterTest : public testing::Test {
 public:
  CachedResultWriterTest() = default;
  ~CachedResultWriterTest() override = default;

  void SetUp() override {
    result_prefs_ = std::make_unique<ClientResultPrefs>(&pref_service_);
    client_result_prefs_ = result_prefs_.get();
    pref_service_.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                                 std::string());

    clock_.SetNow(base::Time::Now());
    cached_result_writer_ =
        std::make_unique<CachedResultWriter>(std::move(result_prefs_), &clock_);
  }

  proto::ClientResult CreateClientResult(std::vector<float> model_scores,
                                         base::Time result_timestamp) {
    proto::ClientResult client_result;
    proto::PredictionResult pred_result =
        metadata_utils::CreatePredictionResult(
            model_scores, GetTestOutputConfigForBinaryClassifier(),
            /*timestamp=*/base::Time::Now());
    client_result.mutable_client_result()->CopyFrom(pred_result);
    client_result.set_timestamp_us(
        result_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
    return client_result;
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClientResultPrefs> result_prefs_;
  raw_ptr<ClientResultPrefs> client_result_prefs_ = nullptr;
  std::unique_ptr<CachedResultWriter> cached_result_writer_;
  base::SimpleTestClock clock_;
};

TEST_F(CachedResultWriterTest, UpdatePrefsIfResultUnavailable) {
  std::unique_ptr<Config> config = CreateTestConfig();
  // Prefs doesn't have result for this client config.
  absl::optional<proto::ClientResult> client_result =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  EXPECT_FALSE(client_result.has_value());
  proto::ClientResult new_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());

  // Pref will be updated with new client result.
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(false));
  absl::optional<proto::ClientResult> result_from_pref =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(result_from_pref.has_value());
  EXPECT_EQ(new_client_result.SerializeAsString(),
            result_from_pref.value().SerializeAsString());
}

TEST_F(CachedResultWriterTest, UpdatePrefsIfForceRefreshResult) {
  std::unique_ptr<Config> config = CreateTestConfig();
  // Saving unexpired result for client in prefs.
  proto::ClientResult unexpired_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());
  client_result_prefs_->SaveClientResultToPrefs(config->segmentation_key,
                                                unexpired_client_result);

  proto::ClientResult new_client_result = CreateClientResult(
      /*model_scores=*/{0.4}, /*result_timestamp=*/base::Time::Now());

  // Pref result not updated as unexpired result.
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(false));
  absl::optional<proto::ClientResult> client_result =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(client_result.has_value());
  EXPECT_EQ(unexpired_client_result.SerializeAsString(),
            client_result.value().SerializeAsString());

  // Unexpired pref updates with new client result as force refresh result is
  // true.
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(true));
  client_result =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(client_result.has_value());
  EXPECT_EQ(new_client_result.SerializeAsString(),
            client_result.value().SerializeAsString());
}

TEST_F(CachedResultWriterTest, UpdatePrefsIfExpiredResult) {
  std::unique_ptr<Config> config = CreateTestConfig();
  // Saving expired result for client in prefs.
  client_result_prefs_->SaveClientResultToPrefs(
      config->segmentation_key,
      CreateClientResult(/*model_scores=*/{0.4},
                         /*result_timestamp=*/base::Time::Now() -
                             base::Days(kDefaultTTL + 3)));

  proto::ClientResult new_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());

  // Expired pref updates with new client result.
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(false));
  absl::optional<proto::ClientResult> result_from_pref =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(result_from_pref.has_value());
  EXPECT_EQ(new_client_result.SerializeAsString(),
            result_from_pref.value().SerializeAsString());
}

}  // namespace segmentation_platform