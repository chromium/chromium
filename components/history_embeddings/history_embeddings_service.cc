// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <algorithm>
#include <tuple>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/core/search_strings_update_listener.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "url/gurl.h"

namespace history_embeddings {

size_t CountWords(const std::string& s) {
  if (s.empty()) {
    return 0;
  }
  size_t word_count = (s[0] == ' ') ? 0 : 1;
  for (size_t i = 1; i < s.length(); i++) {
    if (s[i] != ' ' && s[i - 1] == ' ') {
      word_count++;
    }
  }
  return word_count;
}

namespace {

// This corresponds to UMA histogram enum `EmbeddingsQueryFiltered`
// in tools/metrics/histograms/metadata/history/enums.xml
enum class QueryFiltered {
  NOT_FILTERED,
  FILTERED_NOT_ASCII,
  FILTERED_PHRASE_MATCH,
  FILTERED_TERM_MATCH,
  FILTERED_ONE_WORD_HASH_MATCH,
  FILTERED_TWO_WORD_HASH_MATCH,

  // These enum values are logged in UMA. Do not reuse or skip any values.
  // The order doesn't need to be chronological, but keep identities stable.
  ENUM_COUNT,
};

// Record UMA histogram with query filter status.
void RecordQueryFiltered(QueryFiltered status) {
  base::UmaHistogramEnumeration("History.Embeddings.QueryFiltered", status,
                                QueryFiltered::ENUM_COUNT);
}

void FinishSearchResultWithHistory(
    const scoped_refptr<base::SequencedTaskRunner> task_runner,
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrlRow> scored_url_rows,
    history::HistoryBackend* history_backend,
    history::URLDatabase* url_database) {
  if (url_database) {
    // Move each ScoredUrlRow into the SearchResult with more info from
    // the history database.
    result.scored_url_rows.reserve(scored_url_rows.size());
    for (ScoredUrlRow& scored_url_row : scored_url_rows) {
      result.scored_url_rows.emplace_back(std::move(scored_url_row));
      if (!url_database->GetURLRow(
              result.scored_url_rows.back().scored_url.url_id,
              &result.scored_url_rows.back().row)) {
        // This omission covers an edge case and should generally not happen
        // unless a notification was missed or the history database and
        // history_embeddings database went out of sync. It's theoretically
        // possible since operations across separate databases are not atomic.
        result.scored_url_rows.pop_back();
      } else {
        history_backend->GetIsUrlKnownToSync(
            result.scored_url_rows.back().row.id(),
            &result.scored_url_rows.back().is_url_known_to_sync);
      }
    }
  }
  task_runner->PostTask(FROM_HERE, base::BindOnce(callback, std::move(result)));
}

// When `kSearchScoreThreshold` is set <0, the threshold in the model metadata
// will be used. If the metadata also doesn't specify a threshold (old models
// don't), then 0.9 will be used. This allows finch and command line to override
// the threshold if necessary while ensuring different users with different
// models are all using the correct threshold for their model.
float GetScoreThreshold(
    const passage_embeddings::EmbedderMetadata& embedder_metadata) {
  if (GetFeatureParameters().search_score_threshold >= 0) {
    return GetFeatureParameters().search_score_threshold;
  }
  if (embedder_metadata.search_score_threshold.has_value()) {
    return *embedder_metadata.search_score_threshold;
  }
  // 0.9 was the correct threshold for the original model before the threshold
  // was added to the metadata.
  return 0.9;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

ScoredUrlRow::ScoredUrlRow(ScoredUrl scored_url)
    : scored_url(std::move(scored_url)),
      passages_embeddings(scored_url.url_id,
                          scored_url.visit_id,
                          scored_url.visit_time) {}
ScoredUrlRow::ScoredUrlRow(const ScoredUrlRow&) = default;
ScoredUrlRow::ScoredUrlRow(ScoredUrlRow&&) = default;
ScoredUrlRow::~ScoredUrlRow() = default;
ScoredUrlRow& ScoredUrlRow::operator=(const ScoredUrlRow&) = default;
ScoredUrlRow& ScoredUrlRow::operator=(ScoredUrlRow&&) = default;

std::string ScoredUrlRow::GetBestPassage() const {
  CHECK(passages_embeddings.passages.passages_size() != 0);
  size_t best_index = GetBestScoreIndices(1, 0).front();
  CHECK_LT(best_index,
           static_cast<size_t>(passages_embeddings.passages.passages_size()));
  return passages_embeddings.passages.passages(best_index);
}

std::vector<size_t> ScoredUrlRow::GetBestScoreIndices(
    size_t min_count,
    size_t min_word_count) const {
  using ScoreWordsIndex =
      std::tuple</*score=*/float, /*word_count=*/size_t, /*index=*/size_t>;
  std::vector<ScoreWordsIndex> data;
  data.reserve(scores.size());
  for (size_t i = 0; i < scores.size(); i++) {
    // The word count could be calculated from the passage directly, but
    // since it has already been calculated before, use the value stored
    // with the embedding for efficiency.
    data.emplace_back(
        scores[i], passages_embeddings.embeddings[i].GetPassageWordCount(), i);
  }

  // Sort tuples naturally, descending, so that highest scores come first.
  // Note that if scores are exactly equal, the longer passage is preferred,
  // and the index comes last to break any remaining ties.
  std::sort(data.begin(), data.end(), std::greater());

  size_t word_sum = 0;
  std::vector<size_t> indices;
  indices.reserve(min_count);
  for (const ScoreWordsIndex& item : data) {
    if (indices.size() >= min_count && word_sum >= min_word_count) {
      break;
    }
    indices.push_back(std::get<2>(item));
    word_sum += std::get<1>(item);
  }
  return indices;
}

////////////////////////////////////////////////////////////////////////////////

SearchResult::SearchResult() = default;
SearchResult::SearchResult(SearchResult&&) = default;
SearchResult::~SearchResult() = default;
SearchResult& SearchResult::operator=(SearchResult&&) = default;

SearchResult SearchResult::Clone() {
  // Cannot copy `answerer_result`; it should not have substance.
  CHECK(!answerer_result.log_entry);

  SearchResult clone;
  clone.session_id = session_id;
  clone.query = query;
  clone.time_range_start = time_range_start;
  clone.count = count;
  clone.search_params = search_params;
  clone.scored_url_rows = scored_url_rows;
  return clone;
}

bool SearchResult::IsContinuationOf(const SearchResult& other) {
  return session_id == other.session_id && query == other.query;
}

const std::string& SearchResult::AnswerText() const {
  return answerer_result.answer.text();
}

size_t SearchResult::AnswerIndex() const {
  for (size_t i = 0; i < scored_url_rows.size(); i++) {
    // Note, the spec isn't used because there may be minor differences between
    // the strings, for example "http://other.com" versus "http://other.com/".
    if (scored_url_rows[i].row.url() == GURL(answerer_result.url)) {
      return i;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

HistoryEmbeddingsService::HistoryEmbeddingsService(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    history::HistoryService* history_service,
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* embedder,
    std::unique_ptr<Answerer> answerer,
    std::unique_ptr<IntentClassifier> intent_classifier)
    : os_crypt_async_(os_crypt_async),
      history_service_(history_service),
      page_content_annotations_service_(page_content_annotations_service),
      optimization_guide_decider_(optimization_guide_decider),
      embedder_(embedder),
      answerer_(std::move(answerer)),
      intent_classifier_(std::move(intent_classifier)),
      query_id_weak_ptr_factory_(&query_id_),
      weak_ptr_factory_(this) {
  // The history service is never nullptr; even unit tests should provide it.
  CHECK(history_service_);
  storage_ = base::SequenceBound<Storage>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      history_service_->history_dir(),
      GetFeatureParameters().erase_non_ascii_characters,
      GetFeatureParameters().delete_embeddings);
  history_service_observation_.Observe(history_service_);

  // Notify page content annotations service that we will need the content
  // visibility model during the session.
  if (page_content_annotations_service_) {
    page_content_annotations_service_->RequestAndNotifyWhenModelAvailable(
        page_content_annotations::AnnotationType::kContentVisibility,
        base::DoNothing());
  }

  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::HISTORY_EMBEDDINGS});
  }

  // Observation needs to be set up after the `storage_` construction since the
  // update notification could be invoked immediately.
  if (embedder_metadata_provider) {
    embedder_metadata_observation_.Observe(embedder_metadata_provider);
  }
}

HistoryEmbeddingsService::~HistoryEmbeddingsService() = default;

bool HistoryEmbeddingsService::IsEligible(const GURL& url) {
  bool eligible;
  if (!GetFeatureParameters().use_url_filter || !optimization_guide_decider_) {
    eligible = true;
  } else {
    eligible = optimization_guide_decider_->CanApplyOptimization(
                   url, optimization_guide::proto::HISTORY_EMBEDDINGS,
                   /*optimization_metadata=*/nullptr) !=
               optimization_guide::OptimizationGuideDecision::kFalse;
  }

  if (!eligible) {
    passages_stored_callback_for_tests_.Run(UrlData(0, 0, base::Time()));
  }

  return eligible;
}

void HistoryEmbeddingsService::ComputeAndStorePassageEmbeddings(
    history::URLID url_id,
    history::VisitID visit_id,
    base::Time visit_time,
    std::vector<std::string> passages) {
  GetUrlData(url_id, base::BindOnce(
                         &HistoryEmbeddingsService::
                             ComputeAndStorePassageEmbeddingsWithExistingData,
                         weak_ptr_factory_.GetWeakPtr(),
                         UrlData(url_id, visit_id, visit_time),
                         std::move(passages), base::ElapsedTimer()));
}

void HistoryEmbeddingsService::OnOsCryptAsyncReady(
    os_crypt_async::Encryptor encryptor) {
  storage_.AsyncCall(&Storage::SetEmbedderMetadata)
      .WithArgs(embedder_metadata_, std::move(encryptor));

  if (GetFeatureParameters().rebuild_embeddings) {
    storage_.AsyncCall(&Storage::CollectPassagesWithoutEmbeddings)
        .Then(base::BindOnce(&HistoryEmbeddingsService::RebuildAbsentEmbeddings,
                             weak_ptr_factory_.GetWeakPtr()));
  }
}

SearchResult HistoryEmbeddingsService::Search(
    SearchResult* previous_search_result,
    std::string query,
    std::optional<base::Time> time_range_start,
    size_t count,
    bool skip_answering,
    SearchResultCallback callback) {
  SearchResult result;

  // Create and/or advance a 128-bit base::Token for session_id.
  base::Token token = base::Token::CreateRandom();
  // Start lowest 16-bits sequence number from zero.
  token = base::Token(token.high(), token.low() & ~kSessionIdSequenceBitMask);
  if (previous_search_result && !previous_search_result->session_id.empty()) {
    std::optional<base::Token> parsed =
        base::Token::FromString(previous_search_result->session_id);
    if (parsed.has_value()) {
      token = *parsed;
      // Increment sequence number, allowing any overflow into next higher bits.
      token = base::Token(token.high(), token.low() + 1);
    }
  }
  result.session_id = token.ToString();

  // Note, this is a copy of raw original query, which may or may not include
  // non-ASCII characters. The `query` may later be modified, but not this one.
  result.query = query;
  result.time_range_start = time_range_start;
  result.count = count;

  // Set search parameters, kept within result for caller convenience.
  result.search_params.skip_answering = skip_answering;
  result.search_params.erase_non_ascii_characters =
      GetFeatureParameters().erase_non_ascii_characters;
  result.search_params.word_match_search_non_ascii_passages =
      GetFeatureParameters().word_match_search_non_ascii_passages;
  // TODO(crbug.com/390241271): Move this inside Embedder implementations once
  //  they are no longer wrapped inside the SchedulingEmbedder.
  //  Note that removing the non-ascii characters in the Embedder could result
  //  in a query that contains a non-ascii character to be rejected in
  //  `QueryIsFiltered()` below reducing the chances of the user getting
  //  meaningful results from that query.
  if (result.search_params.erase_non_ascii_characters) {
    EraseNonAsciiCharacters(query);
  }
  result.search_params.word_match_minimum_embedding_score =
      GetFeatureParameters().word_match_min_embedding_score;
  result.search_params.word_match_score_boost_factor =
      GetFeatureParameters().word_match_score_boost_factor;
  result.search_params.word_match_limit =
      GetFeatureParameters().word_match_limit;
  result.search_params.word_match_smoothing_factor =
      GetFeatureParameters().word_match_smoothing_factor;
  result.search_params.word_match_max_term_count =
      GetFeatureParameters().word_match_max_term_count;
  result.search_params.word_match_required_term_ratio =
      GetFeatureParameters().word_match_required_term_ratio;

  if (QueryIsFiltered(query, result.search_params)) {
    result.count = 0;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](SearchResultCallback callback, SearchResult result) {
                         callback.Run(std::move(result));
                       },
                       callback, result.Clone()));
    return result;
  }

  // Try to cancel the embedding task for the previous query, if any.
  if (query_embedding_task_id_) {
    embedder_->TryCancel(*query_embedding_task_id_);
  }

  query_embedding_task_id_ = embedder_->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUserInitiated, {std::move(query)},
      base::BindOnce(&HistoryEmbeddingsService::OnQueryEmbeddingComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     result.Clone()));
  return result;
}

