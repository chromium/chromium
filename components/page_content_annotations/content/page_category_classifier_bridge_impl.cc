// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_category_classifier_bridge_impl.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "components/page_content_annotations/core/on_device_category_classifier.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "content/public/browser/page.h"

namespace page_content_annotations {

PageCategoryClassifierBridgeImpl::PageCategoryClassifierBridgeImpl(
    Producer<std::vector<PassageEmbedding>>& page_embeddings_producer,
    OnDeviceCategoryClassifier& category_classifier)
    : page_embeddings_producer_(page_embeddings_producer),
      category_classifier_(category_classifier) {}

PageCategoryClassifierBridgeImpl::~PageCategoryClassifierBridgeImpl() = default;

void PageCategoryClassifierBridgeImpl::SetDemandActive(bool active) {
  if (active) {
    if (!embeddings_subscription_) {
      embeddings_subscription_ =
          page_embeddings_producer_->Subscribe(base::BindRepeating(
              &PageCategoryClassifierBridgeImpl::OnPageEmbeddingsAvailable,
              base::Unretained(this)));
    }
  } else {
    embeddings_subscription_.reset();
  }
}

void PageCategoryClassifierBridgeImpl::OnPageEmbeddingsAvailable(
    content::Page& page,
    const std::vector<PassageEmbedding>& embeddings) {
  std::optional<passage_embeddings::Embedding> title_url_embedding;
  std::vector<passage_embeddings::Embedding> passage_embeddings;
  for (const PassageEmbedding& embedding : embeddings) {
    if (embedding.passage.second == EmbeddingPassageType::kTitleAndUrl) {
      title_url_embedding = embedding.embedding;
    } else if (embedding.passage.second == EmbeddingPassageType::kPageContent) {
      passage_embeddings.push_back(embedding.embedding);
    }
  }

  category_classifier_->OnPageEmbeddingAvailable(
      page.GetMainDocument().GetLastCommittedURL(),
      page.GetMainDocument().GetPageUkmSourceId(),
      std::move(title_url_embedding), std::move(passage_embeddings));
}

}  // namespace page_content_annotations
