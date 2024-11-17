// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_state.h"

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

constexpr int kConfigVersion = 1;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 2;
constexpr size_t kPaddedTopTopicsStartIndex = 3;
constexpr base::TimeDelta kNextScheduledCalculationDelay = base::Days(7);

EpochTopics CreateTestEpochTopics(base::Time calculation_time,
                                  bool from_manually_triggered_calculation,
                                  int config_version = kConfigVersion) {
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
                           kPaddedTopTopicsStartIndex, config_version,
                           kTaxonomyVersion, kModelVersion, calculation_time,
                           from_manually_triggered_calculation);

  return epoch_topics;
}

}  // namespace

class BrowsingTopicsStateTest : public testing::Test {
 public:
  BrowsingTopicsStateTest()
      : task_environment_(new base::test::TaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {
    // Configure a long epoch_retention_duration to prevent epochs from expiring
    // during tests where expiration is irrelevant.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kBrowsingTopics, {}},
         {blink::features::kBrowsingTopicsParameters,
          {{"epoch_retention_duration", "3650000d"}}}},
        /*disabled_features=*/{});

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
                                std::string hex_encoded_hmac_key) {
    base::Value::List epochs_list;
    for (const EpochTopics& epoch : epochs) {
      epochs_list.Append(epoch.ToDictValue());
    }

    base::Value::Dict dict;
    dict.Set("epochs", std::move(epochs_list));
    dict.Set("next_scheduled_calculation_time",
             base::TimeToValue(next_scheduled_calculation_time));
    dict.Set("hex_encoded_hmac_key", std::move(hex_encoded_hmac_key));

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
      "{\"epochs\": [ ],\"hex_encoded_hmac_key\": "
      "\"0100000000000000000000000000000000000000000000000000000000000000\","
      "\"next_scheduled_calculation_time\": \"0\"}");
}

TEST_F(BrowsingTopicsStateTest,
       UpdateNextScheduledCalculationTime_SaveToDiskAfterDelay) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());

  task_environment_->FastForwardBy(base::Milliseconds(3000));
  EXPECT_FALSE(state.HasScheduledSaveForTesting());

  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  EXPECT_TRUE(state.epochs().empty());
  EXPECT_EQ(state.next_scheduled_calculation_time(),
            base::Time::Now() + kNextScheduledCalculationDelay);
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));

  EXPECT_TRUE(state.HasScheduledSaveForTesting());

  task_environment_->FastForwardBy(base::Milliseconds(2499));
  EXPECT_TRUE(state.HasScheduledSaveForTesting());

  task_environment_->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(state.HasScheduledSaveForTesting());

  std::string expected_content = base::StrCat(
      {"{\"epochs\": [ ],\"hex_encoded_hmac_key\": "
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
  std::optional<EpochTopics> maybe_removed_epoch_1 =
      state.AddEpoch(CreateTestEpochTopics(
          kTime1, /*from_manually_triggered_calculation=*/false));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(maybe_removed_epoch_1.has_value());

  // Successful topics calculation at `kTime2`.
  std::optional<EpochTopics> maybe_removed_epoch_2 =
      state.AddEpoch(CreateTestEpochTopics(
          kTime2, /*from_manually_triggered_calculation=*/false));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);
  EXPECT_FALSE(maybe_removed_epoch_2.has_value());

  // Failed topics calculation.
  std::optional<EpochTopics> maybe_removed_epoch_3 =
      state.AddEpoch(EpochTopics(kTime3));
  EXPECT_EQ(state.epochs().size(), 3u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);
  EXPECT_TRUE(state.epochs()[2].empty());
  EXPECT_EQ(state.epochs()[2].calculation_time(), kTime3);
  EXPECT_FALSE(maybe_removed_epoch_3.has_value());

  // Successful topics calculation at `kTime4`.
  std::optional<EpochTopics> maybe_removed_epoch_4 =
      state.AddEpoch(CreateTestEpochTopics(
          kTime4, /*from_manually_triggered_calculation=*/false));
  EXPECT_EQ(state.epochs().size(), 4u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);
  EXPECT_TRUE(state.epochs()[2].empty());
  EXPECT_FALSE(state.epochs()[3].empty());
  EXPECT_EQ(state.epochs()[3].calculation_time(), kTime4);
  EXPECT_FALSE(maybe_removed_epoch_4.has_value());

  // Successful topics calculation at `kTime5`. When this epoch is added, the
  // first one should be evicted.
  std::optional<EpochTopics> maybe_removed_epoch_5 =
      state.AddEpoch(CreateTestEpochTopics(
          kTime5, /*from_manually_triggered_calculation=*/false));
  EXPECT_EQ(state.epochs().size(), 4u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime2);
  EXPECT_TRUE(state.epochs()[1].empty());
  EXPECT_FALSE(state.epochs()[2].empty());
  EXPECT_EQ(state.epochs()[2].calculation_time(), kTime4);
  EXPECT_FALSE(state.epochs()[3].empty());
  EXPECT_EQ(state.epochs()[3].calculation_time(), kTime5);
  EXPECT_TRUE(maybe_removed_epoch_5.has_value());
  EXPECT_EQ(maybe_removed_epoch_5.value().calculation_time(), kTime1);

  // The `next_scheduled_calculation_time` and `hmac_key` are unaffected.
  EXPECT_EQ(state.next_scheduled_calculation_time(), base::Time());
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_Empty) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  EXPECT_TRUE(state.EpochsForSite(/*top_domain=*/"foo.com").empty());
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_OneEpoch_IntroductionTime) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  ASSERT_EQ(state.CalculateSiteStickyIntroductionDelay("foo.com"),
            base::Seconds(96673));

  // Advance time to just before the epoch introduction.
  task_environment_->FastForwardBy(base::Seconds(96673));
  EXPECT_TRUE(state.EpochsForSite(/*top_domain=*/"foo.com").empty());

  // Advance time to the epoch introduction time.
  task_environment_->FastForwardBy(base::Seconds(1));
  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 1u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
}

