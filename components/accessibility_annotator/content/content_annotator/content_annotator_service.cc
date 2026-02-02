// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include "components/page_content_annotations/core/page_content_annotation_type.h"

namespace accessibility_annotator {

ContentAnnotatorService::ContentAnnotatorService(
    page_content_annotations::PageContentAnnotationsService&
        page_content_annotations_service)
    : page_content_annotations_service_(page_content_annotations_service) {
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
  // TODO(crbug.com/475859254): Implement logic to calculate and store
  // sensitivity scores.
}

}  // namespace accessibility_annotator
