// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/ml_embedder.h"

#include <memory>

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/passage_embeddings/embedder.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

namespace {

constexpr int64_t kEmbeddingsModelVersion = 1l;
constexpr uint32_t kEmbeddingsModelInputWindowSize = 256u;
constexpr size_t kEmbeddingsModelOutputSize = 768ul;

using ComputePassagesEmbeddingsFuture =
    base::test::TestFuture<std::vector<std::string>,
                           std::vector<Embedding>,
                           Embedder::TaskId,
                           ComputeEmbeddingsStatus>;

// Returns a model info builder preloaded with valid model info.
optimization_guide::TestModelInfoBuilder GetBuilderWithValidModelInfo() {
  // Get file paths to the test model files.
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII("components")
                      .AppendASCII("test")
                      .AppendASCII("data")
                      .AppendASCII("passage_embeddings");

  // The files only exist to appease the mojo run-time check for null arguments,
  // and they are not read by the fake embedder.
  base::FilePath embeddings_path = test_data_dir.AppendASCII("fake_model_file");
  base::FilePath sp_path = test_data_dir.AppendASCII("fake_model_file");

  // Create serialized metadata.
  optimization_guide::proto::PassageEmbeddingsModelMetadata model_metadata;
  model_metadata.set_input_window_size(kEmbeddingsModelInputWindowSize);
  model_metadata.set_output_size(kEmbeddingsModelOutputSize);

  // Load a model info builder.
  optimization_guide::TestModelInfoBuilder builder;
  builder.SetModelFilePath(embeddings_path);
  builder.SetAdditionalFiles({sp_path});
  builder.SetVersion(kEmbeddingsModelVersion);
  builder.SetModelMetadata(optimization_guide::AnyWrapProto(model_metadata));

  return builder;
}

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
    std::vector<std::string> passages = inputs;
    std::vector<Embedding> embeddings;
    std::vector<mojom::PassageEmbeddingsResultPtr> results;
    for (const std::string& input : inputs) {
      // Fails the generation on an "error" string to simulate failed model
      // execution.
      if (input == "error") {
        results.clear();
        break;
      }

      results.push_back(mojom::PassageEmbeddingsResult::New());
      results.back()->embeddings =
          std::vector<float>(kEmbeddingsModelOutputSize, 1.0);
      results.back()->passage = input;
    }

    std::move(callback).Run(std::move(results));
  }

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
    if (valid) {
      embedder_ = std::make_unique<FakePassageEmbedder>(std::move(receiver));
    }
    // Use input window size as a signal to fail the request.
    std::move(callback).Run(valid);
  }

  mojo::Receiver<mojom::PassageEmbeddingsService> receiver_;
  std::unique_ptr<FakePassageEmbedder> embedder_;
};

class FakePassageEmbeddingsServiceController
    : public PassageEmbeddingsServiceController {
 public:
  FakePassageEmbeddingsServiceController() = default;
  ~FakePassageEmbeddingsServiceController() override = default;

  void MaybeLaunchService() override {
    service_remote_.reset();
    service_ = std::make_unique<FakePassageEmbeddingsService>(
        service_remote_.BindNewPipeAndPassReceiver());
  }

  using PassageEmbeddingsServiceController::ResetEmbedderRemote;

  void ResetServiceRemote() override {
    ResetEmbedderRemote();
    service_remote_.reset();
  }

 private:
  std::unique_ptr<FakePassageEmbeddingsService> service_;
};

class FakeMlEmbedder : public MlEmbedder, public EmbedderMetadataObserver {
 public:
  explicit FakeMlEmbedder(
      PassageEmbeddingsServiceController* service_controller)
      : MlEmbedder(service_controller) {
    embedder_metadata_observation_.Observe(service_controller);
  }

  using OnEmbedderReadyCallback = base::OnceCallback<void(EmbedderMetadata)>;
  void SetOnEmbedderReadyCallback(OnEmbedderReadyCallback callback) {
    callback_ = std::move(callback);
    if (callback_ && metadata_.IsValid()) {
      std::move(callback_).Run(metadata_);
    }
  }

 protected:
  // EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override {
    metadata_ = metadata;
    if (callback_) {
      std::move(callback_).Run(metadata_);
    }
  }

  EmbedderMetadata metadata_{0, 0};
  OnEmbedderReadyCallback callback_;
  base::ScopedObservation<PassageEmbeddingsServiceController,
                          EmbedderMetadataObserver>
      embedder_metadata_observation_{this};
};

}  // namespace

class MlEmbedderTest : public testing::Test {
 public:
  void SetUp() override {
    service_controller_ =
        std::make_unique<FakePassageEmbeddingsServiceController>();
  }

  void TearDown() override {}

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<FakePassageEmbeddingsServiceController> service_controller_;
};

