// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <numeric>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"

namespace history_embeddings {

void OnGotInnerText(mojo::Remote<blink::mojom::InnerTextAgent> remote,
                    base::TimeTicks start_time,
                    UrlPassages url_passages,
                    base::OnceCallback<void(UrlPassages)> callback,
                    blink::mojom::InnerTextFramePtr mojo_frame) {
  const base::TimeDelta extraction_time = base::TimeTicks::Now() - start_time;
  if (mojo_frame) {
    for (const auto& segment : mojo_frame->segments) {
      if (segment->is_text()) {
        url_passages.passages.add_passages(segment->get_text());
      }
    }
    base::UmaHistogramTimes("History.Embeddings.Passages.ExtractionTime",
                            extraction_time);
  }
  // Save passages
  const size_t total_text_size =
      std::accumulate(url_passages.passages.passages().cbegin(),
                      url_passages.passages.passages().cend(), 0u,
                      [](size_t acc, const std::string& passage) {
                        return acc + passage.size();
                      });
  base::UmaHistogramCounts1000("History.Embeddings.Passages.PassageCount",
                               url_passages.passages.passages_size());
  base::UmaHistogramCounts10M("History.Embeddings.Passages.TotalTextSize",
                              total_text_size);
  std::move(callback).Run(std::move(url_passages));
}

Embedding StubComputeQueryEmbedding(const std::string& query) {
  // TODO(b/328114635): Synchronous inference to compute vector embedding?
  Embedding embedding({1.0f, 2.0f, 3.0f, 4.0f});
  embedding.Normalize();
  return embedding;
}

std::vector<Embedding> StubComputePassagesEmbeddings(
    const UrlPassages& url_passages) {
  // TODO(b/328114635): Synchronous inference to compute vector embeddings?
  return std::vector<Embedding>(url_passages.passages.passages_size(),
                                StubComputeQueryEmbedding(""));
}

////////////////////////////////////////////////////////////////////////////////

HistoryEmbeddingsService::HistoryEmbeddingsService(
    const base::FilePath& storage_dir,
    history::HistoryService* history_service)
    : weak_ptr_factory_(this) {
  if (!base::FeatureList::IsEnabled(kHistoryEmbeddings)) {
    // If the feature flag is disabled, skip initialization. Note we don't also
    // check the pref here, because the pref can change at runtime.
    return;
  }

  CHECK(history_service);
  history_service_observation_.Observe(history_service);

  storage_ = base::SequenceBound<Storage>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      storage_dir);
}

HistoryEmbeddingsService::~HistoryEmbeddingsService() = default;

void HistoryEmbeddingsService::RetrievePassages(content::RenderFrameHost& host,
                                                PassagesCallback callback) {
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
              // TODO(orinj): These IDs should come from history observer or
              // the like once history metadata is plumbed through.
              UrlPassages(1, 1, base::Time::Now()),
              base::BindOnce(&HistoryEmbeddingsService::OnPassagesRetrieved,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback))),
          nullptr));
}

void HistoryEmbeddingsService::Search(std::string query,
                                      size_t count,
                                      SearchResultCallback callback) {
  storage_.AsyncCall(&Storage::Search)
      .WithArgs(std::move(query), count)
      .Then(std::move(callback));
}

void HistoryEmbeddingsService::Shutdown() {
  storage_.Reset();
}

void HistoryEmbeddingsService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // TODO(b/329495955): Implement actual cleanup of storage for this deletion.
}

HistoryEmbeddingsService::Storage::Storage(const base::FilePath& storage_dir)
    : sql_database(storage_dir) {}

void HistoryEmbeddingsService::Storage::ProcessAndStorePassages(
    ComputeEmbeddingsCallback compute_embeddings,
    UrlPassages url_passages) {
  // Compute and save embeddings vectors.
  UrlEmbeddings url_embeddings(url_passages);
  url_embeddings.embeddings = std::move(compute_embeddings).Run(url_passages);
  vector_database.AddUrlEmbeddings(std::move(url_embeddings));
  vector_database.SaveTo(&sql_database);

  sql_database.InsertOrReplacePassages(url_passages);
}

std::vector<ScoredUrl> HistoryEmbeddingsService::Storage::Search(
    std::string query,
    size_t count) {
  std::vector<ScoredUrl> scored_urls =
      sql_database.FindNearest(count, StubComputeQueryEmbedding(query));

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

void HistoryEmbeddingsService::OnPassagesRetrieved(PassagesCallback callback,
                                                   UrlPassages url_passages) {
  storage_.AsyncCall(&Storage::ProcessAndStorePassages)
      .WithArgs(base::BindOnce(&StubComputePassagesEmbeddings), url_passages)
      .Then(base::BindOnce(std::move(callback), url_passages));
}

}  // namespace history_embeddings
