// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prediction_model_component_update_listener.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/types/optional_ref.h"
#include "base/version.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/prediction_model_component_configs.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class FakeOptimizationTargetModelObserver
    : public OptimizationTargetModelObserver {
 public:
  FakeOptimizationTargetModelObserver() = default;
  ~FakeOptimizationTargetModelObserver() override = default;

  void OnModelUpdated(proto::OptimizationTarget target,
                      base::optional_ref<const ModelInfo> model_info) override {
    last_target_ = target;
    if (model_info.has_value()) {
      last_model_info_ = std::make_unique<ModelInfo>(*model_info);
    } else {
      last_model_info_.reset();
    }
    call_count_++;
  }

  std::optional<proto::OptimizationTarget> last_target() const {
    return last_target_;
  }
  const ModelInfo* last_model_info() const { return last_model_info_.get(); }
  int call_count() const { return call_count_; }

  void Reset() {
    last_target_ = std::nullopt;
    last_model_info_.reset();
    call_count_ = 0;
  }

 private:
  std::optional<proto::OptimizationTarget> last_target_;
  std::unique_ptr<ModelInfo> last_model_info_;
  int call_count_ = 0;
};

class SelfRemovingOptimizationTargetModelObserver
    : public FakeOptimizationTargetModelObserver {
 public:
  explicit SelfRemovingOptimizationTargetModelObserver(
      PredictionModelComponentUpdateListener* listener)
      : listener_(listener) {}
  ~SelfRemovingOptimizationTargetModelObserver() override = default;

  void OnModelUpdated(proto::OptimizationTarget target,
                      base::optional_ref<const ModelInfo> model_info) override {
    FakeOptimizationTargetModelObserver::OnModelUpdated(target, model_info);
    listener_->RemoveObserverForOptimizationTargetModel(target, this);
  }

 private:
  raw_ptr<PredictionModelComponentUpdateListener> listener_;
};

}  // namespace

class PredictionModelComponentUpdateListenerTest : public testing::Test {
 public:
  PredictionModelComponentUpdateListenerTest()
      : fallback_provider_(OptimizationGuideLogger::GetInstance()) {}
  ~PredictionModelComponentUpdateListenerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(kPredictionModelComponentDelivery);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    listener_ = std::make_unique<PredictionModelComponentUpdateListener>(
        fallback_provider_, base::DoNothing());
  }

 protected:
  base::FilePath CreateModelDirectory(proto::OptimizationTarget target,
                                      int64_t version) {
    base::FilePath model_dir;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("model"), &model_dir));

    base::WriteFile(model_dir.Append(GetBaseFileNameForModels()),
                    "dummy model");

    proto::ModelInfo model_info;
    model_info.set_optimization_target(target);
    model_info.set_version(version);
    std::string serialized;
    EXPECT_TRUE(model_info.SerializeToString(&serialized));
    base::WriteFile(model_dir.Append(GetBaseFileNameForModelInfo()),
                    serialized);

    return model_dir;
  }

  base::FilePath CreateCorruptModelInfoDirectory() {
    base::FilePath model_dir;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("model"), &model_dir));

    base::WriteFile(model_dir.Append(GetBaseFileNameForModels()),
                    "dummy model");

    base::WriteFile(model_dir.Append(GetBaseFileNameForModelInfo()),
                    "corrupt proto data");

    return model_dir;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
  ModelProviderRegistry fallback_provider_;
  std::unique_ptr<PredictionModelComponentUpdateListener> listener_;
};

TEST_F(PredictionModelComponentUpdateListenerTest, AddObserverAndNotify) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 0);

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  listener_->MaybeUpdateModel(target, version, install_dir);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 1; }));

  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(observer.last_target(), target);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 123);
  EXPECT_EQ(observer.last_model_info()->model_file_path,
            install_dir.Append(GetBaseFileNameForModels()));

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest, AddObserverAfterReady) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  listener_->MaybeUpdateModel(target, version, install_dir);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 123;
  }));

  // Notification should happen synchronously during AddObserver because the
  // model is already loaded in the registry.
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);

  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(observer.last_target(), target);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 123);

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest, UpdateWithOlderVersion) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);

  base::Version version2("2.0.0");
  base::FilePath install_dir2 = CreateModelDirectory(target, /*version=*/200);
  listener_->MaybeUpdateModel(target, version2, install_dir2);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 1; }));
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(observer.last_model_info()->version, 200);
  observer.Reset();

  base::Version version1("1.0.0");
  base::FilePath install_dir1 = CreateModelDirectory(target, /*version=*/100);
  listener_->MaybeUpdateModel(target, version1, install_dir1);
  EXPECT_EQ(observer.call_count(), 0);

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest, GetModelWithoutObserver) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  EXPECT_EQ(listener_->GetModelForTesting(target), nullptr);

  listener_->MaybeUpdateModel(target, version, install_dir);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 123;
  }));

  const ModelInfo* model_info = listener_->GetModelForTesting(target);
  ASSERT_NE(model_info, nullptr);
  EXPECT_EQ(model_info->version, 123);
}

