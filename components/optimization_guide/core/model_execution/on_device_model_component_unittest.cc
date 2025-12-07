// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

#include <memory>

#include "base/byte_count.h"
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
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/cpu.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-data-view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using ::testing::UnorderedElementsAre;

using model_execution::prefs::
    GenAILocalFoundationalModelEnterprisePolicySettings;
using model_execution::prefs::localstate::
    kGenAILocalFoundationalModelEnterprisePolicySettings;
using model_execution::prefs::localstate::
    kLastTimeEligibleForOnDeviceModelDownload;
using model_execution::prefs::localstate::kLastUsageByFeature;
using model_execution::prefs::localstate::kOnDevicePerformanceClassVersion;
using ::on_device_model::mojom::PerformanceClass;

// All hints, in a weird order and with duplicates and unspecified value.
std::vector<proto::OnDeviceModelPerformanceHint> AllHints() {
  return {
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU,
  };
}

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
    broker_.service_settings().performance_class = PerformanceClass::kLow;
    model_execution::prefs::RecordFeatureUsage(
        &broker_.local_state(), mojom::OnDeviceFeature::kCompose);
  }

  void TearDown() override {
    // Try to detect mistakes in the tests. If any lingering tasks affect state,
    // the test may have not waited before asserting state.
    bool uninstalled = broker_.component_state().uninstall_called();
    bool installer_registered =
        broker_.component_state().installer_registered();
    task_environment_.FastForwardBy(base::Seconds(1));
    ASSERT_EQ(uninstalled, broker_.component_state().uninstall_called());
    ASSERT_EQ(installer_registered,
              broker_.component_state().installer_registered());
  }

  void DoStartup() {
    broker_.GetOrCreateBrokerState();  // Force instantiation.
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  void SimulateShutdown() { broker_.SimulateShutdown(); }

  PerformanceClassifier& classifier() {
    return broker_.GetOrCreateBrokerState().performance_classifier();
  }

  OnDeviceModelComponentStateManager& manager() {
    return broker_.GetOrCreateBrokerState().component_state_manager();
  }

  void EnsurePerformanceClassAvailable() {
    broker_.GetOrCreateBrokerState()
        .performance_classifier()
        .EnsurePerformanceClassAvailable(base::DoNothing());
  }

  bool WaitUntilInstallerRegistered() {
    return broker_.component_state().WaitForRegistration();
  }

  bool WaitForUnexpectedInstallerRegistered() {
    task_environment_.FastForwardBy(base::Seconds(1));
    return broker_.component_state().installer_registered();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeModelBroker broker_{{
      .performance_class = OnDeviceModelPerformanceClass::kUnknown,
      .preinstall_base_model = false,
  }};
  base::HistogramTester histograms_;
};

TEST_F(OnDeviceModelComponentTest, InstallsWhenEligible) {
  const auto time_at_start = base::Time::Now();
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_GE(
      broker_.local_state().GetTime(kLastTimeEligibleForOnDeviceModelDownload),
      time_at_start);
  EXPECT_LE(
      broker_.local_state().GetTime(kLastTimeEligibleForOnDeviceModelDownload),
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
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime",
      true, 1);
}

TEST_F(OnDeviceModelComponentTest, NotYetInstalledFlow) {
  // No broker_.component_state().Install() call here.
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
  broker_.local_state().SetInteger(
      kGenAILocalFoundationalModelEnterprisePolicySettings,
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
  broker_.local_state().SetInteger(
      kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return broker_.component_state().uninstall_called(); }));

  // Enabling the policy should trigger installation.
  broker_.local_state().SetInteger(
      kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, NotEnoughDiskSpaceToInstall) {
  // 20gb is the default in `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
  broker_.component_state().SetFreeDiskSpace(base::GiB(20) -
                                             base::ByteCount(1));
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
  broker_.local_state().ClearPref(kLastUsageByFeature);
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
  ASSERT_TRUE(broker_.local_state().GetDict(kLastUsageByFeature).empty());
}

TEST_F(OnDeviceModelComponentTest, NoPerformanceClass) {
  DoStartup();
  // No EnsurePerformanceClassAvailable()
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, PerformanceClassTooLow) {
  broker_.service_settings().performance_class = PerformanceClass::kVeryLow;
  DoStartup();
  EnsurePerformanceClassAvailable();
  // We may still install the model given a "very low" performance class if the
  // device is capable of running the model on CPU.
  const bool expect_device_is_capable = on_device_model::IsCpuCapable();
  if (expect_device_is_capable) {
    ASSERT_TRUE(WaitUntilInstallerRegistered());
  } else {
    ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
  }
  histograms_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DeviceCapability",
      expect_device_is_capable, 1);
}

