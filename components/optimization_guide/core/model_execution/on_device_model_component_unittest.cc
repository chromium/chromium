// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include <memory>

#include "base/scoped_add_feature_flags.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/optimization_guide/core/model_execution/test_on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

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
    prefs::RegisterLocalStatePrefs(local_state_.registry());

    local_state_.SetInteger(
        prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kLow));
    local_state_.SetTime(
        prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed,
        base::Time::Now());

    feature_list_.InitWithFeatures({features::kOptimizationGuideModelExecution,
                                    features::kOptimizationGuideOnDeviceModel,
                                    features::kLogOnDeviceMetricsOnStartup},
                                   {});
  }

  scoped_refptr<OnDeviceModelComponentStateManager> manager() {
    return on_device_component_state_manager_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &local_state_};
};

TEST_F(OnDeviceModelComponentTest, InstallsWhenEligible) {
  manager()->OnStartup();

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
  EXPECT_EQ(local_state_.GetTime(
                prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload),
            base::Time::Now());
}

TEST_F(OnDeviceModelComponentTest, DoesNotInstallWhenFeatureNotEnabled) {
  // It should not install if any of these features are disabled.
  for (const base::Feature* feature :
       {&features::kOptimizationGuideModelExecution,
        &features::kOptimizationGuideOnDeviceModel,
        &features::kLogOnDeviceMetricsOnStartup}) {
    SCOPED_TRACE(feature->name);
    on_device_component_state_manager_.Reset();
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(*feature);

    manager()->OnStartup();
    EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
  }
}

TEST_F(OnDeviceModelComponentTest, NoEligibleFeatureUse) {
  local_state_.ClearPref(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed);

  manager()->OnStartup();
  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, EligibleFeatureUseTooOld) {
  local_state_.SetTime(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed,
      base::Time::Now() - base::Days(31));

  manager()->OnStartup();
  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, NoPerformanceClass) {
  local_state_.ClearPref(prefs::localstate::kOnDevicePerformanceClass);

  manager()->OnStartup();
  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, PerformanceClassTooLow) {
  local_state_.SetInteger(
      prefs::localstate::kOnDevicePerformanceClass,
      base::to_underlying(OnDeviceModelPerformanceClass::kVeryLow));

  manager()->OnStartup();
  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, UninstallNeeded) {
  // This pref records that the model was eligible for download previously,
  // and hasn't been cleaned up yet.
  local_state_.SetTime(
      prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload,
      base::Time::Now() - base::Minutes(1) -
          features::GetOnDeviceModelRetentionTime());
  local_state_.ClearPref(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed);

  // Should uninstall the first time, and skip uninstallation the next time.
  manager()->OnStartup();
  EXPECT_TRUE(on_device_component_state_manager_.WasComponentUninstalled());

  manager()->UninstallComplete();

  manager()->OnStartup();
  EXPECT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, InstallWhileNotEligible) {
  // If the model is already installed, we don't uninstall right away.
  local_state_.SetTime(
      prefs::localstate::kLastTimeEligibleForOnDeviceModelDownload,
      base::Time::Now() - base::Days(1));
  local_state_.ClearPref(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed);

  manager()->OnStartup();
  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, GetStateInitiallyNull) {
  EXPECT_EQ(manager()->GetState(), nullptr);
}

TEST_F(OnDeviceModelComponentTest, SetReady) {
  StubObserver observer;
  manager()->AddObserver(&observer);
  manager()->SetReady(base::Version("0.1.1"),
                      base::FilePath(FILE_PATH_LITERAL("/some/path")),
                      base::Value::Dict());

  const OnDeviceModelComponentState* state = manager()->GetState();
  ASSERT_TRUE(state);

  EXPECT_EQ(state->GetInstallDirectory(),
            base::FilePath(FILE_PATH_LITERAL("/some/path")));
  EXPECT_EQ(state->GetVersion(), base::Version("0.1.1"));
  ASSERT_EQ(observer.GetState(), state);
}

TEST_F(OnDeviceModelComponentTest, InstallAfterPerformanceClassChanges) {
  // This sequence would happen on first run.
  local_state_.ClearPref(prefs::localstate::kOnDevicePerformanceClass);
  manager()->OnStartup();
  ASSERT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());

  manager()->DevicePerformanceClassChanged(OnDeviceModelPerformanceClass::kLow);

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, DontUninstallAfterPerformanceClassChanges) {
  manager()->OnStartup();
  ASSERT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());

  manager()->DevicePerformanceClassChanged(OnDeviceModelPerformanceClass::kLow);

  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
  EXPECT_FALSE(on_device_component_state_manager_.WasComponentUninstalled());
}

TEST_F(OnDeviceModelComponentTest, InstallAfterEligibleFeatureWasUsed) {
  local_state_.ClearPref(
      prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed);
  manager()->OnStartup();
  ASSERT_FALSE(on_device_component_state_manager_.IsInstallerRegistered());

  manager()->OnDeviceEligibleFeatureUsed();
  EXPECT_TRUE(on_device_component_state_manager_.IsInstallerRegistered());
}

}  // namespace
}  // namespace optimization_guide
