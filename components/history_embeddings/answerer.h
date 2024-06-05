// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_ANSWERER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_ANSWERER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace history_embeddings {

// The status of an answer generation attempt.
enum class ComputeAnswerStatus {
  // Answer generated successfully.
  SUCCESS,

  // The model files required for generation are not available.
  MODEL_UNAVAILABLE,

  // Failure occurred during model execution.
  EXECUTION_FAILURE,
};

// Holds potentially multiple answers with scores from the model and
// associations to source context (not implemented yet).
struct AnswererResult {
  ComputeAnswerStatus status;
  std::string query;
  std::string answer;
};

using ComputeAnswerCallback = base::OnceCallback<void(AnswererResult result)>;

// Base class that hides implementation details for how answers are generated.
class Answerer {
 public:
  // This type specifies the query context that can be used to inform
  // generated answers. It may include top search result passages and
  // potentially other data, so this may eventually become a struct.
  using Context = std::vector<std::string>;

  virtual ~Answerer() = default;

  // Returns 0 if not ready, and the nonzero model version number when it's
  // loaded and ready.
  virtual int64_t GetModelVersion() = 0;

  // Calls `callback` asynchronously with answer to `query`.
  virtual void ComputeAnswer(std::string query,
                             Context context,
                             ComputeAnswerCallback callback) = 0;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_ANSWERER_H_
