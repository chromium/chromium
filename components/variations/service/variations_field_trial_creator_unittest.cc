// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "components/variations/seed_response.h"
#endif  // OS_ANDROID

using testing::_;
using testing::Ge;
using testing::Return;

namespace variations {
namespace {

// Constants used to create the test seeds.
const char kTestSeedStudyName[] = "test";
const char kTestSeedExperimentName[] = "abc";
const char kTestSafeSeedExperimentName[] = "abc.safe";
const int kTestSeedExperimentProbability = 100;
const char kTestSeedSerialNumber[] = "123";

// Constants used to mock the serialized seed state.
const char kTestSeedData[] = "a serialized seed, 100% realistic";
const char kTestSeedSignature[] = "a totally valid signature, I swear!";

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
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

// Returns a seed containing simple test data. The resulting seed will contain
// one study called "test", which contains one experiment called "abc.safe" with
// probability weight 100. This is intended to be used whenever a "safe" seed is
// called for, so that test expectations can distinguish between a "safe" seed
// and a "latest" seed.
VariationsSeed CreateTestSafeSeed() {
  VariationsSeed seed = CreateTestSeed();
  Study* study = seed.mutable_study(0);
  study->set_default_experiment_name(kTestSafeSeedExperimentName);
  study->mutable_experiment(0)->set_name(kTestSafeSeedExperimentName);
  return seed;
}

void DisableTestingConfig() {
  // If the testing config is in use, the seed will not be used to set up field
  // trials. Disable the testing config to exercise CreateTrialsFromSeed().
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableFieldTrialTestingConfig);
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
#endif  // OS_ANDROID

class TestPlatformFieldTrials : public PlatformFieldTrials {
 public:
  TestPlatformFieldTrials() = default;
  ~TestPlatformFieldTrials() override = default;

  // PlatformFieldTrials:
  void SetupFieldTrials() override {}
  void SetupFeatureControllingFieldTrials(
      bool has_seed,
      base::FeatureList* feature_list) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPlatformFieldTrials);
};

class MockSafeSeedManager : public SafeSeedManager {
 public:
  explicit MockSafeSeedManager(PrefService* local_state)
      : SafeSeedManager(true, local_state) {}
  ~MockSafeSeedManager() override = default;

  MOCK_CONST_METHOD0(ShouldRunInSafeMode, bool());
  MOCK_METHOD4(DoSetActiveSeedState,
               void(const std::string& seed_data,
                    const std::string& base64_seed_signature,
                    ClientFilterableState* client_filterable_state,
                    base::Time seed_fetch_time));

  void SetActiveSeedState(
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      std::unique_ptr<ClientFilterableState> client_filterable_state,
      base::Time seed_fetch_time) override {
    DoSetActiveSeedState(seed_data, base64_seed_signature,
                         client_filterable_state.get(), seed_fetch_time);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeSeedManager);
};

class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Callback<base::Version(void)> GetVersionForSimulationCallback()
      override {
    return base::Callback<base::Version(void)>();
  }
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

  DISALLOW_COPY_AND_ASSIGN(TestVariationsServiceClient);
};

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state), local_state_(local_state) {}
  ~TestVariationsSeedStore() override = default;

  bool LoadSeed(VariationsSeed* seed,
                std::string* seed_data,
                std::string* base64_signature) override {
    *seed = CreateTestSeed();
    *seed_data = kTestSeedData;
    *base64_signature = kTestSeedSignature;
    return true;
  }

  bool LoadSafeSeed(VariationsSeed* seed,
                    ClientFilterableState* client_state,
                    base::Time* seed_fetch_time) override {
    if (has_corrupted_safe_seed_)
      return false;

    *seed = CreateTestSafeSeed();
    *seed_fetch_time =
        local_state_->GetTime(prefs::kVariationsSafeSeedFetchTime);
    return true;
  }

  void set_has_corrupted_safe_seed(bool is_corrupted) {
    has_corrupted_safe_seed_ = is_corrupted;
  }

 private:
  // Whether to simulate having a corrupted safe seed.
  bool has_corrupted_safe_seed_ = false;

  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsSeedStore);
};

class TestVariationsFieldTrialCreator : public VariationsFieldTrialCreator {
 public:
  TestVariationsFieldTrialCreator(PrefService* local_state,
                                  TestVariationsServiceClient* client,
                                  SafeSeedManager* safe_seed_manager)
      : VariationsFieldTrialCreator(
            local_state,
            client,
            std::make_unique<VariationsSeedStore>(local_state),
            UIStringOverrider()),
        seed_store_(local_state),
        safe_seed_manager_(safe_seed_manager) {}

