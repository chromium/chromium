// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embeddings_service_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

namespace {

using ComputePassagesEmbeddingsFuture =
    base::test::TestFuture<std::vector<std::string>,
                           std::vector<Embedding>,
                           Embedder::TaskId,
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

  using PassageEmbeddingsServiceController::GetEmbeddingsCallback;
  using PassageEmbeddingsServiceController::ResetEmbedderRemote;

  void ResetServiceRemote() override {
    ResetEmbedderRemote();
    service_remote_.reset();
  }

  using PassageEmbeddingsServiceController::GetEmbeddings;

 private:
  std::unique_ptr<FakePassageEmbeddingsService> service_;
};

class FakeEmbedder : public TestEmbedder, public EmbedderMetadataObserver {
 public:
  explicit FakeEmbedder(
      EmbedderMetadataProvider* embedder_metadata_provider,
      FakePassageEmbeddingsServiceController::GetEmbeddingsCallback
          get_embeddings_callback,
      base::test::TestFuture<EmbedderMetadata>* embedder_metadata_future)
      : get_embeddings_callback_(get_embeddings_callback),
        embedder_metadata_future_(embedder_metadata_future) {
    embedder_metadata_observation_.Observe(embedder_metadata_provider);
  }

  // Embedder:
  Embedder::TaskId ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override {
    get_embeddings_callback_.Run(
        passages, priority,
        base::BindOnce(
            [](std::vector<std::string> passages,
               ComputePassagesEmbeddingsCallback callback,
               std::vector<mojom::PassageEmbeddingsResultPtr> results,
               ComputeEmbeddingsStatus status) {
              std::vector<Embedding> embeddings;
              if (status == ComputeEmbeddingsStatus::kSuccess) {
                embeddings = ComputeEmbeddingsForPassages(passages);
              }
              std::move(callback).Run(passages, embeddings, kInvalidTaskId,
                                      status);
            },
            passages, std::move(callback)));
    return kInvalidTaskId;
  }

 protected:
  // EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(EmbedderMetadata metadata) override {
    embedder_metadata_future_->SetValue(metadata);
  }

  base::ScopedObservation<EmbedderMetadataProvider, EmbedderMetadataObserver>
      embedder_metadata_observation_{this};
  FakePassageEmbeddingsServiceController::GetEmbeddingsCallback
      get_embeddings_callback_;
  raw_ptr<base::test::TestFuture<EmbedderMetadata>> embedder_metadata_future_;
};

}  // namespace

class PassageEmbeddingsServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    service_controller_ =
        std::make_unique<FakePassageEmbeddingsServiceController>();
    service_controller_->SetEmbedderForTesting(std::make_unique<FakeEmbedder>(
        /*embedder_metadata_provider=*/service_controller_.get(),
        /*get_embeddings_callback=*/
        base::BindRepeating(
            &FakePassageEmbeddingsServiceController::GetEmbeddings,
            base::Unretained(service_controller_.get())),
        /*embedder_metadata_future=*/embedder_metadata_future()));

    EXPECT_FALSE(embedder_metadata_future()->IsReady());
  }

 protected:
  base::test::TestFuture<EmbedderMetadata>* embedder_metadata_future() {
    return &embedder_metadata_future_;
  }

  Embedder* embedder() { return service_controller_->GetEmbedder(); }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::test::TestFuture<EmbedderMetadata> embedder_metadata_future_;
  std::unique_ptr<FakePassageEmbeddingsServiceController> service_controller_;
};

TEST_F(PassageEmbeddingsServiceControllerTest, ReceivesValidModelInfo) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));
  auto metadata = embedder_metadata_future()->Take();
  EXPECT_TRUE(metadata.IsValid());
  EXPECT_EQ(metadata.model_version, kEmbeddingsModelVersion);
  EXPECT_EQ(metadata.output_size, kEmbeddingsModelOutputSize);

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