void HistoryEmbeddingsService::OnQueryEmbeddingComputed(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<std::string> query_passages,
    std::vector<passage_embeddings::Embedding> query_embeddings,
    passage_embeddings::Embedder::TaskId task_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  bool succeeded =
      status == passage_embeddings::ComputeEmbeddingsStatus::kSuccess;
  base::UmaHistogramBoolean("History.Embeddings.QueryEmbeddingSucceeded",
                            succeeded);

  VLOG(1) << "History.Embeddings.QueryEmbeddingSucceeded: " << succeeded
          << " ; Query: '"
          << (query_passages.empty() ? "(NONE)" : query_passages[0]) << "'";

  // Ignore the previous query if a new one has been submitted to the embedder.
  if (query_embedding_task_id_ && *query_embedding_task_id_ != task_id) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // Reset the query embedding task ID to avoid attempting to cancel it later.
  query_embedding_task_id_.reset();

  if (!succeeded) {
    std::move(callback).Run(std::move(result));
    return;
  }

  CHECK_EQ(query_embeddings.size(), 1u);

  query_id_++;
  storage_.AsyncCall(&Storage::Search)
      .WithArgs(query_id_weak_ptr_factory_.GetWeakPtr(), query_id_.load(),
                result.search_params, std::move(query_embeddings.front()),
                result.time_range_start, result.count)
      .Then(base::BindOnce(&HistoryEmbeddingsService::OnSearchCompleted,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(result)));
}

