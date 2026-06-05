// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/live_tab_context/active_query.h"

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/page_content_annotations/content/embeddings_candidate_generator.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"

namespace accessibility_annotator {

ActiveQuery::SearchablePage::SearchablePage(
    content::Page& page,
    page_content_annotations::ExtractedPageContentResult extraction_result)
    : document_ptr(page.GetMainDocument().GetWeakDocumentPtr()),
      id(page.GetMainDocument().GetGlobalId()),
      extraction_result(std::move(extraction_result)) {}

content::Page* ActiveQuery::SearchablePage::GetPage() const {
  content::RenderFrameHost* frame = document_ptr.AsRenderFrameHostIfValid();
  return frame ? &frame->GetPage() : nullptr;
}

// Handles the search for a single page.
class ActiveQuery::PageSearch {
 public:
  // PageSearch remains idle until both the query embedding and
  // page-specific embeddings are available. If the global timeout fires first,
  // it executes a fallback keyword search to ensure results are returned
  // promptly.
  PageSearch(
      SearchablePage searchable_page,
      base::OnceCallback<void(std::vector<ScoredPassage>)> callback,
      const std::u16string& query,
      page_content_annotations::PageEmbeddingsService& page_embeddings_service);
  ~PageSearch();

  PageSearch(PageSearch&&);
  PageSearch& operator=(PageSearch&&);

  void OnQueryEmbeddingReady(const passage_embeddings::Embedding& embedding);
  void OnPageEmbeddingsUpdated(
      std::vector<page_content_annotations::PassageEmbedding> page_embeddings);
  void OnTimeout();

 private:
  void MaybeRunSearch();
  void Finish(std::vector<ScoredPassage> results);

  SearchablePage searchable_page_;
  base::OnceCallback<void(std::vector<ScoredPassage>)> scored_passages_callback_;
  raw_ref<const std::u16string> query_;
  raw_ref<page_content_annotations::PageEmbeddingsService>
      page_embeddings_service_;

