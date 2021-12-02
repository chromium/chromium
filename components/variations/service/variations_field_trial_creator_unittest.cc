// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <cstring>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_test_utils.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "components/variations/seed_response.h"
#endif

namespace variations {
namespace {

using ::testing::_;
using ::testing::Ge;
using ::testing::NiceMock;
using ::testing::Return;

// Constants used to create the test seeds.
const char kTestSeedStudyName[] = "test";
const char kTestSeedExperimentName[] = "abc";
const char kTestSafeSeedExperimentName[] = "abc.safe";
const int kTestSeedExperimentProbability = 100;
const char kTestSeedSerialNumber[] = "123";

// Constants used to mock the serialized seed state.
const char kTestSeedSerializedData[] = "a serialized seed, 100% realistic";
const char kTestSeedSignature[] = "a totally valid signature, I swear!";
const int kTestSeedMilestone = 90;

// The content of an empty prefs file.
const char kEmptyPrefsFile[] = "{}";

// Used for similar tests.
struct TestParams {
  // Inputs.
  int days;
  const base::Time binary_build_time;
};

// Returns a seed with simple test data. The seed has a single study,
// "UMA-Uniformity-Trial-10-Percent", which has a single experiment, "abc", with
// probability weight 100.
VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name(kTestSeedStudyName);
  study->set_default_experiment_name(kTestSeedExperimentName);
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(kTestSeedExperimentName);
  experiment->set_probability_weight(kTestSeedExperimentProbability);
  seed.set_serial_number(kTestSeedSerialNumber);
  return seed;
}

// Returns a seed with simple test data. The seed has a single study,
// "UMA-Uniformity-Trial-10-Percent", which has a single experiment,
// "abc.safe", with probability weight 100.
//
// Intended to be used when a "safe" seed is needed so that test expectations
// can distinguish between a regular and safe seeds.
VariationsSeed CreateTestSafeSeed() {
  VariationsSeed seed = CreateTestSeed();
  Study* study = seed.mutable_study(0);
  study->set_default_experiment_name(kTestSafeSeedExperimentName);
  study->mutable_experiment(0)->set_name(kTestSafeSeedExperimentName);
  return seed;
}

#if defined(OS_ANDROID)
const char kTestSeedCountry[] = "in";

// Populates |seed| with simple test data, targetting only users in a specific
// country. The resulting seed will contain one study called "test", which
// contains one experiment called "abc" with probability weight 100, restricted
// just to users in |kTestSeedCountry|.
VariationsSeed CreateTestSeedWithCountryFilter() {
  VariationsSeed seed = CreateTestSeed();
  Study* study = seed.mutable_study(0);
  Study::Filter* filter = study->mutable_filter();
  filter->add_country(kTestSeedCountry);
  filter->add_platform(Study::PLATFORM_ANDROID);
  return seed;
}

// Serializes |seed| to protobuf binary format.
std::string SerializeSeed(const VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}
#endif  // defined(OS_ANDROID)

class TestPlatformFieldTrials : public PlatformFieldTrials {
 public:
  TestPlatformFieldTrials() = default;

  TestPlatformFieldTrials(const TestPlatformFieldTrials&) = delete;
  TestPlatformFieldTrials& operator=(const TestPlatformFieldTrials&) = delete;

  ~TestPlatformFieldTrials() override = default;

  // PlatformFieldTrials:
  void SetUpFieldTrials() override {}
  void SetUpFeatureControllingFieldTrials(
      bool has_seed,
      const base::FieldTrial::EntropyProvider* low_entropy_provider,
      base::FeatureList* feature_list) override {}
};

class MockSafeSeedManager : public SafeSeedManager {
 public:
  explicit MockSafeSeedManager(PrefService* local_state)
      : SafeSeedManager(local_state) {}

  MockSafeSeedManager(const MockSafeSeedManager&) = delete;
  MockSafeSeedManager& operator=(const MockSafeSeedManager&) = delete;

  ~MockSafeSeedManager() override = default;

  MOCK_CONST_METHOD0(ShouldRunInSafeMode, bool());
  MOCK_METHOD5(DoSetActiveSeedState,
               void(const std::string& seed_data,
                    const std::string& base64_seed_signature,
                    int seed_milestone,
                    ClientFilterableState* client_filterable_state,
                    base::Time seed_fetch_time));

