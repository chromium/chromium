// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

namespace accessibility_annotator {

AccessibilityAnnotatorBackend::ContentAnnotationsData::
    ContentAnnotationsData() = default;

AccessibilityAnnotatorBackend::ContentAnnotationsData::
    ~ContentAnnotationsData() = default;

AccessibilityAnnotatorBackend::ContentAnnotationsData::ContentAnnotationsData(
    ContentAnnotationsData&& other) = default;

AccessibilityAnnotatorBackend::ContentAnnotationsData&
AccessibilityAnnotatorBackend::ContentAnnotationsData::operator=(
    ContentAnnotationsData&& other) = default;

// LINT.IfChange(ContentAnnotationsDataClone)
AccessibilityAnnotatorBackend::ContentAnnotationsData
AccessibilityAnnotatorBackend::ContentAnnotationsData::Clone() const {
  ContentAnnotationsData clone;
  clone.page_title = page_title;
  clone.tab_id = tab_id;
  clone.content_annotation = content_annotation;
  clone.classifier_results = classifier_results.Clone();
  clone.navigation_timestamp = navigation_timestamp;
  clone.visit_id = visit_id;
  clone.url = url;
  return clone;
}
// LINT.ThenChange(//components/accessibility_annotator/core/storage/accessibility_annotator_backend.h:ContentAnnotationsDataMembers)

}  // namespace accessibility_annotator
