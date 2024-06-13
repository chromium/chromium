// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/limited_entropy_mode_gate.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_field_trial_creator_base.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
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
const char kTestLimitedLayerStudyName[] = "test_study_in_limited_layer";
const char kTestSeedExperimentName[] = "abc";
const char kTestSafeSeedExperimentName[] = "abc.safe";
const int kTestSeedExperimentProbability = 100;
const char kTestSeedSerialNumber[] = "123";

// Constants used to mock the serialized seed state.
const char kTestSeedSerializedData[] = "a serialized seed, 100% realistic";
const char kTestSeedSignature[] = "a totally valid signature, I swear!";
const int kTestSeedMilestone = 90;

struct FetchAndLaunchTimeTestParams {
  // Inputs in relation to the current build time.
  const base::TimeDelta fetch_time;
  const base::TimeDelta launch_time;
};

std::unique_ptr<VariationsSeedStore> CreateSeedStore(PrefService* local_state) {
  return std::make_unique<VariationsSeedStore>(
      local_state,
      std::make_unique<VariationsSafeSeedStoreLocalState>(local_state));
}

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

// Returns a test seed that contains a single study,
// "UMA-Uniformity-Trial-10-Percent", which has a single experiment, "abc", with
// probability weight 100. The study references the 100% slot of a LIMITED
// entropy layer. The LIMITED layer created will use 0 bit of entropy.
VariationsSeed CreateTestSeedWithLimitedEntropyLayer() {
  VariationsSeed seed;
  seed.set_serial_number(kTestSeedSerialNumber);

  auto* layer = seed.add_layers();
  layer->set_id(1);
  layer->set_num_slots(100);
  layer->set_entropy_mode(Layer::LIMITED);

  auto* layer_member = layer->add_members();
  layer_member->set_id(1);
  auto* slot = layer_member->add_slots();
  slot->set_start(0);
  slot->set_end(99);

  auto* study = seed.add_study();
  study->set_name(kTestLimitedLayerStudyName);

  auto* experiment = study->add_experiment();
  experiment->set_name(kTestSeedExperimentName);
  experiment->set_probability_weight(kTestSeedExperimentProbability);

  auto* layer_member_reference = study->mutable_layer();
  layer_member_reference->set_layer_id(1);
  layer_member_reference->add_layer_member_ids(1);

  return seed;
}

VariationsSeed CreateTestSeedWithLimitedEntropyLayerUsingExcessiveEntropy() {
  VariationsSeed seed;
  seed.set_serial_number(kTestSeedSerialNumber);

  auto* layer = seed.add_layers();
  layer->set_id(1);
  layer->set_num_slots(100);
  layer->set_entropy_mode(Layer::LIMITED);

  auto* layer_member = layer->add_members();
  layer_member->set_id(1);
  auto* slot = layer_member->add_slots();
  slot->set_start(0);
  slot->set_end(99);

  Study* study = seed.add_study();
  study->set_name(kTestLimitedLayerStudyName);

  auto* experiment_1 = study->add_experiment();
  experiment_1->set_name("experiment_very_small");
  experiment_1->set_probability_weight(1);
  experiment_1->set_google_web_experiment_id(100001);

  auto* experiment_2 = study->add_experiment();
  experiment_2->set_name("experiment");
  experiment_2->set_probability_weight(999999);
  experiment_1->set_google_web_experiment_id(100002);

  auto* layer_member_reference = study->mutable_layer();
  layer_member_reference->set_layer_id(1);
  layer_member_reference->add_layer_member_ids(1);

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

// A base::Time instance representing a time in the distant past. Here, it would
// return the start for epoch in Unix-like system (Jan 1, 1970).
base::Time DistantPast() {
  return base::Time::UnixEpoch();
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

class MockSafeSeedManager : public SafeSeedManager {
 public:
  explicit MockSafeSeedManager(PrefService* local_state)
      : SafeSeedManager(local_state) {}

  MockSafeSeedManager(const MockSafeSeedManager&) = delete;
  MockSafeSeedManager& operator=(const MockSafeSeedManager&) = delete;

  ~MockSafeSeedManager() override = default;

  MOCK_CONST_METHOD0(GetSeedType, SeedType());
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

class FakeSyntheticTrialObserver : public SyntheticTrialObserver {
 public:
  FakeSyntheticTrialObserver() = default;
  ~FakeSyntheticTrialObserver() override = default;

  void OnSyntheticTrialsChanged(
      const std::vector<SyntheticTrialGroup>& trials_updated,
      const std::vector<SyntheticTrialGroup>& trials_removed,
      const std::vector<SyntheticTrialGroup>& groups) override {
    trials_updated_ = trials_updated;
    trials_removed_ = trials_removed;
    groups_ = groups;
  }

  const std::vector<SyntheticTrialGroup>& trials_updated() {
    return trials_updated_;
  }

  const std::vector<SyntheticTrialGroup>& trials_removed() {
    return trials_removed_;
  }

  const std::vector<SyntheticTrialGroup>& groups() { return groups_; }

 private:
  std::vector<SyntheticTrialGroup> trials_updated_;
  std::vector<SyntheticTrialGroup> trials_removed_;
  std::vector<SyntheticTrialGroup> groups_;
};

// TODO(crbug.com/40742801): Remove when fake VariationsServiceClient created.
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
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}
  std::string_view GetLimitedEntropySyntheticTrialGroupName() {
    return limited_entropy_synthetic_trial_group_;
  }

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }

  std::string restrict_parameter_;
  std::string limited_entropy_synthetic_trial_group_;
};

class TestLimitedEntropySyntheticTrial : public LimitedEntropySyntheticTrial {
 public:
  TestLimitedEntropySyntheticTrial(std::string_view group_name)
      : LimitedEntropySyntheticTrial(group_name) {}