base::WeakPtr<HistoryEmbeddingsService> HistoryEmbeddingsService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HistoryEmbeddingsService::SendQualityLog(
    SearchResult& result,
    std::set<size_t> selections,
    size_t num_entered_characters,
    optimization_guide::proto::UserFeedback user_feedback,
    optimization_guide::proto::UiSurface ui_surface) {
  // Exit early if logging is not enabled.
  if (!GetFeatureParameters().send_quality_log ||
      !embedder_metadata_.IsValid()) {
    return;
  }

  // V1 HistoryQueryLoggingData:
  {
    // Prepare log entry and record a histogram for whether it's prepared.
    QualityLogEntry log_entry = PrepareQualityLogEntry();
    base::UmaHistogramBoolean("History.Embeddings.Quality.LogEntryPrepared",
                              !!log_entry);
    if (!log_entry) {
      return;
    }

    optimization_guide::proto::LogAiDataRequest* request =
        log_entry->log_ai_data_request();
    if (!request) {
      return;
    }

    request->mutable_model_execution_info()->set_execution_id(base::StrCat({
        "history-search-embeddings:",
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
    }));

    optimization_guide::proto::HistoryQueryQuality* query_quality =
        request->mutable_history_query()->mutable_quality();
    if (!query_quality) {
      return;
    }

    // Fill the quality proto with data.
    size_t num_days =
        result.time_range_start.has_value()
            ? (base::Time::Now() - result.time_range_start.value()).InDays() + 1
            : 0;
    query_quality->set_session_id(result.session_id);
    query_quality->set_user_feedback(user_feedback);
    query_quality->set_embedding_model_version(
        embedder_metadata_.model_version);
    query_quality->set_query(result.query);
    query_quality->set_num_days(num_days);
    query_quality->set_num_entered_characters(num_entered_characters);
    query_quality->set_ui_surface(ui_surface);

    bool any_document_clicked = false;
    for (size_t row_index = 0; row_index < result.scored_url_rows.size();
         ++row_index) {
      const ScoredUrlRow& scored_url_row = result.scored_url_rows[row_index];
      optimization_guide::proto::DocumentShown* document_shown =
          query_quality->add_top_documents_shown();
      document_shown->set_url(scored_url_row.row.url().spec());
      document_shown->set_was_clicked(selections.contains(row_index));
      any_document_clicked |= document_shown->was_clicked();
      if (!scored_url_row.scores.empty()) {
        document_shown->set_best_embedding_score(
            std::ranges::max(scored_url_row.scores));
      }
      document_shown->set_total_document_score(scored_url_row.scored_url.score);

      // Log the top passages that may be used as context for the Answerer.
      for (size_t passage_index : scored_url_row.GetBestScoreIndices(
               0, GetFeatureParameters().context_passages_minimum_word_count)) {
        optimization_guide::proto::PassageData* passage_data =
            document_shown->add_passages();
        passage_data->set_text(
            scored_url_row.passages_embeddings.passages.passages(
                passage_index));
        passage_data->set_score(scored_url_row.scores[passage_index]);
        const std::vector<float>& embedding =
            scored_url_row.passages_embeddings.embeddings[passage_index]
                .GetData();
        passage_data->mutable_embedding()
            ->mutable_floats()
            ->mutable_values()
            ->Add(embedding.begin(), embedding.end());
      }
    }
    if (result.scored_url_rows.size() > 0) {
      query_quality->set_final_model_status(
          any_document_clicked ? optimization_guide::proto::FinalModelStatus::
                                     FINAL_MODEL_STATUS_SUCCESS
                               : optimization_guide::proto::FinalModelStatus::
                                     FINAL_MODEL_STATUS_FAILURE);
    }

    // The data is sent when `log_entry` destructs.
    // `ModelQualityLogEntry::Drop(std::move(log_entry))` would be required to
    // avoid logging if `log_entry` escapes the service, but it only exists
    // within this method so we log proactively by destructing it here.
  }

  // V2 HistoryAnswerLoggingData:
  if (GetFeatureParameters().send_quality_log_v2) {
    if (result.answerer_result.log_entry) {
      optimization_guide::proto::HistoryAnswerQuality* answer_quality =
          result.answerer_result.log_entry->log_ai_data_request()
              ->mutable_history_answer()
              ->mutable_quality();
      if (answer_quality) {
        answer_quality->set_session_id(result.session_id);
        answer_quality->set_url(result.answerer_result.url);

        // Take the entry out from the SearchResult so that it will log on
        // destruction at the end of this block.
        std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
            std::move(result.answerer_result.log_entry);
      }
    }
  }
}

