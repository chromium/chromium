// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_add_feature_flags.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-data-view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using ::testing::UnorderedElementsAre;

using ::on_device_model::mojom::PerformanceClass;
using model_execution::prefs::GenAILocalFoundationalModelEnterprisePolicySettings;
using model_execution::prefs::localstate::
    kGenAILocalFoundationalModelEnterprisePolicySettings;
using model_execution::prefs::localstate::
    kLastTimeEligibleForOnDeviceModelDownload;
using model_execution::prefs::localstate::kLastUsageByFeature;
using model_execution::prefs::localstate::kOnDevicePerformanceClassVersion;

class StubObserver : public OnDeviceModelComponentStateManager::Observer {
 public:
  void StateChanged(const OnDeviceModelComponentState* state) override {
    state_ = state;
  }

  const OnDeviceModelComponentState* GetState() { return state_; }

 private:
  raw_ptr<const OnDeviceModelComponentState> state_;
};

class OnDeviceModelComponentTest : public testing::Test {
 public:
  void SetUp() override {
    model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
    fake_settings_.performance_class = PerformanceClass::kLow;
    model_execution::prefs::RecordFeatureUsage(
        &local_state_, ModelBasedCapabilityKey::kCompose);

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel, {}},
         {features::kOnDeviceModelPerformanceParams,
          {{"compatible_on_device_performance_classes", "3,4,5,6"},
           {"compatible_low_tier_on_device_performance_classes", "3"}}}},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    // Try to detect mistakes in the tests. If any lingering tasks affect state,
    // the test may have not waited before asserting state.
    bool uninstalled = test_component_state_.uninstall_called();
    bool installer_registered = test_component_state_.installer_registered();
    task_environment_.FastForwardBy(base::Seconds(1));
    ASSERT_EQ(uninstalled, test_component_state_.uninstall_called());
    ASSERT_EQ(installer_registered,
              test_component_state_.installer_registered());
  }

  void DoStartup() {
    CHECK(!model_broker_state_);
    model_broker_state_.emplace(&local_state_,
                                test_component_state_.CreateDelegate(),
                                fake_launcher_.LaunchFn());
    model_broker_state_->Init();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  void SimulateShutdown() {
    model_broker_state_.reset();
    test_component_state_.SimulateShutdown();
  }

  PerformanceClassifier& classifier() {
    return model_broker_state_->performance_classifier();
  }

  OnDeviceModelComponentStateManager& manager() {
    return model_broker_state_->component_state_manager();
  }

  void EnsurePerformanceClassAvailable() {
    model_broker_state_->performance_classifier()
        .EnsurePerformanceClassAvailable(base::DoNothing());
  }

  bool WaitUntilInstallerRegistered() {
    return test_component_state_.WaitForRegistration();
  }

  bool WaitForUnexpectedInstallerRegistered() {
    task_environment_.FastForwardBy(base::Seconds(1));
    return test_component_state_.installer_registered();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;
  FakeBaseModelAsset fake_asset_;
  TestComponentState test_component_state_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  std::optional<ModelBrokerState> model_broker_state_;
  base::HistogramTester histograms_;
};

TEST_F(OnDeviceModelComponentTest, InstallsWhenEligible) {
  const auto time_at_start = base::Time::Now();
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_GE(local_state_.GetTime(kLastTimeEligibleForOnDeviceModelDownload),
            time_at_start);
  EXPECT_LE(local_state_.GetTime(kLastTimeEligibleForOnDeviceModelDownload),
            base::Time::Now());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DiskSpace",
      true, 1);
  // Device has disk space. Histogram should not log.
  histograms_.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DiskSpaceWhenNotEnoughAvailable",
      0);
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DeviceCapability",
      true, 1);
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.FeatureUse",
      true, 1);
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.EnabledByFeature",
      true, 1);
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.All",
      true, 1);
}

TEST_F(OnDeviceModelComponentTest, AlreadyInstalledFlow) {
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>());
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime",
      true, 1);
}