TEST_F(PredictionModelComponentUpdateListenerTest,
       SelfRemovalDuringNotification) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  SelfRemovingOptimizationTargetModelObserver observer(listener_.get());

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 0);

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  listener_->MaybeUpdateModel(target, version, install_dir);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 1; }));

  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(observer.last_target(), target);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 123);

  // Triggering it again should not notify because observer is removed.
  base::Version version2("2.0.0.0");
  base::FilePath install_dir2 = CreateModelDirectory(target, /*version=*/200);
  listener_->MaybeUpdateModel(target, version2, install_dir2);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 200;
  }));
  EXPECT_EQ(observer.call_count(), 1);
}

TEST_F(PredictionModelComponentUpdateListenerTest,
       DoubleNotificationMitigation) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  // Start loading version 1.
  base::Version version1("1.0.0");
  base::FilePath install_dir1 = CreateModelDirectory(target, /*version=*/100);
  listener_->MaybeUpdateModel(target, version1, install_dir1);

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 0);

  // Immediately update model with version 2 before version 1 finishes loading.
  base::Version version2("2.0.0");
  base::FilePath install_dir2 = CreateModelDirectory(target, /*version=*/200);
  listener_->MaybeUpdateModel(target, version2, install_dir2);

  // Run all pending tasks.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 200;
  }));

  // Observer should only be notified for version 2 (latest).
  // Depending on thread timing, it might be notified for version 1 first, then
  // version 2, or only version 2 if version 1 load was discarded.
  // Our implementation discards version 1 load in OnModelLoaded because
  // component_info_map_ version was updated to version 2.
  // So it should only be notified for version 2.
  EXPECT_EQ(observer.call_count(), 1);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 200);

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest,
       RemoveObserverBeforeAsyncNotification) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  listener_->MaybeUpdateModel(target, version, install_dir);

  // Add observer.
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 0);

  // Immediately remove observer before running loading tasks.
  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 123;
  }));

  // Observer should not have been called.
  EXPECT_EQ(observer.call_count(), 0);
}

TEST_F(PredictionModelComponentUpdateListenerTest, OnModelUninstalled) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  listener_->MaybeUpdateModel(target, version, install_dir);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 123;
  }));

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 1);

  listener_->OnModelUninstalled(target);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 2; }));

  // With model uninstalled, observer should be notified with null model_info,
  // and call count should increase.
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_EQ(observer.last_model_info(), nullptr);
  EXPECT_EQ(listener_->GetModelForTesting(target), nullptr);

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest,
       OnModelUninstalledCancelsPendingNotification) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  listener_->MaybeUpdateModel(target, version, install_dir);
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 0);

  // Immediately uninstall before the loading task runs.
  listener_->OnModelUninstalled(target);

  // Wait until all background tasks are done.
  {
    base::RunLoop run_loop;
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Observer should be notified of uninstall (null model), but not the ready
  // model.
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(observer.last_model_info(), nullptr);

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest,
       SelfRemovalDuringSyncNotification) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  // Set the model as ready.
  listener_->MaybeUpdateModel(target, version, install_dir);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 123;
  }));

  SelfRemovingOptimizationTargetModelObserver observer(listener_.get());
  // Add observer, synchronously notifying the observer because model is ready.
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);

  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(observer.last_target(), target);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 123);

  // Triggering it again should not notify because observer is removed.
  base::Version version2("2.0.0.0");
  base::FilePath install_dir2 = CreateModelDirectory(target, /*version=*/200);
  listener_->MaybeUpdateModel(target, version2, install_dir2);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 200;
  }));
  EXPECT_EQ(observer.call_count(), 1);
}

