// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_category_classifier_bridge_impl.h"

#include "components/page_content_annotations/core/on_device_category_classifier.h"
#include "content/public/browser/page.h"

namespace page_content_annotations {

PageCategoryClassifierBridgeImpl::PageCategoryClassifierBridgeImpl(
    PageEmbeddingsService& page_embeddings_service,
    OnDeviceCategoryClassifier& category_classifier)
    : page_embeddings_service_(page_embeddings_service),
      category_classifier_(category_classifier) {
  scoped_observation_.Observe(&*page_embeddings_service_);
}

PageCategoryClassifierBridgeImpl::~PageCategoryClassifierBridgeImpl() = default;

PageEmbeddingsService::UsageMode
PageCategoryClassifierBridgeImpl::GetUsageMode() const {
  return PageEmbeddingsService::UsageMode::kContinuous;
}

void PageCategoryClassifierBridgeImpl::OnPageEmbeddingsAvailable(
    content::Page& page) {
  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service_->GetEmbeddings(page);
  for (const auto& embedding : embeddings) {
    if (embedding.passage.second == EmbeddingPassageType::kTitle) {
      category_classifier_->OnPageEmbeddingAvailable(
          page.GetMainDocument().GetLastCommittedURL(), embedding.embedding);
      return;
    }
  }
}

}  // namespace page_content_annotations
