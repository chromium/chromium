// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embeddings_service_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

namespace {

using testing::ElementsAre;

using GetEmbeddingsTestFuture =
    base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>,
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
        std::make_unique<FakePassageEmbeddingsServiceController>();
    metadata_observer_.emplace(service_controller_.get(),
                               &embedder_metadata_future_);

    EXPECT_FALSE(embedder_metadata_future()->IsReady());
  }

  void TearDown() override {
    metadata_observer_.reset();
    service_controller_.reset();
  }

 protected:
  base::test::TestFuture<EmbedderMetadata>* embedder_metadata_future() {
    return &embedder_metadata_future_;
  }

  FakePassageEmbeddingsServiceController* service_controller() {
    return service_controller_.get();
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<FakePassageEmbeddingsServiceController> service_controller_;
  base::test::TestFuture<EmbedderMetadata> embedder_metadata_future_;
  std::optional<MetadataObserver> metadata_observer_;
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

TEST_F(PassageEmbeddingsServiceControllerTest, GetEmbeddingsEmpty) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));

  GetEmbeddingsTestFuture future;
  service_controller()->GetEmbeddings({}, PassagePriority::kPassive,
                                      future.GetCallback());

  auto [results, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_TRUE(results.empty());
}

TEST_F(PassageEmbeddingsServiceControllerTest, GetEmbeddingsNonEmpty) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));

  GetEmbeddingsTestFuture future;
  service_controller()->GetEmbeddings({"1.0", "2.0"}, PassagePriority::kPassive,
                                      future.GetCallback());
  auto [results, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_THAT(results[0]->embeddings, ElementsAre(1.0f));
  EXPECT_THAT(results[1]->embeddings, ElementsAre(2.0f));
}

TEST_F(PassageEmbeddingsServiceControllerTest,
       ReturnsModelUnavailableErrorIfModelInfoNotValid) {
  optimization_guide::TestModelInfoBuilder builder =
      GetBuilderWithValidModelInfo();
  builder.SetModelMetadata(std::nullopt);

  EXPECT_FALSE(service_controller_->MaybeUpdateModelInfo(*builder.Build()));

  GetEmbeddingsTestFuture future;
  service_controller()->GetEmbeddings({"1.0"}, PassagePriority::kPassive,
                                      future.GetCallback());
  auto [results, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kModelUnavailable);
  EXPECT_EQ(results.size(), 0u);
}

TEST_F(PassageEmbeddingsServiceControllerTest, ReturnsExecutionFailure) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));

  GetEmbeddingsTestFuture future;
  service_controller()->GetEmbeddings({"error"}, PassagePriority::kPassive,
                                      future.GetCallback());
  auto [results, status] = future.Take();

  EXPECT_EQ(status, ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_EQ(results.size(), 0u);
}

TEST_F(PassageEmbeddingsServiceControllerTest, EmbedderRunningStatus) {
  EXPECT_TRUE(service_controller_->MaybeUpdateModelInfo(
      *GetBuilderWithValidModelInfo().Build()));

  const auto get_embeddings = [this] {
    GetEmbeddingsTestFuture future;
    service_controller()->GetEmbeddings({"1.0"}, PassagePriority::kPassive,
                                        future.GetCallback());
    return future;
  };

  {
    GetEmbeddingsTestFuture future1 = get_embeddings();
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2 = get_embeddings();
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future1.Get<1>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future2.Get<1>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());
  }
  {
    GetEmbeddingsTestFuture future1 = get_embeddings();
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2 = get_embeddings();
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    // Callbacks are invoked synchronously on embedder remote disconnect.
    service_controller_->ResetEmbedderRemote();
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future1.Get<1>(), ComputeEmbeddingsStatus::kExecutionFailure);
    EXPECT_EQ(future2.Get<1>(), ComputeEmbeddingsStatus::kExecutionFailure);
  }
  {
    // Calling `ComputePassagesEmbeddings()` again launches the service.
    GetEmbeddingsTestFuture future1 = get_embeddings();
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2 = get_embeddings();
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future1.Get<1>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future2.Get<1>(), ComputeEmbeddingsStatus::kSuccess);
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());
  }
  {
    GetEmbeddingsTestFuture future1 = get_embeddings();
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    GetEmbeddingsTestFuture future2 = get_embeddings();
    // Embedder is still running.
    EXPECT_TRUE(service_controller_->EmbedderRunning());

    // Callbacks are invoked synchronously on service remote disconnect.
    service_controller_->ResetServiceRemote();
    // Embedder is NOT running.
    EXPECT_FALSE(service_controller_->EmbedderRunning());

    EXPECT_EQ(future1.Get<1>(), ComputeEmbeddingsStatus::kExecutionFailure);
    EXPECT_EQ(future2.Get<1>(), ComputeEmbeddingsStatus::kExecutionFailure);
  }
}

}  // namespace passage_embeddings
