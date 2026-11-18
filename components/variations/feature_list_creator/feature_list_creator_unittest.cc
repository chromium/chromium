// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/feature_list_creator/feature_list_creator.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/feature_list_creator/feature_list_creator_client.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "components/webui/flags/feature_entry.h"
#include "components/webui/flags/feature_entry_macros.h"
#include "components/webui/flags/flags_state.h"
#include "components/webui/flags/flags_storage.h"
#include "components/webui/flags/flags_ui_switches.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

using ::testing::ElementsAre;

BASE_FEATURE(kFlagsTestFeature,
             "FeatureListCreatorFlagsTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kExtraTestFeature,
             "FeatureListCreatorExtraTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSwitchDependentTestFeature,
             "FeatureListCreatorSwitchDependentTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kFlagsTrialName[] = "FeatureListCreatorFlagsTrial";
constexpr char kConvertedSwitchEntryName[] =
    "feature-list-creator-converted-switch-entry";
constexpr char kFeatureWithParamsEntryName[] =
    "feature-list-creator-feature-entry";
// `FlagsState` appends `kConvertedTestSwitch` in `about:flags` switch
// conversion.
constexpr char kConvertedTestSwitch[] = "feature-list-creator-converted-switch";

constexpr unsigned short kAllPlatforms =
    flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
    flags_ui::kOsCrOS | flags_ui::kOsAndroid | flags_ui::kOsIos |
    flags_ui::kOsFuchsia | flags_ui::kOsWebView;

const flags_ui::FeatureEntry::FeatureParam kFlagsTestParams[] = {
    {"param", "value"},
};

const flags_ui::FeatureEntry::FeatureVariation kFlagsTestVariations[] = {
    {"variation", kFlagsTestParams, "3300200"},
};

const auto kTestFeatureEntries = std::to_array<flags_ui::FeatureEntry>({
    {kConvertedSwitchEntryName, "", "", kAllPlatforms,
     SINGLE_VALUE_TYPE(kConvertedTestSwitch)},
    {kFeatureWithParamsEntryName, "", "", kAllPlatforms,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFlagsTestFeature,
                                    kFlagsTestVariations,
                                    kFlagsTrialName)},
});

// Supplies a `MetricsServicesManagerClient` implementation for tests. Creates
// the `MetricsStateManager` and the `VariationsService` on demand.
class TestMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  explicit TestMetricsServicesManagerClient(PrefService* local_state)
      : local_state_(local_state),
        enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {}

  TestMetricsServicesManagerClient(const TestMetricsServicesManagerClient&) =
      delete;
  TestMetricsServicesManagerClient& operator=(
      const TestMetricsServicesManagerClient&) = delete;
  ~TestMetricsServicesManagerClient() override = default;

  // metrics_services_manager::MetricsServicesManagerClient:
  std::unique_ptr<VariationsService> CreateVariationsService() override {
    ++create_variations_service_count_;
    return std::make_unique<TestVariationsService>(local_state_,
                                                   GetMetricsStateManager());
  }
  std::unique_ptr<metrics::MetricsServiceClient> CreateMetricsServiceClient(
      SyntheticTrialRegistry* synthetic_trial_registry) override {
    NOTREACHED();
  }
  metrics::MetricsStateManager* GetMetricsStateManager() override {
    if (!metrics_state_manager_) {
      metrics_state_manager_ = metrics::MetricsStateManager::Create(
          local_state_, &enabled_state_provider_,
          /*backup_registry_key=*/std::wstring(),
          /*user_data_dir=*/base::FilePath());
    }
    return metrics_state_manager_.get();
  }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  const metrics::EnabledStateProvider& GetEnabledStateProvider() override {
    return enabled_state_provider_;
  }
  bool IsOffTheRecordSessionActive() override { return false; }

  // Returns the number of `CreateVariationsService()` calls.
  int create_variations_service_count() const {
    return create_variations_service_count_;
  }

 private:
  const raw_ptr<PrefService> local_state_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  int create_variations_service_count_ = 0;
};

// Records the `PlatformFieldTrials` calls in a shared call log.
class RecordingPlatformFieldTrials : public PlatformFieldTrials {
 public:
  explicit RecordingPlatformFieldTrials(std::vector<std::string>* call_log)
      : call_log_(CHECK_DEREF(call_log)) {}
  RecordingPlatformFieldTrials(const RecordingPlatformFieldTrials&) = delete;
  RecordingPlatformFieldTrials& operator=(const RecordingPlatformFieldTrials&) =
      delete;
  ~RecordingPlatformFieldTrials() override = default;

