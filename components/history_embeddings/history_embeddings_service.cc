// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <numeric>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/ml_answerer.h"
#include "components/history_embeddings/ml_embedder.h"
#include "components/history_embeddings/mock_answerer.h"
#include "components/history_embeddings/mock_embedder.h"
#include "components/history_embeddings/scheduling_embedder.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"

namespace history_embeddings {

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

// This is run on the HistoryService's worker thread to access the full URL
// database and finish the results for a completed embeddings search.
// Finished results are then sent to the given callback using the task_runner.
void FinishSearchResultWithHistory(
    const scoped_refptr<base::SequencedTaskRunner> task_runner,
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrl> scored_urls,
    history::HistoryBackend* history_backend,
    history::URLDatabase* url_database) {
  if (url_database) {
    // Move each ScoredUrl into a more complete ScoredUrlRow with more info from
    // the history database.
    result.scored_url_rows.reserve(scored_urls.size());
    for (ScoredUrl& scored_url : scored_urls) {
      result.scored_url_rows.emplace_back(std::move(scored_url));
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
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(result)));
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

////////////////////////////////////////////////////////////////////////////////

SearchResult::SearchResult() = default;
SearchResult::SearchResult(const SearchResult&) = default;
SearchResult::SearchResult(SearchResult&&) = default;
SearchResult::~SearchResult() = default;
SearchResult& SearchResult::operator=(const SearchResult&) = default;
SearchResult& SearchResult::operator=(SearchResult&&) = default;

////////////////////////////////////////////////////////////////////////////////

HistoryEmbeddingsService::HistoryEmbeddingsService(
    history::HistoryService* history_service,
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service,
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_model_provider,
    PassageEmbeddingsServiceController* service_controller)
    : history_service_(history_service),
      page_content_annotations_service_(page_content_annotations_service),
      query_id_(0u),
      query_id_weak_ptr_factory_(&query_id_),
      weak_ptr_factory_(this) {
  if (!history_embeddings::IsHistoryEmbeddingEnabled()) {
    // If the feature flag is disabled, skip initialization. Note we don't also
    // check the pref here, because the pref can change at runtime.
    return;
  }

  CHECK(history_service_);
  history_service_observation_.Observe(history_service_);

  // Notify page content annotations service that we will need the content
  // visibility model during the session.
  if (page_content_annotations_service_) {
    page_content_annotations_service_->RequestAndNotifyWhenModelAvailable(
        page_content_annotations::AnnotationType::kContentVisibility,
        base::DoNothing());
  }

  if (kUseMlEmbedder.Get()) {
    embedder_ = std::make_unique<MlEmbedder>(optimization_guide_model_provider,
                                             service_controller);
  } else {
    embedder_ = std::make_unique<MockEmbedder>();
  }

  embedder_ = std::make_unique<SchedulingEmbedder>(
      std::move(embedder_), kScheduledEmbeddingsMax.Get());

  if (kUseMlAnswerer.Get()) {
    answerer_ = std::make_unique<MlAnswerer>();
  } else {
    answerer_ = std::make_unique<MockAnswerer>();
  }

  storage_ = base::SequenceBound<Storage>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      history_service_->history_dir());

  // OnEmbedderReady callback needs to be set after the storage_ construction,
  // since the callback could be invoked immediately.
  embedder_->SetOnEmbedderReady(
      base::BindOnce(&HistoryEmbeddingsService::OnEmbedderMetadataReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

HistoryEmbeddingsService::~HistoryEmbeddingsService() = default;

void HistoryEmbeddingsService::OnEmbedderMetadataReady(
    EmbedderMetadata metadata) {
  embedder_metadata_ = metadata;
  storage_.AsyncCall(&Storage::SetEmbedderMetadata).WithArgs(metadata);

  if (kRebuildEmbeddings.Get()) {
    storage_.AsyncCall(&Storage::CollectPassagesWithoutEmbeddings)
        .Then(base::BindOnce(&HistoryEmbeddingsService::RebuildAbsentEmbeddings,
                             weak_ptr_factory_.GetWeakPtr()));
  }
}

void HistoryEmbeddingsService::RetrievePassages(
    const history::VisitRow& visit_row,
    content::WeakDocumentPtr weak_render_frame_host) {
  content::RenderFrameHost* render_frame_host =
      weak_render_frame_host.AsRenderFrameHostIfValid();
  if (!render_frame_host) {
    return;
  }

  if (!render_frame_host->IsRenderFrameLive()) {
    return;
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
                             UrlPassages(visit_row.url_id, visit_row.visit_id,
                                         visit_row.visit_time))),
          nullptr));
}

void HistoryEmbeddingsService::Search(
    std::string query,
    std::optional<base::Time> time_range_start,
    size_t count,
    SearchResultCallback callback) {
  SearchResult result;
  result.query = query;
  result.time_range_start = time_range_start;
  result.count = count;
  embedder_->ComputePassagesEmbeddings(
      PassageKind::QUERY, {std::move(query)},
      base::BindOnce(&HistoryEmbeddingsService::OnQueryEmbeddingComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(result)));
}

void HistoryEmbeddingsService::OnQueryEmbeddingComputed(
    SearchResultCallback callback,
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
                std::move(query_embeddings.front()), result.time_range_start,
                result.count)
      .Then(base::BindOnce(&HistoryEmbeddingsService::OnSearchCompleted,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(result)));
}

