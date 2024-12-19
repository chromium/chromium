// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_intent_classifier.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/intent_classifier.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/history_query_intent.pb.h"
#include "components/optimization_guide/proto/history_query_intent_model_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"

namespace history_embeddings {

namespace {

using ::optimization_guide::ModelBasedCapabilityKey;
using ::optimization_guide::SessionConfigParams;
using Session = ::optimization_guide::OptimizationGuideModelExecutor::Session;

using ::optimization_guide::ParsedAnyMetadata;
using ::optimization_guide::proto::HistoryQueryIntentModelMetadata;
using ::optimization_guide::proto::HistoryQueryIntentRequest;
using ::optimization_guide::proto::HistoryQueryIntentResponse;

}  // namespace

// State for an intent classification.
class MlIntentClassifier::Execution final {
 public:
  Execution() = default;

  void Execute(OptimizationGuideModelExecutor* model_executor,
               std::string query,
               ComputeQueryIntentCallback callback) {
    session_ = model_executor->StartSession(
        ModelBasedCapabilityKey::kHistoryQueryIntent,
        SessionConfigParams{
            .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
        });
    if (!session_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         ComputeIntentStatus::MODEL_UNAVAILABLE, false));
      return;
    }
    const auto any = session_->GetOnDeviceFeatureMetadata();
    model_metadata_ = ParsedAnyMetadata<HistoryQueryIntentModelMetadata>(any);

    callback_ = std::move(callback);
    HistoryQueryIntentRequest request;
    request.set_text(std::move(query));

    if (EnableMlIntentClassifierScore()) {
      session_->AddContext(request);
      session_->Score(GetTokenToScore(),
                      base::BindOnce(&Execution::OnScoreResult,
                                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      session_->ExecuteModel(
          request, base::BindRepeating(&Execution::OnExecutionResult,
                                       weak_ptr_factory_.GetWeakPtr()));
    }
  }

 private:
  void OnExecutionResult(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result) {
    if (!result.response.has_value()) {
      Finish(ComputeIntentStatus::EXECUTION_FAILURE, false);
      return;
    }
    if (!result.response->is_complete) {
      return;
    }
    auto response = ParsedAnyMetadata<HistoryQueryIntentResponse>(
        result.response->response);
    if (!response) {
      Finish(ComputeIntentStatus::EXECUTION_FAILURE, false);
      return;
    }
    Finish(ComputeIntentStatus::SUCCESS, response->is_answer_seeking());
  }

  void OnScoreResult(std::optional<float> score) {
    if (!score.has_value()) {
      Finish(ComputeIntentStatus::EXECUTION_FAILURE, false);
      return;
    }
    bool is_answer_seeking = *score > GetIntentScoreThreshold();
    Finish(ComputeIntentStatus::SUCCESS, is_answer_seeking);
  }

  void Finish(ComputeIntentStatus status, bool is_query_intent) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(session_));
    std::move(callback_).Run(status, is_query_intent);
  }

  bool EnableMlIntentClassifierScore() {
    return GetFeatureParameters().enable_ml_intent_classifier_score &&
           model_metadata_ && !model_metadata_->score_token().empty();
  }

  std::string GetTokenToScore() {
    CHECK(model_metadata_);
    return model_metadata_->score_token();
  }

  float GetIntentScoreThreshold() {
    CHECK(model_metadata_);
    return model_metadata_->score_threshold();
  }

  ComputeQueryIntentCallback callback_;
  std::unique_ptr<Session> session_;
  std::optional<HistoryQueryIntentModelMetadata> model_metadata_;
  base::WeakPtrFactory<Execution> weak_ptr_factory_{this};
};

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
  execution_ = std::make_unique<Execution>();
  execution_->Execute(model_executor_, std::move(query), std::move(callback));
}

}  // namespace history_embeddings
