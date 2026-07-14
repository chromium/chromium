// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

namespace {

using testing::ElementsAre;

using GetEmbeddingsTestFuture = base::test::TestFuture<std::vector<std::string>,
                                                       std::vector<Embedding>,
                                                       uint64_t,
                                                       ComputeEmbeddingsStatus>;

class FakePassageEmbedder : public mojom::PassageEmbedder {
 public:
  explicit FakePassageEmbedder(
      mojo::PendingReceiver<mojom::PassageEmbedder> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // mojom::PassageEmbedder:
  void GenerateEmbeddings(const std::vector<std::string>& inputs,
                          mojom::PassagePriority priority,
                          GenerateEmbeddingsCallback callback) override {
    std::vector<mojom::PassageEmbeddingsResultPtr> results;
    for (const std::string& input : inputs) {
      // Fail Embeddings generation for the entire batch when encountering
      // "error" string to simulate failed model execution.
      if (input == "error") {
        return std::move(callback).Run({});
      }

      // Otherwise convert the string-encoded floating point inputs to provide a
      // signal that the PassageEmbedder was executed.
      double result = 0.0;
      EXPECT_TRUE(base::StringToDouble(input, &result));
      results.push_back(mojom::PassageEmbeddingsResult::New(
          std::vector<float>{static_cast<float>(result)}));
    }
    std::move(callback).Run(std::move(results));
  }

 public:
  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

 private:
  mojo::Receiver<mojom::PassageEmbedder> receiver_;
};

class FakePassageEmbeddingsService : public mojom::PassageEmbeddingsService {
 public:
  explicit FakePassageEmbeddingsService(
      mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // mojom::PassageEmbeddingsService:
  void LoadModels(mojom::PassageEmbeddingsLoadModelsParamsPtr model_params,
                  mojom::PassageEmbedderParamsPtr embedder_params,
                  mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
                  LoadModelsCallback callback) override {
    bool valid = model_params->input_window_size != 0;
    if (valid && !embedder_) {
      embedder_ = std::make_unique<FakePassageEmbedder>(std::move(receiver));
      embedder_->set_disconnect_handler(
          base::BindOnce(&FakePassageEmbeddingsService::OnEmbedderDisconnected,
                         base::Unretained(this)));
    }
    // Use input window size as a signal to fail the request.
    std::move(callback).Run(valid);
  }

  void OnEmbedderDisconnected() { embedder_.reset(); }

  mojo::Receiver<mojom::PassageEmbeddingsService> receiver_;
  std::unique_ptr<FakePassageEmbedder> embedder_;
};

class FakePassageEmbeddingsServiceLauncher
    : public PassageEmbeddingsServiceLauncher {
 public:
  FakePassageEmbeddingsServiceLauncher() = default;
  ~FakePassageEmbeddingsServiceLauncher() override = default;

  void LaunchService(mojo::PendingReceiver<mojom::PassageEmbeddingsService>
                         receiver) override {
    service_ =
        std::make_unique<FakePassageEmbeddingsService>(std::move(receiver));
  }
  void OnServiceDisconnected(bool is_idle) override {
    if (is_idle) {
      idle_disconnects_++;
    } else {
      crash_disconnects_++;
    }
  }
  bool AllowedToLaunch() const override { return true; }
  int idle_disconnects() const { return idle_disconnects_; }
  int crash_disconnects() const { return crash_disconnects_; }
  void CrashService() { service_.reset(); }

 private:
  int idle_disconnects_ = 0;
  int crash_disconnects_ = 0;
  std::unique_ptr<FakePassageEmbeddingsService> service_;
};

class MetadataObserver : public EmbedderMetadataObserver {
 public:
  explicit MetadataObserver(
      EmbedderMetadataProvider* embedder_metadata_provider,
      base::test::TestFuture<EmbedderMetadata>* embedder_metadata_future)
      : embedder_metadata_future_(embedder_metadata_future) {
    embedder_metadata_observation_.Observe(embedder_metadata_provider);
  }

  // EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(EmbedderMetadata metadata) override {
    embedder_metadata_future_->SetValue(metadata);
  }

 private:
  base::ScopedObservation<EmbedderMetadataProvider, EmbedderMetadataObserver>
      embedder_metadata_observation_{this};
  raw_ptr<base::test::TestFuture<EmbedderMetadata>> embedder_metadata_future_;
};

}  // namespace

class PassageEmbeddingsServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    service_controller_ =
        std::make_unique<PassageEmbeddingsServiceController>(launcher_);
    metadata_observer_.emplace(service_controller_.get(),
                               &embedder_metadata_future_);

    EXPECT_FALSE(embedder_metadata_future()->IsReady());
    EXPECT_FALSE(service_controller_->IsModelAvailable());
  }

  void TearDown() override {
    metadata_observer_.reset();
    service_controller_.reset();
  }

 protected:
  base::test::TestFuture<EmbedderMetadata>* embedder_metadata_future() {
    return &embedder_metadata_future_;
  }

  PassageEmbeddingsServiceController* service_controller() {
    return service_controller_.get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  FakePassageEmbeddingsServiceLauncher launcher_;
  std::unique_ptr<PassageEmbeddingsServiceController> service_controller_;
  base::test::TestFuture<EmbedderMetadata> embedder_metadata_future_;
  std::optional<MetadataObserver> metadata_observer_;
};

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReceivesValidModelInfo DISABLED_ReceivesValidModelInfo
#else
#define MAYBE_ReceivesValidModelInfo ReceivesValidModelInfo
#endif
TEST_F(PassageEmbeddingsServiceControllerTest, MAYBE_ReceivesValidModelInfo) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      GetBuilderWithValidModelInfo().Build()));
  EXPECT_TRUE(service_controller_->IsModelAvailable());
  auto metadata = embedder_metadata_future()->Take();
  EXPECT_TRUE(metadata.IsValid());
  EXPECT_EQ(metadata.model_version, kEmbeddingsModelVersion);
  EXPECT_EQ(metadata.output_size, 3ul);

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(kModelInfoMetricName,
                                       EmbeddingsModelInfoStatus::kValid, 1);
}

TEST_F(PassageEmbeddingsServiceControllerTest, ReceivesEmptyModelInfo) {
  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo({}));
  EXPECT_FALSE(embedder_metadata_future()->IsReady());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(kModelInfoMetricName,
                                       EmbeddingsModelInfoStatus::kEmpty, 1);
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReceivesModelInfoWithInvalidModelMetadata \
  DISABLED_ReceivesModelInfoWithInvalidModelMetadata
#else
#define MAYBE_ReceivesModelInfoWithInvalidModelMetadata \
  ReceivesModelInfoWithInvalidModelMetadata
#endif
TEST_F(PassageEmbeddingsServiceControllerTest,
       MAYBE_ReceivesModelInfoWithInvalidModelMetadata) {
  optimization_guide::proto::Any metadata_any;
  metadata_any.set_type_url("not a valid type url");
  metadata_any.set_value("not a valid serialized metadata");
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(metadata_any);

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(builder.Build()));
  EXPECT_FALSE(embedder_metadata_future()->IsReady());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kInvalidMetadata, 1);
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReceivesModelInfoWithoutModelMetadata \
  DISABLED_ReceivesModelInfoWithoutModelMetadata
#else
#define MAYBE_ReceivesModelInfoWithoutModelMetadata \
  ReceivesModelInfoWithoutModelMetadata
#endif
TEST_F(PassageEmbeddingsServiceControllerTest,
       MAYBE_ReceivesModelInfoWithoutModelMetadata) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(std::nullopt);

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(builder.Build()));
  EXPECT_FALSE(embedder_metadata_future()->IsReady());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kNoMetadata, 1);
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReceivesModelInfoWithoutAdditionalFiles \
  DISABLED_ReceivesModelInfoWithoutAdditionalFiles