TEST_F(OnDeviceModelComponentTest, NotYetInstalledFlow) {
  // No test_component_state_.Install() call here.
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime",
      false, 1);
}

TEST_F(OnDeviceModelComponentTest, DoesNotInstallWhenFeatureNotEnabled) {
  // It should not install if any of these features are disabled.
  for (const base::Feature* feature :
       {&features::kOptimizationGuideModelExecution,
        &features::kOptimizationGuideOnDeviceModel}) {
    SCOPED_TRACE(feature->name);
    base::HistogramTester histograms;
    SimulateShutdown();
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(*feature);

    DoStartup();
    EnsurePerformanceClassAvailable();
    ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
    histograms.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
        "AtRegistration.EnabledByFeature",
        false, 1);
  }
}

TEST_F(OnDeviceModelComponentTest,
       DoesNotInstallWhenDisabledByEnterprisePolicy) {
  // It should not install when disabled by enterprise policy.
  local_state_.SetInteger(kGenAILocalFoundationalModelEnterprisePolicySettings,
                          static_cast<int>(
                              GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.EnabledByEnterprisePolicy",
      false, 1);
}

// Dynamically change the enterprise policy and ensure the component is
// installed/uninstalled accordingly.
TEST_F(OnDeviceModelComponentTest, DynamicEnterprisePolicyChange) {
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.EnabledByEnterprisePolicy",
      true, 1);

  // Disabling the policy should trigger uninstallation.
  local_state_.SetInteger(kGenAILocalFoundationalModelEnterprisePolicySettings,
                          static_cast<int>(
                              GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_component_state_.uninstall_called(); }));

  // Enabling the policy should trigger installation.
  local_state_.SetInteger(kGenAILocalFoundationalModelEnterprisePolicySettings,
                          static_cast<int>(GenAILocalFoundationalModelEnterprisePolicySettings::
                                               kAllowed));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, NotEnoughDiskSpaceToInstall) {
  // 20gb is the default in `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
  test_component_state_.SetFreeDiskSpace(20ll * 1024 * 1024 * 1024 - 1);
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DiskSpace",
      false, 1);
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.All",
      false, 1);
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DiskSpaceWhenNotEnoughAvailable",
      19, 1);
}

TEST_F(OnDeviceModelComponentTest, NoEligibleFeatureUse) {
  local_state_.ClearPref(kLastUsageByFeature);
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.FeatureUse",
      false, 1);
}

TEST_F(OnDeviceModelComponentTest, EligibleFeatureUseTooOld) {
  task_environment_.FastForwardBy(base::Days(31));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
  // The usage should also get pruned from the pref.
  ASSERT_TRUE(local_state_.GetDict(kLastUsageByFeature).empty());
}

TEST_F(OnDeviceModelComponentTest, NoPerformanceClass) {
  DoStartup();
  // No EnsurePerformanceClassAvailable()
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, PerformanceClassTooLow) {
  fake_settings_.performance_class = PerformanceClass::kVeryLow;
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DeviceCapability",
      false, 1);
}

TEST_F(OnDeviceModelComponentTest, UninstallNeeded) {
  // This pref records that the model was eligible for download previously,
  // and hasn't been cleaned up yet.
  local_state_.SetTime(kLastTimeEligibleForOnDeviceModelDownload,
                       base::Time::Now() - base::Minutes(1) -
                           features::GetOnDeviceModelRetentionTime());
  local_state_.ClearPref(kLastUsageByFeature);

  // Should uninstall the first time, and skip uninstallation the next time.
  DoStartup();
  EnsurePerformanceClassAvailable();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_component_state_.uninstall_called(); }));

  manager().UninstallComplete();

  SimulateShutdown();
  DoStartup();

  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, UninstallNeededDueToDiskSpace) {
  local_state_.SetTime(kLastTimeEligibleForOnDeviceModelDownload, base::Time::Now());

  // 10gb is the default in `IsFreeDiskSpaceTooLowForOnDeviceModelInstall`.
  test_component_state_.SetFreeDiskSpace(10ll * 1024 * 1024 * 1024 - 1);

  // Should uninstall right away. Unlike most install requirements, the disk
  // space requirement is not subject to `GetOnDeviceModelRetentionTime()`.
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return test_component_state_.uninstall_called(); }));
}

