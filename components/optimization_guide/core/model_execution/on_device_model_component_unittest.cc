// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include <memory>

#include "base/scoped_add_feature_flags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

const base::Value::Dict kTestManifest = base::Value::Dict().Set(
    "BaseModelSpec",
    base::Value::Dict().Set("version", "0.0.1").Set("name", "Test"));

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

    local_state_.SetInteger(
        model_execution::prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kLow));
    local_state_.SetTime(
        model_execution::prefs::GetOnDeviceFeatureRecentlyUsedPref(
            ModelBasedCapabilityKey::kCompose),
        base::Time::Now());

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel,
          {{"compatible_on_device_performance_classes", "3,4,5,6"}}}},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    // Try to detect mistakes in the tests. If any lingering tasks affect state,
    // the test may have not waited before asserting state.
    bool uninstalled =
        on_device_component_state_manager_.WasComponentUninstalled();
    bool installer_registered =
        on_device_component_state_manager_.IsInstallerRegistered();
    WaitForStartup();
    ASSERT_EQ(uninstalled,
              on_device_component_state_manager_.WasComponentUninstalled());
    ASSERT_EQ(installer_registered,
              on_device_component_state_manager_.IsInstallerRegistered());
  }

  scoped_refptr<OnDeviceModelComponentStateManager> manager() {
    return on_device_component_state_manager_.get();
  }

  void WaitForStartup() {
    // In the event we want to verify the installer is not installed, there's no
    // event to quit a RunLoop. For now this works well enough.
    task_environment_.FastForwardBy(base::Seconds(1));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &local_state_};
  base::HistogramTester histograms_;
};

TEST_F(OnDeviceModelComponentTest, InstallsWhenEligible) {
  const auto time_at_start = base::Time::Now();
  manager()->OnStartup();
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
  EXPECT_GE(local_state_.GetTime(model_execution::prefs::localstate::
                                     kLastTimeEligibleForOnDeviceModelDownload),
            time_at_start);
  EXPECT_LE(local_state_.GetTime(model_execution::prefs::localstate::
                                     kLastTimeEligibleForOnDeviceModelDownload),
            base::Time::Now());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DiskSpace",
      true, 1);
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
  manager()->OnStartup();
  WaitForStartup();

  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);

  manager()->InstallerRegistered();

  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime",
      true, 1);
}

TEST_F(OnDeviceModelComponentTest, NotYetInstalledFlow) {
  manager()->OnStartup();
  WaitForStartup();

  manager()->InstallerRegistered();

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
    on_device_component_state_manager_.Reset();
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(*feature);

    manager()->OnStartup();
    WaitForStartup();
    EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
    histograms.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
        "AtRegistration.EnabledByFeature",
        false, 1);
  }
}

TEST_F(OnDeviceModelComponentTest,
       DoesNotInstallWhenDisabledByEnterprisePolicy) {
  // It should not install when disabled by enterprise policy.
  base::HistogramTester histogram_tester;
  local_state_.SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));

  on_device_component_state_manager_.Reset();
  manager()->OnStartup();
  WaitForStartup();
  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.EnabledByEnterprisePolicy",
      false, 1);
}

TEST_F(OnDeviceModelComponentTest, NotEnoughDiskSpaceToInstall) {
  // 20gb is the default in `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
  on_device_component_state_manager_.SetFreeDiskSpace(
      20ll * 1024 * 1024 * 1024 - 1);

  manager()->OnStartup();
  WaitForStartup();

  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DiskSpace",
      false, 1);
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.All",
      false, 1);
}

TEST_F(OnDeviceModelComponentTest, NoEligibleFeatureUse) {
  local_state_.ClearPref(
      model_execution::prefs::GetOnDeviceFeatureRecentlyUsedPref(
          ModelBasedCapabilityKey::kCompose));

  manager()->OnStartup();
  WaitForStartup();

  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.FeatureUse",
      false, 1);
}

