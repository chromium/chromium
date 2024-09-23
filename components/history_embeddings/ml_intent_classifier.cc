// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_intent_classifier.h"

#include "base/task/sequenced_task_runner.h"

namespace history_embeddings {

MlIntentClassifier::MlIntentClassifier(
    OptimizationGuideModelExecutor* model_executor)
    : model_executor_(model_executor) {}

MlIntentClassifier::~MlIntentClassifier() = default;

int64_t MlIntentClassifier::GetModelVersion() {
  // This can be replaced with the real implementation.
  return 0;
}

void MlIntentClassifier::ComputeQueryIntent(
    std::string query,
    ComputeQueryIntentCallback callback) {
  // This can be replaced with the real implementation.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ComputeIntentStatus::SUCCESS, true));
}

}  // namespace history_embeddings