  TestLimitedEntropySyntheticTrial(const TestLimitedEntropySyntheticTrial&) =
      delete;
  TestLimitedEntropySyntheticTrial& operator=(
      const TestLimitedEntropySyntheticTrial&) = delete;

  ~TestLimitedEntropySyntheticTrial() = default;
};

class MockVariationsServiceClient : public TestVariationsServiceClient {
 public:
  MOCK_METHOD(void,
              RemoveGoogleGroupsFromPrefsForDeletedProfiles,
              (PrefService*),
              (override));
  MOCK_METHOD(Study::FormFactor, GetCurrentFormFactor, (), (override));
};

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(
            local_state,
            std::make_unique<VariationsSafeSeedStoreLocalState>(local_state)) {}

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
      const base::FilePath user_data_dir = base::FilePath(),
      metrics::StartupVisibility startup_visibility =
          metrics::StartupVisibility::kUnknown)
      : VariationsFieldTrialCreator(
            client,
            // Pass a VariationsSeedStore to base class.
            CreateSeedStore(local_state),
            UIStringOverrider(),
            /*limited_entropy_synthetic_trial=*/nullptr),
        enabled_state_provider_(/*consent=*/true, /*enabled=*/true),
        // Instead, use a TestVariationsSeedStore as the member variable.
        seed_store_(local_state),
        safe_seed_manager_(safe_seed_manager) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state, &enabled_state_provider_, std::wstring(), user_data_dir,
        startup_visibility);
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
    PlatformFieldTrials platform_field_trials;
    SyntheticTrialRegistry synthetic_trial_registry;
    return VariationsFieldTrialCreator::SetUpFieldTrials(
        /*variation_ids=*/std::vector<std::string>(),
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceVariationIds),
        std::vector<base::FeatureList::FeatureOverrideInfo>(),
        std::make_unique<base::FeatureList>(), metrics_state_manager_.get(),
        &synthetic_trial_registry, &platform_field_trials, safe_seed_manager_,
        /*add_entropy_source_to_variations_ids=*/true);
  }

  // Passthrough, to expose the underlying method to tests without making it
  // public.
  base::flat_set<uint64_t> GetGoogleGroupsFromPrefs() {
    return VariationsFieldTrialCreator::GetGoogleGroupsFromPrefs();
  }

  TestVariationsSeedStore* seed_store() { return &seed_store_; }

 protected:
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

  metrics::TestEnabledStateProvider enabled_state_provider_;
  TestVariationsSeedStore seed_store_;
  const raw_ptr<SafeSeedManager> safe_seed_manager_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
};

}  // namespace

class FieldTrialCreatorTest : public ::testing::Test {
 public:
  FieldTrialCreatorTest() = default;
  FieldTrialCreatorTest(const FieldTrialCreatorTest&) = delete;
  FieldTrialCreatorTest& operator=(const FieldTrialCreatorTest&) = delete;
  ~FieldTrialCreatorTest() override = default;

  void SetUp() override {
    // Register the prefs used by the metrics and variations services.
    metrics::MetricsService::RegisterPrefs(local_state_.registry());
    VariationsService::RegisterPrefs(local_state_.registry());

    // Create a new temp dir for each test, to avoid cross test contamination.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // These tests validate the setup features and field trials: initialize
    // them to null on each test to mimic fresh startup.
    scoped_feature_list_.InitWithNullFeatureAndFieldTrialLists();

    // Do not use the static field trial testing config data. Perform the
    // "real" feature and field trial setup.
    DisableTestingConfig();
  }

  PrefService* local_state() { return &local_state_; }

  const base::FilePath user_data_dir_path() const {
    return temp_dir_.GetPath();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  base::ScopedTempDir temp_dir_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

namespace {

class FieldTrialCreatorFetchAndLaunchTimeTest
    : public FieldTrialCreatorTest,
      public ::testing::WithParamInterface<FetchAndLaunchTimeTestParams> {};

constexpr FetchAndLaunchTimeTestParams kAllFetchAndLaunchTimes[] = {
    // Verify that when the binary is newer than the most recent seed, the
    // seed is applied as long as it was downloaded within the last 30 days.
    {.fetch_time = -base::Days(29), .launch_time = base::Days(1)},
    // Verify that when the binary is older than the most recent seed, the
    // seed is applied even though it was downloaded more than 30 days ago.
    {.fetch_time = base::Days(1), .launch_time = base::Days(32)},
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         FieldTrialCreatorFetchAndLaunchTimeTest,
                         ::testing::ValuesIn(kAllFetchAndLaunchTimes));

// Verify that unexpired seeds are used.
TEST_P(FieldTrialCreatorFetchAndLaunchTimeTest,
       SetUpFieldTrials_ValidSeed_NotExpired) {
  const auto& test_case = GetParam();
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;
  base::Time build_time = base::GetBuildTime();
  mock_clock.Advance(build_time - base::Time::Now());

  // The seed should be used, so the safe seed manager should be informed of
  // the active seed state.
  const base::Time seed_fetch_time = build_time + test_case.fetch_time;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedSerializedData, kTestSeedSignature,
                                   kTestSeedMilestone, _, seed_fetch_time))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Simulate the seed being stored.
  local_state()->SetTime(prefs::kVariationsLastFetchTime, seed_fetch_time);

  // Simulate a seed from an earlier (i.e. valid) milestone.
  local_state()->SetInteger(prefs::kVariationsSeedMilestone,
                            kTestSeedMilestone);

  // Fast forward the clock to launch_time and check that field trials are
  // created from the seed at launch_time. Since the test study has only one
  // experiment with 100% probability weight, we must be part of it.
  mock_clock.Advance(test_case.launch_time);
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());
  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.CreateTrials.SeedExpiry",
                                      VariationsSeedExpiry::kNotExpired, 1);
  int freshness_in_minutes =
      (test_case.launch_time - test_case.fetch_time).InDays() * 24 * 60;
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness",
                                      freshness_in_minutes, 1);
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kRegularSeedUsed, 1);
  histogram_tester.ExpectUniqueSample("Variations.AppliedSeed.Size",
                                      strlen(kTestSeedSerializedData), 1);
}

TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ValidSeed_NoLastFetchTime) {
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

// Verify that no seed is applied when the seed has expired.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_ExpiredSeed) {
  // When the seed is has expired, no field trials should be created from the
  // seed. Hence, no active state should be passed to the safe seed manager.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);
  // Simulate a seed that is fetched a long time ago and should definitely
  // have expired.
  local_state()->SetTime(prefs::kVariationsLastFetchTime, DistantPast());

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
  const int future_seed_milestone = 7890;

  // When the seed is associated with a future milestone (relative to the
  // client's milestone), no field trials should be created from the seed.
  // Hence, no active state should be passed to the safe seed manager.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

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
TEST_P(FieldTrialCreatorFetchAndLaunchTimeTest,
       SetUpFieldTrials_ValidSafeSeed_NewBinaryUsesSeed) {
  const auto& test_case = GetParam();
  // Fast forward the clock to build time.
  base::ScopedMockClockOverride mock_clock;
  base::Time build_time = base::GetBuildTime();
  mock_clock.Advance(build_time - base::Time::Now());

  // With a valid safe seed, the safe seed manager should not be informed of
  // the active seed state. This is an optimization to avoid saving a safe
  // seed when already running in safe mode.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, GetSeedType())
      .WillByDefault(Return(SeedType::kSafeSeed));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Simulate the safe seed being stored.
  local_state()->SetTime(prefs::kVariationsSafeSeedFetchTime,
                         build_time + test_case.fetch_time);

  // Fast forward the clock to launch_time and check that field trials are
  // created from the safe seed. Since the test study has only one experiment
  // with 100% probability weight, we must be part of it.
  mock_clock.Advance(test_case.launch_time);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials());
  EXPECT_EQ(kTestSafeSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.CreateTrials.SeedExpiry",
      VariationsSeedExpiry::kNotExpired, 1);
  int freshness_in_minutes =
      (test_case.launch_time - test_case.fetch_time).InDays() * 24 * 60;
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness",
                                      freshness_in_minutes, 1);
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kSafeSeedUsed, 1);
}

// Verify that Chrome does not apply a variations seed when Chrome should run in
// Variations Safe Mode but the safe seed is unloadable.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_UnloadableSafeSeedNotUsed) {
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, GetSeedType())
      .WillByDefault(Return(SeedType::kSafeSeed));

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
  // With a valid safe seed, the safe seed manager should not be informed of the
  // active seed state. This is an optimization to avoid saving a safe seed when
  // already running in safe mode.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, GetSeedType())
      .WillByDefault(Return(SeedType::kSafeSeed));
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
  // The safe seed manager should not be informed of the active seed state.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, GetSeedType())
      .WillByDefault(Return(SeedType::kSafeSeed));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);
  // Simulate a safe seed that is fetched a long time ago and should definitely
  // have expired.
  local_state()->SetTime(prefs::kVariationsSafeSeedFetchTime, DistantPast());

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
  const int future_seed_milestone = 7890;

  // The safe seed manager should not be informed of the active seed state.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, GetSeedType())
      .WillByDefault(Return(SeedType::kSafeSeed));
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

// Verify that no seed is applied when null seed is triggered.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_NullSeed) {
  // The safe seed manager should not be informed of the active seed state.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  ON_CALL(safe_seed_manager, GetSeedType())
      .WillByDefault(Return(SeedType::kNullSeed));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Check that field trials are not created from the null seed.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(field_trial_creator.SetUpFieldTrials());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedUsage",
                                      SeedUsage::kNullSeedUsed, 1);
}

TEST_F(FieldTrialCreatorTest, LoadSeedFromTestSeedJsonPath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath test_seed_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("TEST SEED"));

  // This seed contains the data for a test experiment.
  base::WriteFile(test_seed_file,
                  base::StringPrintf("{\"variations_compressed_seed\": \"%s\","
                                     "\"variations_seed_signature\": \"%s\"}",
                                     kTestSeedData.base64_compressed_data,
                                     kTestSeedData.base64_signature));

  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      variations::switches::kVariationsTestSeedJsonPath, test_seed_file);

  // Use a real VariationsFieldTrialCreator and VariationsSeedStore to exercise
  // the VariationsSeedStore::LoadSeed() logic.
  TestVariationsServiceClient variations_service_client;
  auto seed_store = CreateSeedStore(local_state());
  VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client, std::move(seed_store), UIStringOverrider(),
      /*limited_entropy_synthetic_trial=*/nullptr);
  metrics::TestEnabledStateProvider enabled_state_provider(
      /*consent=*/true,
      /*enabled=*/true);
  auto metrics_state_manager = metrics::MetricsStateManager::Create(
      local_state(), &enabled_state_provider, std::wstring(), base::FilePath());
  metrics_state_manager->InstantiateFieldTrialList();

  PlatformFieldTrials platform_field_trials;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  SyntheticTrialRegistry synthetic_trial_registry;

  ASSERT_FALSE(base::FieldTrialList::TrialExists(kTestSeedData.study_names[0]));

  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials(
      /*variation_ids=*/{},
      /*command_line_variation_ids=*/std::string(),
      std::vector<base::FeatureList::FeatureOverrideInfo>(),
      std::make_unique<base::FeatureList>(), metrics_state_manager.get(),
      &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/true));

  EXPECT_TRUE(base::FieldTrialList::TrialExists(kTestSeedData.study_names[0]));
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kVariationsFailedToFetchSeedStreak), 0);
  EXPECT_EQ(local_state()->GetInteger(prefs::kVariationsCrashStreak), 0);
}