TEST_F(OnDeviceModelComponentTest, EligibleFeatureUseTooOld) {
  local_state_.SetTime(
      model_execution::prefs::GetOnDeviceFeatureRecentlyUsedPref(
          ModelBasedCapabilityKey::kCompose),
      base::Time::Now() - base::Days(31));

  manager()->OnStartup();
  WaitForStartup();

  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, NoPerformanceClass) {
  local_state_.ClearPref(
      model_execution::prefs::localstate::kOnDevicePerformanceClass);

  manager()->OnStartup();
  WaitForStartup();

  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, PerformanceClassTooLow) {
  local_state_.SetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass,
      base::to_underlying(OnDeviceModelPerformanceClass::kVeryLow));

  manager()->OnStartup();
  WaitForStartup();

  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DeviceCapability",
      false, 1);
}

TEST_F(OnDeviceModelComponentTest, UninstallNeeded) {
  // This pref records that the model was eligible for download previously,
  // and hasn't been cleaned up yet.
  local_state_.SetTime(model_execution::prefs::localstate::
                           kLastTimeEligibleForOnDeviceModelDownload,
                       base::Time::Now() - base::Minutes(1) -
                           features::GetOnDeviceModelRetentionTime());
  local_state_.ClearPref(
      model_execution::prefs::GetOnDeviceFeatureRecentlyUsedPref(
          ModelBasedCapabilityKey::kCompose));

  // Should uninstall the first time, and skip uninstallation the next time.
  manager()->OnStartup();
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.WasComponentUninstalled());

  manager()->UninstallComplete();

  manager()->OnStartup();
  WaitForStartup();

  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, UninstallNeededDueToDiskSpace) {
  local_state_.SetTime(model_execution::prefs::localstate::
                           kLastTimeEligibleForOnDeviceModelDownload,
                       base::Time::Now());

  // 10gb is the default in `IsFreeDiskSpaceTooLowForOnDeviceModelInstall`.
  on_device_component_state_manager_.SetFreeDiskSpace(
      10ll * 1024 * 1024 * 1024 - 1);

  // Should uninstall right away. Unlike most install requirements, the disk
  // space requirement is not subject to `GetOnDeviceModelRetentionTime()`.
  manager()->OnStartup();
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.WasComponentUninstalled());
}

TEST_F(OnDeviceModelComponentTest, KeepInstalledWhileNotEligible) {
  // If the model is already installed, we don't uninstall right away.

  // Trigger installer registration.
  manager()->OnStartup();
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());

  // Simulate a restart, and clear feature recently used pref so that the model
  // is no longer eligible for download.
  on_device_component_state_manager_.Reset();
  local_state_.ClearPref(
      model_execution::prefs::GetOnDeviceFeatureRecentlyUsedPref(
          ModelBasedCapabilityKey::kCompose));
  manager()->OnStartup();
  WaitForStartup();

  // The installer is still registered. Even if the component is ready, it's not
  // exposed via GetState().
  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);

  // The model is still available.
  EXPECT_TRUE(manager()->GetState());
}

TEST_F(OnDeviceModelComponentTest, KeepInstalledWhileNotAllowed) {
  // Same test as KeepInstalledWhileNotEligible, but in this case the model
  // should not be used (because performance class is not supported) even though
  // it's installed.
  manager()->OnStartup();
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
  on_device_component_state_manager_.Reset();
  manager()->DevicePerformanceClassChanged(
      OnDeviceModelPerformanceClass::kVeryLow);
  manager()->OnStartup();
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);

  EXPECT_FALSE(manager()->GetState())
      << "state available even though performance class is not supported";
}

TEST_F(OnDeviceModelComponentTest, GetStateInitiallyNull) {
  EXPECT_EQ(manager()->GetState(), nullptr);
}

