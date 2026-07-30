// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/performance_class.h"

#include "base/memory/safe_ref.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "services/on_device_model/public/cpp/cpu.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(PerformanceClassTest, IsPerformanceClassCompatible) {
  EXPECT_FALSE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kError));
  EXPECT_TRUE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kMedium));
  EXPECT_FALSE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kHigh));
  EXPECT_TRUE(IsPerformanceClassCompatible(
      "4,6", OnDeviceModelPerformanceClass::kVeryHigh));
}

TEST(PerformanceClassTest, UpdatePrefs) {
  TestingPrefServiceSimple prefs_;
  model_execution::prefs::RegisterLocalStatePrefs(prefs_.registry());

  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kUnknown);

  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kMedium);

  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kMedium);
}

TEST(PerformanceClassTest, ConvertFromMojom) {
  // Converting from mojom should not be lossy.
  using PerformanceClass = on_device_model::mojom::PerformanceClass;
  std::vector<PerformanceClass> inputs{
      PerformanceClass::kError,      PerformanceClass::kVeryLow,
      PerformanceClass::kLow,        PerformanceClass::kMedium,
      PerformanceClass::kHigh,       PerformanceClass::kVeryHigh,
      PerformanceClass::kGpuBlocked, PerformanceClass::kFailedToLoadLibrary,
  };
  std::set<OnDeviceModelPerformanceClass> outputs;
  for (auto mojo_val : inputs) {
    outputs.insert(ConvertToOnDeviceModelPerformanceClass(mojo_val));
  }
  EXPECT_EQ(outputs.size(), inputs.size());
}

TEST(PerformanceClassTest, GroupsAreUnique) {
  // Synthetic trial groups should be distinct.
  std::vector<OnDeviceModelPerformanceClass> inputs{
      OnDeviceModelPerformanceClass::kError,
      OnDeviceModelPerformanceClass::kVeryLow,
      OnDeviceModelPerformanceClass::kLow,
      OnDeviceModelPerformanceClass::kMedium,
      OnDeviceModelPerformanceClass::kHigh,
      OnDeviceModelPerformanceClass::kVeryHigh,
      OnDeviceModelPerformanceClass::kGpuBlocked,
      OnDeviceModelPerformanceClass::kFailedToLoadLibrary,
      OnDeviceModelPerformanceClass::kServiceCrash,
  };
  std::set<std::string> outputs;
  for (auto mojo_val : inputs) {
    outputs.insert(
        std::string(SyntheticTrialGroupForPerformanceClass(mojo_val)));
  }
  EXPECT_EQ(outputs.size(), inputs.size());
}

class PerformanceClassPossibleHintsTest : public testing::Test {
 public:
  PerformanceClassPossibleHintsTest()
      : client_(base::BindRepeating(
            [](::mojo::PendingReceiver<
                on_device_model::mojom::OnDeviceModelService>) {})) {
    model_execution::prefs::RegisterLocalStatePrefs(prefs_.registry());
    UpdatePerformanceClassPref(&prefs_,
                               OnDeviceModelPerformanceClass::kUnknown);
    classifier_ =
        std::make_unique<PerformanceClassifier>(&prefs_, client_.GetSafeRef());
  }

  void SetUp() override {
    if (!on_device_model::IsCpuCapable()) {
      GTEST_SKIP() << "CPU not supported";
    }
  }

 protected:
  TestingPrefServiceSimple& prefs() { return prefs_; }
  PerformanceClassifier& classifier() { return *classifier_; }

 private:
  TestingPrefServiceSimple prefs_;
  on_device_model::ServiceClient client_;
  std::unique_ptr<PerformanceClassifier> classifier_;
};

