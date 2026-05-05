// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_LIVE_TAB_CONTEXT_LIVE_TAB_RETRIEVER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_LIVE_TAB_CONTEXT_LIVE_TAB_RETRIEVER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/accessibility_annotator/content/live_tab_context/active_query.h"
#include "components/accessibility_annotator/core/live_tab_context/search.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace passage_embeddings {
class Embedder;
}  // namespace passage_embeddings

namespace page_content_annotations {
class PageContentExtractionService;
class PageEmbeddingsService;
}  // namespace page_content_annotations

namespace accessibility_annotator {

// Retrieve relevant context to a query from a set of docs.
//
// Lifecycle and Ownership:
// LiveTabRetriever is owned by the AccessibilityQueryServiceDelegateImpl.
//
// Through this delegate, its lifecycle is tied to AccessibilityQueryService,
// which is a KeyedService (one instance per Profile). It is created when the
// service is initialized and lives as long as the service.
//
// Only one LiveTabRetriever::Retrieve operation is active at a time. Initiating
// a new request while another is in flight will cancel the previous operation.
class LiveTabRetriever {
 public:
  LiveTabRetriever(
      page_content_annotations::PageContentExtractionService&
          page_content_extraction_service,
      page_content_annotations::PageEmbeddingsService& page_embeddings_service,
      passage_embeddings::Embedder& embedder);
  LiveTabRetriever(const LiveTabRetriever&) = delete;
  LiveTabRetriever& operator=(const LiveTabRetriever&) = delete;
  ~LiveTabRetriever();

  // Retrieve relevant context to a query from a set of docs.
  //
  // Results are aggregated, ranked, and returned via the provided `callback`.
  //
  // Concurrency and Preemption:
  // * Preemption: If a new `Retrieve()` request is initiated while a previous
  //   one is still in flight for a different query or set of documents, the
  //   previous operation is cancelled.
  // * Duplicate Requests: If a `Retrieve()` request exactly matches the query
  //   and documents of an in-flight operation, the new callback replaces the
  //   existing one. The original callback is resolved with an empty result.
  void Retrieve(std::u16string_view query,
                base::span<content::WebContents* const> tabs,
                base::OnceCallback<void(std::vector<ScoredPassage>)> callback);

  // Use this to simulate page embeddings becoming available in tests.
  void NotifyPageEmbeddingsAvailableForTesting(content::Page& page);

 private:
  void CancelActiveQuery();

  void OnQueryCompleted(
      uint64_t query_id,
      base::OnceCallback<void(std::vector<ScoredPassage>)> callback,
      std::vector<ScoredPassage> results);

  std::vector<ActiveQuery::SearchablePage> GetSearchablePages(
      base::span<content::WebContents* const> tabs);

  std::unique_ptr<ActiveQuery> active_query_;

  // A counter to safely manage the lifecycle of `active_query_`.
  // Distinguishes between stale callbacks and the current one.
  uint64_t active_query_id_ = 0;

  // For page and query embeddings, respectively.
  raw_ref<page_content_annotations::PageEmbeddingsService>
      page_embeddings_service_;
  raw_ref<passage_embeddings::Embedder> embedder_;

  raw_ref<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_LIVE_TAB_CONTEXT_LIVE_TAB_RETRIEVER_H_