#else
#define MAYBE_ReceivesModelInfoWithoutAdditionalFiles \
  ReceivesModelInfoWithoutAdditionalFiles
#endif
TEST_F(PassageEmbeddingsServiceControllerTest,
       MAYBE_ReceivesModelInfoWithoutAdditionalFiles) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetAdditionalFiles(
      {test_data_dir.AppendASCII("foo"), test_data_dir.AppendASCII("bar")});

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(builder.Build()));
  EXPECT_FALSE(embedder_metadata_future()->IsReady());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kInvalidAdditionalFiles,
      1);
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetEmbeddingsEmpty DISABLED_GetEmbeddingsEmpty
#else
#define MAYBE_GetEmbeddingsEmpty GetEmbeddingsEmpty
#endif
TEST_F(PassageEmbeddingsServiceControllerTest, MAYBE_GetEmbeddingsEmpty) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      GetBuilderWithValidModelInfo().Build()));

  GetEmbeddingsTestFuture future;
  auto job = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {}, future.GetCallback());

  auto [passages, results, job_id, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_TRUE(results.empty());
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetEmbeddingsNonEmpty DISABLED_GetEmbeddingsNonEmpty
#else
#define MAYBE_GetEmbeddingsNonEmpty GetEmbeddingsNonEmpty
#endif
TEST_F(PassageEmbeddingsServiceControllerTest, MAYBE_GetEmbeddingsNonEmpty) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      GetBuilderWithValidModelInfo().Build()));

  GetEmbeddingsTestFuture future;
  auto job = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"1.0", "2.0"}, future.GetCallback());
  auto [passages, results, job_id, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_THAT(results[0].GetData(), ElementsAre(1.0f));
  EXPECT_THAT(results[1].GetData(), ElementsAre(1.0f));
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReturnsModelUnavailableErrorIfModelInfoNotValid \
  DISABLED_ReturnsModelUnavailableErrorIfModelInfoNotValid
#else
#define MAYBE_ReturnsModelUnavailableErrorIfModelInfoNotValid \
  ReturnsModelUnavailableErrorIfModelInfoNotValid
#endif
TEST_F(PassageEmbeddingsServiceControllerTest,
       MAYBE_ReturnsModelUnavailableErrorIfModelInfoNotValid) {
  optimization_guide::TestModelInfoBuilder valid_builder =
      GetBuilderWithValidModelInfo();
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(valid_builder.Build()));

  optimization_guide::TestModelInfoBuilder invalid_builder =
      GetBuilderWithValidModelInfo();
  invalid_builder.SetVersion(12345);
  invalid_builder.SetModelMetadata(std::nullopt);

  EXPECT_FALSE(
      service_controller_->MaybeUpdateModelInfo(invalid_builder.Build()));

  GetEmbeddingsTestFuture future;
  auto job = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"1.0"}, future.GetCallback());
  auto [passages, results, job_id, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kModelUnavailable);
  EXPECT_EQ(results.size(), 0u);
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReturnsExecutionFailure DISABLED_ReturnsExecutionFailure
#else
#define MAYBE_ReturnsExecutionFailure ReturnsExecutionFailure
#endif
TEST_F(PassageEmbeddingsServiceControllerTest, MAYBE_ReturnsExecutionFailure) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      GetBuilderWithValidModelInfo().Build()));

  GetEmbeddingsTestFuture future;
  auto job = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"error"}, future.GetCallback());
  auto [passages, results, job_id, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_EQ(results.size(), 0u);
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_EmbedderRunningStatus DISABLED_EmbedderRunningStatus
#else
#define MAYBE_EmbedderRunningStatus EmbedderRunningStatus
#endif
TEST_F(PassageEmbeddingsServiceControllerTest, MAYBE_EmbedderRunningStatus) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      GetBuilderWithValidModelInfo().Build()));

  {
    GetEmbeddingsTestFuture future1;
    auto job1 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future1.GetCallback());
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2;
    auto job2 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future2.GetCallback());
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future1.Get<3>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future2.Get<3>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());
  }
  {
    GetEmbeddingsTestFuture future1;
    auto job1 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future1.GetCallback());
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2;
    auto job2 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future2.GetCallback());
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    int initial_crashes = launcher_.crash_disconnects();
    // Reset embedder remote by crashing the service.
    launcher_.CrashService();

    // Wait for the mojo disconnect handler to run
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return launcher_.crash_disconnects() > initial_crashes; }));

    EXPECT_EQ(future1.Get<3>(), ComputeEmbeddingsStatus::kExecutionFailure);
    EXPECT_EQ(future2.Get<3>(), ComputeEmbeddingsStatus::kSuccess);
  }
  {
    // Calling `ComputePassagesEmbeddings()` again launches the service.
    GetEmbeddingsTestFuture future1;
    auto job1 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future1.GetCallback());
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2;
    auto job2 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future2.GetCallback());
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future1.Get<3>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future2.Get<3>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());
  }
  {
    GetEmbeddingsTestFuture future1;
    auto job1 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future1.GetCallback());
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2;
    auto job2 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"1.0"}, future2.GetCallback());
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    int initial_crashes = launcher_.crash_disconnects();
    launcher_.CrashService();

    // Wait for the mojo disconnect handler to run
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return launcher_.crash_disconnects() > initial_crashes; }));

    EXPECT_EQ(future1.Get<3>(), ComputeEmbeddingsStatus::kExecutionFailure);
    EXPECT_EQ(future2.Get<3>(), ComputeEmbeddingsStatus::kSuccess);
  }
}

