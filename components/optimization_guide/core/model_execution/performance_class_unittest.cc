// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/performance_class.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/cpu.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/service_client.h"
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
  };
  std::set<std::string> outputs;
  for (auto mojo_val : inputs) {
    outputs.insert(SyntheticTrialGroupForPerformanceClass(mojo_val));
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

}  // namespace

}  // namespace optimization_guide