TEST_F(PerformanceClassPossibleHintsTest, VeryLowDevice) {
  UpdatePerformanceClassPref(&prefs(), OnDeviceModelPerformanceClass::kVeryLow);
  EXPECT_THAT(
      classifier().GetPossibleHints(),
      testing::ElementsAre(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

TEST_F(PerformanceClassPossibleHintsTest, LowDevice) {
  UpdatePerformanceClassPref(&prefs(), OnDeviceModelPerformanceClass::kLow);
  EXPECT_THAT(classifier().GetPossibleHints(),
              testing::ElementsAre(
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

TEST_F(PerformanceClassPossibleHintsTest, MediumDevice) {
  UpdatePerformanceClassPref(&prefs(), OnDeviceModelPerformanceClass::kMedium);
  EXPECT_THAT(classifier().GetPossibleHints(),
              testing::ElementsAre(
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

TEST_F(PerformanceClassPossibleHintsTest, HighDevice) {
  UpdatePerformanceClassPref(&prefs(), OnDeviceModelPerformanceClass::kHigh);
  EXPECT_THAT(classifier().GetPossibleHints(),
              testing::ElementsAre(
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY,
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

TEST_F(PerformanceClassPossibleHintsTest, VeryHighDevice) {
  UpdatePerformanceClassPref(&prefs(),
                             OnDeviceModelPerformanceClass::kVeryHigh);
  EXPECT_THAT(classifier().GetPossibleHints(),
              testing::ElementsAre(
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY,
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE,
                  proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

TEST_F(PerformanceClassPossibleHintsTest, ForceCpu) {
  base::test::ScopedFeatureList scoped_feature_list(
      on_device_model::features::kOnDeviceModelForceCpuBackend);
  UpdatePerformanceClassPref(&prefs(),
                             OnDeviceModelPerformanceClass::kVeryHigh);
  EXPECT_THAT(
      classifier().GetPossibleHints(),
      testing::ElementsAre(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU));
}

class PerformanceClassCapabilitiesTest : public testing::Test {
 public:
  PerformanceClassCapabilitiesTest()
      : client_(base::BindRepeating(
            [](::mojo::PendingReceiver<
                on_device_model::mojom::OnDeviceModelService>) {})) {
    model_execution::prefs::RegisterLocalStatePrefs(prefs_.registry());
    UpdatePerformanceClassPref(&prefs_,
                               OnDeviceModelPerformanceClass::kUnknown);
    classifier_ =
        std::make_unique<PerformanceClassifier>(&prefs_, client_.GetSafeRef());
  }

 protected:
  TestingPrefServiceSimple& prefs() { return prefs_; }
  PerformanceClassifier& classifier() { return *classifier_; }

 private:
  TestingPrefServiceSimple prefs_;
  on_device_model::ServiceClient client_;
  std::unique_ptr<PerformanceClassifier> classifier_;
};

TEST_F(PerformanceClassCapabilitiesTest,
       SupportsAudioInput_GpuCapable_HighVram) {
  UpdatePerformanceClassPref(&prefs(), OnDeviceModelPerformanceClass::kHigh);
  UpdateVramPref(&prefs(), 6144);
  EXPECT_TRUE(classifier().SupportsAudioInput());
}

TEST_F(PerformanceClassCapabilitiesTest,
       SupportsAudioInput_GpuCapable_LowVram) {
  UpdatePerformanceClassPref(&prefs(), OnDeviceModelPerformanceClass::kHigh);
  UpdateVramPref(&prefs(), 5000);
  EXPECT_FALSE(classifier().SupportsAudioInput());
}

class PerformanceClassifierTest : public testing::Test {
 protected:
  PerformanceClassifierTest() {
    model_execution::prefs::RegisterLocalStatePrefs(prefs_.registry());
  }

  void SetUp() override {
    classifier_ = std::make_unique<PerformanceClassifier>(
        &prefs_, service_client_.GetSafeRef());
  }

  void TearDown() override { classifier_.reset(); }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  on_device_model::ServiceClient service_client_{base::BindRepeating(
      [](mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>) {
      })};
  std::unique_ptr<PerformanceClassifier> classifier_;
};

TEST_F(PerformanceClassifierTest,
       OnDeviceAndPerformanceInfo_SignificantChange) {
  // Set initial performance class.
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kLow);
  UpdateDeviceInfoPrefs(&prefs_, on_device_model::mojom::DeviceInfo(
                                     0x1234, 0x5678, "1.0.0", false));

  // Create performance info with a significant change (Error is always
  // significant).
  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  perf_info->performance_class =
      on_device_model::mojom::PerformanceClass::kError;
  perf_info->vram_mb = 1024;

  auto device_info = on_device_model::mojom::DeviceInfo::New();
  device_info->vendor_id = 0x1234;        // Same as before.
  device_info->device_id = 0x5678;        // Same as before.
  device_info->driver_version = "1.0.0";  // Same as before.
  device_info->supports_fp16 = false;

  classifier_->OnDeviceAndPerformanceInfo(std::move(perf_info),
                                          std::move(device_info));

  // Should update prefs due to significant performance change.
  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kError);
}

TEST_F(PerformanceClassifierTest,
       OnDeviceAndPerformanceInfo_DeviceInfoChanged) {
  // Set initial performance class and device info.
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kMedium);
  UpdateDeviceInfoPrefs(&prefs_, on_device_model::mojom::DeviceInfo(
                                     0x1234, 0x5678, "1.0.0", false));

  // Create performance info with same class but different device info.
  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  perf_info->performance_class =
      on_device_model::mojom::PerformanceClass::kMedium;
  perf_info->vram_mb = 1024;

  auto device_info = on_device_model::mojom::DeviceInfo::New();
  device_info->vendor_id = 0xABCD;  // Different vendor ID.
  device_info->device_id = 0x5678;
  device_info->driver_version = "1.0.0";
  device_info->supports_fp16 = false;

  classifier_->OnDeviceAndPerformanceInfo(std::move(perf_info),
                                          std::move(device_info));

  // Should update prefs due to device info change.
  std::string expected_gpu_id = "abcd:5678:1.0.0";
  EXPECT_EQ(
      prefs_.GetString(
          model_execution::prefs::localstate::kOnDevicePerformanceClassGPUId),
      expected_gpu_id);
}

TEST_F(PerformanceClassifierTest,
       OnDeviceAndPerformanceInfo_InsignificantChange) {
  // Set initial performance class in VeryLow-VeryHigh range.
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kVeryHigh);
  UpdateDeviceInfoPrefs(&prefs_, on_device_model::mojom::DeviceInfo(
                                     0x1234, 0x5678, "1.0.0", false));

  // Create performance info with downgrade to medium, typically this happens
  // when the user is now on battery power.
  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  perf_info->performance_class =
      on_device_model::mojom::PerformanceClass::kMedium;
  perf_info->vram_mb = 1024;

  auto device_info = on_device_model::mojom::DeviceInfo::New();
  device_info->vendor_id = 0x1234;        // Same as before.
  device_info->device_id = 0x5678;        // Same as before.
  device_info->driver_version = "1.0.0";  // Same as before.
  device_info->supports_fp16 = false;

  classifier_->OnDeviceAndPerformanceInfo(std::move(perf_info),
                                          std::move(device_info));
  // Performance class should remain unchanged.
  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kVeryHigh);
}

TEST_F(PerformanceClassifierTest,
       OnDeviceAndPerformanceInfo_SuppressedDowngradeUpdatesVersion) {
  // A suppressed run-to-run downgrade should still advance the stored browser
  // version so that classification is not re-run on every startup.
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kVeryHigh);
  UpdateDeviceInfoPrefs(&prefs_, on_device_model::mojom::DeviceInfo(
                                     0x1234, 0x5678, "1.0.0", false));
  prefs_.SetString(
      model_execution::prefs::localstate::kOnDevicePerformanceClassVersion,
      "0.0.0.0");

  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  perf_info->performance_class =
      on_device_model::mojom::PerformanceClass::kMedium;
  perf_info->vram_mb = 1024;

  auto device_info = on_device_model::mojom::DeviceInfo::New();
  device_info->vendor_id = 0x1234;        // Same as before.
  device_info->device_id = 0x5678;        // Same as before.
  device_info->driver_version = "1.0.0";  // Same as before.
  device_info->supports_fp16 = false;

  classifier_->OnDeviceAndPerformanceInfo(std::move(perf_info),
                                          std::move(device_info));

  // The downgrade is suppressed, so the performance class is unchanged.
  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kVeryHigh);
  // The version pref is advanced to the current browser version regardless.
  EXPECT_EQ(
      prefs_.GetString(
          model_execution::prefs::localstate::kOnDevicePerformanceClassVersion),
      version_info::GetVersionNumber());
}

TEST_F(PerformanceClassifierTest, OnDeviceAndPerformanceInfo_ServiceCrash) {
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kMedium);
  classifier_->OnDeviceAndPerformanceInfo(nullptr, nullptr);
  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kServiceCrash);
}

TEST_F(PerformanceClassifierTest,
       OnDeviceAndPerformanceInfo_SameDeviceUpgrade) {
  // Same device, moving from a lower to a higher class. An upgrade is always
  // significant, so the performance class should be updated.
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kLow);
  UpdateDeviceInfoPrefs(&prefs_, on_device_model::mojom::DeviceInfo(
                                     0x1234, 0x5678, "1.0.0", false));

  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  perf_info->performance_class =
      on_device_model::mojom::PerformanceClass::kHigh;
  perf_info->vram_mb = 1024;

  auto device_info = on_device_model::mojom::DeviceInfo::New();
  device_info->vendor_id = 0x1234;        // Same as before.
  device_info->device_id = 0x5678;        // Same as before.
  device_info->driver_version = "1.0.0";  // Same as before.
  device_info->supports_fp16 = false;

  classifier_->OnDeviceAndPerformanceInfo(std::move(perf_info),
                                          std::move(device_info));

  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kHigh);
}

TEST_F(PerformanceClassifierTest,
       OnDeviceAndPerformanceInfo_DeviceChangedUpgrade) {
  // Device info changed and moving from a lower to a higher class. The changed
  // device info alone forces an update.
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kLow);
  UpdateDeviceInfoPrefs(&prefs_, on_device_model::mojom::DeviceInfo(
                                     0x1234, 0x5678, "1.0.0", false));

  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  perf_info->performance_class =
      on_device_model::mojom::PerformanceClass::kHigh;
  perf_info->vram_mb = 1024;

  auto device_info = on_device_model::mojom::DeviceInfo::New();
  device_info->vendor_id = 0xABCD;  // Different vendor ID.
  device_info->device_id = 0x5678;
  device_info->driver_version = "1.0.0";
  device_info->supports_fp16 = false;

  classifier_->OnDeviceAndPerformanceInfo(std::move(perf_info),
                                          std::move(device_info));

  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kHigh);
  EXPECT_EQ(
      prefs_.GetString(
          model_execution::prefs::localstate::kOnDevicePerformanceClassGPUId),
      "abcd:5678:1.0.0");
}

TEST_F(PerformanceClassifierTest,
       OnDeviceAndPerformanceInfo_DeviceChangedDowngrade) {
  // Device info changed and moving from a higher to a lower class. A downgrade
  // on the same device would be suppressed as run-to-run variation, but a
  // genuine device change must still be recorded.
  UpdatePerformanceClassPref(&prefs_, OnDeviceModelPerformanceClass::kHigh);
  UpdateDeviceInfoPrefs(&prefs_, on_device_model::mojom::DeviceInfo(
                                     0x1234, 0x5678, "1.0.0", false));

  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  perf_info->performance_class = on_device_model::mojom::PerformanceClass::kLow;
  perf_info->vram_mb = 1024;

  auto device_info = on_device_model::mojom::DeviceInfo::New();
  device_info->vendor_id = 0xABCD;  // Different vendor ID.
  device_info->device_id = 0x5678;
  device_info->driver_version = "1.0.0";
  device_info->supports_fp16 = false;

  classifier_->OnDeviceAndPerformanceInfo(std::move(perf_info),
                                          std::move(device_info));

  EXPECT_EQ(PerformanceClassFromPref(prefs_),
            OnDeviceModelPerformanceClass::kLow);
}

}  // namespace

}  // namespace optimization_guide