base::WeakPtr<HistoryEmbeddingsService> HistoryEmbeddingsService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HistoryEmbeddingsService::SendQualityLog(const SearchResult& result,
                                              size_t selection,
                                              size_t num_entered_characters,
                                              bool from_omnibox_history_scope) {
  // TODO: b/343548121 - Decide on semantics and fill num_days using
  //  `result.time_range_start`.
  size_t num_days = 0;

  // Exit early if logging is not enabled.
  if (!kSendQualityLog.Get() || !embedder_metadata_.has_value()) {
    return;
  }

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
  optimization_guide::proto::HistoryQueryQuality* quality_proto =
      optimization_guide::HistoryQueryFeatureTypeMap::GetLoggingData(*request)
          ->mutable_quality();
  if (!quality_proto) {
    return;
  }

  // Fill the quality proto with data.
  quality_proto->set_embedding_model_version(
      embedder_metadata_.value().model_version);
  quality_proto->set_query(result.query);
  quality_proto->set_num_days(num_days);
  quality_proto->set_num_entered_characters(num_entered_characters);

  // For now, only two UI surfaces are planned, but if more are implemented
  // then we can take the `UiSurface` directly as a parameter.
  quality_proto->set_ui_surface(
      from_omnibox_history_scope
          ? optimization_guide::proto::UiSurface::
                UI_SURFACE_OMNIBOX_HISTORY_SCOPE
          : optimization_guide::proto::UiSurface::UI_SURFACE_HISTORY_PAGE);

  for (size_t i = 0; i < result.scored_url_rows.size(); ++i) {
    const ScoredUrlRow& scored_url_row = result.scored_url_rows[i];
    optimization_guide::proto::DocumentShown* document_shown =
        quality_proto->add_top_documents_shown();
    document_shown->set_url(scored_url_row.row.url().spec());
    document_shown->set_was_clicked(i == selection);

    optimization_guide::proto::PassageData* passage_data =
        document_shown->add_passages();
    passage_data->set_text(scored_url_row.scored_url.passage);
    passage_data->set_score(scored_url_row.scored_url.score);
    const std::vector<float>& embedding =
        scored_url_row.scored_url.passage_embedding.GetData();
    passage_data->mutable_embedding()->mutable_floats()->mutable_values()->Add(
        embedding.begin(), embedding.end());
  }

  // The data is sent when `log_entry` destructs. There may eventually
  // be an option to `ModelQualityLogEntry::Drop(std::move(log_entry))`
  // in the event that log data should not be sent, but it isn't ready yet.
  // See b/334993555 for details on that; it may be useful if in the
  // future we decide to let the `log_entry` escape the service. For now,
  // it doesn't, and logging is only done proactively by destructing here.
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
    EmbedderMetadata metadata) {
  sql_database.SetEmbedderMetadata(metadata);
}