  void SetActiveSeedState(
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      int seed_milestone,
      std::unique_ptr<ClientFilterableState> client_filterable_state,
      base::Time seed_fetch_time) override {
    DoSetActiveSeedState(seed_data, base64_seed_signature, seed_milestone,
                         client_filterable_state.get(), seed_fetch_time);
  }
};

// TODO(crbug.com/1167566): Remove when fake VariationsServiceClient created.
class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;

  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;

  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    if (restrict_parameter_.empty())
      return false;
    *parameter = restrict_parameter_;
    return true;
  }
  bool IsEnterprise() override { return false; }

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }

  std::string restrict_parameter_;
};

class MockVariationsServiceClient : public TestVariationsServiceClient {
 public:
  MOCK_METHOD(version_info::Channel, GetChannel, (), (override));
};

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state) {}

  TestVariationsSeedStore(const TestVariationsSeedStore&) = delete;
  TestVariationsSeedStore& operator=(const TestVariationsSeedStore&) = delete;

  ~TestVariationsSeedStore() override = default;

  bool LoadSeed(VariationsSeed* seed,
                std::string* seed_data,
                std::string* base64_signature) override {
    *seed = CreateTestSeed();
    *seed_data = kTestSeedSerializedData;
    *base64_signature = kTestSeedSignature;
    return true;
  }

  LoadSeedResult LoadSafeSeed(VariationsSeed* seed,
                              ClientFilterableState* client_state) override {
    if (has_corrupted_safe_seed_)
      return LoadSeedResult::kCorruptBase64;

    if (has_empty_safe_seed_)
      return LoadSeedResult::kEmpty;

    *seed = CreateTestSafeSeed();
    return LoadSeedResult::kSuccess;
  }

  void set_has_corrupted_safe_seed(bool is_corrupted) {
    has_corrupted_safe_seed_ = is_corrupted;
  }

  void set_has_empty_safe_seed(bool is_empty) {
    has_empty_safe_seed_ = is_empty;
  }

 private:
  // Whether to simulate having a corrupted safe seed.
  bool has_corrupted_safe_seed_ = false;

  // Whether to simulate having an empty safe seed.
  bool has_empty_safe_seed_ = false;
};

class TestVariationsFieldTrialCreator : public VariationsFieldTrialCreator {
 public:
  TestVariationsFieldTrialCreator(
      PrefService* local_state,
      TestVariationsServiceClient* client,
      SafeSeedManager* safe_seed_manager,
      version_info::Channel channel = version_info::Channel::UNKNOWN,
      const base::FilePath user_data_dir = base::FilePath(),
      metrics::StartupVisibility startup_visibility =
          metrics::StartupVisibility::kUnknown)
      : VariationsFieldTrialCreator(
            client,
            std::make_unique<VariationsSeedStore>(local_state),
            UIStringOverrider()),
        enabled_state_provider_(/*consent=*/true, /*enabled=*/true),
        seed_store_(local_state),
        safe_seed_manager_(safe_seed_manager),
        build_time_(base::Time::Now()) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state, &enabled_state_provider_, std::wstring(), user_data_dir,
        startup_visibility, channel);
    metrics_state_manager_->InstantiateFieldTrialList();
  }

  TestVariationsFieldTrialCreator(const TestVariationsFieldTrialCreator&) =
      delete;
  TestVariationsFieldTrialCreator& operator=(
      const TestVariationsFieldTrialCreator&) = delete;

  ~TestVariationsFieldTrialCreator() override = default;

  // A convenience wrapper around SetUpFieldTrials() which passes default values
  // for uninteresting params.
  bool SetUpFieldTrials(bool extend_variations_safe_mode = true) {
    TestPlatformFieldTrials platform_field_trials;
    return VariationsFieldTrialCreator::SetUpFieldTrials(
        /*variation_ids=*/std::vector<std::string>(),
        std::vector<base::FeatureList::FeatureOverrideInfo>(),
        /*low_entropy_provider=*/nullptr, std::make_unique<base::FeatureList>(),
        metrics_state_manager_.get(), &platform_field_trials,
        safe_seed_manager_, /*low_entropy_source_value=*/absl::nullopt,
        extend_variations_safe_mode);
  }

  TestVariationsSeedStore* seed_store() { return &seed_store_; }
  void SetBuildTime(const base::Time& time) { build_time_ = time; }
  bool was_maybe_extend_variations_safe_mode_called() {
    return was_maybe_extend_variations_safe_mode_called_;
  }

 protected:
  void MaybeExtendVariationsSafeMode(
      metrics::MetricsStateManager* metrics_state_manager) override {
    was_maybe_extend_variations_safe_mode_called_ = true;
    VariationsFieldTrialCreator::MaybeExtendVariationsSafeMode(
        metrics_state_manager);
  }

 private:
  VariationsSeedStore* GetSeedStore() override { return &seed_store_; }
  base::Time GetBuildTime() const override { return build_time_; }

  metrics::TestEnabledStateProvider enabled_state_provider_;
  TestVariationsSeedStore seed_store_;
  const raw_ptr<SafeSeedManager> safe_seed_manager_;
  base::Time build_time_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  bool was_maybe_extend_variations_safe_mode_called_ = false;
};

}  // namespace

