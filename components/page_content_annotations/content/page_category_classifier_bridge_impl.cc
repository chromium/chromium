// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_category_classifier_bridge_impl.h"

#include <optional>
#include <utility>
#include <vector>

#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/on_device_category_classifier.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "content/public/browser/page.h"

namespace page_content_annotations {

PageCategoryClassifierBridgeImpl::PageCategoryClassifierBridgeImpl(
    PageEmbeddingsService& page_embeddings_service,
    OnDeviceCategoryClassifier& category_classifier)
    : page_embeddings_service_(page_embeddings_service),
      category_classifier_(category_classifier) {}

PageCategoryClassifierBridgeImpl::~PageCategoryClassifierBridgeImpl() = default;

void PageCategoryClassifierBridgeImpl::SetDemandActive(bool active) {
  if (active) {
    if (!scoped_observation_.IsObserving()) {
      scoped_observation_.Observe(&*page_embeddings_service_);
    }
  } else {
    scoped_observation_.Reset();
  }
}

PageEmbeddingsService::UsageMode
PageCategoryClassifierBridgeImpl::GetUsageMode() const {
  return PageEmbeddingsService::UsageMode::kContinuous;
}

void PageCategoryClassifierBridgeImpl::OnPageEmbeddingsAvailable(
    content::Page& page) {
  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service_->GetEmbeddings(page);

  std::optional<passage_embeddings::Embedding> title_url_embedding;
  std::vector<passage_embeddings::Embedding> passage_embeddings;
  for (PassageEmbedding& embedding : embeddings) {
    if (embedding.passage.second == EmbeddingPassageType::kTitleAndUrl) {
      title_url_embedding = std::move(embedding.embedding);
    } else if (embedding.passage.second == EmbeddingPassageType::kPageContent) {
      passage_embeddings.push_back(std::move(embedding.embedding));
    }
  }

  category_classifier_->OnPageEmbeddingAvailable(
      page.GetMainDocument().GetLastCommittedURL(),
      page.GetMainDocument().GetPageUkmSourceId(),
      std::move(title_url_embedding), std::move(passage_embeddings));
}

}  // namespace page_content_annotations
