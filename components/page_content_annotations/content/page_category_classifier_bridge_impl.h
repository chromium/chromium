// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "components/page_content_annotations/content/producer.h"
#include "components/page_content_annotations/core/page_category_classifier_bridge.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"

namespace content {
class Page;
}  // namespace content

namespace page_content_annotations {

class OnDeviceCategoryClassifier;

// A bridge that subscribes to a passage-embeddings producer and forwards
// available embeddings to the OnDeviceCategoryClassifier when demand is active.
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
  void SetDemandActive(bool active) override;

 private:
  void OnPageEmbeddingsAvailable(
      content::Page& page,
      const std::vector<PassageEmbedding>& embeddings);

  const raw_ref<Producer<std::vector<PassageEmbedding>>>
      page_embeddings_producer_;
  const raw_ref<OnDeviceCategoryClassifier> category_classifier_;

  std::optional<base::CallbackListSubscription> embeddings_subscription_;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