void HistoryEmbeddingsService::Storage::ProcessAndStorePassages(
    UrlPassages url_passages,
    std::vector<Embedding> passages_embeddings) {
  // Compute and save embeddings vectors.
  UrlEmbeddings url_embeddings(url_passages);
  url_embeddings.embeddings = std::move(passages_embeddings);
  CHECK_EQ(url_passages.passages.passages_size(),
           static_cast<int>(url_embeddings.embeddings.size()));
  for (int i = 0; i < url_passages.passages.passages_size(); i++) {
    url_embeddings.embeddings[i].SetPassageWordCount(
        CountWords(url_passages.passages.passages(i)));
  }
  vector_database.AddUrlEmbeddings(std::move(url_embeddings));
  vector_database.SaveTo(&sql_database);

  sql_database.InsertOrReplacePassages(url_passages);
}

std::vector<ScoredUrl> HistoryEmbeddingsService::Storage::Search(
    base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
    size_t query_id,
    Embedding query_embedding,
    std::optional<base::Time> time_range_start,
    size_t count) {
  base::ElapsedTimer timer;
  SearchInfo search_info = sql_database.FindNearest(
      time_range_start, count, std::move(query_embedding),
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
  base::UmaHistogramBoolean("History.Embeddings.Search.Completed",
                            search_info.completed);

  VLOG(1) << "History.Embeddings.Search.Duration (ms): "
          << elapsed.InMilliseconds()
          << " ; .UrlCount: " << search_info.searched_url_count
          << " ; .EmbeddingCount: " << search_info.searched_embedding_count
          << " ; .Completed: " << search_info.completed;

  // Populate source passages.
  for (ScoredUrl& scored_url : search_info.scored_urls) {
    std::optional<proto::PassagesValue> value =
        sql_database.GetPassages(scored_url.url_id);
    if (value &&
        scored_url.index < static_cast<size_t>(value.value().passages_size())) {
      scored_url.passage = value.value().passages(scored_url.index);
      VLOG(3) << "- score: " << scored_url.score
              << " ; passage: " << scored_url.passage;
    }
  }

  return std::move(search_info.scored_urls);
}

void HistoryEmbeddingsService::Storage::HandleHistoryDeletions(
    bool for_all_history,
    history::URLRows deleted_rows,
    std::set<history::VisitID> deleted_visit_ids) {
  if (for_all_history) {
    sql_database.DeleteAllData();
    return;
  }

  for (history::URLRow url_row : deleted_rows) {
    sql_database.DeleteDataForUrlId(url_row.id());
  }

  for (history::VisitID visit_id : deleted_visit_ids) {
    sql_database.DeleteDataForVisitId(visit_id);
  }
}

std::vector<UrlPassages>
HistoryEmbeddingsService::Storage::CollectPassagesWithoutEmbeddings() {
  return sql_database.GetUrlPassagesWithoutEmbeddings();
}

QualityLogEntry HistoryEmbeddingsService::PrepareQualityLogEntry() {
  // This requires some Chrome machinery to upload the log entry, so it's
  // implemented in ChromeHistoryEmbeddingsService.
  return nullptr;
}

void HistoryEmbeddingsService::OnPassagesRetrieved(
    UrlPassages url_passages,
    std::vector<std::string> passages) {
  VLOG(4) << "All " << passages.size() << " passages for url_id "
          << url_passages.url_id << ":";
  for (size_t i = 0; i < passages.size(); i++) {
    VLOG(4) << i << ": \"" << passages[i] << '"';
  }
  embedder_->ComputePassagesEmbeddings(
      PassageKind::PAGE_VISIT_PASSAGE, std::move(passages),
      base::BindOnce(&HistoryEmbeddingsService::OnPassagesEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_passages)));
}