  // PlatformFieldTrials:
  void OnVariationsSetupComplete() override {
    call_log_->push_back("OnVariationsSetupComplete");
  }

 private:
  const base::raw_ref<std::vector<std::string>> call_log_;
};

// Supplies a `FeatureListCreatorClient` implementation for tests.
// Records calls in a shared call log and owns `FlagsState`.
class TestFeatureListCreatorClient : public FeatureListCreatorClient {
 public:
  using ExtraFeatureOverridesCallback =
      base::OnceCallback<std::vector<base::FeatureList::FeatureOverrideInfo>(
          const base::CommandLine& command_line)>;

  TestFeatureListCreatorClient(PrefService* local_state,
                               PlatformFieldTrials* platform_field_trials,
                               std::vector<std::string>* call_log)
      : local_state_(CHECK_DEREF(local_state)),
        platform_field_trials_(CHECK_DEREF(platform_field_trials)),
        call_log_(CHECK_DEREF(call_log)),
        flags_state_(kTestFeatureEntries, nullptr) {}

  TestFeatureListCreatorClient(const TestFeatureListCreatorClient&) = delete;
  TestFeatureListCreatorClient& operator=(const TestFeatureListCreatorClient&) =
      delete;
  ~TestFeatureListCreatorClient() override = default;

  void SetExtraFeatureOverridesCallback(
      ExtraFeatureOverridesCallback callback) {
    extra_feature_overrides_callback_ = std::move(callback);
  }

  void SetSwitchFilter(crash_keys::SwitchFilterFunction switch_filter) {
    switch_filter_ = switch_filter;
  }

  // FeatureListCreatorClient:
  flags_ui::FlagsState& GetFlagsState() override {
    call_log_->push_back("GetFlagsState");
    return flags_state_;
  }
  std::unique_ptr<flags_ui::FlagsStorage> CreateFlagsStorage() override {
    call_log_->push_back("CreateFlagsStorage");
    return std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        &local_state_.get());
  }
  crash_keys::SwitchFilterFunction GetSwitchFilter() override {
    call_log_->push_back("GetSwitchFilter");
    if (switch_filter_) {
      return switch_filter_;
    }
    return FeatureListCreatorClient::GetSwitchFilter();
  }
  metrics_services_manager::MetricsServicesManager&
  GetOrCreateMetricsServicesManager() override {
    call_log_->push_back("GetOrCreateMetricsServicesManager");
    field_trial_list_in_get_or_create_metrics_services_manager_ =
        base::FieldTrialList::GetInstance() != nullptr;
    auto metrics_services_manager_client =
        std::make_unique<TestMetricsServicesManagerClient>(&local_state_.get());
    metrics_services_manager_client_ = metrics_services_manager_client.get();
    metrics_services_manager_ =
        std::make_unique<metrics_services_manager::MetricsServicesManager>(
            std::move(metrics_services_manager_client));
    return *metrics_services_manager_;
  }
  void WillSetUpFieldTrials(base::FeatureList& feature_list) override {
    call_log_->push_back("WillSetUpFieldTrials");
    field_trial_list_in_will_set_up_field_trials_ =
        base::FieldTrialList::GetInstance() != nullptr;
    flags_trial_in_will_set_up_field_trials_ =
        base::FieldTrialList::TrialExists(kFlagsTrialName);
    feature_list_from_hook_ = &feature_list;
    accessor_ = feature_list.ConstructAccessor();
  }
  PlatformFieldTrials& GetPlatformFieldTrials() override {
    call_log_->push_back("GetPlatformFieldTrials");
    return *platform_field_trials_;
  }
  std::vector<base::FeatureList::FeatureOverrideInfo> GetExtraFeatureOverrides(
      const base::CommandLine& command_line) override {
    call_log_->push_back("GetExtraFeatureOverrides");
    if (extra_feature_overrides_callback_) {
      return std::move(extra_feature_overrides_callback_).Run(command_line);
    }
    return FeatureListCreatorClient::GetExtraFeatureOverrides(command_line);
  }

  base::FeatureList* feature_list_from_hook() const {
    return feature_list_from_hook_;
  }
  base::FeatureList::Accessor* accessor() const { return accessor_.get(); }
  metrics_services_manager::MetricsServicesManager* metrics_services_manager()
      const {
    return metrics_services_manager_.get();
  }
  // Returns the number of `CreateVariationsService()` calls. Returns 0 before
  // `GetOrCreateMetricsServicesManager()`.
  int create_variations_service_count() const {
    return metrics_services_manager_client_
               ? metrics_services_manager_client_
                     ->create_variations_service_count()
               : 0;
  }
  bool field_trial_list_in_get_or_create_metrics_services_manager() const {
    return field_trial_list_in_get_or_create_metrics_services_manager_;
  }
  bool field_trial_list_in_will_set_up_field_trials() const {
    return field_trial_list_in_will_set_up_field_trials_;
  }
  // Returns whether the about:flags trial existed when
  // `WillSetUpFieldTrials()` ran.
  bool flags_trial_in_will_set_up_field_trials() const {
    return flags_trial_in_will_set_up_field_trials_;
  }

 private:
  const base::raw_ref<PrefService> local_state_;
  const base::raw_ref<PlatformFieldTrials> platform_field_trials_;
  const base::raw_ref<std::vector<std::string>> call_log_;
  flags_ui::FlagsState flags_state_;
  ExtraFeatureOverridesCallback extra_feature_overrides_callback_;
  crash_keys::SwitchFilterFunction switch_filter_ = nullptr;
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;
  // `metrics_services_manager_` owns the client. Declare the raw pointer after
  // `metrics_services_manager_` for safe destruction order.
  raw_ptr<TestMetricsServicesManagerClient> metrics_services_manager_client_ =
      nullptr;
  raw_ptr<base::FeatureList> feature_list_from_hook_ = nullptr;
  std::unique_ptr<base::FeatureList::Accessor> accessor_;
  bool field_trial_list_in_get_or_create_metrics_services_manager_ = false;
  bool field_trial_list_in_will_set_up_field_trials_ = false;
  bool flags_trial_in_will_set_up_field_trials_ = false;
};