  ~TestVariationsFieldTrialCreator() override = default;

  // A convenience wrapper around SetupFieldTrials() which passes default values
  // for uninteresting params.
  bool SetupFieldTrials() {
    TestPlatformFieldTrials platform_field_trials;
    return VariationsFieldTrialCreator::SetupFieldTrials(
        "", "", "", std::set<std::string>(), std::vector<std::string>(),
        std::vector<base::FeatureList::FeatureOverrideInfo>(), nullptr,
        std::make_unique<base::FeatureList>(), &platform_field_trials,
        safe_seed_manager_);
  }

  TestVariationsSeedStore* seed_store() { return &seed_store_; }

 private:
  VariationsSeedStore* GetSeedStore() override { return &seed_store_; }

  TestVariationsSeedStore seed_store_;
  SafeSeedManager* const safe_seed_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsFieldTrialCreator);
};

}  // namespace

class FieldTrialCreatorTest : public ::testing::Test {
 protected:
  FieldTrialCreatorTest() {
    VariationsService::RegisterPrefs(prefs_.registry());
    global_feature_list_ = base::FeatureList::ClearInstanceForTesting();
  }

  ~FieldTrialCreatorTest() override {
    // Clear out any features created by tests in this suite, and restore the
    // global feature list.
    base::FeatureList::ClearInstanceForTesting();
    base::FeatureList::RestoreInstanceForTesting(
        std::move(global_feature_list_));
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  // The global feature list, which is ignored by tests in this suite.
  std::unique_ptr<base::FeatureList> global_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialCreatorTest);
};

TEST_F(FieldTrialCreatorTest, SetupFieldTrials_ValidSeed) {
  DisableTestingConfig();

  // With a valid seed, the safe seed manager should be informed of the active
  // seed state.
  const base::Time now = base::Time::Now();
  const base::Time recent_time = now - base::TimeDelta::FromMinutes(17);
  testing::NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));
  EXPECT_CALL(
      safe_seed_manager,
      DoSetActiveSeedState(kTestSeedData, kTestSeedSignature, _, recent_time))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  // Simulate a seed having been stored recently.
  prefs_.SetTime(prefs::kVariationsLastFetchTime, recent_time);

  // Check that field trials are created from the seed. Since the test study has
  // only 1 experiment with 100% probability weight, we must be part of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetupFieldTrials());
  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness", 17, 1);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.FellBackToSafeMode2",
                                      false, 1);
}

TEST_F(FieldTrialCreatorTest, SetupFieldTrials_NoLastFetchTime) {
  DisableTestingConfig();

  // With a valid seed on first run, the safe seed manager should be informed of
  // the active seed state. The last fetch time in this case is expected to be
  // inferred to be recent.
  testing::NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));
  const base::Time start_time = base::Time::Now();
  EXPECT_CALL(safe_seed_manager,
              DoSetActiveSeedState(kTestSeedData, kTestSeedSignature, _,
                                   Ge(start_time)))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  // Simulate a first run by leaving |prefs::kVariationsLastFetchTime| empty.
  EXPECT_EQ(0, prefs_.GetInt64(prefs::kVariationsLastFetchTime));

  // Check that field trials are created from the seed. Since the test study has
  // only 1 experiment with 100% probability weight, we must be part of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetupFieldTrials());
  EXPECT_EQ(base::FieldTrialList::FindFullName(kTestSeedStudyName),
            kTestSeedExperimentName);

  // Verify metrics. The seed freshness metric should not be recorded on first
  // run.
  histogram_tester.ExpectTotalCount("Variations.SeedFreshness", 0);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.FellBackToSafeMode2",
                                      false, 1);
}

TEST_F(FieldTrialCreatorTest, SetupFieldTrials_ExpiredSeed) {
  DisableTestingConfig();

  // With an expired seed, there should be no field trials created, and hence no
  // active state should be passed to the safe seed manager.
  testing::NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _)).Times(0);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);

  // Simulate an expired seed.
  const base::Time seed_date =
      base::Time::Now() - base::TimeDelta::FromDays(31);
  prefs_.SetTime(prefs::kVariationsLastFetchTime, seed_date);

  // Check that field trials are not created from the expired seed.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(field_trial_creator.SetupFieldTrials());
  EXPECT_TRUE(base::FieldTrialList::FindFullName(kTestSeedStudyName).empty());

  // Verify metrics. The seed freshness metric should not be recorded for an
  // expired seed.
  histogram_tester.ExpectTotalCount("Variations.SeedFreshness", 0);
  histogram_tester.ExpectTotalCount("Variations.SafeMode.FellBackToSafeMode2",
                                    0);
}

