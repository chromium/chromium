// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <cstring>
#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

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

  // Returns false by default.
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

// TODO(crbug/1167566): Remove when fake VariationsServiceClient created.
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

  bool LoadSafeSeed(VariationsSeed* seed,
                    ClientFilterableState* client_state) override {
    if (has_unloadable_safe_seed_)
      return false;

    *seed = CreateTestSafeSeed();
    return true;
  }

  void set_has_unloadable_safe_seed(bool is_unloadable) {
    has_unloadable_safe_seed_ = is_unloadable;
  }

 private:
  // Whether to simulate having an unloadable (e.g. corrupted, empty, etc.) safe
  // seed.
  bool has_unloadable_safe_seed_ = false;
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
  bool SetUpFieldTrials() {
    TestPlatformFieldTrials platform_field_trials;
    return VariationsFieldTrialCreator::SetUpFieldTrials(
        /*variation_ids=*/std::vector<std::string>(),
        std::vector<base::FeatureList::FeatureOverrideInfo>(),
        /*low_entropy_provider=*/nullptr, std::make_unique<base::FeatureList>(),
        metrics_state_manager_.get(), &platform_field_trials,
        safe_seed_manager_, /*low_entropy_source_value=*/absl::nullopt);
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

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
  // We override this method so that a mock testing config is used instead of
  // the one defined in fieldtrial_testing_config.json.
  void ApplyFieldTrialTestingConfig(base::FeatureList* feature_list) override {
    AssociateParamsFromFieldTrialConfig(
        kTestingConfig,
        base::BindRepeating(&TestVariationsFieldTrialCreator::OverrideUIString,
                            base::Unretained(this)),
        GetPlatform(), GetCurrentFormFactor(), feature_list);
  }
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

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
    metrics::MetricsService::RegisterPrefs(local_state_.registry());
    VariationsService::RegisterPrefs(local_state_.registry());
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

  PrefService* local_state() { return &local_state_; }

 protected:
  TestingPrefServiceSimple local_state_;

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
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath user_data_dir_path() const {
    return temp_dir_.GetPath();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
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

class SafeModeExperimentTestByChannel
    : public FieldTrialCreatorSafeModeExperimentTest,
      public ::testing::WithParamInterface<version_info::Channel> {};

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
    NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
    EXPECT_CALL(
        safe_seed_manager,
        DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                             kTestSeedMilestone, _, seed_fetch_time))
        .Times(1);

    TestVariationsServiceClient variations_service_client;
    TestVariationsFieldTrialCreator field_trial_creator(
        local_state(), &variations_service_client, &safe_seed_manager);
    field_trial_creator.SetBuildTime(test_case.binary_build_time);

    // Simulate the seed being stored.
    local_state()->SetTime(prefs::kVariationsLastFetchTime, seed_fetch_time);

    // Simulate a seed from an earlier (i.e. valid) milestone.
    local_state()->SetInteger(prefs::kVariationsSeedMilestone,
                              kTestSeedMilestone);

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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  const base::Time start_time = base::Time::Now();
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                                   _, _, Ge(start_time)))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Simulate a first run by leaving |prefs::kVariationsLastFetchTime| empty.
  EXPECT_EQ(0, local_state()->GetInt64(prefs::kVariationsLastFetchTime));

  // Check that field trials are created from the seed. Since the test study has
  // only one experiment with 100% probability weight, we must be part of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
  EXPECT_EQ(base::FieldTrialList::FindFullName(kTestSeedStudyName),
            kTestSeedExperimentName);

  // Verify metrics. The seed freshness metric should be recorded with a value
  // of 0 on first run.
  histogram_tester.ExpectUniqueSample("Variations.CreateTrials.SeedExpiry",
                                      VariationsSeedExpiry::kFetchTimeMissing,
                                      1);
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness", 0, 1);
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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  const int minutes = 45;
  const base::Time seed_fetch_time = base::Time::Now() - base::Minutes(minutes);
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                                   0, _, seed_fetch_time))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Simulate the seed being stored.
  local_state()->SetTime(prefs::kVariationsLastFetchTime, seed_fetch_time);

  // Simulate the absence of a milestone by leaving
  // |prefs::kVariationsSeedMilestone| empty.
  EXPECT_EQ(0, local_state()->GetInteger(prefs::kVariationsSeedMilestone));

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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);
  const base::Time now = base::Time::Now();
  field_trial_creator.SetBuildTime(now);

  // Simulate an expired seed. For a seed to be expired, it must be older than
  // 30 days and be older than the binary.
  const base::Time seed_date = now - base::Days(31);
  local_state()->SetTime(prefs::kVariationsLastFetchTime, seed_date);

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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);
  const base::Time now = base::Time::Now();
  field_trial_creator.SetBuildTime(now);

  // Simulate a seed from a future milestone.
  local_state()->SetInteger(prefs::kVariationsSeedMilestone,
                            future_seed_milestone);

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
    NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
    ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
        .WillByDefault(Return(true));
    EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _))
        .Times(0);

    TestVariationsServiceClient variations_service_client;
    TestVariationsFieldTrialCreator field_trial_creator(
        local_state(), &variations_service_client, &safe_seed_manager);
    field_trial_creator.SetBuildTime(test_case.binary_build_time);

    // Simulate the safe seed being stored.
    const base::Time seed_fetch_time = now - base::Days(test_case.days);
    local_state()->SetTime(prefs::kVariationsSafeSeedFetchTime,
                           seed_fetch_time);

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