#if BUILDFLAG(IS_ANDROID)
// This is a regression test for crbug/829527.
TEST_F(FieldTrialCreatorTest, SetUpFieldTrials_LoadsCountryOnFirstRun) {
  // Simulate having received a seed in Java during First Run.
  const base::Time one_day_ago = base::Time::Now() - base::Days(1);
  auto initial_seed = std::make_unique<SeedResponse>();
  initial_seed->data = SerializeSeed(CreateTestSeedWithCountryFilter());
  initial_seed->signature = kTestSeedSignature;
  initial_seed->country = kTestSeedCountry;
  initial_seed->date = one_day_ago;
  initial_seed->is_gzip_compressed = false;

  TestVariationsServiceClient variations_service_client;
  PlatformFieldTrials platform_field_trials;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  // Note: Unlike other tests, this test does not mock out the seed store, since
  // the interaction between these two classes is what's being tested.
  auto seed_store = std::make_unique<VariationsSeedStore>(
      local_state(), std::move(initial_seed),
      /*signature_verification_enabled=*/false,
      std::make_unique<VariationsSafeSeedStoreLocalState>(local_state()));
  VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client, std::move(seed_store), UIStringOverrider(),
      /*limited_entropy_synthetic_trial=*/nullptr);

  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/true,
                                                           /*enabled=*/true);
  auto metrics_state_manager = metrics::MetricsStateManager::Create(
      local_state(), &enabled_state_provider, std::wstring(), base::FilePath());
  metrics_state_manager->InstantiateFieldTrialList();
  SyntheticTrialRegistry synthetic_trial_registry;

  // Check that field trials are created from the seed. The test seed contains a
  // single study with an experiment targeting 100% of users in India. Since
  // |initial_seed| included the country code for India, this study should be
  // active.
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials(
      /*variation_ids=*/std::vector<std::string>(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceVariationIds),
      std::vector<base::FeatureList::FeatureOverrideInfo>(),
      std::make_unique<base::FeatureList>(), metrics_state_manager.get(),
      &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/true));

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
  // Simulate the seed being stored.
  local_state->SetTime(prefs::kVariationsLastFetchTime, seed_fetch_time);
  // Simulate a seed from an earlier (i.e. valid) milestone.
  local_state->SetInteger(prefs::kVariationsSeedMilestone, kTestSeedMilestone);
  return field_trial_creator;
}

// Verifies that a valid seed is used instead of the testing config when we
// disable it.
TEST_F(FieldTrialCreatorTest, NotSetUpFieldTrialConfig_ValidSeed) {
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
  static BASE_FEATURE(kFeature1, "UnitTestEnabled",
                      base::FEATURE_DISABLED_BY_DEFAULT);
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
  static BASE_FEATURE(kFeature1, "UnitTestEnabled",
                      base::FEATURE_DISABLED_BY_DEFAULT);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeature1));
  static BASE_FEATURE(kFeature2, "UnitTest2Enabled",
                      base::FEATURE_DISABLED_BY_DEFAULT);
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
  static BASE_FEATURE(kFeature1, "UnitTestEnabled",
                      base::FEATURE_DISABLED_BY_DEFAULT);
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
  static BASE_FEATURE(kFeature1, "UnitTestEnabled",
                      base::FEATURE_DISABLED_BY_DEFAULT);
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
  static BASE_FEATURE(kFeature1, "UnitTestEnabled",
                      base::FEATURE_DISABLED_BY_DEFAULT);

  EXPECT_EQ(GetParam() == ::switches::kEnableFeatures,
            base::FeatureList::IsEnabled(kFeature1));

  ResetVariations();
}
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

// Verify that a beacon file is not written when passing an empty user data
// directory path. Some platforms deliberately pass an empty path.
TEST_F(FieldTrialCreatorTest, DoNotWriteBeaconFile) {
  TestVariationsServiceClient variations_service_client;
  // Ensure that Variations Safe Mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  // Pass an empty path instead of a path to the user data dir.
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager,
      base::FilePath());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  EXPECT_FALSE(base::PathExists(
      user_data_dir_path().Append(metrics::kCleanExitBeaconFilename)));
  histogram_tester.ExpectTotalCount(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 0);
}

struct StartupVisibilityTestParams {
  const std::string test_name;
  metrics::StartupVisibility startup_visibility;
  bool extend_safe_mode;
};

class FieldTrialCreatorTestWithStartupVisibility
    : public FieldTrialCreatorTest,
      public ::testing::WithParamInterface<StartupVisibilityTestParams> {};

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

// Verify that Chrome starts watching for crashes for unknown and foreground
// startup visibilities. Verify that Chrome does not start watching for crashes
// in background sessions.
TEST_P(FieldTrialCreatorTestWithStartupVisibility,
       StartupVisibilityAffectsBrowserCrashMonitoring) {
  TestVariationsServiceClient variations_service_client;
  // Ensure that Variations Safe Mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  StartupVisibilityTestParams params = GetParam();
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager,
      user_data_dir_path(), params.startup_visibility);

  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that Chrome did (or did not) start watching for crashes.
  EXPECT_EQ(base::PathExists(
                user_data_dir_path().Append(metrics::kCleanExitBeaconFilename)),
            params.extend_safe_mode);
}