void HistoryEmbeddingsService::OnPassagesEmbeddingsComputed(
    UrlPassages url_passages,
    std::vector<std::string> passages,
    std::vector<Embedding> passages_embeddings,
    ComputeEmbeddingsStatus status) {
  url_passages.passages.mutable_passages()->Assign(
      std::make_move_iterator(passages.begin()),
      std::make_move_iterator(passages.end()));
  storage_.AsyncCall(&Storage::ProcessAndStorePassages)
      .WithArgs(url_passages, std::move(passages_embeddings))
      .Then(base::BindOnce(callback_for_tests_, url_passages));
}

void HistoryEmbeddingsService::OnSearchCompleted(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrl> scored_urls) {
  std::vector<ScoredUrl> filtered;
  filtered.reserve(scored_urls.size());
  float threshold = kSearchScoreThreshold.Get();
  std::copy_if(std::make_move_iterator(scored_urls.begin()),
               std::make_move_iterator(scored_urls.end()),
               std::back_inserter(filtered), [=](const ScoredUrl& scored_url) {
                 return scored_url.score > threshold;
               });
  VLOG(3) << "Search found " << scored_urls.size() << " results and kept "
          << filtered.size() << " after score filtering";
  base::UmaHistogramCounts100("History.Embeddings.NumUrlsDiscardedForLowScore",
                              scored_urls.size() - filtered.size());
  DeterminePassageVisibility(std::move(callback), std::move(result),
                             std::move(filtered));
}

void HistoryEmbeddingsService::DeterminePassageVisibility(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrl> scored_urls) {
  bool is_visibility_model_available =
      page_content_annotations_service_ &&
      page_content_annotations_service_->GetModelInfoForType(
          page_content_annotations::AnnotationType::kContentVisibility);
  base::UmaHistogramCounts100("History.Embeddings.NumUrlsMatched",
                              scored_urls.size());
  base::UmaHistogramBoolean(
      "History.Embeddings.VisibilityModelAvailableAtQuery",
      is_visibility_model_available);

  if (!is_visibility_model_available || scored_urls.empty()) {
    OnPassageVisibilityCalculated(std::move(callback), std::move(result),
                                  std::move(scored_urls), {});
    return;
  }

  std::vector<std::string> inputs;
  inputs.reserve(scored_urls.size());
  for (const ScoredUrl& url : scored_urls) {
    inputs.emplace_back(url.passage);
  }
  page_content_annotations_service_->BatchAnnotate(
      base::BindOnce(&HistoryEmbeddingsService::OnPassageVisibilityCalculated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(result), std::move(scored_urls)),
      std::move(inputs),
      page_content_annotations::AnnotationType::kContentVisibility);
}

void HistoryEmbeddingsService::OnPassageVisibilityCalculated(
    SearchResultCallback callback,
    SearchResult result,
    std::vector<ScoredUrl> scored_urls,
    const std::vector<page_content_annotations::BatchAnnotationResult>&
        annotation_results) {
  if (annotation_results.empty()) {
    scored_urls.clear();
  } else {
    CHECK_EQ(scored_urls.size(), annotation_results.size());

    // Filter for scored URLs that are ok to be shown to the user.
    auto urls_it = scored_urls.begin();
    for (const page_content_annotations::BatchAnnotationResult&
             annotation_result : annotation_results) {
      if (annotation_result.visibility_score().value_or(0.0) <=
          kContentVisibilityThreshold.Get()) {
        urls_it = scored_urls.erase(urls_it);
      } else {
        ++urls_it;
      }
    }
  }

  base::UmaHistogramCounts100("History.Embeddings.NumMatchedUrlsVisible",
                              scored_urls.size());

  if (scored_urls.empty()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // Use the callback task mechanism for simplicity and easier control with
  // other standard async machinery.
  history_service_->ScheduleDBTaskForUI(base::BindOnce(
      &FinishSearchResultWithHistory,
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback),
      std::move(result), std::move(scored_urls)));
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
                       std::move(url_passages)));
  }
}

}  // namespace history_embeddings
