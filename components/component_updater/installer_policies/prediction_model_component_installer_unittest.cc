// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/prediction_model_component_installer.h"

#include <array>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/prediction_model_component_configs.h"
#include "components/optimization_guide/core/delivery/prediction_model_component_update_listener.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

namespace {

const char kTestVersion[] = "1.0.0.0";

constexpr optimization_guide::proto::OptimizationTarget kTestTarget =
    optimization_guide::proto::
        OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;

class PredictionModelMockComponentUpdateService
    : public MockComponentUpdateService {
 public:
  PredictionModelMockComponentUpdateService() = default;
  ~PredictionModelMockComponentUpdateService() override = default;
};

class FakeOptimizationTargetModelObserver
    : public optimization_guide::OptimizationTargetModelObserver {
 public:
  FakeOptimizationTargetModelObserver() = default;
  ~FakeOptimizationTargetModelObserver() override = default;

  void OnModelUpdated(optimization_guide::proto::OptimizationTarget target,
                      base::optional_ref<const optimization_guide::ModelInfo>
                          model_info) override {
    state_.target = target;
    if (model_info.has_value()) {
      state_.model_info =
          std::make_unique<optimization_guide::ModelInfo>(*model_info);
    } else {
      state_.model_info.reset();
    }
    state_.call_count++;
  }

  std::optional<optimization_guide::proto::OptimizationTarget> last_target()
      const {
    return state_.target;
  }
  const optimization_guide::ModelInfo* last_model_info() const {
    return state_.model_info.get();
  }
  int call_count() const { return state_.call_count; }

 private:
  struct State {
    std::optional<optimization_guide::proto::OptimizationTarget> target;
    std::unique_ptr<optimization_guide::ModelInfo> model_info;
    int call_count = 0;
  };
  State state_;
};

// A dummy key hash for testing.
const std::array<uint8_t, 32> kTestPublicKeySHA256 = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};

class FakeModelDir {
 public:
  explicit FakeModelDir(int64_t version) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::WriteFile(
        path().Append(optimization_guide::GetBaseFileNameForModels()),
        "dummy model content");

    optimization_guide::proto::ModelInfo model_info;
    model_info.set_optimization_target(kTestTarget);
    model_info.set_version(version);
    std::string serialized;
    EXPECT_TRUE(model_info.SerializeToString(&serialized));
    base::WriteFile(
        path().Append(optimization_guide::GetBaseFileNameForModelInfo()),
        serialized);
  }

  const base::FilePath& path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

}  // namespace

class PredictionModelComponentInstallerTest : public PlatformTest {
 public:
  PredictionModelComponentInstallerTest()
      : fallback_provider_(OptimizationGuideLogger::GetInstance()),
        model_dir_(123) {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeatureWithParameters(
        optimization_guide::kPredictionModelComponentDelivery,
        {{"targets", base::NumberToString(static_cast<int>(kTestTarget))}});

    // Create a default config for testing.
    config_ =
        std::make_unique<optimization_guide::PredictionModelComponentConfig>(
            "Test Component", std::vector<uint8_t>(kTestPublicKeySHA256.begin(),
                                                   kTestPublicKeySHA256.end()));

    policy_ = CreatePredictionModelComponentInstallerPolicy(
        kTestTarget, *config_, listener_.GetWeakPtr());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedPathOverride scoped_path_override_{DIR_COMPONENT_USER};
  base::test::ScopedFeatureList feature_list_;
  optimization_guide::ModelProviderRegistry fallback_provider_;
  optimization_guide::PredictionModelComponentUpdateListener listener_{
      fallback_provider_, base::DoNothing()};
  std::unique_ptr<optimization_guide::PredictionModelComponentConfig> config_;
  std::unique_ptr<ComponentInstallerPolicy> policy_;
  FakeModelDir model_dir_;
  PredictionModelMockComponentUpdateService cus_;
};

TEST_F(PredictionModelComponentInstallerTest,
       VerifyInstallationEmptyDirectory) {
  base::ScopedTempDir empty_dir;
  ASSERT_TRUE(empty_dir.CreateUniqueTempDir());
  EXPECT_FALSE(
      policy_->VerifyInstallation(base::DictValue(), empty_dir.GetPath()));
}

TEST_F(PredictionModelComponentInstallerTest, VerifyInstallationModelFileOnly) {
  // Delete the model info file so only the model file is present.
  ASSERT_TRUE(base::DeleteFile(model_dir_.path().Append(
      optimization_guide::GetBaseFileNameForModelInfo())));
  EXPECT_FALSE(
      policy_->VerifyInstallation(base::DictValue(), model_dir_.path()));
}

TEST_F(PredictionModelComponentInstallerTest,
       VerifyInstallationModelInfoFileOnly) {
  // Delete the model file so only the model info file is present.
  ASSERT_TRUE(base::DeleteFile(model_dir_.path().Append(
      optimization_guide::GetBaseFileNameForModels())));
  EXPECT_FALSE(
      policy_->VerifyInstallation(base::DictValue(), model_dir_.path()));
}

TEST_F(PredictionModelComponentInstallerTest, VerifyInstallationSuccess) {
  EXPECT_TRUE(
      policy_->VerifyInstallation(base::DictValue(), model_dir_.path()));
}

TEST_F(PredictionModelComponentInstallerTest, ComponentReadyNotifiesListener) {
  FakeOptimizationTargetModelObserver observer;
  listener_.AddObserverForOptimizationTargetModel(kTestTarget, std::nullopt,
                                                  nullptr, &observer);

  policy_->ComponentReady(base::Version(kTestVersion), model_dir_.path(),
                          base::DictValue());

  // Loading happens on background thread.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 1; }));

  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(observer.last_target(), kTestTarget);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 123);

