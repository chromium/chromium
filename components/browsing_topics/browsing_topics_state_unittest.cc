// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_state.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browsing_topics/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

constexpr base::Time kTime1 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr base::Time kTime2 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(2));
constexpr base::Time kTime3 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(3));
constexpr base::Time kTime4 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(4));
constexpr base::Time kTime5 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(5));

constexpr browsing_topics::HmacKey kZeroKey = {};
constexpr browsing_topics::HmacKey kTestKey = {1};
constexpr browsing_topics::HmacKey kTestKey2 = {2};

constexpr size_t kTaxonomySize = 349;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 2;
constexpr size_t kPaddedTopTopicsStartIndex = 3;

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

    JSONFileValueSerializer(TestFilePath()).Serialize(dict);
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
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));

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

  state.UpdateNextScheduledCalculationTime();

  EXPECT_TRUE(state.epochs().empty());
  EXPECT_EQ(state.next_scheduled_calculation_time(),
            base::Time::Now() + base::Days(7));
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));

  EXPECT_TRUE(state.HasScheduledSaveForTesting());

  task_environment_->FastForwardBy(base::Milliseconds(2499));
  EXPECT_TRUE(state.HasScheduledSaveForTesting());

  task_environment_->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(state.HasScheduledSaveForTesting());

  std::string expected_content = base::StrCat(
      {"{\"config_version\": 123,\"epochs\": [ ],\"hex_encoded_hmac_key\": "
       "\"0100000000000000000000000000000000000000000000000000000000000000"
       "\",\"next_scheduled_calculation_time\": \"",
       base::NumberToString(state.next_scheduled_calculation_time()
                                .ToDeltaSinceWindowsEpoch()
                                .InMicroseconds()),
       "\"}"});

  EXPECT_EQ(GetTestFileContent(), expected_content);
}

TEST_F(BrowsingTopicsStateTest, AddEpoch) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  // Successful topics calculation at `kTime1`.
  state.AddEpoch(CreateTestEpochTopics(kTime1));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  // Successful topics calculation at `kTime2`.
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  // Failed topics calculation.
  state.AddEpoch(EpochTopics(kTime3));
  EXPECT_EQ(state.epochs().size(), 3u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);
  EXPECT_TRUE(state.epochs()[2].empty());
  EXPECT_EQ(state.epochs()[2].calculation_time(), kTime3);

  // Successful topics calculation at `kTime4`.
  state.AddEpoch(CreateTestEpochTopics(kTime4));
  EXPECT_EQ(state.epochs().size(), 4u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);
  EXPECT_TRUE(state.epochs()[2].empty());
  EXPECT_FALSE(state.epochs()[3].empty());
  EXPECT_EQ(state.epochs()[3].calculation_time(), kTime4);

  // Successful topics calculation at `kTime5`. When this epoch is added, the
  // first one should be evicted.
  state.AddEpoch(CreateTestEpochTopics(kTime5));
  EXPECT_EQ(state.epochs().size(), 4u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime2);
  EXPECT_TRUE(state.epochs()[1].empty());
  EXPECT_FALSE(state.epochs()[2].empty());
  EXPECT_EQ(state.epochs()[2].calculation_time(), kTime4);
  EXPECT_FALSE(state.epochs()[3].empty());
  EXPECT_EQ(state.epochs()[3].calculation_time(), kTime5);

  // The `next_scheduled_calculation_time` and `hmac_key` are unaffected.
  EXPECT_EQ(state.next_scheduled_calculation_time(), base::Time());
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_Empty) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  EXPECT_TRUE(state.EpochsForSite(/*top_domain=*/"foo.com").empty());
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_OneEpoch_SwitchTimeNotArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.UpdateNextScheduledCalculationTime();

  // The random per-site delay happens to be between (one hour, one day).
  ASSERT_GT(state.CalculateSiteStickyTimeDelta("foo.com"), base::Hours(1));
  ASSERT_LT(state.CalculateSiteStickyTimeDelta("foo.com"), base::Days(1));

  task_environment_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(state.EpochsForSite(/*top_domain=*/"foo.com").empty());
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_OneEpoch_SwitchTimeArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.UpdateNextScheduledCalculationTime();

  // The random per-site delay happens to be between (one hour, one day).
  ASSERT_GT(state.CalculateSiteStickyTimeDelta("foo.com"), base::Hours(1));
  ASSERT_LT(state.CalculateSiteStickyTimeDelta("foo.com"), base::Days(1));

  task_environment_->FastForwardBy(base::Days(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 1u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
}

TEST_F(BrowsingTopicsStateTest,
       EpochsForSite_ThreeEpochs_SwitchTimeNotArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  state.AddEpoch(CreateTestEpochTopics(kTime3));
  state.UpdateNextScheduledCalculationTime();

  task_environment_->FastForwardBy(base::Hours(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 2u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_ThreeEpochs_SwitchTimeArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  state.AddEpoch(CreateTestEpochTopics(kTime3));
  state.UpdateNextScheduledCalculationTime();

  task_environment_->FastForwardBy(base::Days(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[2]);
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_FourEpochs_SwitchTimeNotArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  state.AddEpoch(CreateTestEpochTopics(kTime3));
  state.AddEpoch(CreateTestEpochTopics(kTime4));
  state.UpdateNextScheduledCalculationTime();

  task_environment_->FastForwardBy(base::Hours(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[2]);
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_FourEpochs_SwitchTimeArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  state.AddEpoch(CreateTestEpochTopics(kTime3));
  state.AddEpoch(CreateTestEpochTopics(kTime4));
  state.UpdateNextScheduledCalculationTime();

  task_environment_->FastForwardBy(base::Days(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[2]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[3]);
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
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kZeroKey));

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
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].model_version(), kModelVersion);
  EXPECT_EQ(state.next_scheduled_calculation_time(), kTime2);
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey2));

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
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey2));

  histograms.ExpectUniqueSample(
      "BrowsingTopics.BrowsingTopicsState.LoadFinishStatus", true,
      /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsStateTest, ClearOneEpoch) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  state.AddEpoch(CreateTestEpochTopics(kTime2));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  state.ClearOneEpoch(/*epoch_index=*/0);
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_TRUE(state.epochs()[0].empty());
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  state.UpdateNextScheduledCalculationTime();

  EXPECT_EQ(state.next_scheduled_calculation_time(),
            base::Time::Now() + base::Days(7));
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));
}

