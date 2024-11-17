// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_ML_ANSWERER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_ML_ANSWERER_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/mock_answerer.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"

namespace history_embeddings {

using optimization_guide::OptimizationGuideModelExecutor;
using Session = optimization_guide::OptimizationGuideModelExecutor::Session;

class MlAnswerer : public Answerer {
 public:
  explicit MlAnswerer(OptimizationGuideModelExecutor* model_executor);
  ~MlAnswerer() override;

  int64_t GetModelVersion() override;
  void ComputeAnswer(std::string query,
                     Context context,
                     ComputeAnswerCallback callback) override;

 private:
  class SessionManager;
  struct ModelInput;

  // Start and add a session for the url and passages.
  void StartAndAddSession(const std::string& query,
                          const std::string& url,
                          const std::vector<std::string>& passages,
                          std::unique_ptr<Session> session,
                          base::OnceCallback<void(int)> session_started);

  // Guaranteed to outlive `this`, since
  // `model_executor_` is owned by OptimizationGuideKeyedServiceFactory,
  // which HistoryEmbeddingsServiceFactory depends on.
  raw_ptr<OptimizationGuideModelExecutor> model_executor_;
  std::unique_ptr<SessionManager> session_manager_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_ML_ANSWERER_H_