// Verify that the beacon file contents are as expected when Chrome starts
// watching for browser crashes before setting up field trials.
TEST_F(FieldTrialCreatorTest, WriteBeaconFile) {
  TestVariationsServiceClient variations_service_client;
  // Ensure that Variations Safe Mode is not triggered.
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager,
      user_data_dir_path());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(field_trial_creator.SetUpFieldTrials());

  // Verify that the beacon file was written and that the contents are correct.
  const base::FilePath variations_file_path =
      user_data_dir_path().Append(metrics::kCleanExitBeaconFilename);
  EXPECT_TRUE(base::PathExists(variations_file_path));
  std::string beacon_file_contents;
  ASSERT_TRUE(
      base::ReadFileToString(variations_file_path, &beacon_file_contents));
  EXPECT_EQ(beacon_file_contents,
            "{\"user_experience_metrics.stability.exited_cleanly\":false,"
            "\"variations_crash_streak\":0}");

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 1, 1);
}

TEST_F(FieldTrialCreatorTest, GetGoogleGroupsFromPrefsWhenPrefNotPresent) {
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  ASSERT_EQ(field_trial_creator.GetGoogleGroupsFromPrefs(),
            base::flat_set<uint64_t>());
}

TEST_F(FieldTrialCreatorTest, GetGoogleGroupsFromPrefsWhenEmptyDict) {
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Add an empty dict value for the pref.
  base::Value::Dict google_groups_dict;
  local_state()->SetDict(prefs::kVariationsGoogleGroups,
                         std::move(google_groups_dict));

  ASSERT_EQ(field_trial_creator.GetGoogleGroupsFromPrefs(),
            base::flat_set<uint64_t>());
}

TEST_F(FieldTrialCreatorTest,
       GetGoogleGroupsFromPrefsWhenProfileWithEmptyList) {
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Add an empty dict value for the pref.
  base::Value::Dict google_groups_dict;
  base::Value::List profile_1_groups;
  google_groups_dict.Set("Profile 1", std::move(profile_1_groups));
  local_state()->SetDict(prefs::kVariationsGoogleGroups,
                         std::move(google_groups_dict));

  ASSERT_EQ(field_trial_creator.GetGoogleGroupsFromPrefs(),
            base::flat_set<uint64_t>());
}

TEST_F(FieldTrialCreatorTest,
       GetGoogleGroupsFromPrefsWhenProfileWithNonEmptyList) {
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Add an empty dict value for the pref.
  base::Value::Dict google_groups_dict;
  base::Value::List profile_1_groups;
  profile_1_groups.Append("123");
  profile_1_groups.Append("456");
  google_groups_dict.Set("Profile 1", std::move(profile_1_groups));
  local_state()->SetDict(prefs::kVariationsGoogleGroups,
                         std::move(google_groups_dict));

  ASSERT_EQ(field_trial_creator.GetGoogleGroupsFromPrefs(),
            base::flat_set<uint64_t>({123, 456}));
}

TEST_F(FieldTrialCreatorTest,
       GetGoogleGroupsFromPrefsWhenProfileWithNonNumericString) {
  TestVariationsServiceClient variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  // Add an empty dict value for the pref.
  base::Value::Dict google_groups_dict;
  base::Value::List profile_1_groups;
  profile_1_groups.Append("Alice");
  profile_1_groups.Append("Bob");
  google_groups_dict.Set("Profile 1", std::move(profile_1_groups));
  local_state()->SetDict(prefs::kVariationsGoogleGroups,
                         std::move(google_groups_dict));

  ASSERT_EQ(field_trial_creator.GetGoogleGroupsFromPrefs(),
            base::flat_set<uint64_t>());
}

TEST_F(FieldTrialCreatorTest, GetGoogleGroupsFromPrefsClearsDeletedProfiles) {
  NiceMock<MockVariationsServiceClient> variations_service_client;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  TestVariationsFieldTrialCreator field_trial_creator(
      local_state(), &variations_service_client, &safe_seed_manager);

  EXPECT_CALL(variations_service_client,
              RemoveGoogleGroupsFromPrefsForDeletedProfiles(local_state()));
  field_trial_creator.GetGoogleGroupsFromPrefs();
}

namespace {

TestLimitedEntropySyntheticTrial kEnabledTrial(
    kLimitedEntropySyntheticTrialEnabled);
TestLimitedEntropySyntheticTrial kControlTrial(
    kLimitedEntropySyntheticTrialControl);
TestLimitedEntropySyntheticTrial kDefaultTrial(
    kLimitedEntropySyntheticTrialDefault);
const raw_ptr<LimitedEntropySyntheticTrial> kNoLimitedLayerSupport = nullptr;

// LERS: Limited Entropy Randomization Source.
struct GenerateLERSTestCase {
  std::string test_name;
  // Whether the client is behind a channel gate. If so, the client should not
  // be generating a limited entropy randomization source.
  bool is_channel_gated;
  raw_ptr<LimitedEntropySyntheticTrial> trial;

