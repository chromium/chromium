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
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace page_content_annotations {

PageCategoryClassifierBridgeImpl::PageCategoryClassifierBridgeImpl(
    Producer<std::vector<PassageEmbedding>>& page_embeddings_producer,
    OnDeviceCategoryClassifier& category_classifier)
    : page_embeddings_producer_(page_embeddings_producer),
      category_classifier_(category_classifier) {
  callbacks_.set_removal_callback(base::BindRepeating(
      &PageCategoryClassifierBridgeImpl::UpdateDemand, base::Unretained(this)));
}

PageCategoryClassifierBridgeImpl::~PageCategoryClassifierBridgeImpl() = default;

base::CallbackListSubscription PageCategoryClassifierBridgeImpl::Subscribe(
    ClassificationCallback callback) {
  base::CallbackListSubscription subscription =
      callbacks_.Add(std::move(callback));
  UpdateDemand();
  return subscription;
}

void PageCategoryClassifierBridgeImpl::UpdateDemand() {
  if (!callbacks_.empty()) {
    if (!embeddings_subscription_) {
      embeddings_subscription_ =
          page_embeddings_producer_->Subscribe(base::BindRepeating(
              &PageCategoryClassifierBridgeImpl::OnPageEmbeddingsAvailable,
              base::Unretained(this)));
    }
  } else {
    embeddings_subscription_.reset();
    weak_ptr_factory_.InvalidateWeakPtrs();
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

  content::RenderFrameHost& main_document = page.GetMainDocument();
  category_classifier_->ClassifyTextEmbeddings(
      std::move(title_url_embedding), std::move(passage_embeddings),
      base::BindOnce(&PageCategoryClassifierBridgeImpl::OnCategoriesClassified,
                     weak_ptr_factory_.GetWeakPtr(), page.GetWeakPtr(),
                     main_document.GetLastCommittedURL(),
                     main_document.GetPageUkmSourceId()));
}

void PageCategoryClassifierBridgeImpl::OnCategoriesClassified(
    base::WeakPtr<content::Page> page,
    const GURL& url,
    ukm::SourceId source_id,
    std::vector<Category> categories) {
  if (!page || !page->IsPrimary()) {
    return;
  }
  callbacks_.Notify(url, source_id, categories);
}

}  // namespace page_content_annotations
