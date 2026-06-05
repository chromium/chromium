// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_LIVE_TAB_CONTEXT_ACTIVE_QUERY_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_LIVE_TAB_CONTEXT_ACTIVE_QUERY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/accessibility_annotator/core/live_tab_context/search.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/weak_document_ptr.h"

namespace content {
class Page;
}  // namespace content

namespace accessibility_annotator {

// Manages the lifecycle of a single user query across a set of eligible
// pages to find relevant text passages. Essentially acts as an orchestrator for
// page search tasks, managing both the global state (e.g. query embedding
// and overall timeout) and the coordination of results across those pages.
//
// Lifecycle and Ownership:
// ActiveQuery is owned by LiveTabRetriever. Its lifecycle is strictly
// scoped to a single LiveTabRetriever::Retrieve operation, meaning it is
// created when a retrieval starts and is destroyed when the retrieval finishes,
// times out, or is preempted by a new request.
class ActiveQuery
    : public page_content_annotations::PageEmbeddingsService::Observer {
 public:
  // A snapshot representation of an active page and its extracted content.
  struct SearchablePage {
    SearchablePage(
        content::Page& page,
        page_content_annotations::ExtractedPageContentResult extraction_result);

    // Disable copying to prevent copying of large ExtractedPageContentResults.
    SearchablePage(const SearchablePage&) = delete;
    SearchablePage& operator=(const SearchablePage&) = delete;

    SearchablePage(SearchablePage&&) = default;
    SearchablePage& operator=(SearchablePage&&) = default;

    // Returns the underlying Page, or nullptr if invalid.
    content::Page* GetPage() const;

    content::WeakDocumentPtr document_ptr;
    // The unique identifier for the page/tab.
    content::GlobalRenderFrameHostId id;
    page_content_annotations::ExtractedPageContentResult extraction_result;
  };

  ActiveQuery(
      std::u16string query,
      std::vector<SearchablePage> pages,
      base::OnceCallback<void(std::vector<ScoredPassage>)> callback,
      page_content_annotations::PageEmbeddingsService& page_embeddings_service,
      passage_embeddings::Embedder& embedder);

  ActiveQuery(const ActiveQuery&) = delete;
  ActiveQuery& operator=(const ActiveQuery&) = delete;

  ~ActiveQuery() override;

  // Replaces the completion callback, cancelling previous one if any.
  void ReplaceCallback(
      base::OnceCallback<void(std::vector<ScoredPassage>)> callback);

  bool IsEquivalentTo(
      const std::u16string& query,
      const std::vector<content::GlobalRenderFrameHostId>& page_ids) const;

  // page_content_annotations::PageEmbeddingsService::Observer:
  page_content_annotations::PageEmbeddingsService::UsageMode GetUsageMode()
      const override;
  void OnPageEmbeddingsAvailable(content::Page& page) override;

 private:
  // Manages a single page computation.
  class PageSearch;

  void OnQueryEmbeddingComputed(
      std::vector<std::string> query_passages,
      std::vector<passage_embeddings::Embedding> query_embeddings,
      uint64_t job_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  void OnQueryTimeout();

  void OnPageSearchComplete(content::GlobalRenderFrameHostId id,
                          std::vector<ScoredPassage> results);

  const std::u16string query_;
  const std::vector<content::GlobalRenderFrameHostId> page_ids_;
  base::OnceCallback<void(std::vector<ScoredPassage>)> callback_;

  std::optional<passage_embeddings::Embedder::Job> query_embedding_job_;
  base::flat_map<content::GlobalRenderFrameHostId, PageSearch>
      page_computations_;
  std::vector<ScoredPassage> aggregated_results_;
  base::OneShotTimer timeout_;

  const raw_ref<page_content_annotations::PageEmbeddingsService>
      page_embeddings_service_;
  const raw_ref<passage_embeddings::Embedder> embedder_;

  base::ScopedObservation<
      page_content_annotations::PageEmbeddingsService,
      page_content_annotations::PageEmbeddingsService::Observer>
      page_embeddings_service_observation_{this};

  base::WeakPtrFactory<ActiveQuery> weak_ptr_factory_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_LIVE_TAB_CONTEXT_ACTIVE_QUERY_H_