// Together with EpochsForSite_OneEpoch_IntroductionTime, this shows that the
// epoch introduction time is influenced by the specific epoch.
TEST_F(BrowsingTopicsStateTest, EpochsForSite_OneEpoch_IntroductionTime2) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  ASSERT_EQ(state.CalculateSiteStickyIntroductionDelay("foo.com"),
            base::Seconds(151685));

  // Advance time to just before the epoch introduction.
  task_environment_->FastForwardBy(base::Seconds(151685));
  EXPECT_TRUE(state.EpochsForSite(/*top_domain=*/"foo.com").empty());

  // Advance time to the epoch introduction time.
  task_environment_->FastForwardBy(base::Seconds(1));
  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 1u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_OneEpoch_ManuallyTriggered) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/true));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  // There shouldn't be a delay when the latest epoch is manually triggered.
  ASSERT_EQ(state.CalculateSiteStickyIntroductionDelay("foo.com"),
            base::Microseconds(0));
  task_environment_->FastForwardBy(base::Microseconds(10));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 1u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_ThreeEpochs_IntroductionTime) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime3, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  ASSERT_EQ(state.CalculateSiteStickyIntroductionDelay("foo.com"),
            base::Seconds(136778));

  // Advance time to just before the epoch introduction.
  task_environment_->FastForwardBy(base::Seconds(136778));
  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 2u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);

  // Advance time to the epoch introduction time.
  task_environment_->FastForwardBy(base::Seconds(1));

  epochs_for_site = state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[2]);
}

TEST_F(BrowsingTopicsStateTest, EpochsForSite_PhaseOutTime) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopics, {}},
       {blink::features::kBrowsingTopicsParameters,
        {{"epoch_retention_duration", "28d"}}}},
      /*disabled_features=*/{});

  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  base::Time now = base::Time::Now();

  state.AddEpoch(CreateTestEpochTopics(
      now, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  base::TimeDelta phase_out_time_offset =
      state.CalculateSiteStickyPhaseOutTimeOffset("foo.com", state.epochs()[0]);

  ASSERT_GT(phase_out_time_offset, base::Seconds(0));
  ASSERT_LT(phase_out_time_offset, base::Days(2));

  // Advance time to just before the epoch phase out.
  task_environment_->FastForwardBy(base::Days(28) - phase_out_time_offset -
                                   base::Seconds(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 1u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);

  // Advance time to the epoch phase out time.
  task_environment_->FastForwardBy(base::Seconds(1));

  epochs_for_site = state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 0u);
}

TEST_F(BrowsingTopicsStateTest,
       EpochsForSite_ThreeEpochs_LatestManuallyTriggered) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime3, /*from_manually_triggered_calculation=*/true));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  task_environment_->FastForwardBy(base::Microseconds(10));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[2]);
}