  // Whether a limited entropy randomization source is expected to be generated.
  bool expected;
};

class IsLimitedEntropyRandomizationSourceEnabledTest
    : public FieldTrialCreatorTest,
      public ::testing::WithParamInterface<GenerateLERSTestCase> {};

const GenerateLERSTestCase kGenerateLERSTestCases[] = {
    {"GenerateForNonGatedEnabledClient",
     /*is_channel_gated=*/false, &kEnabledTrial, true},
    {"DoNotGenerateForGatedEnabledClient",
     /*is_channel_gated=*/true, &kEnabledTrial, false},
    {"DoNotGenerateForNonGatedControlClient",
     /*is_channel_gated=*/false, &kControlTrial, false},
    {"DoNotGenerateForNonGatedIneligibleClient",
     /*is_channel_gated=*/false, kNoLimitedLayerSupport, false},
};

INSTANTIATE_TEST_SUITE_P(
    FieldTrialCreatorTest,
    IsLimitedEntropyRandomizationSourceEnabledTest,
    ::testing::ValuesIn(kGenerateLERSTestCases),
    [](const ::testing::TestParamInfo<GenerateLERSTestCase>& info) {
      return info.param.test_name;
    });

TEST_P(IsLimitedEntropyRandomizationSourceEnabledTest, ) {
  const GenerateLERSTestCase test_case = GetParam();
  TestVariationsServiceClient client;
  if (test_case.is_channel_gated) {
    DisableLimitedEntropyModeForTesting();
  } else {
    EnableLimitedEntropyModeForTesting();
  }

  const bool actual = VariationsFieldTrialCreatorBase::
      IsLimitedEntropyRandomizationSourceEnabled(
          client.GetChannelForVariations(), test_case.trial);
  EXPECT_EQ(test_case.expected, actual);
}

enum class LimitedModeGate {
  ENABLED,
  DISABLED,
};

struct LimitedEntropyProcessingTestCase {
  std::string test_name;

  LimitedModeGate limited_mode_gate;
  VariationsSeed seed;
  raw_ptr<LimitedEntropySyntheticTrial> trial;

  bool is_group_registration_expected;
  bool is_seed_rejection_expected;
  bool is_limited_study_active;
};

std::vector<LimitedEntropyProcessingTestCase> CreateTestCasesForTrials(
    std::string_view test_name,
    LimitedModeGate limited_mode_gate,
    const VariationsSeed& seed,
    const std::vector<raw_ptr<LimitedEntropySyntheticTrial>>& trials,
    bool is_group_registration_expected,
    bool is_seed_rejection_expected,
    bool is_limited_study_active) {
  std::vector<LimitedEntropyProcessingTestCase> test_cases;
  for (raw_ptr<LimitedEntropySyntheticTrial> trial : trials) {
    std::string test_name_with_trial;
    if (trial) {
      test_name_with_trial =
          base::StrCat({test_name, "_", trial->GetGroupName()});
    } else {
      test_name_with_trial = base::StrCat({test_name, "_IneligibleToTrial"});
    }
    test_cases.push_back({test_name_with_trial, limited_mode_gate, seed, trial,
                          is_group_registration_expected,
                          is_seed_rejection_expected, is_limited_study_active});
  }
  return test_cases;
}

std::vector<LimitedEntropyProcessingTestCase> FlattenTests(
    const std::vector<std::vector<LimitedEntropyProcessingTestCase>>&
        test_cases) {
  CHECK(test_cases.size() > 0 && test_cases[0].size() > 0);
  std::vector<LimitedEntropyProcessingTestCase> flattened;
  for (const std::vector<LimitedEntropyProcessingTestCase>& scenarios :
       test_cases) {
    for (const LimitedEntropyProcessingTestCase& scenario : scenarios) {
      flattened.push_back(scenario);
    }
  }
  return flattened;
}

const std::vector<LimitedEntropyProcessingTestCase> kTestCases = FlattenTests({
    {{"EnabledTrialClient_ShouldProcessLimitedLayer", LimitedModeGate::ENABLED,
      CreateTestSeedWithLimitedEntropyLayer(), &kEnabledTrial,
      /*is_group_registration_expected=*/true,
      /*is_seed_rejection_expected=*/false,
      /*is_limited_study_active=*/true}},

    {{"EnabledTrialClient_ShouldRejectSeedWithExcessiveEntropyUse",
      LimitedModeGate::ENABLED,
      CreateTestSeedWithLimitedEntropyLayerUsingExcessiveEntropy(),
      &kEnabledTrial,
      /*is_group_registration_expected=*/true,
      /*is_seed_rejection_expected=*/true,
      /*is_limited_study_active=*/false}},

    {{"EnabledTrialClient_ShouldNotProcessSeedWithoutLimitedLayer",
      LimitedModeGate::ENABLED, CreateTestSeed(), &kEnabledTrial,
      /*is_group_registration_expected=*/false,
      /*is_seed_rejection_expected=*/false,
      /*is_limited_study_active=*/false}},

    CreateTestCasesForTrials("ControlOrDefaultTrialClient_"
                             "ShouldRegisterGroupButNotProcessLimitedLayer",
                             LimitedModeGate::ENABLED,
                             CreateTestSeedWithLimitedEntropyLayer(),
                             {&kControlTrial, &kDefaultTrial},
                             /*is_group_registration_expected=*/true,
                             /*is_seed_rejection_expected=*/false,
                             /*is_limited_study_active=*/false),

    CreateTestCasesForTrials(
        "ControlOrDefaultTrialClient_"
        "ShouldRegisterGroupButNotProcessLimitedLayerRegardlessOfEntropyUsage",
        LimitedModeGate::ENABLED,
        CreateTestSeedWithLimitedEntropyLayerUsingExcessiveEntropy(),
        {&kControlTrial, &kDefaultTrial},
        /*is_group_registration_expected=*/true,
        /*is_seed_rejection_expected=*/false,
        /*is_limited_study_active=*/false),

    CreateTestCasesForTrials("ControlOrDefaultTrialClient_"
                             "ShouldNotRegisterGroupWithoutLimitedLayer",
                             LimitedModeGate::ENABLED,
                             CreateTestSeed(),
                             {&kControlTrial, &kDefaultTrial},
                             /*is_group_registration_expected=*/false,
                             /*is_seed_rejection_expected=*/false,
                             /*is_limited_study_active=*/false),

    {{"IneligibleClient_ReceivingLimitedLayer", LimitedModeGate::ENABLED,
      CreateTestSeedWithLimitedEntropyLayer(), kNoLimitedLayerSupport,
      /*is_group_registration_expected=*/false,
      /*is_seed_rejection_expected=*/false,
      /*is_limited_study_active=*/false}},

    {{"IneligibleClient_ReceivingLayerUsingExcessiveEntropy",
      LimitedModeGate::ENABLED,
      CreateTestSeedWithLimitedEntropyLayerUsingExcessiveEntropy(),
      kNoLimitedLayerSupport,
      /*is_group_registration_expected=*/false,
      /*is_seed_rejection_expected=*/false,
      /*is_limited_study_active=*/false}},

    {{"IneligibleClient_NotReceivingLimitedLayer", LimitedModeGate::ENABLED,
      CreateTestSeed(), kNoLimitedLayerSupport,
      /*is_group_registration_expected=*/false,
      /*is_seed_rejection_expected=*/false,
      /*is_limited_study_active=*/false}},

    CreateTestCasesForTrials("ChannelGatedClient_ReceivingLimitedLayer",
                             LimitedModeGate::DISABLED,
                             CreateTestSeedWithLimitedEntropyLayer(),
                             {&kEnabledTrial, &kControlTrial, &kDefaultTrial,
                              kNoLimitedLayerSupport},
                             /*is_group_registration_expected=*/false,
                             /*is_seed_rejection_expected=*/false,
                             /*is_limited_study_active=*/false),

    CreateTestCasesForTrials(
        "ChannelGatedClient_ReceivingLimitedLayerWithExcessiveEntropy",
        LimitedModeGate::DISABLED,
        CreateTestSeedWithLimitedEntropyLayerUsingExcessiveEntropy(),
        {&kEnabledTrial, &kControlTrial, &kDefaultTrial,
         kNoLimitedLayerSupport},
        /*is_group_registration_expected=*/false,
        /*is_seed_rejection_expected=*/false,
        /*is_limited_study_active=*/false),

    CreateTestCasesForTrials("ChannelGatedClient_NotReceivingLimitedLayer",
                             LimitedModeGate::DISABLED,
                             CreateTestSeed(),
                             {&kEnabledTrial, &kControlTrial, &kDefaultTrial,
                              kNoLimitedLayerSupport},
                             /*is_group_registration_expected=*/false,
                             /*is_seed_rejection_expected=*/false,
                             /*is_limited_study_active=*/false),
});

class LimitedEntropyProcessingTest
    : public FieldTrialCreatorTest,
      public ::testing::WithParamInterface<LimitedEntropyProcessingTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    FieldTrialCreatorTest,
    LimitedEntropyProcessingTest,
    ::testing::ValuesIn(kTestCases),
    [](const ::testing::TestParamInfo<LimitedEntropyProcessingTestCase>& info) {
      return info.param.test_name;
    });

