// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_state.h"

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browsing_topics/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

const base::Time kTime1 = base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
const base::Time kTime2 = base::Time::FromDeltaSinceWindowsEpoch(base::Days(2));
const base::Time kTime3 = base::Time::FromDeltaSinceWindowsEpoch(base::Days(3));
const base::Time kTime4 = base::Time::FromDeltaSinceWindowsEpoch(base::Days(4));
const base::Time kTime5 = base::Time::FromDeltaSinceWindowsEpoch(base::Days(5));

const browsing_topics::HmacKey kZeroKey = {};
const browsing_topics::HmacKey kTestKey = {1};
const browsing_topics::HmacKey kTestKey2 = {2};

const size_t kTaxonomySize = 349;
const int kTaxonomyVersion = 1;
const int kModelVersion = 2;
const size_t kPaddedTopTopicsStartIndex = 3;

EpochTopics CreateTestEpochTopics(base::Time calculation_time) {
  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(1), {HashedDomain(1)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(2), {HashedDomain(1), HashedDomain(2)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(3), {HashedDomain(1), HashedDomain(3)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(4), {HashedDomain(2), HashedDomain(3)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(5), {HashedDomain(1)}));

  EpochTopics epoch_topics(std::move(top_topics_and_observing_domains),
                           kPaddedTopTopicsStartIndex, kTaxonomySize,
                           kTaxonomyVersion, kModelVersion, calculation_time);

  return epoch_topics;
}

}  // namespace

class BrowsingTopicsStateTest : public testing::Test {
 public:
  BrowsingTopicsStateTest()
      : task_environment_(new base::test::TaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kBrowsingTopics, {{"config_version", "123"}});

    OverrideHmacKeyForTesting(kTestKey);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath TestFilePath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("BrowsingTopicsState"));
  }

  std::string GetTestFileContent() {
    JSONFileValueDeserializer deserializer(TestFilePath());
    std::unique_ptr<base::Value> value = deserializer.Deserialize(
        /*error_code=*/nullptr,
        /*error_message=*/nullptr);

    EXPECT_TRUE(value);
    return base::CollapseWhitespaceASCII(value->DebugString(), true);
  }

  void CreateOrOverrideTestFile(std::vector<EpochTopics> epochs,
                                base::Time next_scheduled_calculation_time,
                                std::string hex_encoded_hmac_key,
                                int config_version) {
    base::Value::List epochs_list;
    for (const EpochTopics& epoch : epochs) {
      epochs_list.Append(epoch.ToDictValue());
    }

    base::Value::Dict dict;
    dict.Set("epochs", std::move(epochs_list));
    dict.Set("next_scheduled_calculation_time",
             base::TimeToValue(next_scheduled_calculation_time));
    dict.Set("hex_encoded_hmac_key", std::move(hex_encoded_hmac_key));
    dict.Set("config_version", config_version);

    JSONFileValueSerializer(TestFilePath())
        .Serialize(base::Value(std::move(dict)));
  }

  void OnBrowsingTopicsStateLoaded() { observed_state_loaded_ = true; }

  bool observed_state_loaded() const { return observed_state_loaded_; }

 protected:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  base::ScopedTempDir temp_dir_;

  bool observed_state_loaded_ = false;
};

