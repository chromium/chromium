// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TYPES_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TYPES_H_

#include <optional>

namespace passage_embeddings {

inline constexpr char kModelInfoMetricName[] =
    "History.Embeddings.Embedder.ModelInfoStatus";

struct EmbedderMetadata {
  EmbedderMetadata(int64_t model_version,
                   size_t output_size,
                   std::optional<double> search_score_threshold = std::nullopt)
      : model_version(model_version),
        output_size(output_size),
        search_score_threshold(search_score_threshold) {}

  bool IsValid() { return model_version != 0 && output_size != 0; }

  int64_t model_version;
  size_t output_size;
  std::optional<double> search_score_threshold;
};

enum class EmbeddingsModelInfoStatus {
  kUnknown = 0,

  // Model info is valid.
  kValid = 1,

  // Model info is empty.
  kEmpty = 2,

  // Model info does not contain model metadata.
  kNoMetadata = 3,

  // Model info has invalid metadata.
  kInvalidMetadata = 4,

  // Model info has invalid additional files.
  kInvalidAdditionalFiles = 5,

  // This must be kept in sync with EmbeddingsModelInfoStatus in
  // history/enums.xml.
  kMaxValue = kInvalidAdditionalFiles,
};

// Classifies the priority of embeddings generation requests.
enum PassagePriority {
  // Executed as quickly as possible, runs faster and costs more resources.
  kUserInitiated = 0,

  // Execution is deprioritized and runs more slowly but more economically.
  kPassive = 1,

  // Execution may be delayed indefinitely and runs economically.
  kLatent = 2,
};

// The status of an embeddings generation attempt.
enum class ComputeEmbeddingsStatus {
  // Embeddings are generated successfully.
  kSuccess = 0,

  // The model files required for generation are not available.
  kModelUnavailable = 1,

  // Failure occurred during model execution.
  kExecutionFailure = 2,

  // The generation request was canceled, either explicitly or due to limits.
  kCanceled = 3,

  // This must be kept in sync with ComputeEmbeddingsStatus in
  // history/enums.xml.
  kMaxValue = kCanceled,
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TYPES_H_