TEST_F(OnDeviceModelComponentTest, UninstallNeeded) {
  // This pref records that the model was eligible for download previously,
  // and hasn't been cleaned up yet.
  broker_.local_state().SetTime(kLastTimeEligibleForOnDeviceModelDownload,
                                base::Time::Now() - base::Minutes(1) -
                                    features::GetOnDeviceModelRetentionTime());
  broker_.local_state().ClearPref(kLastUsageByFeature);

  // Should uninstall the first time, and skip uninstallation the next time.
  DoStartup();
  EnsurePerformanceClassAvailable();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return broker_.component_state().uninstall_called(); }));

  manager().UninstallComplete();

  SimulateShutdown();
  DoStartup();

  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, UninstallNeededDueToDiskSpace) {
  broker_.local_state().SetTime(kLastTimeEligibleForOnDeviceModelDownload,
                                base::Time::Now());

  // 10gb is the default in `IsFreeDiskSpaceTooLowForOnDeviceModelInstall`.
  broker_.component_state().SetFreeDiskSpace(base::GiB(5) - base::ByteCount(1));

  // Should uninstall right away. Unlike most install requirements, the disk
  // space requirement is not subject to `GetOnDeviceModelRetentionTime()`.
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return broker_.component_state().uninstall_called(); }));
}

TEST_F(OnDeviceModelComponentTest, KeepInstalledWhileNotEligible) {
  // If the model is already installed, we don't uninstall right away.

  // Trigger installer registration.
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(WaitUntilInstallerRegistered());
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));
  SimulateShutdown();

  // Clear usage prefs so that the model is no longer eligible for download.
  broker_.local_state().ClearPref(kLastUsageByFeature);
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

  std::vector<proto::OnDeviceModelPerformanceHint> hints{
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY};
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(hints));
  SimulateShutdown();

  broker_.local_state().SetString(kOnDevicePerformanceClassVersion, "0.0.0.1");
  // This performance class is not supported with `hints`.
  broker_.service_settings().performance_class = PerformanceClass::kVeryLow;
  DoStartup();
  EnsurePerformanceClassAvailable();

  EXPECT_TRUE(WaitUntilInstallerRegistered());
  EXPECT_FALSE(manager().GetState())
      << "state available even though performance class is not supported";
}

TEST_F(OnDeviceModelComponentTest, NeedsPerformanceClassUpdateEveryStartup) {
  base::test::ScopedFeatureList feature_list(
      features::kOnDeviceModelFetchPerformanceClassEveryStartup);
  broker_.service_settings().performance_class = PerformanceClass::kVeryHigh;
  DoStartup();
  EXPECT_FALSE(classifier().IsPerformanceClassAvailable());
  base::RunLoop run_loop;
  classifier().EnsurePerformanceClassAvailable(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(broker_.launcher().did_launch_service());
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryHigh);
  SimulateShutdown();

  broker_.launcher().clear_did_launch_service();
  broker_.service_settings().performance_class = PerformanceClass::kLow;
  DoStartup();
  EXPECT_FALSE(classifier().IsPerformanceClassAvailable());
  base::RunLoop run_loop2;
  classifier().EnsurePerformanceClassAvailable(run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_TRUE(broker_.launcher().did_launch_service());
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kLow);

  // The original model is still installed, but we won't run it because the
  // performance class is too low.
  EXPECT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_FALSE(manager().GetState());
}

TEST_F(OnDeviceModelComponentTest, NeedsPerformanceClassUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kOnDeviceModelFetchPerformanceClassEveryStartup);
  broker_.service_settings().performance_class = PerformanceClass::kVeryHigh;
  DoStartup();
  EXPECT_FALSE(classifier().IsPerformanceClassAvailable());
  base::RunLoop run_loop;
  classifier().EnsurePerformanceClassAvailable(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(broker_.launcher().did_launch_service());
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryHigh);
  SimulateShutdown();

  broker_.launcher().clear_did_launch_service();
  broker_.service_settings().performance_class = PerformanceClass::kVeryLow;
  DoStartup();
  EXPECT_TRUE(classifier().IsPerformanceClassAvailable());
  EXPECT_EQ(classifier().GetPerformanceClass(),
            OnDeviceModelPerformanceClass::kVeryHigh);
  base::RunLoop run_loop2;
  classifier().EnsurePerformanceClassAvailable(run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(broker_.launcher().did_launch_service());
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
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));

  const OnDeviceModelComponentState* state = manager().GetState();
  ASSERT_TRUE(state);

  EXPECT_FALSE(state->GetInstallDirectory().empty());
  EXPECT_EQ(state->GetComponentVersion(), base::Version("0.0.1"));
  ASSERT_EQ(observer.GetState(), state);
}

TEST_F(OnDeviceModelComponentTest, InstallAfterEligibleFeatureWasUsed) {
  broker_.local_state().ClearPref(kLastUsageByFeature);
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());

  broker_.GetOrCreateBrokerState().usage_tracker().OnDeviceEligibleFeatureUsed(
      mojom::OnDeviceFeature::kCompose);
  EXPECT_TRUE(WaitUntilInstallerRegistered());
}