class FieldTrialCreatorTest : public ::testing::Test {
 public:
  FieldTrialCreatorTest() {
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    VariationsService::RegisterPrefs(prefs_.registry());
    global_feature_list_ = base::FeatureList::ClearInstanceForTesting();
  }

  FieldTrialCreatorTest(const FieldTrialCreatorTest&) = delete;
  FieldTrialCreatorTest& operator=(const FieldTrialCreatorTest&) = delete;

  ~FieldTrialCreatorTest() override {
    // Clear out any features created by tests in this suite, and restore the
    // global feature list.
    base::FeatureList::ClearInstanceForTesting();
    base::FeatureList::RestoreInstanceForTesting(
        std::move(global_feature_list_));
  }

  void ResetFeatureList() {
    base::FeatureList::ClearInstanceForTesting();
    base::FeatureList::RestoreInstanceForTesting(
        std::move(global_feature_list_));
    global_feature_list_ = base::FeatureList::ClearInstanceForTesting();
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  // The global feature list, which is ignored by tests in this suite.
  std::unique_ptr<base::FeatureList> global_feature_list_;
};

class FieldTrialCreatorSafeModeExperimentTest : public FieldTrialCreatorTest {
 public:
  FieldTrialCreatorSafeModeExperimentTest()
      : field_trial_list_(std::make_unique<base::MockEntropyProvider>(0.1)) {}
  ~FieldTrialCreatorSafeModeExperimentTest() override = default;

  void SetUp() override {
    DisableTestingConfig();

    // Create a temp prefs file with no prefs.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    prefs_file_ = temp_dir_.GetPath().AppendASCII("write.json");
    ASSERT_LT(0, base::WriteFile(prefs_file_, kEmptyPrefsFile));
    ASSERT_TRUE(PathExists(prefs_file_));
  }

  void TearDown() override { ASSERT_TRUE(base::DeleteFile(prefs_file_)); }

  // Creates and returns a PrefService that uses a real JsonPrefStore rather
  // than a TestingPrefStore.
  std::unique_ptr<PrefService> CreatePrefService() {
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    metrics::MetricsService::RegisterPrefs(pref_registry.get());
    VariationsService::RegisterPrefs(pref_registry.get());

    auto pref_store = base::MakeRefCounted<JsonPrefStore>(prefs_file_);
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(pref_store);
    return pref_service_factory.Create(pref_registry);
  }

  const base::FilePath prefs_file() const { return prefs_file_; }
  const base::FilePath user_data_dir_path() const {
    return temp_dir_.GetPath();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath prefs_file_;
  base::test::ScopedFieldTrialListResetter trial_list_resetter_;
  base::FieldTrialList field_trial_list_;
};

struct StartupVisibilityTestParams {
  const std::string test_name;
  metrics::StartupVisibility startup_visibility;
  bool extend_safe_mode;
};

class FieldTrialCreatorTestWithStartupVisibility
    : public FieldTrialCreatorSafeModeExperimentTest,
      public ::testing::WithParamInterface<StartupVisibilityTestParams> {};

// Verify that unexpired seeds are used.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ValidSeed_NotExpired) {
  DisableTestingConfig();
  const base::Time now = base::Time::Now();

  std::vector<TestParams> test_cases = {
      // Verify that when the binary is newer than the most recent seed, the
      // seed is applied as long as it was downloaded within the last 30 days.
      {.days = 30, .binary_build_time = now},
      // Verify that when the binary is older than the most recent seed, the
      // seed is applied even though it was downloaded more than 30 days ago.
      {.days = 31, .binary_build_time = now - base::Days(32)}};

