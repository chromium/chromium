// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_ML_INTENT_CLASSIFIER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_ML_INTENT_CLASSIFIER_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/history_embeddings/intent_classifier.h"
#include "components/history_embeddings/mock_intent_classifier.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"

namespace history_embeddings {

using optimization_guide::OptimizationGuideModelExecutor;

class MlIntentClassifier : public IntentClassifier {
 public:
  explicit MlIntentClassifier(OptimizationGuideModelExecutor* model_executor);
  ~MlIntentClassifier() override;

  int64_t GetModelVersion() override;

  void ComputeQueryIntent(std::string query,
                          ComputeQueryIntentCallback callback) override;

 private:
  // Guaranteed to outlive `this`, since
  // `model_executor_` is owned by OptimizationGuideKeyedServiceFactory,
  // which HistoryEmbeddingsServiceFactory depends on.
  raw_ptr<OptimizationGuideModelExecutor> model_executor_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_ML_INTENT_CLASSIFIER_H_
