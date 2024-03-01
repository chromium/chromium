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
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"

namespace history_embeddings {

UrlPassages::UrlPassages() = default;
UrlPassages::~UrlPassages() = default;
UrlPassages::UrlPassages(UrlPassages&&) = default;
UrlPassages& UrlPassages::operator=(UrlPassages&&) = default;

void OnGotInnerText(mojo::Remote<blink::mojom::InnerTextAgent> remote,
                    base::TimeTicks start_time,
                    GURL url,
                    base::OnceCallback<void(UrlPassages)> callback,
                    blink::mojom::InnerTextFramePtr mojo_frame) {
  const base::TimeDelta extraction_time = base::TimeTicks::Now() - start_time;
  UrlPassages url_passages;
  url_passages.url = std::move(url);
  if (mojo_frame) {
    for (const auto& segment : mojo_frame->segments) {
      if (segment->is_text()) {
        url_passages.passages.push_back(segment->get_text());
      }
    }
    base::UmaHistogramTimes("History.Embeddings.Passages.ExtractionTime",
                            extraction_time);
  }
  const size_t total_text_size =
      std::reduce(url_passages.passages.cbegin(), url_passages.passages.cend(),
                  0u, [](size_t acc, const std::string& passage) {
                    return acc + passage.size();
                  });
  base::UmaHistogramCounts1000("History.Embeddings.Passages.PassageCount",
                               url_passages.passages.size());
  base::UmaHistogramCounts10M("History.Embeddings.Passages.TotalTextSize",
                              total_text_size);
  std::move(callback).Run(std::move(url_passages));
}

////////////////////////////////////////////////////////////////////////////////

HistoryEmbeddingsService::HistoryEmbeddingsService(
    const base::FilePath& storage_dir)
    : weak_ptr_factory_(this) {
  if (!base::FeatureList::IsEnabled(kHistoryEmbeddings)) {
    // If the feature flag is disabled, skip initialization. Note we don't also
    // check the pref here, because the pref can change at runtime.
    return;
  }

  database_ = base::SequenceBound<SqlDatabase>(
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
              host.GetLastCommittedURL(),
              base::BindOnce(&HistoryEmbeddingsService::OnPassagesRetrieved,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback))),
          nullptr));
}

void HistoryEmbeddingsService::Shutdown() {
  database_.Reset();
}

void HistoryEmbeddingsService::OnPassagesRetrieved(PassagesCallback callback,
                                                   UrlPassages url_passages) {
  // TODO(orinj): Store in database. For now just notify callback for testing.
  std::move(callback).Run(std::move(url_passages));
}

}  // namespace history_embeddings
