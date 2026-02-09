// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

namespace accessibility_annotator {

ContentClassificationInput::ContentClassificationInput(GURL url) : url(url) {}
ContentClassificationInput::~ContentClassificationInput() = default;
ContentClassificationInput::ContentClassificationInput(
    const ContentClassificationInput& other) = default;

bool ContentClassificationInput::IsComplete() const {
  return sensitivity_score.has_value() && navigation_timestamp.has_value() &&
         adopted_language.has_value() && page_title.has_value();
}

ContentClassificationResult RunContentClassification(
    const ContentClassificationInput& input) {
  // TODO(crbug.com/479259274): Implement running classification on the input.
  return ContentClassificationResult();
}

}  // namespace accessibility_annotator
