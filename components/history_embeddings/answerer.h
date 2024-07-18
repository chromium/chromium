// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_ANSWERER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_ANSWERER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"

namespace history_embeddings {

// The status of an answer generation attempt.
enum class ComputeAnswerStatus {
  // Answer generated successfully.
  SUCCESS,

  // The model files required for generation are not available.
  MODEL_UNAVAILABLE,

  // Failure occurred during model execution.
  EXECUTION_FAILURE,

  // Model execution cancelled.
  EXECUTION_CANCELLED,
};

// Holds potentially multiple answers with scores from the model and
// associations to source context (not implemented yet).
struct AnswererResult {
  ComputeAnswerStatus status;
  std::string query;
  optimization_guide::proto::Answer answer;
};

using ComputeAnswerCallback = base::OnceCallback<void(AnswererResult result)>;

// Base class that hides implementation details for how answers are generated.
class Answerer {
 public:
  // This type specifies the query context that can be used to inform
  // generated answers. It includes top search result passages and
  // potentially other data.
  struct Context {
    Context();
    Context(const Context& other);
    ~Context();

    // URL to passages.
    std::unordered_map<std::string, std::vector<std::string>> url_passages_map;
  };

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
