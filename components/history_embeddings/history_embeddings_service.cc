// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <algorithm>
#include <numeric>
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
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/core/search_strings_update_listener.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/scheduling_embedder.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"
#include "third_party/farmhash/src/src/farmhash.h"
#include "url/gurl.h"

namespace history_embeddings {

void RecordQueryFiltered(QueryFiltered status) {
  base::UmaHistogramEnumeration("History.Embeddings.QueryFiltered", status,
                                QueryFiltered::ENUM_COUNT);
}

void RecordExtractionCancelled(ExtractionCancelled reason) {
  base::UmaHistogramEnumeration("History.Embeddings.ExtractionCancelled",
                                reason, ExtractionCancelled::ENUM_COUNT);
}

uint32_t HashString(std::string_view str) {
  return util::Fingerprint32(str);
}

void OnGotInnerText(mojo::Remote<blink::mojom::InnerTextAgent> remote,
                    base::TimeTicks start_time,
                    base::OnceCallback<void(std::vector<std::string>)> callback,
                    blink::mojom::InnerTextFramePtr mojo_frame) {
  std::vector<std::string> valid_passages;
  const base::TimeDelta extraction_time = base::TimeTicks::Now() - start_time;
  if (mojo_frame) {
    for (const auto& segment : mojo_frame->segments) {
      if (segment->is_text()) {
        valid_passages.emplace_back(segment->get_text());
      }
    }
    base::UmaHistogramTimes("History.Embeddings.Passages.ExtractionTime",
                            extraction_time);
  }
  // Save passages
  const size_t total_text_size =
      std::accumulate(valid_passages.cbegin(), valid_passages.cend(), 0u,
                      [](size_t acc, const std::string& passage) {
                        return acc + passage.size();
                      });
  base::UmaHistogramCounts1000("History.Embeddings.Passages.PassageCount",
                               valid_passages.size());
  base::UmaHistogramCounts10M("History.Embeddings.Passages.TotalTextSize",
                              total_text_size);
  std::move(callback).Run(std::move(valid_passages));
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
      }
    }
  }
  task_runner->PostTask(FROM_HERE, base::BindOnce(callback, std::move(result)));
}

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