void HistoryEmbeddingsService::Shutdown() {
  query_id_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  storage_.Reset();
}

void HistoryEmbeddingsService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  storage_.AsyncCall(&Storage::HandleHistoryDeletions)
      .WithArgs(deletion_info.IsAllHistory(), deletion_info.deleted_rows(),
                deletion_info.deleted_visit_ids());
}

void HistoryEmbeddingsService::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  if (embedder_metadata_.IsValid()) {
    // TODO(crbug.com/396684224): Handle runtime model changes. For now the
    //  code expects them to remain constant and only processes metadata once.
    return;
  }
  embedder_metadata_ = metadata;
  os_crypt_async_->GetInstance(
      base::BindOnce(&HistoryEmbeddingsService::OnOsCryptAsyncReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool HistoryEmbeddingsService::IsAnswererUseAllowed() const {
  return true;
}

void HistoryEmbeddingsService::GetUrlData(history::URLID url_id,
                                          UrlDataCallback callback) const {
  storage_.AsyncCall(&Storage::GetUrlData)
      .WithArgs(url_id)
      .Then(std::move(callback));
}

void HistoryEmbeddingsService::GetUrlDataInTimeRange(
    base::Time from_time,
    base::Time to_time,
    size_t limit,
    size_t offset,
    base::OnceCallback<void(std::vector<UrlData>)> callback) const {
  storage_.AsyncCall(&Storage::GetUrlDataInTimeRange)
      .WithArgs(from_time, to_time, limit, offset)
      .Then(std::move(callback));
}

void HistoryEmbeddingsService::DeleteDataForTesting(
    bool delete_passages,
    bool delete_embeddings,
    base::OnceClosure callback) {
  storage_
      .AsyncCall(&history_embeddings::HistoryEmbeddingsService::Storage::
                     DeleteDataForTesting)
      .WithArgs(delete_passages, delete_embeddings)
      .Then(std::move(callback));
}

void HistoryEmbeddingsService::SetPassagesStoredCallbackForTesting(
    PassagesStoredCallback callback) {
  passages_stored_callback_for_tests_ = std::move(callback);
}

HistoryEmbeddingsService::Storage::Storage(const base::FilePath& storage_dir,
                                           bool erase_non_ascii_characters,
                                           bool delete_embeddings)
    : sql_database(storage_dir, erase_non_ascii_characters, delete_embeddings) {
}

void HistoryEmbeddingsService::Storage::SetEmbedderMetadata(
    passage_embeddings::EmbedderMetadata metadata,
    os_crypt_async::Encryptor encryptor) {
  sql_database.SetEmbedderMetadata(metadata, std::move(encryptor));
}

void HistoryEmbeddingsService::Storage::ProcessAndStorePassages(
    UrlData url_data) {
  CHECK_EQ(url_data.passages.passages_size(),
           static_cast<int>(url_data.embeddings.size()));
  for (int i = 0; i < url_data.passages.passages_size(); i++) {
    url_data.embeddings[i].SetPassageWordCount(
        CountWords(url_data.passages.passages(i)));
  }

  // Store all embeddings and passages.
  vector_database.AddUrlData(std::move(url_data));
  vector_database.SaveTo(&sql_database);
}

std::vector<ScoredUrlRow> HistoryEmbeddingsService::Storage::Search(
    base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
    size_t query_id,
    SearchParams search_params,
    passage_embeddings::Embedding query_embedding,
    std::optional<base::Time> time_range_start,
    size_t count) {
  base::ElapsedTimer timer;
  SearchInfo search_info = sql_database.FindNearest(
      time_range_start, count, search_params, query_embedding,
      base::BindRepeating(
          [](base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
             size_t query_id) {
            // If the service shut down or started a new query, this one is no
            // longer needed. Signal to exit early. Best result so far will be
            // returned.
            return !weak_latest_query_id || *weak_latest_query_id != query_id;
          },
          std::move(weak_latest_query_id), query_id));
  const base::TimeDelta elapsed = timer.Elapsed();
  base::UmaHistogramTimes("History.Embeddings.Search.Duration", elapsed);
  base::UmaHistogramCounts1M("History.Embeddings.Search.UrlCount",
                             search_info.searched_url_count);
  base::UmaHistogramCounts10M("History.Embeddings.Search.EmbeddingCount",
                              search_info.searched_embedding_count);
  base::UmaHistogramCounts10M(
      "History.Embeddings.Search.SkippedNonAsciiPassageCount",
      search_info.skipped_nonascii_passage_count);
  base::UmaHistogramCounts10M(
      "History.Embeddings.Search.ModifiedNonAsciiPassageCount",
      search_info.modified_nonascii_passage_count);
  base::UmaHistogramBoolean("History.Embeddings.Search.Completed",
                            search_info.completed);
  base::UmaHistogramTimes("History.Embeddings.Search.TotalSearchTime",
                          search_info.total_search_time);
  base::UmaHistogramTimes("History.Embeddings.Search.ScoringTime",
                          search_info.scoring_time);
  base::UmaHistogramTimes("History.Embeddings.Search.PassageScanningTime",
                          search_info.passage_scanning_time);

  VLOG(1) << "History.Embeddings.Search.Duration (ms): "
          << elapsed.InMilliseconds()
          << " ; .UrlCount: " << search_info.searched_url_count
          << " ; .EmbeddingCount: " << search_info.searched_embedding_count
          << " ; .SkippedNonAsciiPassageCount: "
          << search_info.skipped_nonascii_passage_count
          << " ; .Completed: " << search_info.completed;

  // Populate source passages and embeddings to fill out more complete
  // ScoredUrlRow results. Total score top results are first, followed by
  // word match score top results.
  std::vector<ScoredUrlRow> scored_url_rows;
  scored_url_rows.reserve(search_info.scored_urls.size() +
                          search_info.word_match_scored_urls.size());
  auto expand = [&](ScoredUrl& scored_url) {
    ScoredUrlRow& scored_url_row =
        scored_url_rows.emplace_back(std::move(scored_url));
    // Since this data was just found, it must exist in the database, so the
    // returned optional must have its value.
    scored_url_row.passages_embeddings =
        sql_database.GetUrlData(scored_url_row.scored_url.url_id).value();
    // Save scores for logging.
    size_t n = scored_url_row.passages_embeddings.embeddings.size();
    scored_url_row.scores.reserve(n);
    for (size_t i = 0; i < n; i++) {
      SearchInfo discard_recount;
      scored_url_row.scores.push_back(query_embedding.ScoreWith(
          scored_url_row.passages_embeddings.embeddings[i]));
    }
  };
  for (ScoredUrl& scored_url : search_info.scored_urls) {
    expand(scored_url);
  }
  for (ScoredUrl& scored_url : search_info.word_match_scored_urls) {
    if (!std::ranges::any_of(scored_url_rows, [&](const ScoredUrlRow& row) {
          return row.scored_url.url_id == scored_url.url_id;
        })) {
      expand(scored_url);
    }
  }

  for (const auto& sr : scored_url_rows) {
    VLOG(3) << "URL: " << sr.row.url().spec()
            << " score: " << sr.scored_url.score
            << " ; word_match_score: " << sr.scored_url.word_match_score;
    VLOG(3) << "# passages: " << sr.passages_embeddings.passages.passages_size()
            << " # scores: " << sr.scores.size();
    for (size_t i = 0; i < sr.scores.size(); i++) {
      VLOG(3) << "embedding similarity score: " << sr.scores[i];
      VLOG(3) << "passage: " << sr.passages_embeddings.passages.passages(i);
    }
  }

  return scored_url_rows;
}

void HistoryEmbeddingsService::Storage::HandleHistoryDeletions(
    bool for_all_history,
    history::URLRows deleted_rows,
    std::set<history::VisitID> deleted_visit_ids) {
  if (for_all_history) {
    sql_database.DeleteAllData(true, true);
    return;
  }

  for (history::URLRow url_row : deleted_rows) {
    sql_database.DeleteDataForUrlId(url_row.id());
  }

  for (history::VisitID visit_id : deleted_visit_ids) {
    sql_database.DeleteDataForVisitId(visit_id);
  }
}

void HistoryEmbeddingsService::Storage::DeleteDataForTesting(
    bool delete_passages,
    bool delete_embeddings) {
  sql_database.DeleteAllData(delete_passages, delete_embeddings);
}

std::vector<UrlData>
HistoryEmbeddingsService::Storage::CollectPassagesWithoutEmbeddings() {
  return sql_database.GetUrlPassagesWithoutEmbeddings();
}

std::optional<UrlData> HistoryEmbeddingsService::Storage::GetUrlData(
    history::URLID url_id) {
  base::ScopedUmaHistogramTimer timer(
      "History.Embeddings.DatabaseAsCacheAccessTime.StorageRead");
  return sql_database.GetUrlData(url_id);
}

std::vector<UrlData> HistoryEmbeddingsService::Storage::GetUrlDataInTimeRange(
    base::Time from_time,
    base::Time to_time,
    size_t limit,
    size_t offset) {
  return sql_database.GetUrlDataInTimeRange(from_time, to_time, limit, offset);
}

QualityLogEntry HistoryEmbeddingsService::PrepareQualityLogEntry() {
  // This requires some Chrome machinery to upload the log entry, so it's
  // implemented in ChromeHistoryEmbeddingsService.
  return nullptr;
}

void HistoryEmbeddingsService::ComputeAndStorePassageEmbeddingsWithExistingData(
    UrlData url_data,
    std::vector<std::string> passages,
    base::ElapsedTimer database_access_timer,
    std::optional<UrlData> existing_url_data) {
  VLOG(4) << "All " << passages.size() << " passages for url_id "
          << url_data.url_id << ":";
  for (size_t i = 0; i < passages.size(); i++) {
    VLOG(4) << i << ": \"" << passages[i] << '"';
  }

  base::UmaHistogramTimes(
      "History.Embeddings.DatabaseAsCacheAccessTime.TotalWait",
      database_access_timer.Elapsed());

  // Move existing passages and associated embeddings into map for quick
  // hash-based lookup instead of many string comparisons.
  std::unordered_map<std::string, passage_embeddings::Embedding>
      embedding_cache;
  if (existing_url_data.has_value()) {
    size_t passages_size = existing_url_data->passages.passages_size();
    // It's possible to get passages but no embeddings if the model version
    // changed and caused embeddings to be deleted, and they're not rebuilt yet.
    if (passages_size == existing_url_data->embeddings.size()) {
      auto passages_iter = existing_url_data->passages.passages().begin();
      auto embeddings_iter = existing_url_data->embeddings.begin();
      for (size_t i = 0; i < passages_size; i++) {
        embedding_cache.emplace(std::move(*passages_iter),
                                std::move(*embeddings_iter));
        passages_iter++;
        embeddings_iter++;
      }
    }
  }

  // Check the map for identical passages, which can reuse stored embeddings
  // instead of recomputing them with the embedder. Preserve the structure
  // in `url_data` and move any passages that still need embedding to
  // `noncached_passages`. The missing embeddings will be filled in
  // with the computed embeddings in `OnPassagesEmbeddingsComputed()`.
  std::vector<std::string> noncached_passages;
  noncached_passages.reserve(passages.size());
  for (std::string& passage : passages) {
    if (embedding_cache.contains(passage)) {
      VLOG(6) << "Cached passage: " << passage;
      // Reuse the embeddings from the cache.
      url_data.embeddings.emplace_back(embedding_cache[passage]);
    } else {
      VLOG(6) << "Noncached passage: " << passage;
      // Reserve room for the embeddings to be filled in once computed.
      url_data.embeddings.emplace_back(std::vector<float>{});
      noncached_passages.push_back(passage);
    }
    url_data.passages.add_passages(std::move(passage));
  }

  if (passages.size() > 0) {
    base::UmaHistogramPercentage(
        "History.Embeddings.DatabaseCachedPassageRatio",
        100 * (passages.size() - noncached_passages.size()) / passages.size());
    base::UmaHistogramCounts100(
        "History.Embeddings.DatabaseCachedPassageHitCount",
        passages.size() - noncached_passages.size());
    base::UmaHistogramCounts100(
        "History.Embeddings.DatabaseCachedPassageTryCount", passages.size());
    for (size_t i = 0; i < passages.size(); i++) {
      base::UmaHistogramBoolean("History.Embeddings.DatabaseCacheHit",
                                i >= noncached_passages.size());
    }
  }

  VLOG(4) << "All " << noncached_passages.size()
          << " noncached passages for url_id " << url_data.url_id << ":";
  for (size_t i = 0; i < noncached_passages.size(); i++) {
    VLOG(5) << i << ": \"" << noncached_passages[i] << '"';
  }

  // TODO(crbug.com/390241271): Move this inside Embedder implementations once
  //  they are no longer wrapped inside the SchedulingEmbedder.
  if (GetFeatureParameters().erase_non_ascii_characters) {
    EraseNonAsciiCharacters(noncached_passages);
  }
  embedder_->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kPassive,
      std::move(noncached_passages),
      base::BindOnce(&HistoryEmbeddingsService::OnPassagesEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_data)));
}

