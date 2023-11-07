// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/cached_result_writer.h"

#include "base/test/simple_test_clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/public/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {

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
    cached_result_writer_ = std::make_unique<CachedResultWriter>(
        client_result_prefs_.get(), &clock_);
  }

  proto::ClientResult CreateClientResult(
      std::vector<float> model_scores,
      base::Time result_timestamp,
      int64_t model_version = 1,
      bool ignore_previous_model_ttl = false) {
    proto::PredictionResult pred_result =
        metadata_utils::CreatePredictionResult(
            model_scores,
            test_utils::GetTestOutputConfigForBinaryClassifier(
                ignore_previous_model_ttl),
            /*timestamp=*/base::Time::Now(), model_version);
    return metadata_utils::CreateClientResultFromPredResult(pred_result,
                                                            result_timestamp);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClientResultPrefs> result_prefs_;
  raw_ptr<ClientResultPrefs, DanglingUntriaged> client_result_prefs_ = nullptr;
  std::unique_ptr<CachedResultWriter> cached_result_writer_;
  base::SimpleTestClock clock_;
};

TEST_F(CachedResultWriterTest, UpdatePrefsIfResultUnavailable) {
  std::unique_ptr<Config> config = test_utils::CreateTestConfig();
  // Prefs doesn't have result for this client config.
  const proto::ClientResult* client_result =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  EXPECT_FALSE(client_result);
  proto::ClientResult new_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());

  // Pref will be updated with new client result.
  bool is_prefs_updated = cached_result_writer_->UpdatePrefsIfExpired(
      config.get(), new_client_result, PlatformOptions(false));
  EXPECT_TRUE(is_prefs_updated);
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(false));
  const proto::ClientResult* result_from_pref =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(result_from_pref);
  EXPECT_EQ(new_client_result.SerializeAsString(),
            result_from_pref->SerializeAsString());
}

TEST_F(CachedResultWriterTest, UpdatePrefsIfForceRefreshResult) {
  std::unique_ptr<Config> config = test_utils::CreateTestConfig();
  // Saving unexpired result for client in prefs.
  proto::ClientResult unexpired_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());
  client_result_prefs_->SaveClientResultToPrefs(config->segmentation_key,
                                                unexpired_client_result);

  proto::ClientResult new_client_result = CreateClientResult(
      /*model_scores=*/{0.4}, /*result_timestamp=*/base::Time::Now());

  // Pref result not updated as unexpired result.
  bool is_prefs_updated = cached_result_writer_->UpdatePrefsIfExpired(
      config.get(), new_client_result, PlatformOptions(false));
  EXPECT_FALSE(is_prefs_updated);
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(false));
  const proto::ClientResult* client_result =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(client_result);
  EXPECT_EQ(unexpired_client_result.SerializeAsString(),
            client_result->SerializeAsString());

  // Unexpired pref updates with new client result as force refresh result is
  // true.
  is_prefs_updated = cached_result_writer_->UpdatePrefsIfExpired(
      config.get(), new_client_result, PlatformOptions(true));
  client_result =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(is_prefs_updated);
  EXPECT_TRUE(client_result);
  EXPECT_EQ(new_client_result.SerializeAsString(),
            client_result->SerializeAsString());
}

TEST_F(CachedResultWriterTest, UpdatePrefsIfModelIsUpdated) {
  std::unique_ptr<Config> config = test_utils::CreateTestConfig();
  // Saving unexpired result for client in prefs.
  proto::ClientResult unexpired_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());
  client_result_prefs_->SaveClientResultToPrefs(config->segmentation_key,
                                                unexpired_client_result);

  proto::ClientResult new_client_result = CreateClientResult(
      /*model_scores=*/{0.4}, /*result_timestamp=*/base::Time::Now(),
      /*model_version=*/2, /*ignore_previous_model_ttl=*/true);

  // Pref result updated as model is updated.
  bool is_prefs_updated = cached_result_writer_->UpdatePrefsIfExpired(
      config.get(), new_client_result, PlatformOptions(false));
  EXPECT_TRUE(is_prefs_updated);
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(false));
  const proto::ClientResult* client_result =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  EXPECT_TRUE(client_result);
  EXPECT_EQ(new_client_result.SerializeAsString(),
            client_result->SerializeAsString());
}

TEST_F(CachedResultWriterTest, UpdatePrefsIfExpiredResult) {
  std::unique_ptr<Config> config = test_utils::CreateTestConfig();
  // Saving expired result for client in prefs.
  client_result_prefs_->SaveClientResultToPrefs(
      config->segmentation_key,
      CreateClientResult(
          /*model_scores=*/{0.4},
          /*result_timestamp=*/base::Time::Now() - base::Days(8)));

  proto::ClientResult new_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());

  // Expired pref updates with new client result.
  bool is_prefs_updated = cached_result_writer_->UpdatePrefsIfExpired(
      config.get(), new_client_result, PlatformOptions(false));
  EXPECT_TRUE(is_prefs_updated);
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), new_client_result,
                                              PlatformOptions(false));
  const proto::ClientResult* result_from_pref =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  EXPECT_TRUE(result_from_pref);
  EXPECT_EQ(new_client_result.SerializeAsString(),
            result_from_pref->SerializeAsString());
}