TEST_F(PassageEmbeddingsServiceControllerTest,
       ReceivesModelInfoWithInvalidModelMetadata) {
  optimization_guide::proto::Any metadata_any;
  metadata_any.set_type_url("not a valid type url");
  metadata_any.set_value("not a valid serialized metadata");
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(metadata_any);

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(*builder.Build()));
  EXPECT_FALSE(embedder_metadata_future()->IsReady());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kInvalidMetadata, 1);
}

TEST_F(PassageEmbeddingsServiceControllerTest,
       ReceivesModelInfoWithoutModelMetadata) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(std::nullopt);

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(*builder.Build()));
  EXPECT_FALSE(embedder_metadata_future()->IsReady());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kNoMetadata, 1);
}

TEST_F(PassageEmbeddingsServiceControllerTest,
       ReceivesModelInfoWithoutAdditionalFiles) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetAdditionalFiles(
      {test_data_dir.AppendASCII("foo"), test_data_dir.AppendASCII("bar")});

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(*builder.Build()));
  EXPECT_FALSE(embedder_metadata_future()->IsReady());

  histogram_tester_.ExpectTotalCount(kModelInfoMetricName, 1);
  histogram_tester_.ExpectUniqueSample(
      kModelInfoMetricName, EmbeddingsModelInfoStatus::kInvalidAdditionalFiles,
      1);
}

TEST_F(PassageEmbeddingsServiceControllerTest, ReceivesEmptyPassages) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));

  ComputePassagesEmbeddingsFuture future;
  embedder()->ComputePassagesEmbeddings(PassagePriority::kPassive, {},
                                        future.GetCallback());
  auto [passages, embeddings, task_id, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_TRUE(passages.empty());
  EXPECT_TRUE(embeddings.empty());
}

TEST_F(PassageEmbeddingsServiceControllerTest, ReturnsEmbeddings) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));

  ComputePassagesEmbeddingsFuture future;
  embedder()->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                        {"foo", "bar"}, future.GetCallback());
  auto [passages, embeddings, task_id, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(passages[0], "foo");
  EXPECT_EQ(passages[1], "bar");
  EXPECT_EQ(embeddings[0].Dimensions(), kEmbeddingsModelOutputSize);
  EXPECT_EQ(embeddings[1].Dimensions(), kEmbeddingsModelOutputSize);
}

TEST_F(PassageEmbeddingsServiceControllerTest,
       ReturnsModelUnavailableErrorIfModelInfoNotValid) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(std::nullopt);

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(*builder.Build()));

  ComputePassagesEmbeddingsFuture future;
  embedder()->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                        {"foo", "bar"}, future.GetCallback());
  auto [passages, embeddings, task_id, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kModelUnavailable);
  EXPECT_EQ(passages[0], "foo");
  EXPECT_EQ(passages[1], "bar");
  EXPECT_TRUE(embeddings.empty());
}

TEST_F(PassageEmbeddingsServiceControllerTest, ReturnsExecutionFailure) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));

  ComputePassagesEmbeddingsFuture future;
  embedder()->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                        {"error", "baz"}, future.GetCallback());
  auto [passages, embeddings, task_id, status] = future.Get();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_EQ(passages[0], "error");
  EXPECT_EQ(passages[1], "baz");
  EXPECT_TRUE(embeddings.empty());
}

TEST_F(PassageEmbeddingsServiceControllerTest, EmbedderRunningStatus) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));
  {
    ComputePassagesEmbeddingsFuture future1;
    embedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    embedder()->ComputePassagesEmbeddings(
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
    embedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    embedder()->ComputePassagesEmbeddings(
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
    embedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    embedder()->ComputePassagesEmbeddings(
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
    embedder()->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"foo", "bar"}, future1.GetCallback());
    // Embedder is running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    ComputePassagesEmbeddingsFuture future2;
    embedder()->ComputePassagesEmbeddings(
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