class FeatureListCreatorTest : public testing::Test {
 protected:
  FeatureListCreatorTest()
      : platform_field_trials_(&call_log_),
        client_(&local_state_, &platform_field_trials_, &call_log_) {
    // Do not apply the field trial test config.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kDisableFieldTrialTestingConfig);
    // Clear `base::FeatureList` and `base::FieldTrialList` to simulate a new
    // startup.
    scoped_feature_list_.InitWithNullFeatureAndFieldTrialLists();
    flags_ui::PrefServiceFlagsStorage::RegisterPrefs(local_state_.registry());
    flags_ui::PrefServiceFlagsStorage(&local_state_)
        .SetFlags({kConvertedSwitchEntryName,
                   kTestFeatureEntries[1].NameForOption(2)});
    metrics::MetricsStateManager::RegisterPrefs(local_state_.registry());
    VariationsService::RegisterPrefs(local_state_.registry());
  }

  void SetUp() override { crash_reporter::InitializeCrashKeysForTesting(); }

  void TearDown() override {
    clear_crash_keys_.RunAndReset();
    test::ClearAllVariationParams();
    crash_keys::ResetCommandLineForTesting();
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
    crash_reporter::ResetCrashKeysForTesting();
#endif
  }

  // Returns an extra overrides callback. The callback logs
  // `"ExtraFeatureOverrides"` in `call_log_` and enables `kExtraTestFeature`.
  TestFeatureListCreatorClient::ExtraFeatureOverridesCallback
  CreateExtraFeatureOverridesCallback() {
    return base::BindLambdaForTesting(
        [this](const base::CommandLine& command_line)
            -> std::vector<base::FeatureList::FeatureOverrideInfo> {
          call_log_.push_back("ExtraFeatureOverrides");
          return {{std::cref(kExtraTestFeature),
                   base::FeatureList::OVERRIDE_ENABLE_FEATURE}};
        });
  }