TEST_F(CachedResultWriterTest, MarkResultAsUsed) {
  std::unique_ptr<Config> config = test_utils::CreateTestConfig();
  proto::ClientResult client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());
  bool is_prefs_updated = cached_result_writer_->UpdatePrefsIfExpired(
      config.get(), client_result, PlatformOptions(false));

  const proto::ClientResult* client_result_from_pref =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  // Writing results to prefs the first time should not update used timestamp.
  EXPECT_TRUE(is_prefs_updated);
  ASSERT_TRUE(client_result_from_pref);
  EXPECT_EQ(0, client_result_from_pref->first_used_timestamp());

  // Marking result as used should update the used timestamp.
  cached_result_writer_->MarkResultAsUsed(config.get());

  const proto::ClientResult* client_result_first_use =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  ASSERT_TRUE(client_result_first_use);
  EXPECT_GT(client_result_first_use->first_used_timestamp(), 0);

  // Marking result as used in the future should not reset first used timestamp.
  clock_.Advance(base::Seconds(10));
  cached_result_writer_->MarkResultAsUsed(config.get());

  const proto::ClientResult* client_result_second_use =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);
  ASSERT_TRUE(client_result_second_use);
  EXPECT_EQ(client_result_first_use->first_used_timestamp(),
            client_result_second_use->first_used_timestamp());
}

TEST_F(CachedResultWriterTest, CacheModelExecution) {
  std::unique_ptr<Config> config = test_utils::CreateTestConfig();
  proto::ClientResult save_result1 = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());

  // Caching should save the result to prefs and mark as used.
  cached_result_writer_->CacheModelExecution(config.get(),
                                             save_result1.client_result());

  const proto::ClientResult* client_result_first_exec =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  ASSERT_TRUE(client_result_first_exec);
  EXPECT_EQ(save_result1.client_result().SerializeAsString(),
            client_result_first_exec->client_result().SerializeAsString());
  EXPECT_GT(client_result_first_exec->first_used_timestamp(), 0);
  EXPECT_EQ(client_result_first_exec->timestamp_us(),
            client_result_first_exec->first_used_timestamp());

  constexpr base::TimeDelta kLater = base::Seconds(10);
  clock_.Advance(kLater);

  // Caching another result (before expiry) should still overwrite the existing
  // result.
  proto::ClientResult save_result2 = CreateClientResult(
      /*model_scores=*/{0.5}, /*result_timestamp=*/base::Time::Now());
  cached_result_writer_->CacheModelExecution(config.get(),
                                             save_result2.client_result());

  const proto::ClientResult* client_result_second_exec =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  ASSERT_TRUE(client_result_second_exec);
  EXPECT_EQ(save_result2.client_result().SerializeAsString(),
            client_result_second_exec->client_result().SerializeAsString());
  EXPECT_GT(client_result_second_exec->first_used_timestamp(), 0);
  EXPECT_EQ(client_result_second_exec->timestamp_us(),
            client_result_second_exec->first_used_timestamp());
}

TEST_F(CachedResultWriterTest, CacheModelExecutionOverwritesAnyPrefs) {
  std::unique_ptr<Config> config = test_utils::CreateTestConfig();
  // Saving unexpired result for client in prefs.
  proto::ClientResult saved_client_result = CreateClientResult(
      /*model_scores=*/{0.8}, /*result_timestamp=*/base::Time::Now());
  cached_result_writer_->UpdatePrefsIfExpired(config.get(), saved_client_result,
                                              PlatformOptions(false));
  EXPECT_TRUE(client_result_prefs_->ReadClientResultFromPrefs(
      config->segmentation_key));

  // Cache execution should overwrite the existing result.
  proto::ClientResult save_result2 = CreateClientResult(
      /*model_scores=*/{0.5}, /*result_timestamp=*/base::Time::Now());
  cached_result_writer_->CacheModelExecution(config.get(),
                                             save_result2.client_result());

  const proto::ClientResult* client_result_after_exec =
      client_result_prefs_->ReadClientResultFromPrefs(config->segmentation_key);

  ASSERT_TRUE(client_result_after_exec);
  EXPECT_EQ(save_result2.client_result().SerializeAsString(),
            client_result_after_exec->client_result().SerializeAsString());
  EXPECT_GT(client_result_after_exec->first_used_timestamp(), 0);
  EXPECT_EQ(client_result_after_exec->timestamp_us(),
            client_result_after_exec->first_used_timestamp());
}

}  // namespace segmentation_platform