TEST_F(OnDeviceModelComponentTest, LogsStatusOnUse) {
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));
  DoStartup();
  EnsurePerformanceClassAvailable();
  EXPECT_TRUE(WaitUntilInstallerRegistered());

  broker_.GetOrCreateBrokerState().usage_tracker().OnDeviceEligibleFeatureUsed(
      mojom::OnDeviceFeature::kCompose);

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

TEST_F(OnDeviceModelComponentTest, SetStateWhenModelOverridden) {
  FakeBaseModelAsset asset;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOnDeviceModelExecutionOverride, asset.path().MaybeAsASCII());
  DoStartup();
  EnsurePerformanceClassAvailable();
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(manager().GetState());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "override");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "override");
}

TEST_F(OnDeviceModelComponentTest, EmptyPerformanceHintsRejected) {
  broker_.service_settings().performance_class = PerformanceClass::kHigh;
  broker_.component_state().Install(std::make_unique<FakeBaseModelAsset>(
      std::vector<proto::OnDeviceModelPerformanceHint>{}));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_FALSE(manager().GetState());
}

TEST_F(OnDeviceModelComponentTest, HighTierDeviceSelectsHighestQualityHint) {
  broker_.service_settings().performance_class = PerformanceClass::kHigh;
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_TRUE(manager().GetState());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().selected_performance_hint,
            proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY);
}

TEST_F(OnDeviceModelComponentTest, LowTierDeviceSelectsFastestInferenceHint) {
  broker_.service_settings().performance_class = PerformanceClass::kLow;
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_TRUE(manager().GetState());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().selected_performance_hint,
            proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE);
}

TEST_F(OnDeviceModelComponentTest, CpuOnlyDeviceRejectsGpuOnlyModel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      on_device_model::features::kOnDeviceModelCpuBackend,
      {{"on_device_cpu_ram_threshold_mb", "0"},
       {"on_device_cpu_processor_count_threshold", "0"},
       {"on_device_cpu_require_64_bit_processor", "false"}});
  broker_.service_settings().performance_class = PerformanceClass::kVeryLow;
  std::vector<proto::OnDeviceModelPerformanceHint> gpu_hints{
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY,
  };
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(gpu_hints));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_FALSE(manager().GetState());
}

TEST_F(OnDeviceModelComponentTest, CpuOnlyDeviceSelectsCpuHint) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      on_device_model::features::kOnDeviceModelCpuBackend,
      {{"on_device_cpu_ram_threshold_mb", "0"},
       {"on_device_cpu_processor_count_threshold", "0"},
       {"on_device_cpu_require_64_bit_processor", "false"}});
  broker_.service_settings().performance_class = PerformanceClass::kVeryLow;
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_TRUE(manager().GetState());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().selected_performance_hint,
            proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
}

TEST_F(OnDeviceModelComponentTest, CpuOnlyRequire64BitProcessor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      on_device_model::features::kOnDeviceModelCpuBackend,
      {{"on_device_cpu_ram_threshold_mb", "0"},
       {"on_device_cpu_processor_count_threshold", "0"},
       // Require 64-bit devices.
       {"on_device_cpu_require_64_bit_processor", "true"}});
  broker_.service_settings().performance_class = PerformanceClass::kVeryLow;
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(AllHints()));
  DoStartup();
  EnsurePerformanceClassAvailable();

#if defined(ARCH_CPU_64_BITS)
  // If the device has a 64-bit processor, the model should be downloaded.
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_TRUE(manager().GetState());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().selected_performance_hint,
            proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
#else
  // Otherwise, the model should not be downloaded.
  ASSERT_FALSE(WaitForUnexpectedInstallerRegistered());
#endif  // defined(ARCH_CPU_64_BITS)
}

TEST_F(OnDeviceModelComponentTest, GpuCapableDeviceAndCpuOnlyManifest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      on_device_model::features::kOnDeviceModelCpuBackend,
      {{"on_device_cpu_ram_threshold_mb", "0"},
       {"on_device_cpu_processor_count_threshold", "0"},
       {"on_device_cpu_require_64_bit_processor", "false"}});
  broker_.service_settings().performance_class = PerformanceClass::kHigh;
  std::vector<proto::OnDeviceModelPerformanceHint> hints{
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU,
  };
  broker_.component_state().Install(
      std::make_unique<FakeBaseModelAsset>(hints));
  DoStartup();
  EnsurePerformanceClassAvailable();
  ASSERT_TRUE(WaitUntilInstallerRegistered());
  ASSERT_TRUE(manager().GetState());
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_name, "Test");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().model_version, "0.0.1");
  EXPECT_EQ(manager().GetState()->GetBaseModelSpec().selected_performance_hint,
            proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
}

}  // namespace
}  // namespace optimization_guide