// Verify that Chrome does not apply a variations seed when Chrome should run in
// Variations Safe Mode but the safe seed is unloadable.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_UnloadableSafeSeedNotUsed) {
  DisableTestingConfig();

  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));

  // When falling back to client-side defaults, the safe seed manager should not
  // be informed of the active seed state.
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);
  field_trial_creator.seed_store()->set_has_unloadable_safe_seed(true);

  base::HistogramTester histogram_tester;

  // Verify that field trials were not set up.
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that Chrome did not apply the safe seed.
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kUnloadableSafeSeedNotUsed, 1);
}

// Verify that valid safe seeds with missing download times are applied.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ValidSafeSeed_NoLastFetchTime) {
  DisableTestingConfig();

  // With a valid safe seed, the safe seed manager should not be informed of the
  // active seed state. This is an optimization to avoid saving a safe seed when
  // already running in safe mode.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Verify that the safe seed does not have a fetch time.
  EXPECT_EQ(0, local_state()->GetInt64(prefs::kVariationsSafeSeedFetchTime));

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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);
  const base::Time now = base::Time::Now();
  field_trial_creator.SetBuildTime(now);

  // Simulate an expired seed. For a seed to be expired, it must be older than
  // 30 days and be older than the binary.
  const base::Time seed_date = now - base::Days(31);
  local_state()->SetTime(prefs::kVariationsSafeSeedFetchTime, seed_date);

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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Simulate a safe seed that was fetched with a future milestone.
  local_state()->SetInteger(prefs::kVariationsSafeSeedMilestone,
                            future_seed_milestone);

  // Check that field trials are not created from the safe seed.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedUsage", SeedUsage::kSafeSeedForFutureMilestoneNotUsed, 1);
}

#if BUILDFLAG(IS_ANDROID)
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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  // Note: Unlike other tests, this test does not mock out the seed store, since
  // the interaction between these two classes is what's being tested.
  auto seed_store = std::make_unique<VariationsSeedStore>(
      local_state(), std::move(initial_seed),
      /*signature_verification_enabled=*/false);
  VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client, std::move(seed_store), UIStringOverrider());

  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/true,
                                                           /*enabled=*/true);
  auto metrics_state_manager = metrics::MetricsStateManager::Create(
      local_state(), &enabled_state_provider, std::wstring(), base::FilePath());

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
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  const base::Version& current_version = version_info::GetVersion();
  EXPECT_TRUE(current_version.IsValid());

  std::unique_ptr<ClientFilterableState> client_filterable_state =
      field_trial_creator.GetClientFilterableStateForVersion(current_version);
  EXPECT_NE(client_filterable_state->hardware_class, std::string());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