// When `kSearchScoreThreshold` is set <0, the threshold in the model metadata
// will be used. If the metadata also doesn't specify a threshold (old models
// don't), then 0.9 will be used. This allows finch and command line to override
// the threshold if necessary while ensuring different users with different
// models are all using the correct threshold for their model.
float GetScoreThreshold(const EmbedderMetadata& embedder_metadata) {
  if (kSearchScoreThreshold.Get() >= 0)
    return kSearchScoreThreshold.Get();
  if (embedder_metadata.search_score_threshold.has_value())
    return *embedder_metadata.search_score_threshold;
  // 0.9 was the correct threshold for the original model before the threshold
  // was added to the metadata.
  return .9;
}

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
  CHECK(passages_embeddings.url_passages.passages.passages_size() != 0);
  size_t best_index = GetBestScoreIndices(1, 0).front();
  CHECK_LT(best_index,
           static_cast<size_t>(
               passages_embeddings.url_passages.passages.passages_size()));
  return passages_embeddings.url_passages.passages.passages(best_index);
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
        scores[i],
        passages_embeddings.url_embeddings.embeddings[i].GetPassageWordCount(),
        i);
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
    std::unique_ptr<Embedder> embedder,
    std::unique_ptr<Answerer> answerer,
    std::unique_ptr<IntentClassifier> intent_classifier)
    : os_crypt_async_(os_crypt_async),
      history_service_(history_service),
      page_content_annotations_service_(page_content_annotations_service),
      optimization_guide_decider_(optimization_guide_decider),
      embedder_(
          std::make_unique<SchedulingEmbedder>(std::move(embedder),
                                               kScheduledEmbeddingsMax.Get())),
      answerer_(std::move(answerer)),
      intent_classifier_(std::move(intent_classifier)),
      query_id_(0u),
      query_id_weak_ptr_factory_(&query_id_),
      weak_ptr_factory_(this) {
  if (!history_embeddings::IsHistoryEmbeddingsEnabled()) {
    // If the feature flag is disabled, skip initialization. Note we don't also
    // check the pref here, because the pref can change at runtime.
    return;
  }

  // The history service is never nullptr; even unit tests should provide it.
  CHECK(history_service_);
  storage_ = base::SequenceBound<Storage>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      history_service_->history_dir());
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

  // OnEmbedderReady callback needs to be set after the storage_ construction,
  // since the callback could be invoked immediately.
  embedder_->SetOnEmbedderReady(
      base::BindOnce(&HistoryEmbeddingsService::OnEmbedderMetadataReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

HistoryEmbeddingsService::~HistoryEmbeddingsService() = default;

bool HistoryEmbeddingsService::IsEligible(const GURL& url) {
  bool eligible;
  if (!kUseUrlFilter.Get() || !optimization_guide_decider_) {
    eligible = true;
  } else {
    eligible = optimization_guide_decider_->CanApplyOptimization(
                   url, optimization_guide::proto::HISTORY_EMBEDDINGS,
                   /*optimization_metadata=*/nullptr) !=
               optimization_guide::OptimizationGuideDecision::kFalse;
  }

  if (!eligible) {
    callback_for_tests_.Run(UrlPassages(0, 0, base::Time()));
  }

  return eligible;
}

void HistoryEmbeddingsService::OnEmbedderMetadataReady(
    EmbedderMetadata metadata) {
  subscription_ = os_crypt_async_->GetInstance(
      base::BindOnce(&HistoryEmbeddingsService::OnOsCryptAsyncReady,
                     weak_ptr_factory_.GetWeakPtr(), metadata));
}

void HistoryEmbeddingsService::OnOsCryptAsyncReady(
    EmbedderMetadata metadata,
    os_crypt_async::Encryptor encryptor,
    bool success) {
  embedder_metadata_ = metadata;
  storage_.AsyncCall(&Storage::SetEmbedderMetadata)
      .WithArgs(metadata, std::move(encryptor));

  if (kRebuildEmbeddings.Get()) {
    storage_.AsyncCall(&Storage::CollectPassagesWithoutEmbeddings)
        .Then(base::BindOnce(&HistoryEmbeddingsService::RebuildAbsentEmbeddings,
                             weak_ptr_factory_.GetWeakPtr()));
  }
}

void HistoryEmbeddingsService::RetrievePassages(
    history::URLID url_id,
    history::VisitID visit_id,
    base::Time visit_time,
    content::WeakDocumentPtr weak_render_frame_host) {
  content::RenderFrameHost* render_frame_host =
      weak_render_frame_host.AsRenderFrameHostIfValid();
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    RecordExtractionCancelled(ExtractionCancelled::SERVICE_RETRIEVE_PASSAGES);
    return;
  }

  if (kUseDatabaseBeforeEmbedder.Get()) {
    base::Time time_before_database_access = base::Time::Now();
    storage_.AsyncCall(&Storage::GetUrlData)
        .WithArgs(url_id)
        .Then(base::BindOnce(
            &HistoryEmbeddingsService::RetrievePassagesWithUrlData,
            weak_ptr_factory_.GetWeakPtr(), url_id, visit_id, visit_time,
            std::move(weak_render_frame_host), time_before_database_access));
  } else {
    RetrievePassagesWithUrlData(url_id, visit_id, visit_time,
                                std::move(weak_render_frame_host),
                                base::Time::Now(), std::nullopt);
  }
}

SearchResult HistoryEmbeddingsService::Search(
    SearchResult* previous_search_result,
    std::string query,
    std::optional<base::Time> time_range_start,
    size_t count,
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

  result.query = query;
  result.time_range_start = time_range_start;
  result.count = count;

  SearchParams search_params;
  if (QueryIsFiltered(query, search_params)) {
    result.count = 0;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](SearchResultCallback callback, SearchResult result) {
                         callback.Run(std::move(result));
                       },
                       callback, result.Clone()));
    return result;
  }
  search_params.word_match_minimum_embedding_score =
      kWordMatchMinEmbeddingScore.Get();
  search_params.word_match_score_boost_factor =
      kWordMatchScoreBoostFactor.Get();
  search_params.word_match_limit = kWordMatchLimit.Get();
  search_params.word_match_smoothing_factor = kWordMatchSmoothingFactor.Get();

  if (search_params.query_terms.size() >
      static_cast<size_t>(kWordMatchMaxTermCount.Get())) {
    // Disable word match boosting for this long query.
    search_params.query_terms.clear();
  }

  embedder_->ComputePassagesEmbeddings(
      PassageKind::QUERY, {std::move(query)},
      base::BindOnce(&HistoryEmbeddingsService::OnQueryEmbeddingComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(search_params), result.Clone()));
  return result;
}

