// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/on_device_category_classifier.h"
#include "components/page_content_annotations/core/page_category_classifier_bridge.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace page_content_annotations {

// A bridge that observes the PageEmbeddingsService and forwards available
// embeddings to the OnDeviceCategoryClassifier.
class PageCategoryClassifierBridgeImpl
    : public PageCategoryClassifierBridge,
      public PageEmbeddingsService::Observer,
      public OnDeviceCategoryClassifier::Observer {
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

  // OnDeviceCategoryClassifier::Observer:
  void OnCategoriesClassified(const GURL& url,
                              ukm::SourceId source_id,
                              const std::vector<Category>& categories) override;

 private:
  const raw_ref<PageEmbeddingsService> page_embeddings_service_;
  const raw_ref<OnDeviceCategoryClassifier> category_classifier_;

  base::ScopedObservation<PageEmbeddingsService,
                          PageEmbeddingsService::Observer>
      scoped_observation_{this};

  base::ScopedObservation<OnDeviceCategoryClassifier,
                          OnDeviceCategoryClassifier::Observer>
      category_classifier_observation_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CATEGORY_CLASSIFIER_BRIDGE_IMPL_H_