TEST_F(BrowsingTopicsStateTest,
       EpochsForSite_ThreeEpochs_EarlierEpochManuallyTriggered) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/true));
  state.AddEpoch(CreateTestEpochTopics(
      kTime3, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  task_environment_->FastForwardBy(base::Microseconds(10));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  // The latest epoch shouldn't be included because it wasn't manually
  // triggered.
  EXPECT_EQ(epochs_for_site.size(), 2u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
}

TEST_F(BrowsingTopicsStateTest,
       EpochsForSite_FourEpochs_IntroductionTimeNotArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime3, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime4, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  task_environment_->FastForwardBy(base::Hours(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[2]);
}

TEST_F(BrowsingTopicsStateTest,
       EpochsForSite_FourEpochs_IntroductionTimeArrived) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime3, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime4, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  task_environment_->FastForwardBy(base::Days(1));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[2]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[3]);
}

TEST_F(BrowsingTopicsStateTest,
       EpochsForSite_FourEpochs_LatestManuallyTriggered) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime3, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime4, /*from_manually_triggered_calculation=*/true));

  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  task_environment_->FastForwardBy(base::Microseconds(10));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[2]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[3]);
}

TEST_F(BrowsingTopicsStateTest,
       EpochsForSite_FourEpochs_EarlierEpochManuallyTriggered) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/true));
  state.AddEpoch(CreateTestEpochTopics(
      kTime3, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime4, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  task_environment_->FastForwardBy(base::Microseconds(10));

  std::vector<const EpochTopics*> epochs_for_site =
      state.EpochsForSite(/*top_domain=*/"foo.com");
  // The latest epoch shouldn't be included because it wasn't manually
  // triggered.
  EXPECT_EQ(epochs_for_site.size(), 3u);
  EXPECT_EQ(epochs_for_site[0], &state.epochs()[0]);
  EXPECT_EQ(epochs_for_site[1], &state.epochs()[1]);
  EXPECT_EQ(epochs_for_site[2], &state.epochs()[2]);
}

TEST_F(BrowsingTopicsStateTest, InitFromPreexistingFile_CorruptedHmacKey) {
  base::HistogramTester histograms;

  std::vector<EpochTopics> epochs;
  epochs.emplace_back(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/"123");

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
  epochs.emplace_back(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/base::HexEncode(kTestKey2));

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
       InitFromPreexistingFile_ForwardCompatibleConfigVersion) {
  base::HistogramTester histograms;

  std::vector<EpochTopics> epochs;
  // Current version is 1 but it's forward compatible with 2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kBrowsingTopicsParameters,
      {{"prioritized_topics_list", ""}});
  EXPECT_EQ(CurrentConfigVersion(), 1);
  epochs.emplace_back(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false,
      /*config_version=*/2));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/base::HexEncode(kTestKey2));

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
       InitFromPreexistingFile_BackwardCompatibleConfigVersion) {
  base::HistogramTester histograms;

  std::vector<EpochTopics> epochs;
  // Current version is 2 but it's backward compatible with 1.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kBrowsingTopicsParameters,
      {{"prioritized_topics_list", "4,57"}});
  EXPECT_EQ(CurrentConfigVersion(), 2);
  epochs.emplace_back(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false,
      /*config_version=*/1));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/base::HexEncode(kTestKey2));

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
       InitFromPreexistingFile_IncompatibleConfigVersion) {
  base::HistogramTester histograms;

  std::vector<EpochTopics> epochs;
  epochs.emplace_back(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false,
      /*config_version=*/100));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/base::HexEncode(kTestKey2));

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

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
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

  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  EXPECT_EQ(state.next_scheduled_calculation_time(),
            base::Time::Now() + kNextScheduledCalculationDelay);
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));
}

TEST_F(BrowsingTopicsStateTest, ClearAllTopics) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));

  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);

  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_FALSE(state.epochs()[0].empty());
  EXPECT_EQ(state.epochs()[0].calculation_time(), kTime1);
  EXPECT_FALSE(state.epochs()[1].empty());
  EXPECT_EQ(state.epochs()[1].calculation_time(), kTime2);

  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

  state.ClearAllTopics();
  EXPECT_EQ(state.epochs().size(), 0u);

  EXPECT_EQ(state.next_scheduled_calculation_time(),
            base::Time::Now() + kNextScheduledCalculationDelay);
  EXPECT_TRUE(base::ranges::equal(state.hmac_key(), kTestKey));
}

