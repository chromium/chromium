// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_embedder.h"

#include <memory>

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/history_embeddings/passage_embeddings_service_controller.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

constexpr int64_t kEmbeddingsModelVersion = 1l;
constexpr uint32_t kEmbeddingsModelInputWindowSize = 256u;
constexpr size_t kEmbeddingsModelOutputSize = 768ul;

// Returns a model info builder preloaded with valid model info.
optimization_guide::TestModelInfoBuilder GetBuilderWithValidModelInfo() {
  // Get file paths to the test model files.
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII("components")
                      .AppendASCII("test")
                      .AppendASCII("data")
                      .AppendASCII("history_embeddings");

  // The files only exist to appease the mojo run-time check for null arguments,
  // and they are not read by the fake embedder.
  base::FilePath embeddings_path = test_data_dir.AppendASCII("fake_model_file");
  base::FilePath sp_path = test_data_dir.AppendASCII("fake_model_file");

  // Create serialized metadata.
  proto::PassageEmbeddingsModelMetadata model_metadata;
  model_metadata.set_input_window_size(kEmbeddingsModelInputWindowSize);
  model_metadata.set_output_size(kEmbeddingsModelOutputSize);
  std::string model_metadata_string;
  model_metadata.SerializeToString(&model_metadata_string);
  optimization_guide::proto::Any metadata_any;
  metadata_any.set_type_url("PassageEmbeddingsModelMetadata");
  metadata_any.set_value(model_metadata_string);

  // Load a model info builder.
  optimization_guide::TestModelInfoBuilder builder;
  builder.SetModelFilePath(embeddings_path);
  builder.SetAdditionalFiles({sp_path});
  builder.SetVersion(kEmbeddingsModelVersion);
  builder.SetModelMetadata(metadata_any);

  return builder;
}

class FakePassageEmbedder : public passage_embeddings::mojom::PassageEmbedder {
 public:
  explicit FakePassageEmbedder(
      mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbedder>
          receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // mojom::PassageEmbedder:
  void GenerateEmbeddings(const std::vector<std::string>& inputs,
                          passage_embeddings::mojom::PassagePriority priority,
                          GenerateEmbeddingsCallback callback) override {
    std::vector<std::string> passages = inputs;
    std::vector<Embedding> embeddings;
    std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr> results;
    for (const std::string& input : inputs) {
      // Fails the generation on an "error" string to simulate failed model
      // execution.
      if (input == "error") {
        results.clear();
        break;
      }

      results.push_back(
          passage_embeddings::mojom::PassageEmbeddingsResult::New());
      results.back()->embeddings =
          std::vector<float>(kEmbeddingsModelOutputSize, 1.0);
      results.back()->passage = input;
    }

    std::move(callback).Run(std::move(results));
  }

  mojo::Receiver<passage_embeddings::mojom::PassageEmbedder> receiver_;
};

class FakePassageEmbeddingsService
    : public passage_embeddings::mojom::PassageEmbeddingsService {
 public:
  explicit FakePassageEmbeddingsService(
      mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbeddingsService>
          receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // mojom::PassageEmbeddingsService:
  void LoadModels(
      passage_embeddings::mojom::PassageEmbeddingsLoadModelsParamsPtr params,
      mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbedder> model,
      LoadModelsCallback callback) override {
    bool valid = params->input_window_size != 0;
    if (valid) {
      embedder_ = std::make_unique<FakePassageEmbedder>(std::move(model));
    }
    // Use input window size as a signal to fail the request.
    std::move(callback).Run(valid);
  }

  mojo::Receiver<passage_embeddings::mojom::PassageEmbeddingsService> receiver_;
  std::unique_ptr<FakePassageEmbedder> embedder_;
};

class FakePassageEmbeddingsServiceController
    : public PassageEmbeddingsServiceController {
 public:
  FakePassageEmbeddingsServiceController() = default;
  ~FakePassageEmbeddingsServiceController() override = default;

  void LaunchService() override {
    did_launch_service_ = true;
    service_remote_.reset();
    service_ = std::make_unique<FakePassageEmbeddingsService>(
        service_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  std::unique_ptr<FakePassageEmbeddingsService> service_;
  bool did_launch_service_ = false;
};

class TestOptimizationGuideModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER) {
      passage_embedder_target_registered_ = true;
    }

    if (!model_info_) {
      observer->OnModelUpdated(
          optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
          std::nullopt);
    } else {
      observer->OnModelUpdated(
          optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
          *model_info_);
    }
  }

  bool passage_embedder_target_registered() const {
    return passage_embedder_target_registered_;
  }

  // Set the model info to be sent to the observer.
  void SetModelInfo(std::unique_ptr<optimization_guide::ModelInfo> model_info) {
    model_info_ = std::move(model_info);
  }

 private:
  bool passage_embedder_target_registered_ = false;
  std::unique_ptr<optimization_guide::ModelInfo> model_info_;
};

}  // namespace

class MlEmbedderTest : public testing::Test {
 public:
  void SetUp() override {
    model_provider_ = std::make_unique<TestOptimizationGuideModelProvider>();
    service_controller_ =
        std::make_unique<FakePassageEmbeddingsServiceController>();
  }

  void TearDown() override {}

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<TestOptimizationGuideModelProvider> model_provider_;
  std::unique_ptr<FakePassageEmbeddingsServiceController> service_controller_;
};

