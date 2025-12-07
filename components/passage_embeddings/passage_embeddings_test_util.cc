// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embeddings_test_util.h"

#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"

namespace passage_embeddings {

namespace {

inline constexpr uint32_t kEmbeddingsModelInputWindowSize = 256u;

Embedding ComputeEmbeddingForPassage(size_t embeddings_model_output_size) {
  constexpr size_t kMockPassageWordCount = 10;
  Embedding embedding(std::vector<float>(embeddings_model_output_size, 1.0f));
  embedding.Normalize();
  embedding.SetPassageWordCount(kMockPassageWordCount);
  return embedding;
}

EmbedderMetadata GetValidEmbedderMetadata() {
  return EmbedderMetadata(kEmbeddingsModelVersion, kEmbeddingsModelOutputSize);
}

}  // namespace

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

std::vector<Embedding> ComputeEmbeddingsForPassages(
    const std::vector<std::string>& passages) {
  return std::vector<Embedding>(
      passages.size(), ComputeEmbeddingForPassage(kEmbeddingsModelOutputSize));
}

////////////////////////////////////////////////////////////////////////////////

Embedder::TaskId TestEmbedder::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::vector<std::string> passages,
                        ComputePassagesEmbeddingsCallback callback) {
                       std::move(callback).Run(
                           passages, ComputeEmbeddingsForPassages(passages),
                           /*task_id=*/0, ComputeEmbeddingsStatus::kSuccess);
                     },
                     passages, std::move(callback)));
  return 0;
}

void TestEmbedder::ReprioritizeTasks(PassagePriority priority,
                                     const std::set<TaskId>& tasks) {}

bool TestEmbedder::TryCancel(TaskId task_id) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////

TestEmbedderMetadataProvider::TestEmbedderMetadataProvider() = default;
TestEmbedderMetadataProvider::~TestEmbedderMetadataProvider() = default;

void TestEmbedderMetadataProvider::AddObserver(
    EmbedderMetadataObserver* observer) {
  observer->EmbedderMetadataUpdated(GetValidEmbedderMetadata());
  observer_list_.AddObserver(observer);
}
void TestEmbedderMetadataProvider::RemoveObserver(
    EmbedderMetadataObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////

TestEnvironment::TestEnvironment()
    : embedder_(std::make_unique<TestEmbedder>()),
      embedder_metadata_provider_(
          std::make_unique<TestEmbedderMetadataProvider>()) {}

TestEnvironment::~TestEnvironment() = default;

}  // namespace passage_embeddings
