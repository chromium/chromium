// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"

namespace accessibility_annotator {

ContentAnnotatorService::ContentAnnotatorService(
    page_content_annotations::PageContentAnnotationsService&
        page_content_annotations_service)
    : page_content_annotations_service_(page_content_annotations_service),
      join_entries_(kContentAnnotatorMaxPendingUrls.Get()) {
  page_content_annotations_service_->AddObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

ContentAnnotatorService::~ContentAnnotatorService() {
  page_content_annotations_service_->RemoveObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

void ContentAnnotatorService::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  CHECK(result.GetType() ==
        page_content_annotations::AnnotationType::kContentVisibility);
  CacheIterator it = GetOrCreateJoinEntry(visit.url);
  // Invert the visibility score to get a sensitivity score.
  it->second.sensitivity_score = 1.0 - result.GetContentVisibilityScore();
  it->second.navigation_timestamp = visit.nav_entry_timestamp;
  MaybeAnnotate(it);
}

ContentAnnotatorService::CacheIterator
ContentAnnotatorService::GetOrCreateJoinEntry(const GURL& url) {
  CacheIterator it = join_entries_.Get(url);
  if (it != join_entries_.end()) {
    return it;
  }
  return join_entries_.Put(url, ContentClassificationInput(url));
}

void ContentAnnotatorService::MaybeAnnotate(CacheIterator it) {
  if (!it->second.IsComplete()) {
    return;
  }
  ContentClassificationInput complete_data = std::move(it->second);
  join_entries_.Erase(it);
  // TODO(crbug.com/475859254): Move this call to a separate task/sequence as
  // needed.
  RunContentClassification(std::move(complete_data));
  // TODO(crbug.com/476434957): Process classification result.
}
}  // namespace accessibility_annotator
