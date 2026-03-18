// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_category_classifier_bridge.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"

namespace page_content_annotations {

class OnDeviceCategoryClassifier;

// A bridge that observes the PageEmbeddingsService and forwards available
// embeddings to the OnDeviceCategoryClassifier.
class PageCategoryClassifierBridgeImpl
    : public PageCategoryClassifierBridge,
      public PageEmbeddingsService::Observer {
 public:
  PageCategoryClassifierBridgeImpl(
      PageEmbeddingsService& page_embeddings_service,
      OnDeviceCategoryClassifier& category_classifier);
  ~PageCategoryClassifierBridgeImpl() override;
  PageCategoryClassifierBridgeImpl(const PageCategoryClassifierBridgeImpl&) =
      delete;
  PageCategoryClassifierBridgeImpl& operator=(
      const PageCategoryClassifierBridgeImpl&) = delete;

  // PageEmbeddingsService::Observer:
  PageEmbeddingsService::UsageMode GetUsageMode() const override;
  void OnPageEmbeddingsAvailable(content::Page& page) override;

 private:
  void OnCategoryClassifiersCompleted(const std::vector<Category>& outputs);

  const raw_ref<PageEmbeddingsService> page_embeddings_service_;
  const raw_ref<OnDeviceCategoryClassifier> category_classifier_;

  base::ScopedObservation<PageEmbeddingsService,
                          PageEmbeddingsService::Observer>
      scoped_observation_{this};

  base::WeakPtrFactory<PageCategoryClassifierBridgeImpl> weak_ptr_factory_{
      this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