// Used to create a TestVariationsFieldTrialCreator with a valid unexpired seed.
std::unique_ptr<TestVariationsFieldTrialCreator>
SetUpFieldTrialCreatorWithValidSeed(
    PrefService* local_state,
    TestVariationsServiceClient* variations_service_client,
    NiceMock<MockSafeSeedManager>* safe_seed_manager) {
  // Set up a valid unexpired seed.
  const base::Time now = base::Time::Now();
  const base::Time seed_fetch_time = now - base::Days(1);
  std::unique_ptr<TestVariationsFieldTrialCreator> field_trial_creator =
      std::make_unique<TestVariationsFieldTrialCreator>(
          local_state, variations_service_client, safe_seed_manager);
  field_trial_creator->SetBuildTime(now);
  // Simulate the seed being stored.
  local_state->SetTime(prefs::kVariationsLastFetchTime, seed_fetch_time);
  // Simulate a seed from an earlier (i.e. valid) milestone.
  local_state->SetInteger(prefs::kVariationsSeedMilestone, kTestSeedMilestone);
  return field_trial_creator;
}

// Verifies that a valid seed is used instead of the testing config when we
// disable it.
TEST_F(FieldTrialCreatorTest, NotSetUpFieldTrialConfig_ValidSeed) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Note: Non-Google Chrome branded builds do not disable the testing config by
  // default. We explicitly disable it.
  DisableTestingConfig();
#endif

  // Create a field trial creator with a valid unexpired seed.
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  std::unique_ptr<TestVariationsFieldTrialCreator> field_trial_creator =
      SetUpFieldTrialCreatorWithValidSeed(
          local_state(), &variations_service_client, &safe_seed_manager);

  // Verify that |SetUpFieldTrials| uses the seed. |SetUpFieldTrials| returns
  // true if it used a seed.
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                                   kTestSeedMilestone, _, _))
      .Times(1);
  EXPECT_TRUE(field_trial_creator->SetUpFieldTrials());
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that the |UnitTest| trial from the field trial testing config was
  // not registered.
  ASSERT_FALSE(base::FieldTrialList::TrialExists("UnitTest"));

  ResetVariations();
}

// Verifies that field trial testing config is used when enabled, even when
// there is a valid unexpired seed.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrialConfig_ValidSeed) {
  EnableTestingConfig();

  // Create a field trial creator with a valid unexpired seed.
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  std::unique_ptr<TestVariationsFieldTrialCreator> field_trial_creator =
      SetUpFieldTrialCreatorWithValidSeed(
          local_state(), &variations_service_client, &safe_seed_manager);

  // Verify that |SetUpFieldTrials| does not use the seed, despite it being
  // valid and unexpired. |SetUpFieldTrials| returns false if it did not use a
  // seed.
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);
  EXPECT_FALSE(field_trial_creator->SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that the |UnitTest| trial from the field trial testing config has
  // been registered, and that the group name is |Enabled|.
  ASSERT_EQ("Enabled", base::FieldTrialList::FindFullName("UnitTest"));

  // Verify the |UnitTest| trial params.
  base::FieldTrialParams params;
  ASSERT_TRUE(base::GetFieldTrialParams("UnitTest", &params));
  ASSERT_EQ(1U, params.size());
  EXPECT_EQ("1", params["x"]);

  // Verify that the |UnitTestEnabled| feature is active.
  const base::Feature kFeature1{"UnitTestEnabled",
                                base::FEATURE_DISABLED_BY_DEFAULT};
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeature1));

  ResetVariations();
}