  // Runs `variations::CreateFeatureList()` with `client_` and
  // `extra_feature_overrides_callback`.
  void CreateFeatureList(
      TestFeatureListCreatorClient::ExtraFeatureOverridesCallback
          extra_feature_overrides_callback) {
    client_.SetExtraFeatureOverridesCallback(
        std::move(extra_feature_overrides_callback));
    variations::CreateFeatureList(client_);
    // `SetUpFieldTrials()` starts the variations crash keys. Remove the crash
    // keys at test end, before `scoped_feature_list_` restores
    // `base::FieldTrialList`.
    clear_crash_keys_ = base::ScopedClosureRunner(
        base::BindOnce(&ClearCrashKeysInstanceForTesting));
  }

  // Runs `variations::CreateFeatureList()` with `client_` and the callback
  // from `CreateExtraFeatureOverridesCallback()`.
  void CreateFeatureList() {
    CreateFeatureList(CreateExtraFeatureOverridesCallback());
  }

  // Declare `scoped_feature_list_` before `task_environment_` so
  // `base::FeatureList` outlives worker threads. Worker threads read feature
  // state until `task_environment_` stops the thread pool.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedCommandLine scoped_command_line_;
  test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      VariationsIdsProvider::Mode::kUseSignedInState};
  TestingPrefServiceSimple local_state_;
  std::vector<std::string> call_log_;
  RecordingPlatformFieldTrials platform_field_trials_;
  TestFeatureListCreatorClient client_;
  base::ScopedClosureRunner clear_crash_keys_;
};

TEST_F(FeatureListCreatorTest, CreateFeatureList_SetsGlobalInstances) {
  CreateFeatureList();

  EXPECT_NE(base::FieldTrialList::GetInstance(), nullptr);
  EXPECT_NE(base::FeatureList::GetInstance(), nullptr);
  EXPECT_EQ(base::FeatureList::GetInstance(), client_.feature_list_from_hook());
}

TEST_F(FeatureListCreatorTest, CreateFeatureList_CallsClientInOrder) {
  CreateFeatureList();

  EXPECT_THAT(
      call_log_,
      ElementsAre("CreateFlagsStorage", "GetFlagsState", "GetSwitchFilter",
                  "GetOrCreateMetricsServicesManager", "WillSetUpFieldTrials",
                  "GetExtraFeatureOverrides", "ExtraFeatureOverrides",
                  "GetPlatformFieldTrials", "OnVariationsSetupComplete"));
}

TEST_F(FeatureListCreatorTest, CreateFeatureList_AppliesAboutFlagsOverrides) {
  CreateFeatureList();

  EXPECT_TRUE(
      base::CommandLine::ForCurrentProcess()->HasSwitch(kConvertedTestSwitch));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFlagsTestFeature));
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kFlagsTrialName));
  EXPECT_THAT(VariationsIdsProvider::GetInstance()
                  ->GetVariationsVectorForWebPropertiesKeys(),
              ElementsAre(3300200));
  // `WillSetUpFieldTrials()` runs after about:flags parameter registration.
  EXPECT_TRUE(client_.flags_trial_in_will_set_up_field_trials());
}

TEST_F(FeatureListCreatorTest, CreateFeatureList_AppliesExtraFeatureOverrides) {
  CreateFeatureList();

  EXPECT_TRUE(base::FeatureList::IsEnabled(kExtraTestFeature));
}

// The test covers the ChromeOS accessor use case.
TEST_F(FeatureListCreatorTest,
       CreateFeatureList_AccessorFromHookReadsOverrides) {
  CreateFeatureList();

  ASSERT_NE(client_.accessor(), nullptr);
  EXPECT_EQ(
      client_.accessor()->GetOverrideStateByFeatureName(kFlagsTestFeature.name),
      base::FeatureList::OVERRIDE_ENABLE_FEATURE);
}

// `GetExtraFeatureOverrides()` must see the switches from the `about:flags`
// conversion.
TEST_F(FeatureListCreatorTest,
       CreateFeatureList_ExtraOverridesSeeConvertedSwitches) {
  ASSERT_FALSE(
      base::CommandLine::ForCurrentProcess()->HasSwitch(kConvertedTestSwitch));

  CreateFeatureList(base::BindOnce(
      [](const base::CommandLine& command_line)
          -> std::vector<base::FeatureList::FeatureOverrideInfo> {
        if (!command_line.HasSwitch(kConvertedTestSwitch)) {
          return {};
        }
        return {{std::cref(kSwitchDependentTestFeature),
                 base::FeatureList::OVERRIDE_ENABLE_FEATURE}};
      }));

  EXPECT_TRUE(base::FeatureList::IsEnabled(kSwitchDependentTestFeature));
}