// TODO(crbug.com/524801761): Re-enable this test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_RecordsGemmaHistograms DISABLED_RecordsGemmaHistograms
#else
#define MAYBE_RecordsGemmaHistograms RecordsGemmaHistograms
#endif
TEST_F(PassageEmbeddingsServiceControllerTest, MAYBE_RecordsGemmaHistograms) {
  auto gemma_service_controller =
      std::make_unique<PassageEmbeddingsServiceController>(
          launcher_, /*execute_for_gemma=*/true);
  EXPECT_TRUE(gemma_service_controller->MaybeUpdateModelInfo(
      GetBuilderWithValidModelInfo().Build()));

  GetEmbeddingsTestFuture future;
  auto job = gemma_service_controller->GetEmbedder()->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"1.0"}, future.GetCallback());
  auto [passages, results, job_id, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_THAT(results[0].GetData(), ElementsAre(1.0f));

  histogram_tester_.ExpectTotalCount("AI.SemanticEmbedder.TaskDuration", 1);
  histogram_tester_.ExpectTotalCount("AI.SemanticEmbedder.LaunchDuration", 1);
  histogram_tester_.ExpectTotalCount("History.Embeddings.TaskDuration", 0);
}

TEST_F(PassageEmbeddingsServiceControllerTest, DistinguishesIdleFromCrashes) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(builder.Build()));

  // Run the embedder to launch the service.
  GetEmbeddingsTestFuture future;
  auto job = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"1.0"}, future.GetCallback());
  auto [passages, results, job_id, status] = future.Take();

  EXPECT_EQ(launcher_.idle_disconnects(), 0);
  EXPECT_EQ(launcher_.crash_disconnects(), 0);

  // Simulate an expected idle timeout disconnect
  task_environment_.FastForwardBy(base::Seconds(125));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return launcher_.idle_disconnects() == 1; }));
  EXPECT_EQ(launcher_.crash_disconnects(), 0);

  // Simulate an unexpected crash disconnect. First, launch the service again.
  GetEmbeddingsTestFuture future2;
  auto job2 = service_controller_->GetEmbedder()->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"1.0"}, future2.GetCallback());
  auto [passages2, results2, job_id2, status2] = future2.Take();

  launcher_.CrashService();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return launcher_.crash_disconnects() == 1; }));
  EXPECT_EQ(launcher_.idle_disconnects(), 1);
}

}  // namespace passage_embeddings