TEST_P(LimitedEntropyProcessingTest,
       RandomizeLimitedEntropyStudyOrRejectTheSeed) {
  const LimitedEntropyProcessingTestCase test_case = GetParam();

  // Simulate the limited entropy channel gate setting.
  if (test_case.limited_mode_gate == LimitedModeGate::ENABLED) {
    EnableLimitedEntropyModeForTesting();
  } else {
    ASSERT_EQ(LimitedModeGate::DISABLED, test_case.limited_mode_gate);
    DisableLimitedEntropyModeForTesting();
  }

  auto encoded_and_compressed = GZipAndB64EncodeToHexString(test_case.seed);
  local_state()->SetString(prefs::kVariationsCompressedSeed,
                           encoded_and_compressed);

  // Allows and writes an empty signature for the test seed.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);
  local_state()->SetString(prefs::kVariationsSeedSignature, "");

  // Sets up dependencies and mocks.
  TestVariationsServiceClient variations_service_client;
  auto seed_store = CreateSeedStore(local_state());
  VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client, std::move(seed_store), UIStringOverrider(),
      test_case.trial);
  metrics::TestEnabledStateProvider enabled_state_provider(
      /*consent=*/true,
      /*enabled=*/true);
  auto metrics_state_manager = metrics::MetricsStateManager::Create(
      local_state(), &enabled_state_provider, std::wstring(), base::FilePath());
  metrics_state_manager->InstantiateFieldTrialList();
  PlatformFieldTrials platform_field_trials;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());

  SyntheticTrialRegistry synthetic_trial_registry;
  FakeSyntheticTrialObserver observer;
  synthetic_trial_registry.AddObserver(&observer);

  EXPECT_NE(
      test_case.is_seed_rejection_expected,
      field_trial_creator.SetUpFieldTrials(
          /*variation_ids=*/{},
          /*command_line_variation_ids=*/std::string(),
          std::vector<base::FeatureList::FeatureOverrideInfo>(),
          std::make_unique<base::FeatureList>(), metrics_state_manager.get(),
          &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
          /*add_entropy_source_to_variations_ids=*/true));

  // Verifies that the limited entropy test study is randomized.
  EXPECT_EQ(test_case.is_limited_study_active,
            base::FieldTrialList::TrialExists(kTestLimitedLayerStudyName));

  // Verifies that the limited entropy synthetic trial is randomized.
  if (test_case.is_group_registration_expected) {
    EXPECT_EQ(1u, observer.trials_updated().size());
    EXPECT_EQ(kLimitedEntropySyntheticTrialName,
              observer.trials_updated()[0].trial_name());
    EXPECT_EQ(test_case.trial->GetGroupName(),
              observer.trials_updated()[0].group_name());

    EXPECT_EQ(0u, observer.trials_removed().size());

    EXPECT_EQ(1u, observer.groups().size());
    EXPECT_EQ(kLimitedEntropySyntheticTrialName,
              observer.groups()[0].trial_name());
    EXPECT_EQ(test_case.trial->GetGroupName(),
              observer.groups()[0].group_name());
  } else {
    EXPECT_EQ(0u, observer.trials_updated().size());
    EXPECT_EQ(0u, observer.trials_removed().size());
    EXPECT_EQ(0u, observer.groups().size());
  }

  synthetic_trial_registry.RemoveObserver(&observer);
}