  for (const TestParams& test_case : test_cases) {
    const base::Time seed_fetch_time = now - base::Days(test_case.days);
    // The seed should be used, so the safe seed manager should be informed of
    // the active seed state.
    NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
    ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
        .WillByDefault(Return(false));
    EXPECT_CALL(
        safe_seed_manager,
        DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                             kTestSeedMilestone, _, seed_fetch_time))
        .Times(1);

    TestVariationsServiceClient variations_service_client;
    TestVariationsFieldTrialCreator field_trial_creator(
        &prefs_, &variations_service_client, &safe_seed_manager);
    field_trial_creator.SetBuildTime(test_case.binary_build_time);

    // Simulate the seed being stored.
    prefs_.SetTime(prefs::kVariationsLastFetchTime, seed_fetch_time);

    // Simulate a seed from an earlier (i.e. valid) milestone.
    prefs_.SetInteger(prefs::kVariationsSeedMilestone, kTestSeedMilestone);

    // Check that field trials are created from the seed. Since the test study
    // has only one experiment with 100% probability weight, we must be part of
    // it.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
    EXPECT_EQ(kTestSeedExperimentName,
              base::FieldTrialList::FindFullName(kTestSeedStudyName));

    // Verify metrics.
    histogram_tester.ExpectUniqueSample("Variations.CreateTrials.SeedExpiry",
                                        VariationsSeedExpiry::kNotExpired, 1);
    int freshness_in_minutes = test_case.days * 24 * 60;
    histogram_tester.ExpectUniqueSample("Variations.SeedFreshness",
                                        freshness_in_minutes, 1);
    histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                        SeedUsage::kRegularSeedUsed, 1);
    histogram_tester.ExpectUniqueSample("Variations.AppliedSeed.Size",
                                        strlen(kTestSeedSerializedData), 1);

    ResetFeatureList();
  }
}

TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ValidSeed_NoLastFetchTime) {
  DisableTestingConfig();

  // With a valid seed on first run, the safe seed manager should be informed of
  // the active seed state. The last fetch time in this case is expected to be
  // inferred to be recent.
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));
  const base::Time start_time = base::Time::Now();
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                                   _, _, Ge(start_time)))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  // Simulate a first run by leaving |prefs::kVariationsLastFetchTime| empty.
  EXPECT_EQ(0, prefs_.GetInt64(prefs::kVariationsLastFetchTime));

  // Check that field trials are created from the seed. Since the test study has
  // only one experiment with 100% probability weight, we must be part of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
  EXPECT_EQ(base::FieldTrialList::FindFullName(kTestSeedStudyName),
            kTestSeedExperimentName);

  // Verify metrics. The seed freshness metric should not be recorded on first
  // run.
  histogram_tester.ExpectUniqueSample("Variations.CreateTrials.SeedExpiry",
                                      VariationsSeedExpiry::kFetchTimeMissing,
                                      1);
  histogram_tester.ExpectTotalCount("Variations.SeedFreshness", 0);
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kRegularSeedUsed, 1);
}

// Verify that a regular seed can be used when the milestone with which the seed
// was fetched is unknown. This can happen if the seed was fetched before the
// milestone pref was added.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ValidSeed_NoMilestone) {
  DisableTestingConfig();

  // The regular seed should be used, so the safe seed manager should be
  // informed of the active seed state.
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));
  const int minutes = 45;
  const base::Time seed_fetch_time = base::Time::Now() - base::Minutes(minutes);
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                                   0, _, seed_fetch_time))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  // Simulate the seed being stored.
  prefs_.SetTime(prefs::kVariationsLastFetchTime, seed_fetch_time);

  // Simulate the absence of a milestone by leaving
  // |prefs::kVariationsSeedMilestone| empty.
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kVariationsSeedMilestone));

  // Check that field trials are created from the seed. Since the test study has
  // only one experiment with 100% probability weight, we must be part of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
  EXPECT_EQ(base::FieldTrialList::FindFullName(kTestSeedStudyName),
            kTestSeedExperimentName);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.CreateTrials.SeedExpiry",
                                      VariationsSeedExpiry::kNotExpired, 1);
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness", minutes, 1);
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kRegularSeedUsed, 1);
}

TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ExpiredSeed) {
  DisableTestingConfig();

  // When the seed is older than 30 days and older than the binary, no field
  // trials should be created from the seed. Hence, no active state should be
  // passed to the safe seed manager.
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);
  const base::Time now = base::Time::Now();
  field_trial_creator.SetBuildTime(now);

  // Simulate an expired seed. For a seed to be expired, it must be older than
  // 30 days and be older than the binary.
  const base::Time seed_date = now - base::Days(31);
  prefs_.SetTime(prefs::kVariationsLastFetchTime, seed_date);

  // Check that field trials are not created from the expired seed.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify metrics. The seed freshness metric should not be recorded for an
  // expired seed.
  histogram_tester.ExpectUniqueSample("Variations.CreateTrials.SeedExpiry",
                                      VariationsSeedExpiry::kExpired, 1);
  histogram_tester.ExpectTotalCount("Variations.SeedFreshness", 0);
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kExpiredRegularSeedNotUsed, 1);
}

// Verify that a regular seed is not used when the milestone with which it was
// fetched is greater than the client's milestone.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_FutureMilestone) {
  DisableTestingConfig();
  const int future_seed_milestone = 7890;

  // When the seed is associated with a future milestone (relative to the
  // client's milestone), no field trials should be created from the seed.
  // Hence, no active state should be passed to the safe seed manager.
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);
  const base::Time now = base::Time::Now();
  field_trial_creator.SetBuildTime(now);

  // Simulate a seed from a future milestone.
  prefs_.SetInteger(prefs::kVariationsSeedMilestone, future_seed_milestone);

  // Check that field trials are not created from the seed.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedUsage", SeedUsage::kRegularSeedForFutureMilestoneNotUsed,
      1);
}

// Verify that unexpired safe seeds are used.
TEST_F(FieldTrialCreatorTest,
       SetUpFieldTrials_ValidSafeSeed_NewBinaryUsesSeed) {
  DisableTestingConfig();
  const base::Time now = base::Time::Now();

  std::vector<TestParams> test_cases = {
      // Verify that when (i) safe mode is triggered and (ii) the binary is
      // newer than the safe seed, the safe seed is applied as long as it was
      // downloaded within the last 30 days.
      {.days = 30, .binary_build_time = now},
      // Verify that when (i) safe mode is triggered and (ii) the binary is
      // older than the safe seed, the safe seed is applied even though it was
      // downloaded more than 30 days ago.
      {.days = 31, .binary_build_time = now - base::Days(32)}};

  for (const TestParams& test_case : test_cases) {
    // With a valid safe seed, the safe seed manager should not be informed of
    // the active seed state. This is an optimization to avoid saving a safe
    // seed when already running in safe mode.
    NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
    ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
        .WillByDefault(Return(true));
    EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _))
        .Times(0);

    TestVariationsServiceClient variations_service_client;
    TestVariationsFieldTrialCreator field_trial_creator(
        &prefs_, &variations_service_client, &safe_seed_manager);
    field_trial_creator.SetBuildTime(test_case.binary_build_time);

    // Simulate the safe seed being stored.
    const base::Time seed_fetch_time = now - base::Days(test_case.days);
    prefs_.SetTime(prefs::kVariationsSafeSeedFetchTime, seed_fetch_time);

    // Check that field trials are created from the safe seed. Since the test
    // study has only one experiment with 100% probability weight, we must be
    // part of it.
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
    EXPECT_EQ(kTestSafeSeedExperimentName,
              base::FieldTrialList::FindFullName(kTestSeedStudyName));

    // Verify metrics.
    histogram_tester.ExpectUniqueSample(
        "Variations.SafeMode.CreateTrials.SeedExpiry",
        VariationsSeedExpiry::kNotExpired, 1);
    int freshness_in_minutes = test_case.days * 24 * 60;
    histogram_tester.ExpectUniqueSample("Variations.SeedFreshness",
                                        freshness_in_minutes, 1);
    histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                        SeedUsage::kSafeSeedUsed, 1);

    ResetFeatureList();
  }
}

// Verify that Chrome applies the regular variations seed when Chrome should run
// in variations safe mode but the safe seed is empty.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_EmptySafeSeed_UsesRegularSeed) {
  DisableTestingConfig();

  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));

  const base::Time recent_time = base::Time::Now() - base::Minutes(17);
  prefs_.SetTime(prefs::kVariationsLastFetchTime, recent_time);
  // When using the regular seed, the safe seed manager should be informed of
  // the active seed state.
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                                   _, _, recent_time))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);
  field_trial_creator.seed_store()->set_has_empty_safe_seed(true);

  // Check that field trials are created from the regular seed. Since the test
  // study has only one experiment with 100% probability weight, we must be part
  // of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.CreateTrials.SeedExpiry",
                                      VariationsSeedExpiry::kNotExpired, 1);
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness", 17, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedUsage",
      SeedUsage::kRegularSeedUsedAfterEmptySafeSeedLoaded, 1);
}