// Verifies that trials from the testing config and the |--force-fieldtrials|
// switch are registered when they are both used (assuming there are no
// conflicts).
TEST_F(FieldTrialCreatorTest, SetUpFieldTrialConfig_ForceFieldTrials) {
  EnableTestingConfig();

  // Simulate passing |--force-fieldtrials="UnitTest2/Enabled"|.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kForceFieldTrials, "UnitTest2/Enabled");
  // Simulate passing |--force-fieldtrial-params="UnitTest2.Enabled:y/1"|.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceFieldTrialParams, "UnitTest2.Enabled:y/1");
  // Simulate passing |--enable-features="UnitTest2Enabled<UnitTest2"|.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kEnableFeatures, "UnitTest2Enabled<UnitTest2");

  // Create a field trial creator with a valid unexpired seed.
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  std::unique_ptr<TestVariationsFieldTrialCreator> field_trial_creator =
      SetUpFieldTrialCreatorWithValidSeed(
          local_state(), &variations_service_client, &safe_seed_manager);

  // Verify that |SetUpFieldTrials| does not use the seed, despite it being
  // valid and unexpired. |SetUpFieldTrials| returns false if it did not use a
  // seed.
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);
  EXPECT_FALSE(field_trial_creator->SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that the |UnitTest| trial from the field trial testing config has
  // been registered, and that the group name is |Enabled|.
  ASSERT_EQ("Enabled", base::FieldTrialList::FindFullName("UnitTest"));
  // Verify that the |UnitTest2| trial from the |--force-fieldtrials| switch has
  // been registered, and that the group name is |Enabled|.
  ASSERT_EQ("Enabled", base::FieldTrialList::FindFullName("UnitTest2"));

  // Verify the |UnitTest| trial params.
  base::FieldTrialParams params;
  ASSERT_TRUE(base::GetFieldTrialParams("UnitTest", &params));
  ASSERT_EQ(1U, params.size());
  EXPECT_EQ("1", params["x"]);
  // Verify the |UnitTest2| trial params.
  base::FieldTrialParams params2;
  ASSERT_TRUE(base::GetFieldTrialParams("UnitTest2", &params2));
  ASSERT_EQ(1U, params2.size());
  EXPECT_EQ("1", params2["y"]);

  // Verify that the |UnitTestEnabled| and |UnitTestEnabled2| features are
  // active.
  const base::Feature kFeature1{"UnitTestEnabled",
                                base::FEATURE_DISABLED_BY_DEFAULT};
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeature1));
  const base::Feature kFeature2{"UnitTest2Enabled",
                                base::FEATURE_DISABLED_BY_DEFAULT};
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeature2));

  ResetVariations();
}

// Verifies that when field trial testing config is used, trials and groups
// specified using |--force-fieldtrials| take precedence if they specify the
// same trials but different groups.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrialConfig_ForceFieldTrialsOverride) {
  EnableTestingConfig();

  // Simulate passing |--force-fieldtrials="UnitTest/Disabled"| switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kForceFieldTrials, "UnitTest/Disabled");

  // Create a field trial creator with a valid unexpired seed.
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  std::unique_ptr<TestVariationsFieldTrialCreator> field_trial_creator =
      SetUpFieldTrialCreatorWithValidSeed(
          local_state(), &variations_service_client, &safe_seed_manager);

  // Verify that |SetUpFieldTrials| does not use the seed, despite it being
  // valid and unexpired. |SetUpFieldTrials| returns false if it did not use a
  // seed.
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);
  EXPECT_FALSE(field_trial_creator->SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that the |UnitTest| trial from the |--force-fieldtrials| switch (and
  // not from the field trial testing config) has been registered, and that the
  // group name is |Disabled|.
  ASSERT_EQ("Disabled", base::FieldTrialList::FindFullName("UnitTest"));

  // Verify that the |UnitTest| trial params from the field trial testing config
  // were not used. |GetFieldTrialParams| returns false if no parameters are
  // defined for a specified trial.
  base::FieldTrialParams params;
  ASSERT_FALSE(base::GetFieldTrialParams("UnitTest", &params));

  // Verify that the |UnitTestEnabled| feature from the testing config is not
  // active.
  const base::Feature kFeature1{"UnitTestEnabled",
                                base::FEATURE_DISABLED_BY_DEFAULT};
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeature1));

  ResetVariations();
}