TEST_F(BrowsingTopicsStateTest, InitFromNoFile_SaveToDiskAfterDelay) {
  base::HistogramTester histograms;

  BrowsingTopicsState state(
      temp_dir_.GetPath(),
      base::BindOnce(&BrowsingTopicsStateTest::OnBrowsingTopicsStateLoaded,
                     base::Unretained(this)));

  EXPECT_FALSE(state.HasScheduledSaveForTesting());
  EXPECT_FALSE(observed_state_loaded());

  // UMA should not be recorded yet.
  histograms.ExpectTotalCount(
      "BrowsingTopics.BrowsingTopicsState.LoadFinishStatus", 0);

  // Let the backend file read task finish.
  task_environment_->RunUntilIdle();

  histograms.ExpectUniqueSample(
      "BrowsingTopics.BrowsingTopicsState.LoadFinishStatus", true,
      /*expected_bucket_count=*/1);

  EXPECT_TRUE(state.epochs().empty());
  EXPECT_TRUE(state.next_scheduled_calculation_time().is_null());
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kTestKey.begin()));

  EXPECT_TRUE(state.HasScheduledSaveForTesting());
  EXPECT_TRUE(observed_state_loaded());

  // Advance clock until immediately before saving takes place.
  task_environment_->FastForwardBy(base::Milliseconds(2499));
  EXPECT_TRUE(state.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(TestFilePath()));

  // Advance clock past the saving moment.
  task_environment_->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(state.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(TestFilePath()));
  EXPECT_EQ(
      GetTestFileContent(),
      "{\"config_version\": 123,\"epochs\": [ ],\"hex_encoded_hmac_key\": "
      "\"0100000000000000000000000000000000000000000000000000000000000000\","
      "\"next_scheduled_calculation_time\": \"0\"}");
}

TEST_F(BrowsingTopicsStateTest,
       UpdateNextScheduledCalculationTime_SaveToDiskAfterDelay) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());

  task_environment_->FastForwardBy(base::Milliseconds(3000));
  EXPECT_FALSE(state.HasScheduledSaveForTesting());

  state.UpdateNextScheduledCalculationTime(kTime1);

  EXPECT_TRUE(state.epochs().empty());
  EXPECT_EQ(state.next_scheduled_calculation_time(), kTime1);
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kTestKey.begin()));

  EXPECT_TRUE(state.HasScheduledSaveForTesting());

  task_environment_->FastForwardBy(base::Milliseconds(2499));
  EXPECT_TRUE(state.HasScheduledSaveForTesting());

  task_environment_->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(state.HasScheduledSaveForTesting());

  EXPECT_EQ(
      GetTestFileContent(),
      "{\"config_version\": 123,\"epochs\": [ ],\"hex_encoded_hmac_key\": "
      "\"0100000000000000000000000000000000000000000000000000000000000000\","
      "\"next_scheduled_calculation_time\": \"86400000000\"}");
}

TEST_F(BrowsingTopicsStateTest, AddEpoch) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  // Successful topics calculation at `kTime1`.
  state.AddEpoch(CreateTestEpochTopics(kTime1));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  // Successful topics calculation at `kTime2`.
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_TRUE(state.epochs()[1].HasValidTopics());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  // Failed topics calculation.
  state.AddEpoch(EpochTopics());
  EXPECT_EQ(state.epochs().size(), 3u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_TRUE(state.epochs()[1].HasValidTopics());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);
  EXPECT_FALSE(state.epochs()[2].HasValidTopics());

  // Successful topics calculation at `kTime4`.
  state.AddEpoch(CreateTestEpochTopics(kTime4));
  EXPECT_EQ(state.epochs().size(), 4u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_TRUE(state.epochs()[1].HasValidTopics());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);
  EXPECT_FALSE(state.epochs()[2].HasValidTopics());
  EXPECT_TRUE(state.epochs()[3].HasValidTopics());
  EXPECT_EQ(state.epochs()[3].calculation_time(), kTime4);

  // Successful topics calculation at `kTime5`. When this epoch is added, the
  // first one should be evicted.
  state.AddEpoch(CreateTestEpochTopics(kTime5));
  EXPECT_EQ(state.epochs().size(), 4u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime2);
  EXPECT_FALSE(state.epochs()[1].HasValidTopics());
  EXPECT_TRUE(state.epochs()[2].HasValidTopics());
  EXPECT_EQ(state.epochs()[2].calculation_time(), kTime4);
  EXPECT_TRUE(state.epochs()[3].HasValidTopics());
  EXPECT_EQ(state.epochs()[3].calculation_time(), kTime5);

  // The `next_scheduled_calculation_time` and `hmac_key` are unaffected.
  EXPECT_EQ(state.next_scheduled_calculation_time(), base::Time());
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kTestKey.begin()));
}

