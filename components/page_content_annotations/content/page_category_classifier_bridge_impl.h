// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/page_content_annotations/content/producer.h"
#include "components/page_content_annotations/core/page_category_classifier_bridge.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace content {
class Page;
}  // namespace content

namespace page_content_annotations {

class OnDeviceCategoryClassifier;

// A bridge that subscribes to a passage-embeddings producer, classifies
// available embeddings via `OnDeviceCategoryClassifier`, and delivers the
// resulting categories to subscribers when demand is active.
class PageCategoryClassifierBridgeImpl : public PageCategoryClassifierBridge {
 public:
  PageCategoryClassifierBridgeImpl(
      Producer<std::vector<PassageEmbedding>>& page_embeddings_producer,
      OnDeviceCategoryClassifier& category_classifier);
  ~PageCategoryClassifierBridgeImpl() override;
  PageCategoryClassifierBridgeImpl(const PageCategoryClassifierBridgeImpl&) =
      delete;
  PageCategoryClassifierBridgeImpl& operator=(
      const PageCategoryClassifierBridgeImpl&) = delete;

  // PageCategoryClassifierBridge:
  base::CallbackListSubscription Subscribe(
      ClassificationCallback callback) override;

 private:
  void UpdateDemand();
  void OnPageEmbeddingsAvailable(
      content::Page& page,
      const std::vector<PassageEmbedding>& embeddings);
  void OnCategoriesClassified(base::WeakPtr<content::Page> page,
                              const GURL& url,
                              ukm::SourceId source_id,
                              std::vector<Category> categories);

  const raw_ref<Producer<std::vector<PassageEmbedding>>>
      page_embeddings_producer_;
  const raw_ref<OnDeviceCategoryClassifier> category_classifier_;

  base::RepeatingCallbackList<
      void(const GURL&, ukm::SourceId, const std::vector<Category>&)>
      callbacks_;
  std::optional<base::CallbackListSubscription> embeddings_subscription_;

  base::WeakPtrFactory<PageCategoryClassifierBridgeImpl> weak_ptr_factory_{
      this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