void HistoryEmbeddingsService::OnPassagesEmbeddingsComputed(
    UrlData url_passages,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    passage_embeddings::Embedder::TaskId task_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
    return;
  }

  // Merge the new and the existing embeddings.
  size_t embeddings_index = 0;
  for (auto& embedding : url_passages.embeddings) {
    if (embedding.Dimensions() == 0) {
      embedding = embeddings[embeddings_index++];
    }
  }
  // Make sure all the new embeddings are accounted for.
  CHECK_EQ(embeddings_index, embeddings.size());

  storage_.AsyncCall(&Storage::ProcessAndStorePassages)
      .WithArgs(url_passages)
      .Then(base::BindOnce(passages_stored_callback_for_tests_, url_passages));
}

void HistoryEmbeddingsService::OnSearchCompleted(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrlRow> scored_url_rows) {
  std::vector<ScoredUrlRow> filtered;
  filtered.reserve(scored_url_rows.size());
  float score_threshold = GetScoreThreshold(embedder_metadata_);
  float word_match_score_threshold =
      GetFeatureParameters().search_word_match_score_threshold;
  std::copy_if(std::make_move_iterator(scored_url_rows.begin()),
               std::make_move_iterator(scored_url_rows.end()),
               std::back_inserter(filtered),
               [=](const ScoredUrlRow& scored_url_row) {
                 // The `score` is the total for the URL, including the
                 // best embedding score plus a holistic word match boost.
                 // The `word_match_score` is just the boost part, and a
                 // result item could be included after primary results
                 // if it exceeds a different threshold for word match.
                 return scored_url_row.scored_url.score > score_threshold ||
                        scored_url_row.scored_url.word_match_score >
                            word_match_score_threshold;
               });

  base::UmaHistogramCounts100("History.Embeddings.NumUrlsDiscardedForLowScore",
                              scored_url_rows.size() - filtered.size());

  auto is_kept_by_word_match = [=](const ScoredUrlRow& scored_url_row) {
    return !(scored_url_row.scored_url.score > score_threshold);
  };
  size_t num_added_by_word_match =
      std::ranges::count_if(filtered, is_kept_by_word_match);
  base::UmaHistogramCounts100("History.Embeddings.NumUrlsAddedByWordMatch",
                              num_added_by_word_match);

  // Trim final result set to not exceed requested `count`.
  while (filtered.size() > result.count) {
    filtered.pop_back();
  }

  size_t num_kept_by_word_match =
      std::ranges::count_if(filtered, is_kept_by_word_match);
  base::UmaHistogramCounts100("History.Embeddings.NumUrlsKeptByWordMatch",
                              num_kept_by_word_match);

  // The score used for filtering is the scored_url.score but this can exceed
  // the maximum embedding score due to word match boosting across all passages.
  // Detect and log cases that would have been filtered if not for text search.
  for (const ScoredUrlRow& row : filtered) {
    float best_embedding_score = std::ranges::max(row.scores);
    bool sufficient = best_embedding_score > score_threshold;
    base::UmaHistogramBoolean("History.Embeddings.EmbeddingScoreSufficient",
                              sufficient);
  }

  VLOG(3) << "Search found " << scored_url_rows.size() << " results, leaving "
          << filtered.size() << " after all filtering, with "
          << num_added_by_word_match << " added by word match and "
          << num_kept_by_word_match << " kept by word match after capping";

  DeterminePassageVisibility(std::move(callback), std::move(result),
                             std::move(filtered));
}