  listener_.RemoveObserverForOptimizationTargetModel(kTestTarget, &observer);
}

TEST_F(PredictionModelComponentInstallerTest, GetRelativeInstallDir) {
  base::FilePath expected_dir =
      base::FilePath(FILE_PATH_LITERAL("OptGuidePredictionModels"))
          .AppendASCII("GeolocationPermissions");
  EXPECT_EQ(policy_->GetRelativeInstallDir(), expected_dir);
}

TEST_F(PredictionModelComponentInstallerTest, GetHashAndName) {
  std::vector<uint8_t> hash;
  policy_->GetHash(&hash);
  EXPECT_EQ(hash, std::vector<uint8_t>(kTestPublicKeySHA256.begin(),
                                       kTestPublicKeySHA256.end()));
  EXPECT_EQ(policy_->GetName(), "Test Component");
}

TEST_F(PredictionModelComponentInstallerTest,
       DoesNotRegisterComponentWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      optimization_guide::kPredictionModelComponentDelivery);

  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(0);

  RegisterPredictionModelComponent(&cus_, kTestTarget, listener_.GetWeakPtr());
}

TEST_F(PredictionModelComponentInstallerTest,
       RegistersComponentWithFeatureEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::kPredictionModelComponentDelivery,
      {{"targets", base::NumberToString(static_cast<int>(kTestTarget))}});

  base::RunLoop run_loop;
  EXPECT_CALL(cus_, RegisterComponent(testing::_))
      .WillOnce([&](const ComponentRegistration& registration) {
        run_loop.Quit();
        return true;
      });

  RegisterPredictionModelComponent(&cus_, kTestTarget, listener_.GetWeakPtr());
  run_loop.Run();
}

TEST_F(PredictionModelComponentInstallerTest,
       DoesNotRegisterComponentWhenNoConfig) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::kPredictionModelComponentDelivery,
      {{"targets", base::NumberToString(static_cast<int>(
                       optimization_guide::proto::
                           OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD))}});

  // OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD does not have config in
  // prediction_model_component_configs.cc.
  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(0);

  RegisterPredictionModelComponent(
      &cus_, optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      listener_.GetWeakPtr());
}

TEST_F(PredictionModelComponentInstallerTest,
       DoesNotRegisterComponentWhenTargetsEmpty) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::kPredictionModelComponentDelivery);

  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(0);

  RegisterPredictionModelComponent(&cus_, kTestTarget, listener_.GetWeakPtr());
}

TEST_F(PredictionModelComponentInstallerTest, UninstallNotifiesListener) {
  // First load a model to ensure it is in the listener's registry.
  FakeOptimizationTargetModelObserver observer;
  listener_.AddObserverForOptimizationTargetModel(kTestTarget, std::nullopt,
                                                  nullptr, &observer);

  policy_->ComponentReady(base::Version(kTestVersion), model_dir_.path(),
                          base::DictValue());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 1; }));

  // Verify it was loaded.
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_NE(observer.last_model_info(), nullptr);

  // Now trigger uninstall.
  policy_->OnCustomUninstall();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 2; }));

  // The observer should be notified that the model was removed (null model
  // info).
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_EQ(observer.last_model_info(), nullptr);

  listener_.RemoveObserverForOptimizationTargetModel(kTestTarget, &observer);
}

TEST_F(PredictionModelComponentInstallerTest,
       ComponentReadyNotifiesMultipleObservers) {
  FakeOptimizationTargetModelObserver observer1;
  FakeOptimizationTargetModelObserver observer2;
  listener_.AddObserverForOptimizationTargetModel(kTestTarget, std::nullopt,
                                                  nullptr, &observer1);
  listener_.AddObserverForOptimizationTargetModel(kTestTarget, std::nullopt,
                                                  nullptr, &observer2);

  policy_->ComponentReady(base::Version(kTestVersion), model_dir_.path(),
                          base::DictValue());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return observer1.call_count() == 1 && observer2.call_count() == 1;
  }));

  EXPECT_EQ(observer1.call_count(), 1);
  EXPECT_EQ(observer1.last_target(), kTestTarget);
  ASSERT_TRUE(observer1.last_model_info());
  EXPECT_EQ(observer1.last_model_info()->version, 123);

  EXPECT_EQ(observer2.call_count(), 1);
  EXPECT_EQ(observer2.last_target(), kTestTarget);
  ASSERT_TRUE(observer2.last_model_info());
  EXPECT_EQ(observer2.last_model_info()->version, 123);

  listener_.RemoveObserverForOptimizationTargetModel(kTestTarget, &observer1);
  listener_.RemoveObserverForOptimizationTargetModel(kTestTarget, &observer2);
}

TEST_F(PredictionModelComponentInstallerTest, UninstallBeforeComponentReady) {
  FakeOptimizationTargetModelObserver observer;
  listener_.AddObserverForOptimizationTargetModel(kTestTarget, std::nullopt,
                                                  nullptr, &observer);

  // Trigger uninstall before ComponentReady is ever called.
  policy_->OnCustomUninstall();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 1; }));
  EXPECT_EQ(observer.last_target(), kTestTarget);
  EXPECT_EQ(observer.last_model_info(), nullptr);

  listener_.RemoveObserverForOptimizationTargetModel(kTestTarget, &observer);
}

}  // namespace component_updater
