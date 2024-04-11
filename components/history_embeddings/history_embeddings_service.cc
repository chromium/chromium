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
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/mock_embedder.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
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
    std::vector<ScoredUrl> scored_urls,
    history::HistoryBackend* history_backend,
    history::URLDatabase* url_database) {
  SearchResult result;
  if (url_database) {
    // Move each ScoredUrl into a more complete ScoredUrlRow with more info from
    // the history database.
    result.reserve(scored_urls.size());
    for (ScoredUrl& scored_url : scored_urls) {
      result.emplace_back(std::move(scored_url));
      if (!url_database->GetURLRow(result.back().scored_url.url_id,
                                   &result.back().row)) {
        // This omission covers an edge case and should generally not happen
        // unless a notification was missed or the history database and
        // history_embeddings database went out of sync. It's theoretically
        // possible since operations across separate databases are not atomic.
        result.pop_back();
      }
    }
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(result)));
}

////////////////////////////////////////////////////////////////////////////////

HistoryEmbeddingsService::HistoryEmbeddingsService(
    history::HistoryService* history_service,
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service)
    : history_service_(history_service),
      page_content_annotations_service_(page_content_annotations_service),
      query_id_(0u),
      query_id_weak_ptr_factory_(&query_id_),
      weak_ptr_factory_(this) {
  if (!base::FeatureList::IsEnabled(kHistoryEmbeddings)) {
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

  // TODO: b/333094780 - Swap this to the model-backed embedder once ready.
  embedder_ = std::make_unique<MockEmbedder>();

  storage_ = base::SequenceBound<Storage>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      history_service_->history_dir());
}

HistoryEmbeddingsService::~HistoryEmbeddingsService() = default;

void HistoryEmbeddingsService::RetrievePassages(
    const history::VisitRow& visit_row,
    content::RenderFrameHost& host) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  mojo::Remote<blink::mojom::InnerTextAgent> agent;
  host.GetRemoteInterfaces()->GetInterface(agent.BindNewPipeAndPassReceiver());
  auto params = blink::mojom::InnerTextParams::New();
  params->max_words_per_aggregate_passage =
      std::max(0, kPassageExtractionMaxWordsPerAggregatePassage.Get());
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

void HistoryEmbeddingsService::Search(std::string query,
                                      size_t count,
                                      SearchResultCallback callback) {
  embedder_->ComputePassagesEmbeddings(
      {std::move(query)},
      base::BindOnce(&HistoryEmbeddingsService::OnQueryEmbeddingComputed,
                     weak_ptr_factory_.GetWeakPtr(), count,
                     std::move(callback)));
}

void HistoryEmbeddingsService::OnQueryEmbeddingComputed(
    size_t count,
    SearchResultCallback callback,
    std::vector<std::string> query_passages,
    std::vector<Embedding> query_embeddings) {
  bool succeeded = !query_embeddings.empty();
  base::UmaHistogramBoolean("History.Embeddings.QueryEmbeddingSucceeded",
                            succeeded);
  if (!succeeded) {
    // Query embedding failed. Just return no search results.
    std::move(callback).Run({});
    return;
  }

  CHECK_EQ(query_embeddings.size(), 1u);

  query_id_++;
  storage_.AsyncCall(&Storage::Search)
      .WithArgs(query_id_weak_ptr_factory_.GetWeakPtr(), query_id_.load(),
                std::move(query_embeddings.front()), count)
      .Then(base::BindOnce(&HistoryEmbeddingsService::OnSearchCompleted,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

base::WeakPtr<HistoryEmbeddingsService> HistoryEmbeddingsService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

void HistoryEmbeddingsService::Storage::ProcessAndStorePassages(
    UrlPassages url_passages,
    std::vector<Embedding> passages_embeddings) {
  // Compute and save embeddings vectors.
  UrlEmbeddings url_embeddings(url_passages);
  url_embeddings.embeddings = std::move(passages_embeddings);
  vector_database.AddUrlEmbeddings(std::move(url_embeddings));
  vector_database.SaveTo(&sql_database);

  sql_database.InsertOrReplacePassages(url_passages);
}

std::vector<ScoredUrl> HistoryEmbeddingsService::Storage::Search(
    base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
    size_t query_id,
    Embedding query_embedding,
    size_t count) {
  std::vector<ScoredUrl> scored_urls = sql_database.FindNearest(
      count, std::move(query_embedding),
      base::BindRepeating(
          [](base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
             size_t query_id) {
            // If the service shut down or started a new query, this one is no
            // longer needed. Signal to exit early. Best result so far will be
            // returned.
            return !weak_latest_query_id || *weak_latest_query_id != query_id;
          },
          std::move(weak_latest_query_id), query_id));

  // Populate source passages.
  for (ScoredUrl& scored_url : scored_urls) {
    std::optional<proto::PassagesValue> value =
        sql_database.GetPassages(scored_url.url_id);
    if (value &&
        scored_url.index < static_cast<size_t>(value.value().passages_size())) {
      scored_url.passage = value.value().passages(scored_url.index);
    }
  }

  return scored_urls;
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

void HistoryEmbeddingsService::OnPassagesRetrieved(
    UrlPassages url_passages,
    std::vector<std::string> passages) {
  embedder_->ComputePassagesEmbeddings(
      std::move(passages),
      base::BindOnce(&HistoryEmbeddingsService::OnPassagesEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_passages)));
}

void HistoryEmbeddingsService::OnPassagesEmbeddingsComputed(
    UrlPassages url_passages,
    std::vector<std::string> passages,
    std::vector<Embedding> passages_embeddings) {
  url_passages.passages.mutable_passages()->Assign(
      std::make_move_iterator(passages.begin()),
      std::make_move_iterator(passages.end()));
  storage_.AsyncCall(&Storage::ProcessAndStorePassages)
      .WithArgs(url_passages, std::move(passages_embeddings))
      .Then(base::BindOnce(callback_for_tests_, url_passages));
}

void HistoryEmbeddingsService::OnSearchCompleted(
    SearchResultCallback callback,
    std::vector<ScoredUrl> scored_urls) {
  // TODO(b/330925683): Handle search interruption. This may not still need to
  //  happen by now.
  DeterminePassageVisibility(std::move(callback), std::move(scored_urls));
}

void HistoryEmbeddingsService::DeterminePassageVisibility(
    SearchResultCallback callback,
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
  if (!is_visibility_model_available) {
    OnPassageVisibilityCalculated(std::move(callback), std::move(scored_urls),
                                  {});
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
                     std::move(scored_urls)),
      std::move(inputs),
      page_content_annotations::AnnotationType::kContentVisibility);
}

void HistoryEmbeddingsService::OnPassageVisibilityCalculated(
    SearchResultCallback callback,
    std::vector<ScoredUrl> scored_urls,
    const std::vector<page_content_annotations::BatchAnnotationResult>&
        annotation_results) {
  if (annotation_results.empty()) {
    scored_urls.clear();
  } else {
    CHECK_EQ(scored_urls.size(), annotation_results.size());

    // Filter for scored URLs that are ok to be shown to the user.
    auto urls_it = scored_urls.begin();
    for (const page_content_annotations::BatchAnnotationResult& result :
         annotation_results) {
      if (result.visibility_score().value_or(0.0) <=
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
    std::move(callback).Run({});
    return;
  }

  // Use the callback task mechanism for simplicity and easier control with
  // other standard async machinery.
  history_service_->ScheduleDBTaskForUI(
      base::BindOnce(&FinishSearchResultWithHistory,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(callback), std::move(scored_urls)));
}

}  // namespace history_embeddings