// Verify that Chrome does not apply a variations seed when Chrome should run in
// variations safe mode and a safe seed cannot be loaded.
TEST_F(FieldTrialCreatorTest,
       SetUpFieldTrials_CorruptedSafeSeed_DoesNotUseSeed) {
  DisableTestingConfig();

  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));

  // When falling back to client-side defaults, the safe seed manager should not
  // be informed of the active seed state.
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);
  field_trial_creator.seed_store()->set_has_corrupted_safe_seed(true);

  base::HistogramTester histogram_tester;

  // Verify that field trials were not set up.
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that Chrome did not apply the safe seed.
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kCorruptedSafeSeedNotUsed, 1);
}

// Verify that valid safe seeds with missing download times are applied.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ValidSafeSeed_NoLastFetchTime) {
  DisableTestingConfig();

  // With a valid safe seed, the safe seed manager should not be informed of the
  // active seed state. This is an optimization to avoid saving a safe seed when
  // already running in safe mode.
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  // Verify that the safe seed does not have a fetch time.
  EXPECT_EQ(0, prefs_.GetInt64(prefs::kVariationsSafeSeedFetchTime));

  // Check that field trials are created from the safe seed. Since the test
  // study has only one experiment with 100% probability weight, we must be part
  // of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
  EXPECT_EQ(kTestSafeSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));

  // Verify metrics. The freshness should not be recorded when the fetch time is
  // missing.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.CreateTrials.SeedExpiry",
      VariationsSeedExpiry::kFetchTimeMissing, 1);
  histogram_tester.ExpectTotalCount("Variations.SeedFreshness", 0);
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kSafeSeedUsed, 1);
}

// Verify that no seed is applied when (i) safe mode is triggered and (ii) the
// loaded safe seed has expired.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ExpiredSafeSeed) {
  DisableTestingConfig();

  // The safe seed manager should not be informed of the active seed state.
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);
  const base::Time now = base::Time::Now();
  field_trial_creator.SetBuildTime(now);

  // Simulate an expired seed. For a seed to be expired, it must be older than
  // 30 days and be older than the binary.
  const base::Time seed_date = now - base::Days(31);
  prefs_.SetTime(prefs::kVariationsSafeSeedFetchTime, seed_date);

  // Check that field trials are not created from the expired seed.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify metrics. The seed freshness metric should not be recorded for an
  // expired seed.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.CreateTrials.SeedExpiry",
      VariationsSeedExpiry::kExpired, 1);
  histogram_tester.ExpectTotalCount("Variations.SeedFreshness", 0);
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kExpiredSafeSeedNotUsed, 1);
}

// Verify that no seed is applied when (i) safe mode is triggered and (ii) the
// loaded safe seed was fetched with a future milestone.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_SafeSeedForFutureMilestone) {
  DisableTestingConfig();
  const int future_seed_milestone = 7890;

  // The safe seed manager should not be informed of the active seed state.
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  // Simulate a safe seed that was fetched with a future milestone.
  prefs_.SetInteger(prefs::kVariationsSafeSeedMilestone, future_seed_milestone);

  // Check that field trials are not created from the safe seed.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedUsage", SeedUsage::kSafeSeedForFutureMilestoneNotUsed, 1);
}

#if defined(OS_ANDROID)
// This is a regression test for crbug/829527.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_LoadsCountryOnFirstRun) {
  DisableTestingConfig();

  // Simulate having received a seed in Java during First Run.
  const base::Time one_day_ago = base::Time::Now() - base::Days(1);
  auto initial_seed = std::make_unique<SeedResponse>();
  initial_seed->data = SerializeSeed(CreateTestSeedWithCountryFilter());
  initial_seed->signature = kTestSeedSignature;
  initial_seed->country = kTestSeedCountry;
  initial_seed->date = one_day_ago.ToJavaTime();
  initial_seed->is_gzip_compressed = false;

  TestVariationsServiceClient variations_service_client;
  TestPlatformFieldTrials platform_field_trials;
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

  // Note: Unlike other tests, this test does not mock out the seed store, since
  // the interaction between these two classes is what's being tested.
  auto seed_store = std::make_unique<VariationsSeedStore>(
      &prefs_, std::move(initial_seed),
      /*signature_verification_enabled=*/false);
  VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client, std::move(seed_store), UIStringOverrider());

  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/true,
                                                           /*enabled=*/true);
  auto metrics_state_manager = metrics::MetricsStateManager::Create(
      &prefs_, &enabled_state_provider, std::wstring(), base::FilePath());

  // Check that field trials are created from the seed. The test seed contains a
  // single study with an experiment targeting 100% of users in India. Since
  // |initial_seed| included the country code for India, this study should be
  // active.
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials(
      /*variation_ids=*/std::vector<std::string>(),
      std::vector<base::FeatureList::FeatureOverrideInfo>(),
      /*low_entropy_provider=*/nullptr, std::make_unique<base::FeatureList>(),
      metrics_state_manager.get(), &platform_field_trials, &safe_seed_manager,
      /*low_entropy_source_value=*/absl::nullopt));

  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));
}