TEST_F(BrowsingTopicsStateTest, ClearAllTopics) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  state.AddEpoch(CreateTestEpochTopics(kTime2));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  state.UpdateNextScheduledCalculationTime();

  state.ClearAllTopics();
  EXPECT_EQ(state.epochs().size(), 0u);

  EXPECT_EQ(state.next_scheduled_calculation_time(),
            base::Time::Now() + base::Days(7));
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));
}

TEST_F(BrowsingTopicsStateTest, ClearTopic) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  state.UpdateNextScheduledCalculationTime();

  state.ClearTopic(Topic(3));

  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_EQ(state.epochs()[0].top_topics_and_observing_domains()[0].topic(),
            Topic(1));
  EXPECT_EQ(state.epochs()[0].top_topics_and_observing_domains()[1].topic(),
            Topic(2));
  EXPECT_FALSE(
      state.epochs()[0].top_topics_and_observing_domains()[2].IsValid());
  EXPECT_EQ(state.epochs()[0].top_topics_and_observing_domains()[3].topic(),
            Topic(4));
  EXPECT_EQ(state.epochs()[0].top_topics_and_observing_domains()[4].topic(),
            Topic(5));

  EXPECT_EQ(state.epochs()[1].top_topics_and_observing_domains()[0].topic(),
            Topic(1));
  EXPECT_EQ(state.epochs()[1].top_topics_and_observing_domains()[1].topic(),
            Topic(2));
  EXPECT_FALSE(
      state.epochs()[1].top_topics_and_observing_domains()[2].IsValid());
  EXPECT_EQ(state.epochs()[1].top_topics_and_observing_domains()[3].topic(),
            Topic(4));
  EXPECT_EQ(state.epochs()[1].top_topics_and_observing_domains()[4].topic(),
            Topic(5));
}

TEST_F(BrowsingTopicsStateTest, ClearContextDomain) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(kTime1));
  state.AddEpoch(CreateTestEpochTopics(kTime2));
  state.UpdateNextScheduledCalculationTime();

  state.ClearContextDomain(HashedDomain(1));

  EXPECT_EQ(
      state.epochs()[0].top_topics_and_observing_domains()[0].hashed_domains(),
      std::set<HashedDomain>{});
  EXPECT_EQ(
      state.epochs()[0].top_topics_and_observing_domains()[1].hashed_domains(),
      std::set<HashedDomain>({HashedDomain(2)}));
  EXPECT_EQ(
      state.epochs()[0].top_topics_and_observing_domains()[2].hashed_domains(),
      std::set<HashedDomain>({HashedDomain(3)}));
  EXPECT_EQ(
      state.epochs()[0].top_topics_and_observing_domains()[3].hashed_domains(),
      std::set<HashedDomain>({HashedDomain(2), HashedDomain(3)}));
  EXPECT_EQ(
      state.epochs()[0].top_topics_and_observing_domains()[4].hashed_domains(),
      std::set<HashedDomain>{});

  EXPECT_EQ(
      state.epochs()[1].top_topics_and_observing_domains()[0].hashed_domains(),
      std::set<HashedDomain>{});
  EXPECT_EQ(
      state.epochs()[1].top_topics_and_observing_domains()[1].hashed_domains(),
      std::set<HashedDomain>({HashedDomain(2)}));
  EXPECT_EQ(
      state.epochs()[1].top_topics_and_observing_domains()[2].hashed_domains(),
      std::set<HashedDomain>({HashedDomain(3)}));
  EXPECT_EQ(
      state.epochs()[1].top_topics_and_observing_domains()[3].hashed_domains(),
      std::set<HashedDomain>({HashedDomain(2), HashedDomain(3)}));
  EXPECT_EQ(
      state.epochs()[1].top_topics_and_observing_domains()[4].hashed_domains(),
      std::set<HashedDomain>{});
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