TEST_F(OnDeviceModelComponentTest, SetReady) {
  manager()->OnStartup();
  WaitForStartup();

  StubObserver observer;
  manager()->AddObserver(&observer);
  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);

  const OnDeviceModelComponentState* state = manager()->GetState();
  ASSERT_TRUE(state);

  EXPECT_EQ(state->GetInstallDirectory(),
            base::FilePath(FILE_PATH_LITERAL("/some/path")));
  EXPECT_EQ(state->GetComponentVersion(), base::Version("0.1.1"));
  ASSERT_EQ(observer.GetState(), state);
}

TEST_F(OnDeviceModelComponentTest, InstallAfterPerformanceClassChanges) {
  // This sequence would happen on first run.
  local_state_.ClearPref(
      model_execution::prefs::localstate::kOnDevicePerformanceClass);

  StubObserver observer;
  manager()->AddObserver(&observer);
  manager()->OnStartup();
  WaitForStartup();

  ASSERT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
  EXPECT_FALSE(manager()->GetState());

  manager()->DevicePerformanceClassChanged(OnDeviceModelPerformanceClass::kLow);
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());

  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);

  EXPECT_TRUE(manager()->GetState());
  EXPECT_TRUE(observer.GetState());
}

TEST_F(OnDeviceModelComponentTest, PerformanceClassChangesAfterInstall) {
  // Start 1: registers the component as device is eligible.
  StubObserver observer;
  manager()->AddObserver(&observer);
  manager()->OnStartup();
  WaitForStartup();
  ASSERT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());

  // Start 2: device is no longer eligible, but component is registered because
  // it's already installed.
  on_device_component_state_manager_.Reset();
  manager()->AddObserver(&observer);
  manager()->DevicePerformanceClassChanged(
      OnDeviceModelPerformanceClass::kServiceCrash);
  manager()->OnStartup();
  WaitForStartup();
  ASSERT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());

  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);

  // State is not available, because device is not eligible.
  EXPECT_FALSE(manager()->GetState());
  EXPECT_FALSE(observer.GetState());

  // Device is now eligible
  manager()->DevicePerformanceClassChanged(
      OnDeviceModelPerformanceClass::kHigh);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager()->GetState());
  EXPECT_TRUE(observer.GetState());
}

TEST_F(OnDeviceModelComponentTest, DontUninstallAfterPerformanceClassChanges) {
  manager()->OnStartup();
  WaitForStartup();

  ASSERT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());

  manager()->DevicePerformanceClassChanged(OnDeviceModelPerformanceClass::kLow);
  WaitForStartup();

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
  EXPECT_FALSE(on_device_component_state_manager_.WasComponentUninstalled());
}

TEST_F(OnDeviceModelComponentTest, InstallAfterEligibleFeatureWasUsed) {
  local_state_.ClearPref(
      model_execution::prefs::GetOnDeviceFeatureRecentlyUsedPref(
          ModelBasedCapabilityKey::kCompose));
  manager()->OnStartup();
  WaitForStartup();

  ASSERT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());

  manager()->OnDeviceEligibleFeatureUsed(ModelBasedCapabilityKey::kCompose);
  WaitForStartup();
  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, LogsStatusOnUse) {
  manager()->OnStartup();
  WaitForStartup();

  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);

  manager()->InstallerRegistered();

  manager()->OnDeviceEligibleFeatureUsed(ModelBasedCapabilityKey::kCompose);

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

TEST_F(OnDeviceModelComponentTest, SetPrefsWhenManifestContainsBaseModelSpec) {
  manager()->OnStartup();
  WaitForStartup();
  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      kTestManifest);  // manifest is populated with test data.
  EXPECT_EQ(manager()->GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager()->GetState()->GetBaseModelSpec().model_version, "0.0.1");
}

TEST_F(OnDeviceModelComponentTest, SetStateWhenModelOverridden) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOnDeviceModelExecutionOverride, "/some/path");
  manager()->OnStartup();
  EXPECT_EQ(manager()->GetState()->GetBaseModelSpec().model_name, "override");
  EXPECT_EQ(manager()->GetState()->GetBaseModelSpec().model_version,
            "override");
}

}  // namespace
}  // namespace optimization_guide
