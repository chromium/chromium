// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_answerer.h"

#include "base/barrier_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"

namespace history_embeddings {

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;
using Session = optimization_guide::OptimizationGuideModelExecutor::Session;
using optimization_guide::OptimizationGuideModelExecutionError;
using optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using optimization_guide::proto::Answer;
using optimization_guide::proto::HistoryAnswerRequest;
using optimization_guide::proto::Passage;

namespace {

static constexpr std::string kPassageIdToken = "ID";

std::string GetPassageIdStr(size_t id) {
  return base::StringPrintf("%04d", static_cast<int>(id));
}

float GetMlAnswerScoreThreshold() {
  return kMlAnswererMinScore.Get();
}

void AddQueryAndPassagesToSession(const std::string& query,
                                  const std::vector<std::string>& passages,
                                  Session* session) {
  HistoryAnswerRequest request;
  request.set_query(query);
  for (size_t i = 0; i < passages.size(); i++) {
    auto* passage = request.add_passages();
    passage->set_text(passages[i]);
    passage->set_passage_id(GetPassageIdStr(i + 1));
  }
  session->AddContext(request);
}

}  // namespace

// Manages sessions for generating an answer for a given query and multiple
// URLs.
class MlAnswerer::SessionManager {
 public:
  using SessionScoreType = std::tuple<int, std::optional<float>>;

  SessionManager(std::string query,
                 Context context,
                 ComputeAnswerCallback callback)
      : query_(std::move(query)),
        context_(std::move(context)),
        callback_(std::move(callback)),
        origin_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        weak_ptr_factory_(this) {}

  ~SessionManager() {
    // Run the existing callback if not called yet with canceled status.
    if (!callback_.is_null()) {
      Finish(AnswererResult(ComputeAnswerStatus::EXECUTION_CANCELLED, query_,
                            Answer()));
    }
  }

  // Adds a session that contains query and passage context.
  // It exists until this manager resets or gets destroyed.
  void AddSession(
      std::unique_ptr<OptimizationGuideModelExecutor::Session> session,
      std::string url) {
    sessions_.push_back(std::move(session));
    urls_.push_back(url);
  }

  // Runs speculative decoding by first getting scores for each URL candidate
  // and continuing decoding with only the highest scored session.
  void RunSpeculativeDecoding() {
    const size_t num_sessions = GetNumberOfSessions();
    base::OnceCallback<void(const std::vector<SessionScoreType>&)> cb =
        base::BindOnce(&SessionManager::SortAndDecode,
                       weak_ptr_factory_.GetWeakPtr());
    const auto barrier_cb =
        base::BarrierCallback<SessionScoreType>(num_sessions, std::move(cb));
    for (size_t s_index = 0; s_index < num_sessions; s_index++) {
      sessions_[s_index]->Score(
          kPassageIdToken, base::BindOnce(
                               [](size_t index, std::optional<float> score) {
                                 return std::make_tuple(index, score);
                               },
                               s_index)
                               .Then(barrier_cb));
    }
  }

  size_t GetNumberOfSessions() { return sessions_.size(); }

  // Finishes and cleans up sessions.
  void Finish(AnswererResult answer_result) {
    CHECK(!callback_.is_null());
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::move(answer_result)));

    // Destroy all existing sessions.
    sessions_.clear();
    urls_.clear();
  }

 private:
  // Callback to be repeatedly called during streaming execution.
  void StreamingExecutionCallback(
      size_t session_index,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result) {
    if (!result.response.has_value()) {
      Finish(AnswererResult(ComputeAnswerStatus::EXECUTION_FAILURE, query_,
                            Answer()));
    } else if (result.response->is_complete) {
      auto response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::HistoryAnswerResponse>(
          std::move(result.response).value().response);
      Finish(AnswererResult(ComputeAnswerStatus::SUCCESS, query_,
                            response->answer(), std::move(result.log_entry),
                            urls_[session_index], {}));
    }
  }

  // Decodes with the highest scored session.
  void SortAndDecode(const std::vector<SessionScoreType>& session_scores) {
    size_t max_index = 0;
    float max_score = 0.0;
    for (size_t i = 0; i < session_scores.size(); i++) {
      const auto score = std::get<1>(session_scores[i]);
      if (score.has_value() && *score > max_score) {
        max_score = *score;
        max_index = i;
      }
    }

    // Return unanswerable status due to highest score is below the threshold.
    if (max_score < GetMlAnswerScoreThreshold()) {
      Finish(
          AnswererResult{ComputeAnswerStatus::UNANSWERABLE, query_, Answer()});
      return;
    }

    // Continue decoding using the session with the highest score.
    // Use a dummy request here since both passages and query are already added
    // to context.
    if (!sessions_.empty()) {
      optimization_guide::proto::HistoryAnswerRequest request;
      const size_t session_index = std::get<0>(session_scores[max_index]);
      sessions_[session_index]->ExecuteModel(
          request,
          base::BindRepeating(&SessionManager::StreamingExecutionCallback,
                              weak_ptr_factory_.GetWeakPtr(), session_index));
    } else {
      // If sessions are already cleaned up, run callback with canceled status.
      Finish(AnswererResult{ComputeAnswerStatus::EXECUTION_CANCELLED, query_,
                            Answer()});
    }
  }

  std::vector<std::unique_ptr<OptimizationGuideModelExecutor::Session>>
      sessions_;
  // URLs associated with sessions by index.
  std::vector<std::string> urls_;
  std::string query_;
  Context context_;
  ComputeAnswerCallback callback_;
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  base::WeakPtrFactory<SessionManager> weak_ptr_factory_;
};

MlAnswerer::MlAnswerer(OptimizationGuideModelExecutor* model_executor)
    : model_executor_(model_executor) {}

MlAnswerer::~MlAnswerer() = default;

void MlAnswerer::ComputeAnswer(std::string query,
                               Context context,
                               ComputeAnswerCallback callback) {
  CHECK(model_executor_);

  // Assign a new session manager (and destroy the existing one).
  session_manager_ =
      std::make_unique<SessionManager>(query, context, std::move(callback));

  // Start a session for each URL.
  for (const auto& url_and_passages : context.url_passages_map) {
    std::unique_ptr<Session> session = model_executor_->StartSession(
        optimization_guide::ModelBasedCapabilityKey::kHistorySearch,
        /*config_params=*/std::nullopt);
    if (session == nullptr) {
      session_manager_->Finish(AnswererResult(
          ComputeAnswerStatus::MODEL_UNAVAILABLE, query, Answer()));
      return;
    }

    AddQueryAndPassagesToSession(query, url_and_passages.second, session.get());
    session_manager_->AddSession(std::move(session), url_and_passages.first);
  }

  session_manager_->RunSpeculativeDecoding();
}

}  // namespace history_embeddings