TEST_F(BrowsingTopicsStateTest, InitFromPreexistingFile_CorruptedHmacKey) {
  base::HistogramTester histograms;

  std::vector<EpochTopics> epochs;
  epochs.emplace_back(CreateTestEpochTopics(kTime1));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/"123",
                           /*config_version=*/123);

  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  EXPECT_EQ(state.epochs().size(), 0u);
  EXPECT_TRUE(state.next_scheduled_calculation_time().is_null());
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kZeroKey.begin()));

  histograms.ExpectUniqueSample(
      "BrowsingTopics.BrowsingTopicsState.LoadFinishStatus", false,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsStateTest, InitFromPreexistingFile_SameConfigVersion) {
  base::HistogramTester histograms;

  std::vector<EpochTopics> epochs;
  epochs.emplace_back(CreateTestEpochTopics(kTime1));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/base::HexEncode(kTestKey2),
                           /*config_version=*/123);

  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].model_version(), kModelVersion);
  EXPECT_EQ(state.next_scheduled_calculation_time(), kTime2);
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kTestKey2.begin()));

  histograms.ExpectUniqueSample(
      "BrowsingTopics.BrowsingTopicsState.LoadFinishStatus", true,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsStateTest,
       InitFromPreexistingFile_DifferentConfigVersion) {
  base::HistogramTester histograms;

  std::vector<EpochTopics> epochs;
  epochs.emplace_back(CreateTestEpochTopics(kTime1));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/base::HexEncode(kTestKey2),
                           /*config_version=*/100);

  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  EXPECT_TRUE(state.epochs().empty());
  EXPECT_TRUE(state.next_scheduled_calculation_time().is_null());
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kTestKey2.begin()));

  histograms.ExpectUniqueSample(
      "BrowsingTopics.BrowsingTopicsState.LoadFinishStatus", true,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsStateTest, ClearOneEpoch) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  state.AddEpoch(CreateTestEpochTopics(kTime2));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_TRUE(state.epochs()[1].HasValidTopics());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  state.UpdateNextScheduledCalculationTime(kTime3);

  state.ClearOneEpoch(/*epoch_index=*/0);
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_FALSE(state.epochs()[0].HasValidTopics());
  EXPECT_TRUE(state.epochs()[1].HasValidTopics());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  EXPECT_EQ(state.next_scheduled_calculation_time(), kTime3);
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kTestKey.begin()));
}

TEST_F(BrowsingTopicsStateTest, ClearAllTopics) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  state.AddEpoch(CreateTestEpochTopics(kTime2));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_TRUE(state.epochs()[0].HasValidTopics());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_TRUE(state.epochs()[1].HasValidTopics());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  state.UpdateNextScheduledCalculationTime(kTime3);

  state.ClearAllTopics();
  EXPECT_EQ(state.epochs().size(), 0u);

  EXPECT_EQ(state.next_scheduled_calculation_time(), kTime3);
  EXPECT_TRUE(std::equal(state.hmac_key().begin(), state.hmac_key().end(),
                         kTestKey.begin()));
}

TEST_F(BrowsingTopicsStateTest, ShouldSaveFileDespiteShutdownWhileScheduled) {
  auto state = std::make_unique<BrowsingTopicsState>(temp_dir_.GetPath(),
                                                     base::DoNothing());
  task_environment_->RunUntilIdle();

  ASSERT_TRUE(state->HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(TestFilePath()));

  state.reset();
  task_environment_.reset();

  // TaskEnvironment and BrowsingTopicsState both have been destroyed, mimic-ing
  // a browser shutdown.

  EXPECT_TRUE(base::PathExists(TestFilePath()));
  EXPECT_EQ(
      GetTestFileContent(),
      "{\"config_version\": 123,\"epochs\": [ ],\"hex_encoded_hmac_key\": "
      "\"0100000000000000000000000000000000000000000000000000000000000000\","
      "\"next_scheduled_calculation_time\": \"0\"}");
}

}  // namespace browsing_topics
