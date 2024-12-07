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

// The status of an embeddings generation attempt.
enum class ComputeEmbeddingsStatus {
  // Embeddings are generated successfully.
  KSuccess = 0,

  // The model files required for generation are not available.
  KModelUnavailable = 1,

  // Failure occurred during model execution.
  kExecutionFailure = 2,

  // The generation request was skipped. This could happen if the embeddings
  // request for a user query, which may have been obsolete (by a newer user
  // query) by the time the embedder is free.
  KSkipped = 3,
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TYPES_H_