TEST_F(OnDeviceModelComponentTest, KeepInstalledWhileNotEligible) {
  // If the model is already installed, we don't uninstall right away.

  // Trigger installer registration.
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(WaitUntilInstallerRegistered());
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>());
  SimulateShutdown();

  // Clear usage prefs so that the model is no longer eligible for download.
  local_state_.ClearPref(kLastUsageByFeature);
  DoStartup();
  EnsurePerformanceClassAvailable();

  // The installer is still registered.
  EXPECT_TRUE(WaitUntilInstallerRegistered());
  // The model is still available.
  EXPECT_TRUE(manager().GetState());
}

TEST_F(OnDeviceModelComponentTest, KeepInstalledWhileNotAllowed) {
  // Same test as KeepInstalledWhileNotEligible, but in this case the model
  // should not be used (because performance class is not supported) even though
  // it's installed.
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(WaitUntilInstallerRegistered());
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>());
  SimulateShutdown();

  local_state_.SetString(kOnDevicePerformanceClassVersion, "0.0.0.1");
  fake_settings_.performance_class = PerformanceClass::kVeryLow;
  DoStartup();
  EnsurePerformanceClassAvailable();

  EXPECT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_FALSE(manager().GetState())
      << "state available even though performance class is not supported";
}

TEST_F(OnDeviceModelComponentTest, NeedsPerformanceClassUpdateEveryStartup) {
  base::test::ScopedFeatureList feature_list(
      features::kOnDeviceModelFetchPerformanceClassEveryStartup);
  fake_settings_.performance_class = PerformanceClass::kVeryHigh;
  DoStartup();
  EXPECT_FALSE(classifier().IsPerformanceClassAvailable());
  base::RunLoop run_loop;
  classifier().EnsurePerformanceClassAvailable(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(fake_launcher_.did_launch_service());
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryHigh);
  SimulateShutdown();

  fake_launcher_.clear_did_launch_service();
  fake_settings_.performance_class = PerformanceClass::kVeryLow;
  DoStartup();
  EXPECT_FALSE(classifier().IsPerformanceClassAvailable());
  base::RunLoop run_loop2;
  classifier().EnsurePerformanceClassAvailable(run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_TRUE(fake_launcher_.did_launch_service());
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryLow);
}

TEST_F(OnDeviceModelComponentTest, NeedsPerformanceClassUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kOnDeviceModelFetchPerformanceClassEveryStartup);
  fake_settings_.performance_class = PerformanceClass::kVeryHigh;
  DoStartup();
  EXPECT_FALSE(classifier().IsPerformanceClassAvailable());
  base::RunLoop run_loop;
  classifier().EnsurePerformanceClassAvailable(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(fake_launcher_.did_launch_service());
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryHigh);
  SimulateShutdown();

  fake_launcher_.clear_did_launch_service();
  fake_settings_.performance_class = PerformanceClass::kVeryLow;
  DoStartup();
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryHigh);
  base::RunLoop run_loop2;
  classifier().EnsurePerformanceClassAvailable(run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(fake_launcher_.did_launch_service());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryHigh);
}

TEST_F(OnDeviceModelComponentTest, GetStateInitiallyNull) {
  DoStartup();
  EXPECT_EQ(manager().GetState(), nullptr);
}

TEST_F(OnDeviceModelComponentTest, SetReady) {
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(WaitUntilInstallerRegistered());

  StubObserver observer;
  manager().AddObserver(&observer);
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>());

  const OnDeviceModelComponentState* state = manager().GetState();
  ASSERT_TRUE(state);

  EXPECT_FALSE(state->GetInstallDirectory().empty());
  EXPECT_EQ(state->GetComponentVersion(), base::Version("0.0.1"));
  ASSERT_EQ(observer.GetState(), state);
}