TEST_F(PredictionModelComponentUpdateListenerTest, LoadFailure) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  // First load a good model.
  base::Version version1("1.0.0");
  base::FilePath install_dir1 = CreateModelDirectory(target, /*version=*/100);
  listener_->MaybeUpdateModel(target, version1, install_dir1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 100;
  }));

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_NE(observer.last_model_info(), nullptr);
  EXPECT_EQ(observer.last_model_info()->version, 100);

  // Now trigger update with non-existent directory.
  base::Version version2("2.0.0");
  base::FilePath install_dir2(FILE_PATH_LITERAL("/invalid_path"));

  listener_->MaybeUpdateModel(target, version2, install_dir2);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 2; }));

  // Observer should be notified of model removal.
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_EQ(observer.last_model_info(), nullptr);
}

TEST_F(PredictionModelComponentUpdateListenerTest, EmptyInstallDirIgnored) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  // First load a good model.
  base::Version version1("1.0.0");
  base::FilePath install_dir1 = CreateModelDirectory(target, /*version=*/100);
  listener_->MaybeUpdateModel(target, version1, install_dir1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 100;
  }));

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 1);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 100);

  // Now trigger update with an empty directory.
  base::Version version2("2.0.0");
  base::FilePath install_dir2;

  listener_->MaybeUpdateModel(target, version2, install_dir2);

  // Flush the thread pool and main thread tasks. If a task was erroneously
  // posted and executed, it would have notified the observer.
  {
    base::RunLoop run_loop;
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Observer should NOT be notified, and the old model should still be active.
  EXPECT_EQ(observer.call_count(), 1);
  ASSERT_TRUE(listener_->GetModelForTesting(target));
  EXPECT_EQ(listener_->GetModelForTesting(target)->version, 100);

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest, CorruptModelInfo) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  // First load a good model.
  base::Version version1("1.0.0");
  base::FilePath install_dir1 = CreateModelDirectory(target, /*version=*/100);
  listener_->MaybeUpdateModel(target, version1, install_dir1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 100;
  }));

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_EQ(observer.call_count(), 1);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 100);

  // Now trigger update with corrupt model info.
  base::Version version2("2.0.0");
  base::FilePath install_dir2 = CreateCorruptModelInfoDirectory();

  listener_->MaybeUpdateModel(target, version2, install_dir2);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 2; }));

  // Observer should be notified of model removal.
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_EQ(observer.last_model_info(), nullptr);
}

TEST_F(PredictionModelComponentUpdateListenerTest, UseObserverTaskRunner) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;
  auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();

  // Add observer with custom task runner.
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   test_task_runner, &observer);
  EXPECT_EQ(observer.call_count(), 0);

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  listener_->MaybeUpdateModel(target, version, install_dir);

  // The load task should be posted to test_task_runner, but not run yet.
  EXPECT_FALSE(observer.last_model_info());
  EXPECT_TRUE(test_task_runner->HasPendingTask());

  // Run the pending task on test_task_runner.
  test_task_runner->RunPendingTasks();

  // Now run the reply task on main thread (task_environment).
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.call_count() == 1; }));

  EXPECT_EQ(observer.call_count(), 1);
  ASSERT_TRUE(observer.last_model_info());
  EXPECT_EQ(observer.last_model_info()->version, 123);

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
}

TEST_F(PredictionModelComponentUpdateListenerTest,
       TaskRunnerClearedOnObserverRemoval) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;
  auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();

  // Add observer with custom task runner.
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   test_task_runner, &observer);

  // Remove observer. Since it was the only observer, the task runner should be
  // cleared.
  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);

  base::Version version("1.2.3.4");
  base::FilePath install_dir = CreateModelDirectory(target, /*version=*/123);

  // Trigger update. It should use the default task runner, not the custom one.
  listener_->MaybeUpdateModel(target, version, install_dir);

  // The custom task runner should NOT have any pending tasks.
  EXPECT_FALSE(test_task_runner->HasPendingTask());

  // The model should still load eventually via the default task runner.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return listener_->GetModelForTesting(target) &&
           listener_->GetModelForTesting(target)->version == 123;
  }));
}

TEST_F(PredictionModelComponentUpdateListenerTest, RegisterCallbackCalled) {
  proto::OptimizationTarget target =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  FakeOptimizationTargetModelObserver observer;

  using MockRegisterCallback = base::MockCallback<
      PredictionModelComponentUpdateListener::RegisterComponentCallback>;
  testing::StrictMock<MockRegisterCallback> mock_callback;

  // Re-create listener with a custom callback for this test.
  listener_ = std::make_unique<PredictionModelComponentUpdateListener>(
      fallback_provider_, mock_callback.Get());

  // Adding observer for migrated target should trigger the callback.
  EXPECT_CALL(mock_callback, Run(target, testing::_));
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Adding observer again for the same target should NOT trigger the callback.
  FakeOptimizationTargetModelObserver observer2;
  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer2);

  // Adding observer for a non-migrated target should NOT trigger the callback
  // and should get rerouted to fallback provider.
  proto::OptimizationTarget target2 = proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2;
  FakeOptimizationTargetModelObserver observer3;
  listener_->AddObserverForOptimizationTargetModel(target2, std::nullopt,
                                                   nullptr, &observer3);

  // Verify it was rerouted.
  EXPECT_TRUE(fallback_provider_.IsRegistered(target2));

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
  listener_->RemoveObserverForOptimizationTargetModel(target, &observer2);
  listener_->RemoveObserverForOptimizationTargetModel(target2, &observer3);
  listener_.reset();
}