TEST_F(FieldTrialCreatorTest, SetupFieldTrials_ValidSafeSeed) {
  DisableTestingConfig();

  // With a valid safe seed, the safe seed manager should *not* be informed of
  // the active seed state. This is an optimization to avoid saving a safe seed
  // when already running in safe mode.
  testing::NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(safe_seed_manager, DoSetActiveSeedState(_, _, _, _)).Times(0);

  const base::Time now = base::Time::Now();
  const base::Time earlier = now - base::TimeDelta::FromMinutes(17);
  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);
  prefs_.SetTime(prefs::kVariationsLastFetchTime, now);
  prefs_.SetTime(prefs::kVariationsSafeSeedFetchTime, earlier);

  // Check that field trials are created from the safe seed. Since the test
  // study has only 1 experiment with 100% probability weight, we must be part
  // of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetupFieldTrials());
  EXPECT_EQ(kTestSafeSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness", 17, 1);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.FellBackToSafeMode2",
                                      true, 1);
}

TEST_F(FieldTrialCreatorTest,
       SetupFieldTrials_CorruptedSafeSeed_FallsBackToLatestSeed) {
  DisableTestingConfig();

  // With a corrupted safe seed, the field trial creator should fall back to the
  // latest seed. Hence, the safe seed manager *should* be informed of the
  // active seed state.
  const base::Time now = base::Time::Now();
  const base::Time recent_time = now - base::TimeDelta::FromMinutes(17);
  testing::NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode()).WillByDefault(Return(true));
  EXPECT_CALL(
      safe_seed_manager,
      DoSetActiveSeedState(kTestSeedData, kTestSeedSignature, _, recent_time))
      .Times(1);

  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, &safe_seed_manager);
  field_trial_creator.seed_store()->set_has_corrupted_safe_seed(true);
  prefs_.SetTime(prefs::kVariationsLastFetchTime, recent_time);
  prefs_.SetTime(prefs::kVariationsSafeSeedFetchTime,
                 now - base::TimeDelta::FromDays(4));

  // Check that field trials are created from the latest seed. Since the test
  // study has only 1 experiment with 100% probability weight, we must be part
  // of it.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(field_trial_creator.SetupFieldTrials());
  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedFreshness", 17, 1);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.FellBackToSafeMode2",
                                      false, 1);
}

#if defined(OS_ANDROID)
// This is a regression test for https://crbug.com/829527
TEST_F(FieldTrialCreatorTest, SetupFieldTrials_LoadsCountryOnFirstRun) {
  DisableTestingConfig();

  // Simulate having received a seed in Java during First Run.
  const base::Time one_day_ago =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  auto initial_seed = std::make_unique<SeedResponse>();
  initial_seed->data = SerializeSeed(CreateTestSeedWithCountryFilter());
  initial_seed->signature = kTestSeedSignature;
  initial_seed->country = kTestSeedCountry;
  initial_seed->date = one_day_ago.ToJavaTime();
  initial_seed->is_gzip_compressed = false;

  TestVariationsServiceClient variations_service_client;
  TestPlatformFieldTrials platform_field_trials;
  testing::NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
  ON_CALL(safe_seed_manager, ShouldRunInSafeMode())
      .WillByDefault(Return(false));

  // Note: Unlike other tests, this test does not mock out the seed store, since
  // the interaction between these two classes is what's being tested.
  auto seed_store = std::make_unique<VariationsSeedStore>(
      &prefs_, std::move(initial_seed),
      /*on_initial_seed_stored=*/base::DoNothing());
  VariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client, std::move(seed_store),
      UIStringOverrider());

  // Check that field trials are created from the seed. The test seed contains a
  // single study with an experiment targeting 100% of users in India. Since
  // |initial_seed| included the country code for India, this study should be
  // active.
  EXPECT_TRUE(field_trial_creator.SetupFieldTrials(
      "", "", "", std::set<std::string>(), std::vector<std::string>(),
      std::vector<base::FeatureList::FeatureOverrideInfo>(), nullptr,
      std::make_unique<base::FeatureList>(), &platform_field_trials,
      &safe_seed_manager));

  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));
}

// Tests that the hardware class is set on Android.
TEST_F(FieldTrialCreatorTest, ClientFilterableState_HardwareClass) {
  testing::NiceMock<MockSafeSeedManager> safe_seed_manager(&prefs_);
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
#endif  // OS_ANDROID

}  // namespace variations
