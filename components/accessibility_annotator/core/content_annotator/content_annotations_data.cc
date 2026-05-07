// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/content_annotator/content_annotations_data.h"

namespace accessibility_annotator {

ContentAnnotationsData::ContentAnnotationsData() = default;

ContentAnnotationsData::~ContentAnnotationsData() = default;

ContentAnnotationsData::ContentAnnotationsData(ContentAnnotationsData&& other) =
    default;

ContentAnnotationsData& ContentAnnotationsData::operator=(
    ContentAnnotationsData&& other) = default;

// LINT.IfChange(ContentAnnotationsDataClone)
ContentAnnotationsData ContentAnnotationsData::Clone() const {
  ContentAnnotationsData clone;
  clone.page_title = page_title;
  clone.tab_id = tab_id;
  clone.content_annotation = content_annotation;
  clone.classifier_results = classifier_results.Clone();
  clone.navigation_timestamp = navigation_timestamp;
  clone.url = url;
  return clone;
}
// LINT.ThenChange(//components/accessibility_annotator/core/content_annotator/content_annotations_data.h)

}  // namespace accessibility_annotator