  std::optional<passage_embeddings::Embedding> query_embedding_;
  std::optional<std::vector<page_content_annotations::PassageEmbedding>>
      page_embeddings_;
};

ActiveQuery::ActiveQuery(
    std::u16string query,
    std::vector<SearchablePage> pages,
    base::OnceCallback<void(std::vector<ScoredPassage>)> callback,
    page_content_annotations::PageEmbeddingsService& page_embeddings_service,
    passage_embeddings::Embedder& embedder)
    : query_(std::move(query)),
      page_ids_(base::ToVector(pages, &SearchablePage::id)),
      callback_(std::move(callback)),
      page_embeddings_service_(page_embeddings_service),
      embedder_(embedder) {
  DCHECK(callback_);

  // If there are no pages or query, terminate.
  if (pages.empty() || query_.empty()) {
    std::move(callback_).Run({});
    return;
  }

  page_embeddings_service_observation_.Observe(&*page_embeddings_service_);

  // Transform the pages into a map of PageSearches.
  for (SearchablePage& page : pages) {
    content::GlobalRenderFrameHostId id = page.id;
    page_computations_.try_emplace(
        id, std::move(page),
        base::BindOnce(&ActiveQuery::OnPageSearchComplete,
                       weak_ptr_factory_.GetWeakPtr(), id),
        query_, *page_embeddings_service_);
  }
  DCHECK_EQ(page_computations_.size(), page_ids_.size());

  // Start the query timeout timer.
  timeout_.Start(
      FROM_HERE,
      features::kAccessibilityAnnotatorLiveTabContextRequestTimeout.Get(),
      base::BindOnce(&ActiveQuery::OnQueryTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  // Kick off embeddings
  page_embeddings_service_->ProcessEmbeddingsOnDemand();
  query_embedding_job_ = embedder_->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUserInitiated,
      {base::UTF16ToUTF8(query_)},
      base::BindOnce(&ActiveQuery::OnQueryEmbeddingComputed,
                     weak_ptr_factory_.GetWeakPtr()));
}

ActiveQuery::~ActiveQuery() = default;

void ActiveQuery::ReplaceCallback(
    base::OnceCallback<void(std::vector<ScoredPassage>)> new_callback) {
  std::move(callback_).Run({});
  callback_ = std::move(new_callback);
}

bool ActiveQuery::IsEquivalentTo(
    const std::u16string& query,
    const std::vector<content::GlobalRenderFrameHostId>& page_ids) const {
  return query_ == query && page_ids_ == page_ids;
}

page_content_annotations::PageEmbeddingsService::UsageMode
ActiveQuery::GetUsageMode() const {
  return page_content_annotations::PageEmbeddingsService::kOnDemand;
}

void ActiveQuery::OnPageEmbeddingsAvailable(content::Page& page) {
  content::GlobalRenderFrameHostId id = page.GetMainDocument().GetGlobalId();
  auto it = page_computations_.find(id);
  if (it != page_computations_.end()) {
    it->second.OnPageEmbeddingsUpdated(
        page_embeddings_service_->GetEmbeddings(page));
  }
}

ActiveQuery::PageSearch::PageSearch(
    SearchablePage searchable_page,
    base::OnceCallback<void(std::vector<ScoredPassage>)> callback,
    const std::u16string& query,
    page_content_annotations::PageEmbeddingsService& page_embeddings_service)
    : searchable_page_(std::move(searchable_page)),
      scored_passages_callback_(std::move(callback)),
      query_(query),
      page_embeddings_service_(page_embeddings_service) {
  DCHECK(scored_passages_callback_);
}

ActiveQuery::PageSearch::~PageSearch() = default;

ActiveQuery::PageSearch::PageSearch(PageSearch&&) = default;
ActiveQuery::PageSearch& ActiveQuery::PageSearch::operator=(
    PageSearch&&) = default;

void ActiveQuery::PageSearch::OnQueryEmbeddingReady(
    const passage_embeddings::Embedding& embedding) {
  query_embedding_ = embedding;
  MaybeRunSearch();
}

void ActiveQuery::PageSearch::OnPageEmbeddingsUpdated(
    std::vector<page_content_annotations::PassageEmbedding> page_embeddings) {
  page_embeddings_ = std::move(page_embeddings);
  MaybeRunSearch();
}

void ActiveQuery::PageSearch::OnTimeout() {
  // Fall back to keyword matching.
  const page_content_annotations::ExtractedPageContentResult&
      page_content_result = searchable_page_.extraction_result;
  if (!page_content_result.page_content) {
    Finish({});
    return;
  }

  std::vector<std::string> passages = base::ToVector(
      page_content_annotations::GenerateEmbeddingsCandidates(
          page_content_result.page_content,
          features::kAccessibilityAnnotatorLiveTabContextPassagesPerPage.Get(),
          page_content_result.page_content->data.main_frame_data().title(),
          page_content_result.page_content->data.main_frame_data().url()),
      [](auto& pair) { return std::move(pair.first); });
  Finish(FindPassagesByKeywordMatching(*query_, passages));
}

void ActiveQuery::PageSearch::MaybeRunSearch() {
  if (!query_embedding_) {
    return;
  }

  // If page embeddings were generated before ActiveQuery, they may not have
  // triggered OnPageEmbeddingsAvailable. We need to check for them here.
  if (!page_embeddings_) {
    content::Page* page = searchable_page_.GetPage();
    if (!page) {
      Finish({});
      return;
    }

    std::vector<page_content_annotations::PassageEmbedding> page_embeddings =
        page_embeddings_service_->GetEmbeddings(*page);
    if (page_embeddings.empty()) {
      // Still no embeddings available; wait for OnPageEmbeddingsUpdated.
      return;
    }
    page_embeddings_ = std::move(page_embeddings);
  }

  Finish(
      RankPassagesBySemanticSimilarity(*query_embedding_, *page_embeddings_));
}

void ActiveQuery::PageSearch::Finish(std::vector<ScoredPassage> results) {
  DCHECK(scored_passages_callback_);
  std::move(scored_passages_callback_).Run(std::move(results));
}

void ActiveQuery::OnQueryEmbeddingComputed(
    std::vector<std::string> /*query_passages*/,
    std::vector<passage_embeddings::Embedding> query_embeddings,
    uint64_t job_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  DCHECK(query_embedding_job_.has_value());
  DCHECK_EQ(query_embedding_job_->id(), job_id);
  query_embedding_job_.reset();  // Indicates that job is completed.

  std::optional<passage_embeddings::Embedding> query_embedding;
  if (status == passage_embeddings::ComputeEmbeddingsStatus::kSuccess &&
      !query_embeddings.empty()) {
    query_embedding = std::move(query_embeddings[0]);
  }

  base::WeakPtr<ActiveQuery> weak_this = weak_ptr_factory_.GetWeakPtr();
  std::vector<content::GlobalRenderFrameHostId> pending_page_ids = page_ids_;

  // Once we have the query state (embedding or not), we can search each page.
  for (const content::GlobalRenderFrameHostId& page_id : pending_page_ids) {
    // Don't execute page computations if ActiveQuery was completed.
    if (!weak_this) {
      return;
    }
    auto it = page_computations_.find(page_id);
    if (it == page_computations_.end()) {
      continue;
    }

    if (query_embedding) {
      it->second.OnQueryEmbeddingReady(*query_embedding);
    } else {
      it->second.OnTimeout();
    }
  }
}

void ActiveQuery::OnQueryTimeout() {
  // Cancel the query embedding task if it's still pending.
  query_embedding_job_.reset();

  base::WeakPtr<ActiveQuery> weak_this = weak_ptr_factory_.GetWeakPtr();
  std::vector<content::GlobalRenderFrameHostId> pending_page_ids = page_ids_;

  // Fall back to keyword matching for all pending pages.
  for (const content::GlobalRenderFrameHostId& page_id : pending_page_ids) {
    // Don't execute page computations if ActiveQuery was completed.
    if (!weak_this) {
      return;
    }
    auto it = page_computations_.find(page_id);
    if (it != page_computations_.end()) {
      it->second.OnTimeout();
    }
  }
}

void ActiveQuery::OnPageSearchComplete(content::GlobalRenderFrameHostId id,
                                       std::vector<ScoredPassage> results) {
  // Accumulate results.
  base::Extend(aggregated_results_, std::move(results));

  // Task is done, remove it from the map.
  auto it = page_computations_.find(id);
  CHECK(it != page_computations_.end());
  page_computations_.erase(it);

  // If no tasks remain, we are done.
  if (page_computations_.empty()) {
    size_t num_passages = std::min(
        aggregated_results_.size(),
        static_cast<size_t>(
            features::kAccessibilityAnnotatorLiveTabContextMaxSearchResults
                .Get()));
    std::ranges::partial_sort(
        aggregated_results_, aggregated_results_.begin() + num_passages,
        [](const ScoredPassage& a, const ScoredPassage& b) {
          return a.score > b.score;
        });
    aggregated_results_.erase(aggregated_results_.begin() + num_passages,
                              aggregated_results_.end());

    // Terminate computation and run the callback.
    // `this` may be destroyed after this line.
    std::move(callback_).Run(std::move(aggregated_results_));
  }
}

}  // namespace accessibility_annotator