TEST_F(BrowsingTopicsStateTest, ClearTopic) {
  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

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

  state.AddEpoch(CreateTestEpochTopics(
      kTime1, /*from_manually_triggered_calculation=*/false));
  state.AddEpoch(CreateTestEpochTopics(
      kTime2, /*from_manually_triggered_calculation=*/false));
  state.UpdateNextScheduledCalculationTime(kNextScheduledCalculationDelay);

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
      "{\"epochs\": [ ],\"hex_encoded_hmac_key\": "
      "\"0100000000000000000000000000000000000000000000000000000000000000\","
      "\"next_scheduled_calculation_time\": \"0\"}");
}

TEST_F(BrowsingTopicsStateTest, ScheduleEpochsExpiration) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopics, {}},
       {blink::features::kBrowsingTopicsParameters,
        {{"epoch_retention_duration", "28s"}}}},
      /*disabled_features=*/{});

  base::Time start_time = base::Time::Now();

  std::vector<EpochTopics> epochs;
  epochs.emplace_back(
      CreateTestEpochTopics(start_time - base::Seconds(29),
                            /*from_manually_triggered_calculation=*/false));
  epochs.emplace_back(
      CreateTestEpochTopics(start_time - base::Seconds(28),
                            /*from_manually_triggered_calculation=*/false));
  epochs.emplace_back(
      CreateTestEpochTopics(start_time - base::Seconds(27),
                            /*from_manually_triggered_calculation=*/false));
  epochs.emplace_back(
      CreateTestEpochTopics(start_time - base::Seconds(26),
                            /*from_manually_triggered_calculation=*/false));

  CreateOrOverrideTestFile(std::move(epochs),
                           /*next_scheduled_calculation_time=*/kTime2,
                           /*hex_encoded_hmac_key=*/base::HexEncode(kTestKey));

  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  EXPECT_EQ(state.epochs().size(), 4u);

  state.ScheduleEpochsExpiration();

  // Verify that two epochs have been removed immediately due to expiration.
  EXPECT_EQ(state.epochs().size(), 2u);
  EXPECT_EQ(state.epochs()[0].calculation_time(),
            start_time - base::Seconds(27));
  EXPECT_EQ(state.epochs()[1].calculation_time(),
            start_time - base::Seconds(26));

  // Process any pending tasks to ensure any asynchronous expirations are
  // handled.
  task_environment_->RunUntilIdle();
  EXPECT_EQ(state.epochs().size(), 2u);

  // Trigger another epoch expiration.
  task_environment_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_EQ(state.epochs()[0].calculation_time(),
            start_time - base::Seconds(26));

  // Trigger the final epoch expiration.
  task_environment_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(state.epochs().size(), 0u);
}

TEST_F(BrowsingTopicsStateTest, AddEpochAndVerifyExpiration) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopics, {}},
       {blink::features::kBrowsingTopicsParameters,
        {{"epoch_retention_duration", "28s"}}}},
      /*disabled_features=*/{});

  base::Time start_time = base::Time::Now();

  BrowsingTopicsState state(temp_dir_.GetPath(), base::DoNothing());
  task_environment_->RunUntilIdle();

  state.AddEpoch(CreateTestEpochTopics(
      base::Time::Now(), /*from_manually_triggered_calculation=*/false));

  task_environment_->FastForwardBy(base::Seconds(1));
  state.AddEpoch(CreateTestEpochTopics(
      base::Time::Now(), /*from_manually_triggered_calculation=*/false));

  EXPECT_EQ(state.epochs().size(), 2u);

  // Verify epochs haven't expired prematurely.
  task_environment_->FastForwardBy(base::Seconds(26));
  EXPECT_EQ(state.epochs().size(), 2u);

  // Verify the first epoch expired at the expected expiration time.
  task_environment_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(state.epochs().size(), 1u);
  EXPECT_EQ(state.epochs()[0].calculation_time(),
            start_time + base::Seconds(1));

  // Verify the second epoch has also expired at the expected expiration time.
  task_environment_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(state.epochs().size(), 0u);
}

}  // namespace browsing_topics