TEST_F(PredictionModelComponentUpdateListenerTest, RerouteNonMigratedTarget) {
  proto::OptimizationTarget target = proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2;
  FakeOptimizationTargetModelObserver observer;

  listener_->AddObserverForOptimizationTargetModel(target, std::nullopt,
                                                   nullptr, &observer);
  EXPECT_TRUE(fallback_provider_.IsRegistered(target));

  listener_->RemoveObserverForOptimizationTargetModel(target, &observer);
  EXPECT_FALSE(fallback_provider_.IsRegistered(target));
}

TEST_F(PredictionModelComponentUpdateListenerTest,
       MultipleTargetsIndependentObservers) {
  proto::OptimizationTarget target1 =
      proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS;
  proto::OptimizationTarget target2 =
      proto::OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS;

  FakeOptimizationTargetModelObserver observer1;
  FakeOptimizationTargetModelObserver observer2;

  auto test_task_runner1 = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto test_task_runner2 = base::MakeRefCounted<base::TestSimpleTaskRunner>();

  listener_->AddObserverForOptimizationTargetModel(
      target1, std::nullopt, test_task_runner1, &observer1);
  listener_->AddObserverForOptimizationTargetModel(
      target2, std::nullopt, test_task_runner2, &observer2);

  EXPECT_EQ(observer1.call_count(), 0);
  EXPECT_EQ(observer2.call_count(), 0);

  base::Version version1("1.0.0");
  base::FilePath install_dir1 = CreateModelDirectory(target1, /*version=*/100);
  base::Version version2("2.0.0");
  base::FilePath install_dir2 = CreateModelDirectory(target2, /*version=*/200);

  listener_->MaybeUpdateModel(target1, version1, install_dir1);
  listener_->MaybeUpdateModel(target2, version2, install_dir2);

  // Each target's load task should be posted to its own task runner.
  EXPECT_TRUE(test_task_runner1->HasPendingTask());
  EXPECT_TRUE(test_task_runner2->HasPendingTask());

  // Run pending tasks on both task runners.
  test_task_runner1->RunPendingTasks();
  test_task_runner2->RunPendingTasks();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return observer1.call_count() == 1 && observer2.call_count() == 1;
  }));
  EXPECT_EQ(observer1.last_target(), target1);
  ASSERT_TRUE(observer1.last_model_info());
  EXPECT_EQ(observer1.last_model_info()->GetVersion(), 100);
  EXPECT_EQ(observer2.last_target(), target2);
  ASSERT_TRUE(observer2.last_model_info());
  EXPECT_EQ(observer2.last_model_info()->GetVersion(), 200);

  // Verify GetModelForTesting returns the isolated models.
  ASSERT_TRUE(listener_->GetModelForTesting(target1));
  EXPECT_EQ(listener_->GetModelForTesting(target1)->GetVersion(), 100);
  ASSERT_TRUE(listener_->GetModelForTesting(target2));
  EXPECT_EQ(listener_->GetModelForTesting(target2)->GetVersion(), 200);

  // Uninstalling target1 should only affect target1.
  listener_->OnModelUninstalled(target1);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer1.call_count() == 2; }));
  EXPECT_EQ(observer1.call_count(), 2);
  EXPECT_EQ(observer1.last_model_info(), nullptr);
  EXPECT_EQ(listener_->GetModelForTesting(target1), nullptr);
  EXPECT_EQ(observer2.call_count(), 1);
  ASSERT_TRUE(listener_->GetModelForTesting(target2));
  EXPECT_EQ(listener_->GetModelForTesting(target2)->GetVersion(), 200);

  // Clean up observers.
  listener_->RemoveObserverForOptimizationTargetModel(target1, &observer1);
  listener_->RemoveObserverForOptimizationTargetModel(target2, &observer2);
}

}  // namespace optimization_guide
