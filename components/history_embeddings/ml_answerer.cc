// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_answerer.h"

#include "base/barrier_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"

namespace history_embeddings {

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;
using optimization_guide::OptimizationGuideModelExecutionError;
using optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using optimization_guide::proto::Answer;
using optimization_guide::proto::HistoryAnswerRequest;
using optimization_guide::proto::Passage;

namespace {

static constexpr std::string kPassageIdToken = "ID";
// Estimated token count of the preamble text in prompt.
static constexpr size_t kPreambleTokenBufferSize = 100u;
// Estimated token count of overhead text per passage.
static constexpr size_t kExtraTokensPerPassage = 10u;

std::string GetPassageIdStr(size_t id) {
  return base::StringPrintf("%04d", static_cast<int>(id));
}

float GetMlAnswerScoreThreshold() {
  return kMlAnswererMinScore.Get();
}

}  // namespace

// Helper struct to bundle raw model input (queries/passages) with its metadata.
struct MlAnswerer::ModelInput {
  // The string content of this input.
  std::string text;
  // Index 0 is reserved for queries, i.e. this index will be 0 iff. this input
  // is a query. If the input is a passage, index will contain the index of the
  // passage in the original passage vector (where lower index means higher
  // relevance), plus 1 to offset for query.
  size_t index;
  // The size of `text` in tokens.
  uint32_t token_count;
};

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
      FinishAndResetSessions(AnswererResult(
          ComputeAnswerStatus::EXECUTION_CANCELLED, query_, Answer()));
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

  base::WeakPtr<MlAnswerer::SessionManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Runs callback with result.
  void FinishCallback(AnswererResult answer_result) {
    CHECK(!callback_.is_null());
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::move(answer_result)));
  }

  // Finishes and cleans up sessions.
  void FinishAndResetSessions(AnswererResult answer_result) {
    FinishCallback(std::move(answer_result));

    // Destroy all existing sessions.
    sessions_.clear();
    urls_.clear();
  }

  // Called when all sessions are started and added.
  void OnSessionsStarted(std::vector<int> args) { RunSpeculativeDecoding(); }

  // Called when token counts of the query and all passages of a session are
  // computed.
  void OnTokenCountRetrieved(std::unique_ptr<Session> session,
                             const std::string url,
                             base::OnceCallback<void(int)> session_added_cb,
                             std::vector<ModelInput> inputs) {
    HistoryAnswerRequest request;
    // TODO(crbug.com/372535307): use actual model limit.
    int token_limit = 1024;
    // Reserve space for preamble text.
    int token_count = kPreambleTokenBufferSize;

    // Sort the inputs according to their indices in the original vector, so
    // we prioritize passages that are more relevant.
    base::ranges::sort(
        inputs.begin(), inputs.end(),
        [](ModelInput& i1, ModelInput& i2) { return i1.index < i2.index; });

    // Add the query to the request. The query will always have index 0.
    token_count += inputs[0].token_count;
    request.set_query(inputs[0].text);

    // Add as many passages as the input window can fit.
    for (size_t i = 1; i < inputs.size(); ++i) {
      token_count += (inputs[i].token_count + kExtraTokensPerPassage);
      if (token_count > token_limit) {
        break;
      }

      auto* passage = request.add_passages();
      passage->set_text(inputs[i].text);
      passage->set_passage_id(GetPassageIdStr(i));
    }

    session->AddContext(request);
    AddSession(std::move(session), url);
    std::move(session_added_cb).Run(1);
  }

 private:
  // Callback to be repeatedly called during streaming execution.
  void StreamingExecutionCallback(
      size_t session_index,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result) {
    if (!result.response.has_value()) {
      FinishCallback(AnswererResult(ComputeAnswerStatus::EXECUTION_FAILURE,
                                    query_, Answer()));
    } else if (result.response->is_complete) {
      auto response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::HistoryAnswerResponse>(
          std::move(result.response).value().response);
      FinishCallback(AnswererResult(
          ComputeAnswerStatus::SUCCESS, query_, response->answer(),
          std::move(result.log_entry), urls_[session_index], {}));
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
      FinishAndResetSessions(
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
      FinishAndResetSessions(AnswererResult{
          ComputeAnswerStatus::EXECUTION_CANCELLED, query_, Answer()});
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

int64_t MlAnswerer::GetModelVersion() {
  // This can be replaced with the real implementation.
  return 0;
}

void MlAnswerer::ComputeAnswer(std::string query,
                               Context context,
                               ComputeAnswerCallback callback) {
  CHECK(model_executor_);

  // Assign a new session manager (and destroy the existing one).
  session_manager_ =
      std::make_unique<SessionManager>(query, context, std::move(callback));

  const auto sessions_started_callback = base::BarrierCallback<int>(
      context.url_passages_map.size(),
      base::BindOnce(&MlAnswerer::SessionManager::OnSessionsStarted,
                     session_manager_->GetWeakPtr()));

  // Start a session for each URL.
  for (const auto& url_and_passages : context.url_passages_map) {
    std::unique_ptr<Session> session = model_executor_->StartSession(
        optimization_guide::ModelBasedCapabilityKey::kHistorySearch,
        /*config_params=*/std::nullopt);
    if (session == nullptr) {
      session_manager_->FinishAndResetSessions(AnswererResult(
          ComputeAnswerStatus::MODEL_UNAVAILABLE, query, Answer()));
      return;
    }

    StartAndAddSession(query, url_and_passages.first, url_and_passages.second,
                       std::move(session), sessions_started_callback);
  }
}

void MlAnswerer::StartAndAddSession(
    const std::string& query,
    const std::string& url,
    const std::vector<std::string>& passages,
    std::unique_ptr<Session> session,
    base::OnceCallback<void(int)> session_started) {
  Session* raw_session = session.get();
  const auto token_count_callback = base::BarrierCallback<ModelInput>(
      passages.size() + 1,  // We need token count for passages + query.
      base::BindOnce(&MlAnswerer::SessionManager::OnTokenCountRetrieved,
                     session_manager_->GetWeakPtr(), std::move(session), url,
                     std::move(session_started)));

  const auto make_model_input = [](std::string text, size_t index,
                                   uint32_t token_count) {
    return ModelInput{text, index, token_count};
  };

  // Get token count for query, always assign index 0 to query to make a
  // ModelInput.
  raw_session->GetSizeInTokens(
      query,
      base::BindOnce(make_model_input, query, 0).Then(token_count_callback));

  // Get token count for passages, and assign their index + 1 to make
  // ModelInput, in order to reserve index 0 for query.
  for (size_t i = 0; i < passages.size(); ++i) {
    raw_session->GetSizeInTokens(
        passages[i], base::BindOnce(make_model_input, passages[i], i + 1)
                         .Then(token_count_callback));
  }
}

}  // namespace history_embeddings