// Test feature names prefixed with __ to avoid collision with real features.
BASE_FEATURE(kDesktopFeature, "__Desktop", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPhoneFeature, "__Phone", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabletFeature, "__Tablet", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kKioskFeature, "__Kiosk", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMeetFeature, "__Meet", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTVFeature, "__TV", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutomotiveFeature, "__Auto", base::FEATURE_DISABLED_BY_DEFAULT);

class FieldTrialCreatorFormFactorTest
    : public FieldTrialCreatorTest,
      public ::testing::WithParamInterface<Study::FormFactor> {};

constexpr Study::FormFactor kAllFormFactors[] = {
    Study::DESKTOP,     Study::PHONE, Study::TABLET,    Study::KIOSK,
    Study::MEET_DEVICE, Study::TV,    Study::AUTOMOTIVE};

// A test seed that enables form-factor specific features across all platforms
// and channels. I.e. the __Desktop feature is enabled only on the Desktop form
// factor, the __Phone feature is enabled only on the Phone form factor, and so
// forth.  The seed applies to all platforms and all channels, except "unknown".
constexpr char kFormFactorTestSeedData[] =
    "H4sIAAAAAAAA/4TPT2vCQBAF8Gz+Z0qh7K20lVAvcxdkLzksVmRjLVqDPQ6xXTQoSakJ/"
    "fplrbeAe34zP96D4Xox2b115cfvWlZSqp1ScpVlw/FsLCfL0Wj1ozI+BV92bSNY/"
    "gC307rcHvXXa9nVn/s7to0hJDLx+yB1Upa6qYcOMnTRwwR9DDDECGMR8jlEL/"
    "p0aJtvwfJBX7qBhOhyYcEcPoNgXjWng2D5Y59KICI65xbIM+"
    "MWWrdXxpnYwvimz3Lf1PpKn3NugRiX4BYbwfL7vhKCT1RsLETAFYSF+"
    "TSjnvoMQEz0f2Ch3Gevro5/AQAA//8RFDdTJQIAAA==";
constexpr char kFormFactorTestSeedSignature[] = "";  // Deliberately empty.

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         FieldTrialCreatorFormFactorTest,
                         ::testing::ValuesIn(kAllFormFactors));

TEST_P(FieldTrialCreatorFormFactorTest, FilterByFormFactor) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFakeVariationsChannel,
      "dev");  // Seed supports canary, dev, beta and stable, but not "unknown".

  const auto current_form_factor = GetParam();

  // Override Local State seed prefs to use the form factor test seed constants.
  local_state()->SetString(prefs::kVariationsCompressedSeed,
                           kFormFactorTestSeedData);
  local_state()->SetString(prefs::kVariationsSeedSignature,
                           kFormFactorTestSeedSignature);
  local_state()->CommitPendingWrite();

  // Mock the variations service client to send the parameritized form factor.
  NiceMock<MockVariationsServiceClient> variations_service_client;
  ON_CALL(variations_service_client, GetCurrentFormFactor())
      .WillByDefault(Return(current_form_factor));

  // Create the other field trial creator dependencies.
  metrics::TestEnabledStateProvider enabled_state_provider(
      /*consent=*/true,
      /*enabled=*/true);
  auto metrics_state_manager = metrics::MetricsStateManager::Create(
      local_state(), &enabled_state_provider, std::wstring(), base::FilePath());
  metrics_state_manager->InstantiateFieldTrialList();

  PlatformFieldTrials platform_field_trials;
  NiceMock<MockSafeSeedManager> safe_seed_manager(local_state());
  SyntheticTrialRegistry synthetic_trial_registry;

  // Set up the field trials.
  VariationsFieldTrialCreator field_trial_creator{
      &variations_service_client, CreateSeedStore(local_state()),
      UIStringOverrider(), /*limited_entropy_synthetic_trial=*/nullptr};
  EXPECT_TRUE(field_trial_creator.SetUpFieldTrials(
      /*variation_ids=*/{},
      /*command_line_variation_ids=*/std::string(),
      std::vector<base::FeatureList::FeatureOverrideInfo>(),
      std::make_unique<base::FeatureList>(), metrics_state_manager.get(),
      &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/true));

  // Each form factor specific feature should be enabled iff the current form
  // factor matches the feature's targetted form factor.

  EXPECT_EQ(base::FeatureList::IsEnabled(kDesktopFeature),
            current_form_factor == Study::DESKTOP);

  EXPECT_EQ(base::FeatureList::IsEnabled(kPhoneFeature),
            current_form_factor == Study::PHONE);

  EXPECT_EQ(base::FeatureList::IsEnabled(kTabletFeature),
            current_form_factor == Study::TABLET);

  EXPECT_EQ(base::FeatureList::IsEnabled(kKioskFeature),
            current_form_factor == Study::KIOSK);

  EXPECT_EQ(base::FeatureList::IsEnabled(kMeetFeature),
            current_form_factor == Study::MEET_DEVICE);

  EXPECT_EQ(base::FeatureList::IsEnabled(kTVFeature),
            current_form_factor == Study::TV);

  EXPECT_EQ(base::FeatureList::IsEnabled(kAutomotiveFeature),
            current_form_factor == Study::AUTOMOTIVE);
}

}  // namespace variations