TEST_F(OnDeviceModelComponentTest, InstallAfterEligibleFeatureWasUsed) {
  local_state_.ClearPref(kLastUsageByFeature);
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());

  manager().OnDeviceEligibleFeatureUsed(ModelBasedCapabilityKey::kCompose);
  EXPECT_TRUE(WaitUntilInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, LogsStatusOnUse) {
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>());
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(WaitUntilInstallerRegistered());

  manager().OnDeviceEligibleFeatureUsed(ModelBasedCapabilityKey::kCompose);

  histograms_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelStatusAtUseTime",
      OnDeviceModelStatus::kReady, 1);
  histograms_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtAttemptedUse.All",
      true, 1);
  histograms_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtAttemptedUse.DeviceCapability",
      true, 1);
  histograms_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtAttemptedUse.DiskSpace",
      true, 1);
  histograms_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtAttemptedUse.EnabledByFeature",
      true, 1);
  histograms_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtAttemptedUse.FeatureUse",
      true, 1);
}

TEST_F(OnDeviceModelComponentTest, SetStateWhenManifestContainsBaseModelSpec) {
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>(
      std::vector<proto::OnDeviceModelPerformanceHint>{}));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_TRUE(manager()
                  .GetState()
                  ->GetBaseModelSpec()
                  .supported_performance_hints.empty());
}

TEST_F(OnDeviceModelComponentTest, SetStateWhenModelOverridden) {
  FakeBaseModelAsset asset;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOnDeviceModelExecutionOverride, asset.path().MaybeAsASCII());
  DoStartup();
  EnsurePerformanceClassAvailable();
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "override");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "override");
}

TEST_F(OnDeviceModelComponentTest, SetReadyManifestContainsPerformanceHints) {
  fake_settings_.performance_class = PerformanceClass::kHigh;
  std::vector<proto::OnDeviceModelPerformanceHint> hints{
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU,
  };
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>(hints));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_THAT(
      manager().GetState()->GetBaseModelSpec().supported_performance_hints,
      UnorderedElementsAre(
          proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY));
}

TEST_F(OnDeviceModelComponentTest,
       SetReadyManifestContainsPerformanceHintsLowTierDevice) {
  fake_settings_.performance_class = PerformanceClass::kLow;
  std::vector<proto::OnDeviceModelPerformanceHint> hints{
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU,
  };
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>(hints));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_THAT(
      manager().GetState()->GetBaseModelSpec().supported_performance_hints,
      UnorderedElementsAre(
          proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE));
}

TEST_F(OnDeviceModelComponentTest, ManifestContainsPerformanceHintsCPU) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      on_device_model::features::kOnDeviceModelCpuBackend,
      {{"on_device_cpu_ram_threshold_mb", "0"},
       {"on_device_cpu_processor_count_threshold", "0"}});
  fake_settings_.performance_class = PerformanceClass::kVeryLow;
  std::vector<proto::OnDeviceModelPerformanceHint> hints{
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU,
  };
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>(hints));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_TRUE(manager().GetState());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_THAT(
      manager().GetState()->GetBaseModelSpec().supported_performance_hints,
      UnorderedElementsAre(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

TEST_F(OnDeviceModelComponentTest, ManifestContainsPerformanceHintsCPUOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      on_device_model::features::kOnDeviceModelCpuBackend,
      {{"on_device_cpu_ram_threshold_mb", "0"},
       {"on_device_cpu_processor_count_threshold", "0"}});
  fake_settings_.performance_class = PerformanceClass::kHigh;
  std::vector<proto::OnDeviceModelPerformanceHint> hints{
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU,
  };
  test_component_state_.Install(std::make_unique<FakeBaseModelAsset>(hints));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_THAT(
      manager().GetState()->GetBaseModelSpec().supported_performance_hints,
      UnorderedElementsAre(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

}  // namespace
}  // namespace optimization_guide
