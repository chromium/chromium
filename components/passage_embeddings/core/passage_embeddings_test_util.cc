// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/core/passage_embeddings_test_util.h"

#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"

namespace passage_embeddings {

namespace {

inline constexpr uint32_t kEmbeddingsModelInputWindowSize = 256u;

EmbedderMetadata GetValidEmbedderMetadata() {
  return EmbedderMetadata(kEmbeddingsModelVersion, 3ul);
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
  model_metadata.set_output_size(3ul);

  // Load a model info builder.
  optimization_guide::TestModelInfoBuilder builder;
  builder.SetModelFilePath(embeddings_path);
  builder.SetAdditionalFiles({sp_path});
  builder.SetVersion(kEmbeddingsModelVersion);
  builder.SetModelMetadata(optimization_guide::AnyWrapProto(model_metadata));

  return builder;
}

////////////////////////////////////////////////////////////////////////////////

TestEmbedder::TestEmbedder() = default;
TestEmbedder::~TestEmbedder() = default;

Embedder::Job TestEmbedder::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  uint64_t job_id = next_job_id_++;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::vector<std::string> passages, uint64_t job_id,
                        ComputePassagesEmbeddingsCallback callback) {
                       std::vector<Embedding> embeddings(
                           passages.size(), Embedding({1.0f, 0.0f, 0.0f}));
                       std::move(callback).Run(
                           passages, std::move(embeddings), job_id,
                           ComputeEmbeddingsStatus::kSuccess);
                     },
                     passages, job_id, std::move(callback)));
  return Embedder::Job(weak_ptr_factory_.GetWeakPtr(), job_id);
}

base::WeakPtr<Embedder> TestEmbedder::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TestEmbedder::ReprioritizeJobs(PassagePriority priority,
                                    const std::set<uint64_t>& job_ids) {}

bool TestEmbedder::TryCancel(uint64_t job_id) {
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
