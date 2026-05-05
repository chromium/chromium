// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/live_tab_context/live_tab_retriever.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace accessibility_annotator {

LiveTabRetriever::LiveTabRetriever(
    page_content_annotations::PageContentExtractionService&
        page_content_extraction_service,
    page_content_annotations::PageEmbeddingsService& page_embeddings_service,
    passage_embeddings::Embedder& embedder)
    : page_embeddings_service_(page_embeddings_service),
      embedder_(embedder),
      page_content_extraction_service_(page_content_extraction_service) {}

LiveTabRetriever::~LiveTabRetriever() {
  CancelActiveQuery();
}

void LiveTabRetriever::CancelActiveQuery() {
  if (active_query_) {
    // Increment the counter so the old callback is marked as stale.
    // This is needed to prevent active_query_ from being destroyed in the
    // middle of active_query->ReplaceCallback.
    ++active_query_id_;
    active_query_->ReplaceCallback(base::DoNothing());
    active_query_.reset();
  }
}

void LiveTabRetriever::Retrieve(
    std::u16string_view query,
    base::span<content::WebContents* const> tabs,
    base::OnceCallback<void(std::vector<ScoredPassage>)> callback) {
  // If there are no tabs or the query is empty, terminate.
  // TODO(b/502966529): Add stopword and safety filtering to query.
  std::u16string query_trim(base::TrimWhitespace(query, base::TRIM_ALL));
  if (tabs.empty() || query_trim.empty()) {
    std::move(callback).Run({});
    return;
  }

  // If there are no eligible tabs, terminate.
  std::vector<ActiveQuery::SearchablePage> searchable_pages =
      GetSearchablePages(tabs);
  if (searchable_pages.empty()) {
    std::move(callback).Run({});
    return;
  }

  // If duplicate request (same query and docs),
  // preserve the existing ActiveQuery but update its callback and id.
  if (active_query_ &&
      active_query_->IsEquivalentTo(
          query_trim,
          base::ToVector(searchable_pages, &ActiveQuery::SearchablePage::id))) {
    active_query_->ReplaceCallback(base::BindOnce(
        &LiveTabRetriever::OnQueryCompleted, base::Unretained(this),
        ++active_query_id_, std::move(callback)));
  } else {
    // Otherwise, cancel any in-flight computations and start a new ActiveQuery.
    CancelActiveQuery();
    active_query_ = std::make_unique<ActiveQuery>(
        std::move(query_trim), std::move(searchable_pages),
        base::BindOnce(&LiveTabRetriever::OnQueryCompleted,
                       base::Unretained(this), ++active_query_id_,
                       std::move(callback)),
        *page_embeddings_service_, *embedder_);
  }
}

std::vector<ActiveQuery::SearchablePage> LiveTabRetriever::GetSearchablePages(
    base::span<content::WebContents* const> tabs) {
  // Skip tabs that have no active page, have no extracted page content, or
  // are not eligible for server upload.
  std::vector<ActiveQuery::SearchablePage> searchable_pages;
  for (content::WebContents* tab : tabs) {
    content::Page& page = tab->GetPrimaryPage();
    if (!page.GetMainDocument().IsActive()) {
      // Page is frozen, pending deletion, not yet loaded, etc.
      continue;
    }
    std::optional<page_content_annotations::ExtractedPageContentResult>
        extraction_result =
            page_content_extraction_service_
                ->GetExtractedPageContentAndEligibilityForPage(page);
    if (!extraction_result || !extraction_result->page_content ||
        !extraction_result->is_eligible_for_server_upload) {
      continue;
    }
    searchable_pages.emplace_back(page, std::move(*extraction_result));
  }

  return searchable_pages;
}

void LiveTabRetriever::NotifyPageEmbeddingsAvailableForTesting(  // IN-TEST
    content::Page& page) {
  if (active_query_) {
    active_query_->OnPageEmbeddingsAvailable(page);
  }
}

void LiveTabRetriever::OnQueryCompleted(
    uint64_t query_id,
    base::OnceCallback<void(std::vector<ScoredPassage>)> callback,
    std::vector<ScoredPassage> results) {
  // Check that callback belongs to current query.
  if (query_id == active_query_id_) {
    active_query_.reset();  // If so, mark query completed.
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace accessibility_annotator