void HistoryEmbeddingsService::OnQueryEmbeddingComputed(
    SearchResultCallback callback,
    SearchParams search_params,
    SearchResult result,
    std::vector<std::string> query_passages,
    std::vector<Embedding> query_embeddings,
    ComputeEmbeddingsStatus status) {
  bool succeeded = status == ComputeEmbeddingsStatus::SUCCESS;
  base::UmaHistogramBoolean("History.Embeddings.QueryEmbeddingSucceeded",
                            succeeded);

  VLOG(1) << "History.Embeddings.QueryEmbeddingSucceeded: " << succeeded
          << " ; Query: '"
          << (query_passages.empty() ? "(NONE)" : query_passages[0]) << "'";

  if (!succeeded) {
    // Query embedding failed. Just return no search results.
    std::move(callback).Run({});
    return;
  }

  CHECK_EQ(query_embeddings.size(), 1u);

  query_id_++;
  storage_.AsyncCall(&Storage::Search)
      .WithArgs(query_id_weak_ptr_factory_.GetWeakPtr(), query_id_.load(),
                std::move(search_params), std::move(query_embeddings.front()),
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
    optimization_guide::proto::UserFeedback user_feedback,
    std::set<size_t> selections,
    size_t num_entered_characters,
    bool from_omnibox_history_scope) {
  // Exit early if logging is not enabled.
  if (!kSendQualityLog.Get() || !embedder_metadata_.has_value()) {
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
        optimization_guide::HistoryQueryFeatureTypeMap::GetLoggingData(*request)
            ->mutable_quality();
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
        embedder_metadata_.value().model_version);
    query_quality->set_query(result.query);
    query_quality->set_num_days(num_days);
    query_quality->set_num_entered_characters(num_entered_characters);

    // For now, only two UI surfaces are planned, but if more are implemented
    // then we can take the `UiSurface` directly as a parameter.
    query_quality->set_ui_surface(
        from_omnibox_history_scope
            ? optimization_guide::proto::UiSurface::
                  UI_SURFACE_OMNIBOX_HISTORY_SCOPE
            : optimization_guide::proto::UiSurface::UI_SURFACE_HISTORY_PAGE);

    for (size_t row_index = 0; row_index < result.scored_url_rows.size();
         ++row_index) {
      const ScoredUrlRow& scored_url_row = result.scored_url_rows[row_index];
      optimization_guide::proto::DocumentShown* document_shown =
          query_quality->add_top_documents_shown();
      document_shown->set_url(scored_url_row.row.url().spec());
      document_shown->set_was_clicked(selections.contains(row_index));
      if (!scored_url_row.scores.empty()) {
        document_shown->set_best_embedding_score(
            std::ranges::max(scored_url_row.scores));
      }
      document_shown->set_total_document_score(scored_url_row.scored_url.score);

      // Log the top passages that may be used as context for the Answerer.
      for (size_t passage_index : scored_url_row.GetBestScoreIndices(
               0, kContextPassagesMinimumWordCount.Get())) {
        optimization_guide::proto::PassageData* passage_data =
            document_shown->add_passages();
        passage_data->set_text(
            scored_url_row.passages_embeddings.url_passages.passages.passages(
                passage_index));
        passage_data->set_score(scored_url_row.scores[passage_index]);
        const std::vector<float>& embedding =
            scored_url_row.passages_embeddings.url_embeddings
                .embeddings[passage_index]
                .GetData();
        passage_data->mutable_embedding()
            ->mutable_floats()
            ->mutable_values()
            ->Add(embedding.begin(), embedding.end());
      }
    }

    // The data is sent when `log_entry` destructs.
    // `ModelQualityLogEntry::Drop(std::move(log_entry))` would be required to
    // avoid logging if `log_entry` escapes the service, but it only exists
    // within this method so we log proactively by destructing it here.
  }

  // V2 HistoryAnswerLoggingData:
  if (kSendQualityLogV2.Get()) {
    // Take the entry out from the SearchResult so that it will log on
    // destruction at the end of this block.
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
        std::move(result.answerer_result.log_entry);
    if (log_entry) {
      optimization_guide::proto::HistoryAnswerQuality* answer_quality =
          log_entry
              ->quality_data<optimization_guide::HistoryAnswerFeatureTypeMap>();
      if (answer_quality) {
        answer_quality->set_session_id(result.session_id);
        answer_quality->set_url(result.answerer_result.url);
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

HistoryEmbeddingsService::Storage::Storage(const base::FilePath& storage_dir)
    : sql_database(storage_dir) {}

void HistoryEmbeddingsService::Storage::SetEmbedderMetadata(
    EmbedderMetadata metadata,
    os_crypt_async::Encryptor encryptor) {
  sql_database.SetEmbedderMetadata(metadata, std::move(encryptor));
}

void HistoryEmbeddingsService::Storage::ProcessAndStorePassages(
    UrlPassages url_passages,
    std::vector<Embedding> embeddings) {
  UrlPassagesEmbeddings url_data(url_passages.url_id, url_passages.visit_id,
                                 url_passages.visit_time);
  // Construct embeddings, including some information from passages.
  url_data.url_embeddings.embeddings = std::move(embeddings);
  CHECK_EQ(url_passages.passages.passages_size(),
           static_cast<int>(url_data.url_embeddings.embeddings.size()));
  for (int i = 0; i < url_passages.passages.passages_size(); i++) {
    url_data.url_embeddings.embeddings[i].SetPassageWordCount(
        CountWords(url_passages.passages.passages(i)));
  }
  url_data.url_passages = std::move(url_passages);

  // Store all embeddings and passages.
  vector_database.AddUrlData(std::move(url_data));
  vector_database.SaveTo(&sql_database);
}

std::vector<ScoredUrlRow> HistoryEmbeddingsService::Storage::Search(
    base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
    size_t query_id,
    SearchParams search_params,
    Embedding query_embedding,
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
  // ScoredUrlRow results.
  std::vector<ScoredUrlRow> scored_url_rows;
  scored_url_rows.reserve(search_info.scored_urls.size());
  for (ScoredUrl& scored_url : search_info.scored_urls) {
    ScoredUrlRow& scored_url_row =
        scored_url_rows.emplace_back(std::move(scored_url));
    // Since this data was just found, it must exist in the database, so the
    // returned optional must have its value.
    scored_url_row.passages_embeddings =
        sql_database.GetUrlData(scored_url_row.scored_url.url_id).value();
    // Save scores for logging.
    size_t n =
        scored_url_row.passages_embeddings.url_embeddings.embeddings.size();
    scored_url_row.scores.reserve(n);
    for (size_t i = 0; i < n; i++) {
      SearchInfo discard_recount;
      scored_url_row.scores.push_back(query_embedding.ScoreWith(
          scored_url_row.passages_embeddings.url_embeddings.embeddings[i]));
    }
  }

  for (const auto& sr : scored_url_rows) {
    VLOG(3) << "URL: " << sr.row.url().spec()
            << " Score: " << sr.scored_url.score;
    VLOG(3) << "# passages: "
            << sr.passages_embeddings.url_passages.passages.passages_size()
            << " # scores: " << sr.scores.size();
    for (size_t i = 0; i < sr.scores.size(); i++) {
      VLOG(3) << "score: " << sr.scores[i];
      VLOG(3) << "passage: "
              << sr.passages_embeddings.url_passages.passages.passages(i);
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

std::vector<UrlPassages>
HistoryEmbeddingsService::Storage::CollectPassagesWithoutEmbeddings() {
  return sql_database.GetUrlPassagesWithoutEmbeddings();
}

std::optional<UrlPassagesEmbeddings>
HistoryEmbeddingsService::Storage::GetUrlData(history::URLID url_id) {
  base::ScopedUmaHistogramTimer timer(
      "History.Embeddings.DatabaseAsCacheAccessTime.StorageRead");
  return sql_database.GetUrlData(url_id);
}

bool HistoryEmbeddingsService::IsAnswererUseAllowed() const {
  return true;
}

QualityLogEntry HistoryEmbeddingsService::PrepareQualityLogEntry() {
  // This requires some Chrome machinery to upload the log entry, so it's
  // implemented in ChromeHistoryEmbeddingsService.
  return nullptr;
}

void HistoryEmbeddingsService::OnPassagesRetrieved(
    std::optional<UrlPassagesEmbeddings> existing_url_data,
    UrlPassages url_passages,
    std::vector<std::string> passages) {
  VLOG(4) << "All " << passages.size() << " passages for url_id "
          << url_passages.url_id << ":";
  for (size_t i = 0; i < passages.size(); i++) {
    VLOG(4) << i << ": \"" << passages[i] << '"';
  }

  // Move existing passages and associated embeddings into map for quick
  // hash-based lookup instead of many string comparisons.
  std::unordered_map<std::string, Embedding> embedding_cache;
  if (existing_url_data.has_value()) {
    size_t n = existing_url_data.value().url_passages.passages.passages_size();
    // It's possible to get passages but no embeddings if the model version
    // changed and caused embeddings to be deleted, and they're not rebuilt yet.
    if (n == existing_url_data.value().url_embeddings.embeddings.size()) {
      auto passages_iter =
          existing_url_data.value().url_passages.passages.passages().begin();
      auto embeddings_iter =
          existing_url_data.value().url_embeddings.embeddings.begin();
      for (size_t i = 0; i < n; i++) {
        embedding_cache.emplace(std::move(*passages_iter),
                                std::move(*embeddings_iter));
        passages_iter++;
        embeddings_iter++;
      }
    }
  }

  // Check the map for identical passages, which can reuse stored embeddings
  // instead of recomputing them with the embedder. Preserve the structure
  // in `url_passages` and remove already-embedded passages from the `passages`
  // that get sent to the embedder. Then piece them all together in
  // `OnPassagesEmbeddingsComputed` using the cache plus new embeddings.
  for (std::string& passage : passages) {
    if (embedding_cache.contains(passage)) {
      VLOG(5) << "Cached passage: " << passage;
      url_passages.passages.add_passages(std::move(passage));
      passage.clear();
    } else {
      VLOG(5) << "Noncached passage: " << passage;
      url_passages.passages.add_passages(passage);
    }
  }
  size_t old_size = passages.size();
  if (old_size > 0 && kUseDatabaseBeforeEmbedder.Get()) {
    // Erase all the blanks that were cleared by cache check above.
    std::erase(passages, "");
    size_t new_size = passages.size();
    base::UmaHistogramPercentage(
        "History.Embeddings.DatabaseCachedPassageRatio",
        100 * (old_size - new_size) / old_size);
    base::UmaHistogramCounts100(
        "History.Embeddings.DatabaseCachedPassageHitCount",
        old_size - new_size);
    base::UmaHistogramCounts100(
        "History.Embeddings.DatabaseCachedPassageTryCount", old_size);
    for (size_t i = 0; i < old_size; i++) {
      base::UmaHistogramBoolean("History.Embeddings.DatabaseCacheHit",
                                i >= new_size);
    }

    VLOG(4) << "All " << passages.size() << " non-cached passages for url_id "
            << url_passages.url_id << ":";
    for (size_t i = 0; i < passages.size(); i++) {
      VLOG(5) << i << ": \"" << passages[i] << '"';
    }
  }

  embedder_->ComputePassagesEmbeddings(
      PassageKind::PAGE_VISIT_PASSAGE, std::move(passages),
      base::BindOnce(&HistoryEmbeddingsService::OnPassagesEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(embedding_cache),
                     std::move(url_passages)));
}

void HistoryEmbeddingsService::OnPassagesEmbeddingsComputed(
    std::unordered_map<std::string, Embedding> embedding_cache,
    UrlPassages url_passages,
    std::vector<std::string> passages,
    std::vector<Embedding> embeddings,
    ComputeEmbeddingsStatus status) {
  // Merge new and cached embeddings, expanding the `embeddings`
  // vector to fit the passages structure of `url_passages.passages`.
  size_t passages_index = 0;
  size_t embeddings_index = 0;
  for (int i = 0; i < url_passages.passages.passages_size(); i++) {
    const std::string& passage = url_passages.passages.passages(i);
    if (passages_index < passages.size() &&
        passage == passages[passages_index]) {
      // New embedding for non-cached passage; advance both.
      CHECK(!embedding_cache.contains(passage));
      passages_index++;
      embeddings_index++;
    } else {
      // Cached embedding for existing passage; insert and advance on embeddings
      // only.
      auto cached_embedding = embedding_cache.find(passage);
      CHECK(cached_embedding != embedding_cache.end());
      CHECK_EQ(embedder_metadata_->output_size,
               cached_embedding->second.Dimensions());
      embeddings.insert(embeddings.begin() + embeddings_index,
                        cached_embedding->second);
      embeddings_index++;
    }
  }
  CHECK_EQ(passages_index, passages.size());
  CHECK_EQ(embeddings_index, embeddings.size());
  CHECK_EQ(embeddings_index,
           static_cast<size_t>(url_passages.passages.passages_size()));

  storage_.AsyncCall(&Storage::ProcessAndStorePassages)
      .WithArgs(url_passages, std::move(embeddings))
      .Then(base::BindOnce(callback_for_tests_, url_passages));
}

void HistoryEmbeddingsService::OnSearchCompleted(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrlRow> scored_url_rows) {
  std::vector<ScoredUrlRow> filtered;
  filtered.reserve(scored_url_rows.size());
  float threshold = GetScoreThreshold(*embedder_metadata_);
  std::copy_if(std::make_move_iterator(scored_url_rows.begin()),
               std::make_move_iterator(scored_url_rows.end()),
               std::back_inserter(filtered),
               [=](const ScoredUrlRow& scored_url_row) {
                 // This score is the total for the URL, including the
                 // best embedding score plus a holistic word match boost.
                 return scored_url_row.scored_url.score > threshold;
               });
  VLOG(3) << "Search found " << scored_url_rows.size() << " results and kept "
          << filtered.size() << " after score filtering";

  base::UmaHistogramCounts100("History.Embeddings.NumUrlsDiscardedForLowScore",
                              scored_url_rows.size() - filtered.size());

  // The score used for filtering is the scored_url.score but this can exceed
  // the maximum embedding score due to word match boosting across all passages.
  // Detect and log cases that would have been filtered if not for text search.
  for (const ScoredUrlRow& row : filtered) {
    float best_embedding_score = std::ranges::max(row.scores);
    bool sufficient = best_embedding_score > threshold;
    base::UmaHistogramBoolean("History.Embeddings.EmbeddingScoreSufficient",
                              sufficient);
  }

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
          kContentVisibilityThreshold.Get()) {
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
  VLOG(3) << "OnQueryIntentComputed for '" << result.query << "'";
  const bool answerable = status == ComputeIntentStatus::SUCCESS &&
                          query_is_answerable && answerer_ &&
                          IsAnswererUseAllowed();
  base::UmaHistogramBoolean("History.Embeddings.QueryAnswerable", answerable);
  if (!answerable) {
    return;
  }

  // Send a result indicating that an answer generation is being attempted so
  // that the UI can show a loading state.
  SearchResult loadingResult = result.Clone();
  loadingResult.answerer_result =
      AnswererResult(ComputeAnswerStatus::LOADING, result.query,
                     optimization_guide::proto::Answer());
  callback.Run(std::move(loadingResult));

  Answerer::Context context(result.session_id);
  for (size_t url_index = 0;
       url_index <
       std::min(result.scored_url_rows.size(),
                static_cast<size_t>(kMaxAnswererContextUrlCount.Get()));
       url_index++) {
    const ScoredUrlRow& scored_url_row = result.scored_url_rows[url_index];
    std::vector<size_t> best_indices = scored_url_row.GetBestScoreIndices(
        0, kContextPassagesMinimumWordCount.Get());
    std::vector<std::string>& best_passages =
        context.url_passages_map[scored_url_row.row.url().spec()];
    best_passages.reserve(best_indices.size());
    for (size_t index : best_indices) {
      best_passages.push_back(
          scored_url_row.passages_embeddings.url_passages.passages.passages(
              index));
    }
  }
  std::string query = result.query;
  VLOG(3) << "ComputeAnswer for '" << query << "'";
  answerer_->ComputeAnswer(
      std::move(query), std::move(context),
      base::BindOnce(&HistoryEmbeddingsService::OnAnswerComputed,
                     weak_ptr_factory_.GetWeakPtr(), callback,
                     std::move(result)));
}

void HistoryEmbeddingsService::OnAnswerComputed(
    SearchResultCallback callback,
    SearchResult search_result,
    AnswererResult answerer_result) {
  search_result.answerer_result = std::move(answerer_result);
  VLOG(3) << "Query '" << search_result.answerer_result.query
          << "' computed answer '" << search_result.AnswerText() << "'";
  VLOG(3) << "ComputeAnswerStatus: "
          << static_cast<int>(search_result.answerer_result.status);
  callback.Run(std::move(search_result));
}

void HistoryEmbeddingsService::RebuildAbsentEmbeddings(
    std::vector<UrlPassages> all_url_passages) {
  VLOG(3) << "Rebuilding embeddings for " << all_url_passages.size() << " rows";
  for (UrlPassages& url_passages : all_url_passages) {
    std::vector<std::string> passages(url_passages.passages.passages().begin(),
                                      url_passages.passages.passages().end());
    VLOG(3) << "Rebuild scheduled for url_id " << url_passages.url_id
            << " with " << passages.size() << " passages";
    embedder_->ComputePassagesEmbeddings(
        PassageKind::REBUILD_PASSAGE, std::move(passages),
        base::BindOnce(&HistoryEmbeddingsService::OnPassagesEmbeddingsComputed,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::unordered_map<std::string, Embedding>(),
                       std::move(url_passages)));
  }
}

void HistoryEmbeddingsService::RetrievePassagesWithUrlData(
    history::URLID url_id,
    history::VisitID visit_id,
    base::Time visit_time,
    content::WeakDocumentPtr weak_render_frame_host,
    base::Time time_before_database_access,
    std::optional<UrlPassagesEmbeddings> existing_url_data) {
  content::RenderFrameHost* render_frame_host =
      weak_render_frame_host.AsRenderFrameHostIfValid();
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    RecordExtractionCancelled(
        ExtractionCancelled::SERVICE_RETRIEVE_PASSAGES_WITH_URL_DATA);
    return;
  }

  if (kUseDatabaseBeforeEmbedder.Get()) {
    base::TimeDelta database_access_time =
        base::Time::Now() - time_before_database_access;
    base::UmaHistogramTimes(
        "History.Embeddings.DatabaseAsCacheAccessTime.TotalWait",
        database_access_time);
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();
  mojo::Remote<blink::mojom::InnerTextAgent> agent;
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      agent.BindNewPipeAndPassReceiver());
  auto params = blink::mojom::InnerTextParams::New();
  params->max_words_per_aggregate_passage =
      std::max(0, kPassageExtractionMaxWordsPerAggregatePassage.Get());
  params->max_passages = kMaxPassagesPerPage.Get();
  params->min_words_per_passage = kSearchPassageMinimumWordCount.Get();
  auto* agent_ptr = agent.get();
  agent_ptr->GetInnerText(
      std::move(params),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &OnGotInnerText, std::move(agent), start_time,
              base::BindOnce(&HistoryEmbeddingsService::OnPassagesRetrieved,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(existing_url_data),
                             UrlPassages(url_id, visit_id, visit_time))),
          nullptr));
}

bool HistoryEmbeddingsService::QueryIsFiltered(
    const std::string& raw_query,
    SearchParams& search_params) const {
  if (!base::IsStringASCII(raw_query)) {
    RecordQueryFiltered(QueryFiltered::FILTERED_NOT_ASCII);
    return true;
  }
  std::string query = base::ToLowerASCII(raw_query);
  std::vector<std::string_view> query_terms = base::SplitStringPiece(
      query, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (std::string_view& query_term : query_terms) {
    query_term = base::TrimString(
        query_term,
        ".?!,:;-()[]{}<>\"'/\\*&#~@^|%$`+=", base::TrimPositions::TRIM_ALL);
  }
  // Erase any query terms that were trimmed to empty so they don't disrupt
  // the two term pairing logic below.
  std::erase(query_terms, "");
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
  size_t min_term_length = kWordMatchMinTermLength.Get();
  const std::unordered_set<uint32_t>& stop_words_hashes =
      SearchStringsUpdateListener::GetInstance()->stop_words_hashes();
  for (std::string_view term : query_terms) {
    if (query_terms.size() >= min_term_length &&
        !stop_words_hashes.contains(HashString(term))) {
      search_params.query_terms.emplace_back(term);
    }
  }
  return false;
}

}  // namespace history_embeddings