TEST_F(MlEmbedderTest, ReceivesValidModelInfo) {
  service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build());

  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReadyCallback(
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
  service_controller_->MaybeUpdateModelInfo({});
  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReadyCallback(base::BindLambdaForTesting(
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

  service_controller_->MaybeUpdateModelInfo(*builder.Build());
  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReadyCallback(base::BindLambdaForTesting(
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

  service_controller_->MaybeUpdateModelInfo(*builder.Build());
  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReadyCallback(base::BindLambdaForTesting(
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

  service_controller_->MaybeUpdateModelInfo(*builder.Build());
  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());
  bool on_embedder_ready_invoked = false;

  ml_embedder->SetOnEmbedderReadyCallback(base::BindLambdaForTesting(
      [&](EmbedderMetadata metadata) { on_embedder_ready_invoked = true; }));

  EXPECT_FALSE(on_embedder_ready_invoked);
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kInvalidAdditionalFiles,
      1);
}

TEST_F(MlEmbedderTest, ReturnsEmbeddings) {
  service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build());

  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(kModelInfoMetricName,
                                       EmbeddingsModelInfoStatus::kValid, 1);

  ComputePassagesEmbeddingsFuture future;
  ml_embedder->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                         {"foo", "bar"}, future.GetCallback());
  auto [passages, embeddings, task_id, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(passages[0], "foo");
  EXPECT_EQ(passages[1], "bar");
  EXPECT_EQ(embeddings[0].Dimensions(), kEmbeddingsModelOutputSize);
  EXPECT_EQ(embeddings[1].Dimensions(), kEmbeddingsModelOutputSize);
}

TEST_F(MlEmbedderTest, ReturnsModelUnavailableErrorIfModelInfoNotValid) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(std::nullopt);

  service_controller_->MaybeUpdateModelInfo(*builder.Build());
  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());

  ComputePassagesEmbeddingsFuture future;
  ml_embedder->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                         {"foo", "bar"}, future.GetCallback());
  auto [passages, embeddings, task_id, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kModelUnavailable);
  EXPECT_TRUE(passages.empty());
  EXPECT_TRUE(embeddings.empty());
  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kNoMetadata, 1);
}

TEST_F(MlEmbedderTest, ReturnsExecutionFailure) {
  service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build());
  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());

  ComputePassagesEmbeddingsFuture future;
  ml_embedder->ComputePassagesEmbeddings(PassagePriority::kPassive, {"error"},
                                         future.GetCallback());
  auto [passages, embeddings, task_id, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_TRUE(passages.empty());
  EXPECT_TRUE(embeddings.empty());
}

TEST_F(MlEmbedderTest, EmbedderRunningStatus) {
  service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build());
  auto ml_embedder =
      std::make_unique<FakeMlEmbedder>(service_controller_.get());

  {
    ComputePassagesEmbeddingsFuture future1;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"baz", "qux"}, future2.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    auto status1 = future1.Get<3>();
    EXPECT_EQ(status1, ComputeEmbeddingsStatus::kSuccess);
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    auto status2 = future2.Get<3>();
    EXPECT_EQ(status2, ComputeEmbeddingsStatus::kSuccess);
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());
  }
  {
    ComputePassagesEmbeddingsFuture future1;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"baz", "qux"}, future2.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    // Callbacks are invoked synchronously on embedder remote disconnect.
    service_controller_->ResetEmbedderRemote();
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());

    auto status1 = future1.Get<3>();
    EXPECT_EQ(status1, ComputeEmbeddingsStatus::kExecutionFailure);
    auto status2 = future2.Get<3>();
    EXPECT_EQ(status2, ComputeEmbeddingsStatus::kExecutionFailure);
  }
  {
    // Calling `ComputePassagesEmbeddings()` again launches the service.
    ComputePassagesEmbeddingsFuture future1;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"baz", "qux"}, future2.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    auto status1 = future1.Get<3>();
    EXPECT_EQ(status1, ComputeEmbeddingsStatus::kSuccess);
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    auto status2 = future2.Get<3>();
    EXPECT_EQ(status2, ComputeEmbeddingsStatus::kSuccess);
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());
  }
  {
    ComputePassagesEmbeddingsFuture future1;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    ml_embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"baz", "qux"}, future2.GetCallback());
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    // Callbacks are invoked synchronously on service remote disconnect.
    service_controller_->ResetServiceRemote();
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());

    auto status1 = future1.Get<3>();
    EXPECT_EQ(status1, ComputeEmbeddingsStatus::kExecutionFailure);
    auto status2 = future2.Get<3>();
    EXPECT_EQ(status2, ComputeEmbeddingsStatus::kExecutionFailure);
  }
}

}  // namespace passage_embeddings