// Tests that the hardware class is set on Android.
TEST_F(FieldTrialCreatorTest, ClientFilterableState_HardwareClass) {
  NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  const base::Version& current_version = version_info::GetVersion();
  EXPECT_TRUE(current_version.IsValid());

  std::unique_ptr<ClientFilterableState> client_filterable_state =
      field_trial_creator.GetClientFilterableStateForVersion(current_version);
  EXPECT_NE(client_filterable_state->hardware_class, std::string());
}
#endif  // defined(OS_ANDROID)

TEST_F(FieldTrialCreatorSafeModeExperimentTest, OptOutOfExperiment) {
  std::unique_ptr<PrefService> pref_service(CreatePrefService());

  // Ensure that variations safe mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(pref_service.get());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

  version_info::Channel channel = version_info::Channel::DEV;
  NiceMock<MockVariationsServiceClient> variations_service_client;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  TestVariationsFieldTrialCreator field_trial_creator(
      pref_service.get(), &variations_service_client, &safe_seed_manager,
      channel);

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials(
      /*extend_variations_safe_mode=*/false));

  // Verify that the experiment is not active and that the WritePrefsTime metric
  // was not recorded.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));
  histogram_tester.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 0);

  base::FeatureList::ClearInstanceForTesting();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FieldTrialCreatorTestWithStartupVisibility,
    ::testing::Values(
        StartupVisibilityTestParams{
            .test_name = "UnknownVisibility",
            .startup_visibility = metrics::StartupVisibility::kUnknown,
            .extend_safe_mode = true},
        StartupVisibilityTestParams{
            .test_name = "BackgroundVisibility",
            .startup_visibility = metrics::StartupVisibility::kBackground,
            .extend_safe_mode = false},
        StartupVisibilityTestParams{
            .test_name = "ForegroundVisibility",
            .startup_visibility = metrics::StartupVisibility::kForeground,
            .extend_safe_mode = true}),
    [](const ::testing::TestParamInfo<StartupVisibilityTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(FieldTrialCreatorTestWithStartupVisibility,
       SkipExperimentInBackgroundSessions) {
  std::unique_ptr<PrefService> pref_service(CreatePrefService());

  // Ensure that variations safe mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(pref_service.get());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

  NiceMock<MockVariationsServiceClient> variations_service_client;
  version_info::Channel channel = version_info::Channel::DEV;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  StartupVisibilityTestParams params = GetParam();
  TestVariationsFieldTrialCreator field_trial_creator(
      pref_service.get(), &variations_service_client, &safe_seed_manager,
      channel, base::FilePath(), params.startup_visibility);
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that MaybeExtendVariationsSafeMode() was (or wasn't) called.
  EXPECT_EQ(field_trial_creator.was_maybe_extend_variations_safe_mode_called(),
            params.extend_safe_mode);

  base::FeatureList::ClearInstanceForTesting();
}

// TODO(b/184937096): Update this test if and when the extended variations safe
// mode experiment is rolled out to beta or stable.
TEST_F(FieldTrialCreatorSafeModeExperimentTest,
       DisableExperimentOnSelectChannels) {
  std::unique_ptr<PrefService> pref_service(CreatePrefService());

  // Ensure that variations safe mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(pref_service.get());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

// For desktop and iOS, the Extended Variations Safe Mode experiment is enabled
// on pre-stable channels; for Android Chrome, on canary and dev.
#if defined(OS_ANDROID)
  std::vector<version_info::Channel> channels = {version_info::Channel::BETA,
                                                 version_info::Channel::STABLE};
#else
  std::vector<version_info::Channel> channels = {version_info::Channel::STABLE};
#endif  // defined(OS_ANDROID)

  for (const version_info::Channel channel : channels) {
    NiceMock<MockVariationsServiceClient> variations_service_client;
    ON_CALL(variations_service_client, GetChannel())
        .WillByDefault(Return(channel));

    TestVariationsFieldTrialCreator field_trial_creator(
        pref_service.get(), &variations_service_client, &safe_seed_manager,
        channel);

    base::HistogramTester histogram_tester;
    ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

    // Verify that the experiment is not active.
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));

    // Check that no prefs were written and that the WritePrefsTime metric was
    // not recorded.
    std::string pref_file_contents;
    ASSERT_TRUE(base::ReadFileToString(prefs_file(), &pref_file_contents));
    EXPECT_EQ(kEmptyPrefsFile, pref_file_contents);
    histogram_tester.ExpectTotalCount(
        "Variations.ExtendedSafeMode.WritePrefsTime", 0);

    base::FeatureList::ClearInstanceForTesting();
  }
}