void HistoryEmbeddingsService::DeterminePassageVisibility(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrlRow> scored_url_rows) {
  bool is_visibility_model_available =
      page_content_annotations_service_ &&
      page_content_annotations_service_->GetModelInfoForType(
          page_content_annotations::AnnotationType::kContentVisibility);
  base::UmaHistogramCounts100("History.Embeddings.NumUrlsMatched",
                              scored_url_rows.size());
  base::UmaHistogramBoolean(
      "History.Embeddings.VisibilityModelAvailableAtQuery",
      is_visibility_model_available);

  if (!is_visibility_model_available || scored_url_rows.empty()) {
    OnPassageVisibilityCalculated(std::move(callback), std::move(result),
                                  std::move(scored_url_rows), {});
    return;
  }

  std::vector<std::string> inputs;
  inputs.reserve(scored_url_rows.size());
  for (const ScoredUrlRow& url_row : scored_url_rows) {
    inputs.emplace_back(url_row.GetBestPassage());
  }
  page_content_annotations_service_->BatchAnnotate(
      base::BindOnce(&HistoryEmbeddingsService::OnPassageVisibilityCalculated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(result), std::move(scored_url_rows)),
      std::move(inputs),
      page_content_annotations::AnnotationType::kContentVisibility);
}

void HistoryEmbeddingsService::OnPassageVisibilityCalculated(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrlRow> scored_url_rows,
    const std::vector<page_content_annotations::BatchAnnotationResult>&
        annotation_results) {
  if (annotation_results.empty()) {
    scored_url_rows.clear();
  } else {
    CHECK_EQ(scored_url_rows.size(), annotation_results.size());

    // Filter for scored URLs that are ok to be shown to the user.
    auto url_rows_it = scored_url_rows.begin();
    for (const page_content_annotations::BatchAnnotationResult&
             annotation_result : annotation_results) {
      // Note, if threshold is configured at exactly zero then it's
      // intentionally allowing everything through.
      if (annotation_result.visibility_score().value_or(0.0) <
          GetFeatureParameters().content_visibility_threshold) {
        url_rows_it = scored_url_rows.erase(url_rows_it);
      } else {
        ++url_rows_it;
      }
    }
  }

  base::UmaHistogramCounts100("History.Embeddings.NumMatchedUrlsVisible",
                              scored_url_rows.size());

  if (scored_url_rows.empty()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  history_service_->ScheduleDBTaskForUI(base::BindOnce(
      &FinishSearchResultWithHistory,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&HistoryEmbeddingsService::OnPrimarySearchResultReady,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      std::move(result), std::move(scored_url_rows)));
}

void HistoryEmbeddingsService::OnPrimarySearchResultReady(
    SearchResultCallback callback,
    SearchResult result) {
  callback.Run(result.Clone());

  // Do no intent classification or answering if `Search` caller requested
  // to `skip_answering`.
  if (result.search_params.skip_answering) {
    return;
  }

  // TODO(b/369446266): Intent classification can execute in parallel with
  //  initial query embedding computation and search. This doesn't make
  //  much difference when the mock is used but could save time when the
  //  real ML intent classifier is working.
  if (answerer_ && intent_classifier_ && IsAnswererUseAllowed()) {
    std::string query = result.query;
    VLOG(3) << "ComputeQueryIntent for '" << query << "'";
    intent_classifier_->ComputeQueryIntent(
        std::move(query),
        base::BindOnce(&HistoryEmbeddingsService::OnQueryIntentComputed,
                       weak_ptr_factory_.GetWeakPtr(), callback,
                       std::move(result)));
  } else {
    // Intent classification is explicitly disabled; bypass to answerer.
    OnQueryIntentComputed(callback, std::move(result),
                          ComputeIntentStatus::SUCCESS,
                          /*query_is_answerable=*/true);
  }
}

void HistoryEmbeddingsService::OnQueryIntentComputed(
    SearchResultCallback callback,
    SearchResult result,
    ComputeIntentStatus status,
    bool query_is_answerable) {
  const bool answerable = status == ComputeIntentStatus::SUCCESS &&
                          query_is_answerable && answerer_ &&
                          IsAnswererUseAllowed();
  VLOG(3) << "OnQueryIntentComputed for '" << result.query << "' ("
          << query_is_answerable << "," << answerable << ")";
  VLOG(3) << "ComputeIntentStatus: " << static_cast<int>(status);
  base::UmaHistogramBoolean("History.Embeddings.QueryAnswerable", answerable);
  if (!answerable) {
    return;
  }

  // Send a result indicating that an answer generation is being attempted so
  // that the UI can show a loading state.
  SearchResult loadingResult = result.Clone();
  loadingResult.answerer_result =
      AnswererResult(ComputeAnswerStatus::kLoading, result.query,
                     optimization_guide::proto::Answer());
  callback.Run(std::move(loadingResult));

  Answerer::Context context(result.session_id);
  for (size_t url_index = 0;
       url_index <
       std::min(result.scored_url_rows.size(),
                static_cast<size_t>(
                    GetFeatureParameters().max_answerer_context_url_count));
       url_index++) {
    const ScoredUrlRow& scored_url_row = result.scored_url_rows[url_index];
    std::vector<size_t> best_indices = scored_url_row.GetBestScoreIndices(
        0, GetFeatureParameters().context_passages_minimum_word_count);
    std::vector<std::string>& best_passages =
        context.url_passages_map[scored_url_row.row.url().spec()];
    best_passages.reserve(best_indices.size());
    for (size_t index : best_indices) {
      best_passages.push_back(
          scored_url_row.passages_embeddings.passages.passages(index));
    }
  }
  std::string query = result.query;
  VLOG(3) << "ComputeAnswer for '" << query << "'";
  answerer_->ComputeAnswer(
      std::move(query), std::move(context),
      base::BindOnce(&HistoryEmbeddingsService::OnAnswerComputed,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(),
                     callback, std::move(result)));
}

void HistoryEmbeddingsService::OnAnswerComputed(
    base::Time start_time,
    SearchResultCallback callback,
    SearchResult search_result,
    AnswererResult answerer_result) {
  base::TimeDelta waited = base::Time::Now() - start_time;
  search_result.answerer_result = std::move(answerer_result);
  VLOG(3) << "Query '" << search_result.answerer_result.query
          << "' computed answer '" << search_result.AnswerText() << "'";
  VLOG(3) << "ComputeAnswerStatus: "
          << static_cast<int>(search_result.answerer_result.status) << " ("
          << waited.InMilliseconds() << " ms)";

  base::UmaHistogramEnumeration("History.Embeddings.ComputeAnswerStatus",
                                answerer_result.status);
  const std::string compute_answer_time_histogram_name =
      "History.Embeddings.ComputeAnswerTime";
  base::UmaHistogramTimes(compute_answer_time_histogram_name, waited);
  switch (answerer_result.status) {
    case ComputeAnswerStatus::kLoading:
      base::UmaHistogramTimes(compute_answer_time_histogram_name + ".Loading",
                              waited);
      break;
    case ComputeAnswerStatus::kSuccess:
      base::UmaHistogramTimes(compute_answer_time_histogram_name + ".Success",
                              waited);
      break;
    case ComputeAnswerStatus::kUnanswerable:
      base::UmaHistogramTimes(
          compute_answer_time_histogram_name + ".Unanswerable", waited);
      break;
    case ComputeAnswerStatus::kModelUnavailable:
      base::UmaHistogramTimes(
          compute_answer_time_histogram_name + ".ModelUnavailable", waited);
      break;
    case ComputeAnswerStatus::kExecutionFailure:
      base::UmaHistogramTimes(
          compute_answer_time_histogram_name + ".ExecutionFailure", waited);
      break;
    case ComputeAnswerStatus::kExecutionCancelled:
      base::UmaHistogramTimes(
          compute_answer_time_histogram_name + ".ExecutionCancelled", waited);
      break;
    case ComputeAnswerStatus::kFiltered:
      base::UmaHistogramTimes(compute_answer_time_histogram_name + ".Filtered",
                              waited);
      break;
    case ComputeAnswerStatus::kUnspecified:
      break;
  }

  callback.Run(std::move(search_result));
}

void HistoryEmbeddingsService::RebuildAbsentEmbeddings(
    std::vector<UrlData> all_url_passages) {
  VLOG(3) << "Rebuilding embeddings for " << all_url_passages.size() << " rows";
  for (UrlData& url_passages : all_url_passages) {
    std::vector<std::string> passages(url_passages.passages.passages().begin(),
                                      url_passages.passages.passages().end());
    VLOG(3) << "Rebuild scheduled for url_id " << url_passages.url_id
            << " with " << passages.size() << " passages";

    // Reserve room for the embeddings to be filled in once computed.
    url_passages.embeddings = std::vector<passage_embeddings::Embedding>(
        url_passages.passages.passages_size(),
        passage_embeddings::Embedding(std::vector<float>{}));

    // TODO(crbug.com/390241271): Move this inside Embedder implementations once
    //  they are no longer wrapped inside the SchedulingEmbedder.
    if (GetFeatureParameters().erase_non_ascii_characters) {
      EraseNonAsciiCharacters(passages);
    }
    embedder_->ComputePassagesEmbeddings(
        passage_embeddings::PassagePriority::kLatent, std::move(passages),
        base::BindOnce(&HistoryEmbeddingsService::OnPassagesEmbeddingsComputed,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(url_passages)));
  }
}

bool HistoryEmbeddingsService::QueryIsFiltered(
    const std::string& raw_query,
    SearchParams& search_params) const {
  if (!base::IsStringASCII(raw_query)) {
    RecordQueryFiltered(QueryFiltered::FILTERED_NOT_ASCII);
    return true;
  }
  const std::unordered_set<uint32_t>& stop_words_hashes =
      SearchStringsUpdateListener::GetInstance()->stop_words_hashes();
  size_t min_term_length = GetFeatureParameters().word_match_min_term_length;
  std::vector<std::string> query_terms =
      SplitQueryToTerms(stop_words_hashes, raw_query, min_term_length);
  const std::unordered_set<uint32_t>& filter_words_hashes =
      SearchStringsUpdateListener::GetInstance()->filter_words_hashes();
  if (std::ranges::any_of(query_terms, [&](std::string_view query_term) {
        uint32_t hash = HashString(query_term);
        return filter_words_hashes.contains(hash);
      })) {
    RecordQueryFiltered(QueryFiltered::FILTERED_ONE_WORD_HASH_MATCH);
    return true;
  }
  for (size_t i = 1; i < query_terms.size(); i++) {
    std::string two_terms =
        base::StrCat({query_terms[i - 1], " ", query_terms[i]});
    uint32_t hash = HashString(two_terms);
    if (filter_words_hashes.contains(hash)) {
      RecordQueryFiltered(QueryFiltered::FILTERED_TWO_WORD_HASH_MATCH);
      return true;
    }
  }
  RecordQueryFiltered(QueryFiltered::NOT_FILTERED);
  search_params.query_terms = std::move(query_terms);
  return false;
}

}  // namespace history_embeddings
