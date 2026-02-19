// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"

namespace accessibility_annotator {

ContentClassificationInput::ContentClassificationInput(GURL url) : url(url) {}
ContentClassificationInput::ContentClassificationInput(
    const ContentClassificationInput&) = default;
ContentClassificationInput& ContentClassificationInput::operator=(
    const ContentClassificationInput&) = default;
ContentClassificationInput::ContentClassificationInput(
    ContentClassificationInput&&) = default;
ContentClassificationInput& ContentClassificationInput::operator=(
    ContentClassificationInput&&) = default;
ContentClassificationInput::~ContentClassificationInput() = default;

bool ContentClassificationInput::IsComplete() const {
  // LINT.IfChange(ContentClassificationInputIsComplete)
  return sensitivity_score.has_value() && navigation_timestamp.has_value() &&
         adopted_language.has_value() && page_title.has_value() &&
         annotated_page_content;
  // LINT.ThenChange()
}

ContentClassificationResult::ContentClassificationResult() = default;
ContentClassificationResult::ContentClassificationResult(
    const ContentClassificationResult&) = default;
ContentClassificationResult& ContentClassificationResult::operator=(
    const ContentClassificationResult&) = default;
ContentClassificationResult::ContentClassificationResult(
    ContentClassificationResult&&) = default;
ContentClassificationResult& ContentClassificationResult::operator=(
    ContentClassificationResult&&) = default;
ContentClassificationResult::~ContentClassificationResult() = default;

ContentClassificationResult::Result::Result() = default;
ContentClassificationResult::Result::Result(const Result&) = default;
ContentClassificationResult::Result&
ContentClassificationResult::Result::operator=(const Result&) = default;
ContentClassificationResult::Result::Result(Result&&) = default;
ContentClassificationResult::Result&
ContentClassificationResult::Result::operator=(Result&&) = default;
ContentClassificationResult::Result::~Result() = default;

}  // namespace accessibility_annotator