TEST_F(FieldTrialCreatorSafeModeExperimentTest,
       EnableExperimentOnCanary_ControlGroup) {
  std::unique_ptr<PrefService> pref_service(CreatePrefService());

  NiceMock<MockVariationsServiceClient> variations_service_client;
  version_info::Channel channel = version_info::Channel::CANARY;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  // Ensure that variations safe mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(pref_service.get());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

  // Assign the client to a specific experiment group before creating the
  // TestVariationsFieldTrialCreator so that the CleanExitBeacon uses the
  // desired group.
  int active_group = SetUpExtendedSafeModeExperiment(kControlGroup);
  TestVariationsFieldTrialCreator field_trial_creator(
      pref_service.get(), &variations_service_client, &safe_seed_manager,
      channel);

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that the field trial is active and that the client is in the
  // control group.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));
  EXPECT_EQ(active_group,
            base::FieldTrialList::FindValue(kExtendedSafeModeTrial));

  // Check that no prefs were written and that the WritePrefsTime metric was
  // recorded.
  std::string pref_file_contents;
  ASSERT_TRUE(base::ReadFileToString(prefs_file(), &pref_file_contents));
  EXPECT_EQ(kEmptyPrefsFile, pref_file_contents);
  histogram_tester.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 1);

  // Verify that the Variations Safe Mode file does not exist.
  EXPECT_FALSE(base::PathExists(
      user_data_dir_path().Append(variations::kVariationsFilename)));
}

TEST_F(FieldTrialCreatorSafeModeExperimentTest,
       EnableExperimentOnDev_SignalAndWriteViaFileUtilGroup) {
  std::unique_ptr<PrefService> pref_service(CreatePrefService());

  NiceMock<MockVariationsServiceClient> variations_service_client;
  version_info::Channel channel = version_info::Channel::DEV;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  // Ensure that variations safe mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(pref_service.get());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

  // Assign the client to a specific experiment group before creating the
  // TestVariationsFieldTrialCreator so that the CleanExitBeacon uses the
  // desired group.
  int active_group =
      SetUpExtendedSafeModeExperiment(kSignalAndWriteViaFileUtilGroup);
  TestVariationsFieldTrialCreator field_trial_creator(
      pref_service.get(), &variations_service_client, &safe_seed_manager,
      channel, user_data_dir_path());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that the field trial is active and that the client is in the
  // SignalAndWriteViaFileUtil group.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));
  EXPECT_EQ(active_group,
            base::FieldTrialList::FindValue(kExtendedSafeModeTrial));

  // Verify that the Variations Safe Mode file was written and that the contents
  // are correct.
  const base::FilePath variations_file_path =
      user_data_dir_path().Append(variations::kVariationsFilename);
  EXPECT_TRUE(base::PathExists(variations_file_path));
  std::string beacon_file_contents;
  ASSERT_TRUE(
      base::ReadFileToString(variations_file_path, &beacon_file_contents));
  EXPECT_EQ(beacon_file_contents,
            "{\"user_experience_metrics.stability.exited_cleanly\":false,"
            "\"variations_crash_streak\":0}");

  // Verify that the WritePrefsTime metric was recorded.
  histogram_tester.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 1);

  // Check that no prefs were written to the Local State file.
  std::string pref_file_contents;
  ASSERT_TRUE(base::ReadFileToString(prefs_file(), &pref_file_contents));
  EXPECT_EQ(kEmptyPrefsFile, pref_file_contents);
}

}  // namespace variations
