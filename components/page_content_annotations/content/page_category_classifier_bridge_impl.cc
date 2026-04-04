// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_category_classifier_bridge_impl.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "components/page_content_annotations/core/on_device_category_classifier.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "content/public/browser/page.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace page_content_annotations {

PageCategoryClassifierBridgeImpl::PageCategoryClassifierBridgeImpl(
    PageEmbeddingsService& page_embeddings_service,
    OnDeviceCategoryClassifier& category_classifier)
    : page_embeddings_service_(page_embeddings_service),
      category_classifier_(category_classifier) {
  scoped_observation_.Observe(&*page_embeddings_service_);
  category_classifier_observation_.Observe(&*category_classifier_);
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

  passage_embeddings::Embedding title_url_embedding;
  std::vector<passage_embeddings::Embedding> passage_embeddings;
  for (PassageEmbedding embedding : embeddings) {
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

// TODO - crbug.com/434047312: Move the metrics recording to the actual user of
// the classifier scores once implemented.
void PageCategoryClassifierBridgeImpl::OnCategoriesClassified(
    const GURL& url,
    ukm::SourceId source_id,
    const std::vector<Category>& categories) {
  ukm::builders::PageContentAnnotations2 builder(source_id);
  bool has_ukm = false;

  for (const Category& category : categories) {
    int64_t score = base::ClampRound(category.score * 100);
    int64_t noisy_score = GenerateRapporNoisedScore(category.score);
    switch (category.category_type) {
      case CategoryType::kEducation:
        base::UmaHistogramPercentage(
            "OptimizationGuide.PageContentAnnotations.CategoryClassifier."
            "EducationScore",
            score);
        builder.SetCategoryClassifier_EducationScore(noisy_score);
        has_ukm = true;
        break;
      case CategoryType::kShopping:
        base::UmaHistogramPercentage(
            "OptimizationGuide.PageContentAnnotations.CategoryClassifier."
            "ShoppingScore",
            score);
        builder.SetCategoryClassifier_ShoppingScore(noisy_score);
        has_ukm = true;
        break;
    }
  }

  if (has_ukm) {
    builder.Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace page_content_annotations