// Verifies that when field trial testing config is used, params specified using
// |--force-fieldtrial-params| take precedence if they specify the same trial
// and group.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrialConfig_ForceFieldTrialParams) {
  EnableTestingConfig();

  // Simulate passing |--force-fieldtrial-params="UnitTest.Enabled:x/2/y/2"|
  // switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceFieldTrialParams, "UnitTest.Enabled:x/2/y/2");

  // Create a field trial creator with a valid unexpired seed.
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  std::unique_ptr<TestVariationsFieldTrialCreator> field_trial_creator =
      SetUpFieldTrialCreatorWithValidSeed(
          local_state(), &variations_service_client, &safe_seed_manager);

  // Verify that |SetUpFieldTrials| does not use the seed, despite it being
  // valid and unexpired. |SetUpFieldTrials| returns false if it did not use a
  // seed.
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);
  EXPECT_FALSE(field_trial_creator->SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that the |UnitTest| trial from the field trial testing config has
  // been registered, and that the group name is |Enabled|.
  ASSERT_EQ("Enabled", base::FieldTrialList::FindFullName("UnitTest"));

  // Verify the |UnitTest| trial params, and that the
  // |--force-fieldtrial-params| took precedence over the params defined in the
  // field trial testing config.
  base::FieldTrialParams params;
  ASSERT_TRUE(base::GetFieldTrialParams("UnitTest", &params));
  ASSERT_EQ(2U, params.size());
  EXPECT_EQ("2", params["x"]);
  EXPECT_EQ("2", params["y"]);

  // Verify that the |UnitTestEnabled| feature is still active.
  const base::Feature kFeature1{"UnitTestEnabled",
                                base::FEATURE_DISABLED_BY_DEFAULT};
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeature1));

  ResetVariations();
}

class FieldTrialCreatorTestWithFeatures
    : public FieldTrialCreatorTest,
      public ::testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(All,
                         FieldTrialCreatorTestWithFeatures,
                         ::testing::Values(::switches::kEnableFeatures,
                                           ::switches::kDisableFeatures));

// Verifies that studies from field trial testing config should be ignored
// if they enable/disable features overridden by |--enable-features| or
// |--disable-features|.
TEST_P(FieldTrialCreatorTestWithFeatures,
       SetUpFieldTrialConfig_OverrideFeatures) {
  EnableTestingConfig();

  // Simulate passing either |--enable-features="UnitTestEnabled"| or
  // |--disable-features="UnitTestEnabled"| switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(GetParam(),
                                                            "UnitTestEnabled");

  // Create a field trial creator with a valid unexpired seed.
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  std::unique_ptr<TestVariationsFieldTrialCreator> field_trial_creator =
      SetUpFieldTrialCreatorWithValidSeed(
          local_state(), &variations_service_client, &safe_seed_manager);

  // Verify that |SetUpFieldTrials| does not use the seed, despite it being
  // valid and unexpired. |SetUpFieldTrials| returns false if it did not use a
  // seed.
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);
  EXPECT_FALSE(field_trial_creator->SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify that the |UnitTest| trial from the field trial testing config was
  // NOT registered. Even if the study |UnitTest| enables feature
  // |UnitTestEnabled|, and we pass |--enable-features="UnitTestEnabled"|, the
  // study should be disabled.
  EXPECT_FALSE(base::FieldTrialList::TrialExists("UnitTest"));

  // Verify that the |UnitTestEnabled| feature is enabled or disabled depending
  // on whether we passed it in |--enable-features| or |--disable-features|.
  const base::Feature kFeature1{"UnitTestEnabled",
                                base::FEATURE_DISABLED_BY_DEFAULT};
  EXPECT_EQ(GetParam() == ::switches::kEnableFeatures,
            base::FeatureList::IsEnabled(kFeature1));

  ResetVariations();
}
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