// The test covers the iOS case, which has no extra overrides.
TEST_F(FeatureListCreatorTest, CreateFeatureList_DefaultExtraOverrides) {
  CreateFeatureList(
      TestFeatureListCreatorClient::ExtraFeatureOverridesCallback());

  EXPECT_NE(base::FeatureList::GetInstance(), nullptr);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kExtraTestFeature));
  EXPECT_THAT(
      call_log_,
      ElementsAre("CreateFlagsStorage", "GetFlagsState", "GetSwitchFilter",
                  "GetOrCreateMetricsServicesManager", "WillSetUpFieldTrials",
                  "GetExtraFeatureOverrides", "GetPlatformFieldTrials",
                  "OnVariationsSetupComplete"));
}

TEST_F(FeatureListCreatorTest,
       CreateFeatureList_CreatesFieldTrialListAfterMetricsServicesManager) {
  CreateFeatureList();

  EXPECT_FALSE(
      client_.field_trial_list_in_get_or_create_metrics_services_manager());
  EXPECT_TRUE(client_.field_trial_list_in_will_set_up_field_trials());
}

TEST_F(FeatureListCreatorTest,
       CreateFeatureList_ClientKeepsMetricsServicesManager) {
  ASSERT_EQ(client_.metrics_services_manager(), nullptr);

  CreateFeatureList();

  // `CreateFeatureList()` creates `VariationsService` one time in the
  // client-owned `MetricsServicesManager`.
  ASSERT_NE(client_.metrics_services_manager(), nullptr);
  EXPECT_EQ(client_.create_variations_service_count(), 1);
}

TEST_F(FeatureListCreatorTest,
       CreateFeatureList_SkipsSwitchConversionWhenNoExperimentsSet) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      ::switches::kNoExperiments);

  CreateFeatureList();

  EXPECT_THAT(
      call_log_,
      ElementsAre("GetSwitchFilter", "GetOrCreateMetricsServicesManager",
                  "WillSetUpFieldTrials", "GetExtraFeatureOverrides",
                  "ExtraFeatureOverrides", "GetPlatformFieldTrials",
                  "OnVariationsSetupComplete"));
  EXPECT_FALSE(
      base::CommandLine::ForCurrentProcess()->HasSwitch(kConvertedTestSwitch));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFlagsTestFeature));
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kFlagsTrialName));
  EXPECT_TRUE(VariationsIdsProvider::GetInstance()
                  ->GetVariationsVectorForWebPropertiesKeys()
                  .empty());
  EXPECT_FALSE(client_.flags_trial_in_will_set_up_field_trials());
}

#if !BUILDFLAG(USE_CRASH_KEY_STUBS)
constexpr char kFilteredTestSwitch[] = "feature-list-creator-filtered-switch";

bool FilterOutTestSwitch(const std::string& flag) {
  if (crash_keys::IsDefaultBoringSwitch(flag)) {
    return true;
  }
  return flag == std::string("--") + kFilteredTestSwitch;
}

TEST_F(FeatureListCreatorTest, CreateFeatureList_UpdatesCommandLineCrashKeys) {
  base::CommandLine* command_line =
      scoped_command_line_.GetProcessCommandLine();
  *command_line = base::CommandLine(base::CommandLine::NO_PROGRAM);
  command_line->AppendSwitch(switches::kDisableFieldTrialTestingConfig);
  command_line->AppendSwitch(kFilteredTestSwitch);
  client_.SetSwitchFilter(&FilterOutTestSwitch);

  CreateFeatureList();

  EXPECT_EQ(crash_reporter::GetCrashKeyValue("switch-1"),
            std::string("--") + switches::kDisableFieldTrialTestingConfig);
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("switch-2"),
            std::string("--") + kConvertedTestSwitch);
  EXPECT_TRUE(crash_reporter::GetCrashKeyValue("switch-3").empty());
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("commandline-enabled-feature-1"),
            std::string(kFlagsTestFeature.name) + ":param/value");
}
#endif  // !BUILDFLAG(USE_CRASH_KEY_STUBS)

TEST_F(FeatureListCreatorTest, CreateFeatureList_CrashesWhenAlreadyCreated) {
  CreateFeatureList();

  EXPECT_CHECK_DEATH(variations::CreateFeatureList(client_));
}

}  // namespace
}  // namespace variations