TEST_F(MlEmbedderTest, RegistersForTarget) {
  auto ml_embedder = std::make_unique<MlEmbedder>(
      model_provider_.get(), /*service_controller=*/nullptr);

  EXPECT_TRUE(model_provider_->passage_embedder_target_registered());
}

TEST_F(MlEmbedderTest, ReceivesValidModelInfo) {
  model_provider_->SetModelInfo(GetBuilderWithValidModelInfo().Build());

  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReady(
      base::BindLambdaForTesting([&](EmbedderMetadata metadata) {
        EXPECT_EQ(metadata.model_version, kEmbeddingsModelVersion);
        EXPECT_EQ(metadata.output_size, kEmbeddingsModelOutputSize);
        on_embedder_ready_invoked = true;
      }));

  EXPECT_TRUE(on_embedder_ready_invoked);
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(kModelInfoMetricName,
                                       EmbeddingsModelInfoStatus::kValid, 1);
}

TEST_F(MlEmbedderTest, ReceivesEmptyModelInfo) {
  model_provider_->SetModelInfo(nullptr);
  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReady(base::BindLambdaForTesting(
      [&](EmbedderMetadata metadata) { on_embedder_ready_invoked = true; }));

  EXPECT_FALSE(on_embedder_ready_invoked);
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(kModelInfoMetricName,
                                       EmbeddingsModelInfoStatus::kEmpty, 1);
}

TEST_F(MlEmbedderTest, ReceivesModelInfoWithInvalidModelMetadata) {
  // Make some invalid metadata.
  optimization_guide::proto::Any metadata_any;
  metadata_any.set_type_url("not a valid type url");
  metadata_any.set_value("not a valid serialized metadata");

  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(metadata_any);

  model_provider_->SetModelInfo(builder.Build());
  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReady(base::BindLambdaForTesting(
      [&](EmbedderMetadata metadata) { on_embedder_ready_invoked = true; }));

  EXPECT_FALSE(on_embedder_ready_invoked);
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kInvalidMetadata, 1);
}

TEST_F(MlEmbedderTest, ReceivesModelInfoWithoutModelMetadata) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(std::nullopt);

  model_provider_->SetModelInfo(builder.Build());
  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReady(base::BindLambdaForTesting(
      [&](EmbedderMetadata metadata) { on_embedder_ready_invoked = true; }));

  EXPECT_FALSE(on_embedder_ready_invoked);
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kNoMetadata, 1);
}

TEST_F(MlEmbedderTest, ReceivesModelInfoWithoutAdditionalFiles) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetAdditionalFiles(
      {test_data_dir.AppendASCII("foo"), test_data_dir.AppendASCII("bar")});

  model_provider_->SetModelInfo(builder.Build());
  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReady(base::BindLambdaForTesting(
      [&](EmbedderMetadata metadata) { on_embedder_ready_invoked = true; }));

  EXPECT_FALSE(on_embedder_ready_invoked);
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kInvalidAdditionalFiles,
      1);
}

TEST_F(MlEmbedderTest, GeneratesEmbeddings) {
  model_provider_->SetModelInfo(GetBuilderWithValidModelInfo().Build());

  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(kModelInfoMetricName,
                                       EmbeddingsModelInfoStatus::kValid, 1);

  base::test::TestFuture<std::vector<std::string>, std::vector<Embedding>,
                         ComputeEmbeddingsStatus>
      future;
  ml_embedder->ComputePassagesEmbeddings(PassageKind::PAGE_VISIT_PASSAGE,
                                         {"foo", "bar"}, future.GetCallback());
  auto [passages, embeddings, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::SUCCESS);
  EXPECT_EQ(passages[0], "foo");
  EXPECT_EQ(passages[1], "bar");
  EXPECT_EQ(embeddings[0].Dimensions(), kEmbeddingsModelOutputSize);
  EXPECT_EQ(embeddings[1].Dimensions(), kEmbeddingsModelOutputSize);
}

TEST_F(MlEmbedderTest, ReturnsModelUnavailableErrorIfModelInfoNotValid) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(std::nullopt);

  model_provider_->SetModelInfo(builder.Build());
  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());

  base::test::TestFuture<std::vector<std::string>, std::vector<Embedding>,
                         ComputeEmbeddingsStatus>
      future;
  ml_embedder->ComputePassagesEmbeddings(PassageKind::PAGE_VISIT_PASSAGE,
                                         {"foo", "bar"}, future.GetCallback());
  auto [passages, embeddings, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::MODEL_UNAVAILABLE);
  EXPECT_TRUE(passages.empty());
  EXPECT_TRUE(embeddings.empty());
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kNoMetadata, 1);
}

TEST_F(MlEmbedderTest, ReturnsExecutionFailure) {
  model_provider_->SetModelInfo(GetBuilderWithValidModelInfo().Build());
  auto ml_embedder = std::make_unique<MlEmbedder>(model_provider_.get(),
                                                  service_controller_.get());

  base::test::TestFuture<std::vector<std::string>, std::vector<Embedding>,
                         ComputeEmbeddingsStatus>
      future;
  ml_embedder->ComputePassagesEmbeddings(PassageKind::PAGE_VISIT_PASSAGE,
                                         {"error"}, future.GetCallback());
  auto [passages, embeddings, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::EXECUTION_FAILURE);
  EXPECT_TRUE(passages.empty());
  EXPECT_TRUE(embeddings.empty());
}

}  // namespace history_embeddings