// Verify that providing an empty user data directory opts the client out of the
// Extended Variations Safe Mode experiment.
TEST_F(FieldTrialCreatorSafeModeExperimentTest, OptOutOfExperiment) {
  // Ensure that variations safe mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  // Specify a channel on which the Extended Variations Safe Mode experiment is
  // running.
  version_info::Channel channel = version_info::Channel::DEV;
  NiceMock<MockVariationsServiceClient> variations_service_client;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager, channel,
      base::FilePath());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that the experiment is not active and that Variations Safe Mode was
  // not extended.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));
  EXPECT_FALSE(
      field_trial_creator.was_maybe_extend_variations_safe_mode_called());
  histogram_tester.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 0);
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
  // Ensure that Variations Safe Mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  NiceMock<MockVariationsServiceClient> variations_service_client;
  version_info::Channel channel = version_info::Channel::DEV;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  StartupVisibilityTestParams params = GetParam();
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager, channel,
      user_data_dir_path(), params.startup_visibility);

  // Verify that the Extended Variations Safe Mode experiment is active to be
  // certain that Safe Mode is (or isn't) extended due to the StartupVisibility.
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));

  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());
  // Verify that MaybeExtendVariationsSafeMode() was (or wasn't) called.
  EXPECT_EQ(field_trial_creator.was_maybe_extend_variations_safe_mode_called(),
            params.extend_safe_mode);

  base::FeatureList::ClearInstanceForTesting();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SafeModeExperimentTestByChannel,
                         ::testing::Values(version_info::Channel::UNKNOWN,
                                           version_info::Channel::CANARY,
                                           version_info::Channel::DEV,
                                           version_info::Channel::BETA,
                                           version_info::Channel::STABLE));

// Verify that the Extended Variations Safe Mode experiment is active on all
// channels.
TEST_P(SafeModeExperimentTestByChannel, FieldTrialActivationIsValid) {
  // Ensure that Variations Safe Mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  version_info::Channel channel = GetParam();
  NiceMock<MockVariationsServiceClient> variations_service_client;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager, channel,
      user_data_dir_path());

  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());
  // Verify that the experiment is (or is not) active.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));
  EXPECT_TRUE(
      field_trial_creator.was_maybe_extend_variations_safe_mode_called());
}

TEST_F(FieldTrialCreatorSafeModeExperimentTest,
       ControlGroupDoesNotWriteBeaconFile) {
  NiceMock<MockVariationsServiceClient> variations_service_client;
  version_info::Channel channel = version_info::Channel::BETA;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  // Ensure that Variations Safe Mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  // Assign the client to a specific experiment group before creating the
  // TestVariationsFieldTrialCreator so that the CleanExitBeacon uses the
  // desired group.
  int active_group = SetUpExtendedSafeModeExperiment(kControlGroup);
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager, channel,
      user_data_dir_path());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that the field trial is active and that the client is in the
  // control group.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));
  EXPECT_EQ(active_group,
            base::FieldTrialList::FindValue(kExtendedSafeModeTrial));

  // Check metrics.
  histogram_tester.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 1);

  // Verify that the beacon file does not exist.
  EXPECT_FALSE(base::PathExists(
      user_data_dir_path().Append(variations::kVariationsFilename)));
}

TEST_F(FieldTrialCreatorSafeModeExperimentTest,
       ExperimentGroupWritesBeaconFile) {
  NiceMock<MockVariationsServiceClient> variations_service_client;
  version_info::Channel channel = version_info::Channel::BETA;
  ON_CALL(variations_service_client, GetChannel())
      .WillByDefault(Return(channel));

  // Ensure that Variations Safe Mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  // Assign the client to a specific experiment group before creating the
  // TestVariationsFieldTrialCreator so that the CleanExitBeacon uses the
  // desired group.
  int active_group =
      SetUpExtendedSafeModeExperiment(kSignalAndWriteViaFileUtilGroup);
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager, channel,
      user_data_dir_path());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that the field trial is active and that the client is in the
  // SignalAndWriteViaFileUtil group.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial));
  EXPECT_EQ(active_group,
            base::FieldTrialList::FindValue(kExtendedSafeModeTrial));

  // Verify that the beacon file was written and that the contents are correct.
  const base::FilePath variations_file_path =
      user_data_dir_path().Append(variations::kVariationsFilename);
  EXPECT_TRUE(base::PathExists(variations_file_path));
  std::string beacon_file_contents;
  ASSERT_TRUE(
      base::ReadFileToString(variations_file_path, &beacon_file_contents));
  EXPECT_EQ(beacon_file_contents,
            "{\"monitoring_stage\":2,"
            "\"user_experience_metrics.stability.exited_cleanly\":false,"
            "\"variations_crash_streak\":0}");

  // Verify metrics.
  histogram_tester.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 1, 1);
}

}  // namespace variations
