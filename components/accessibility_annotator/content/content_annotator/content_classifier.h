// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_

#include "base/time/time.h"
#include "url/gurl.h"

namespace accessibility_annotator {

// Data collected from various observations about a URL for classification.
struct ContentClassificationInput {
  explicit ContentClassificationInput(GURL url);
  ContentClassificationInput(const ContentClassificationInput&);
  ~ContentClassificationInput();

  GURL url;
  std::optional<float> sensitivity_score;
  std::optional<base::Time> navigation_timestamp;
  std::optional<std::string> adopted_language;
  std::optional<std::string> page_title;

  // Returns true if all fields are populated.
  bool IsComplete() const;
};

// TODO(crbug.com/479259274): Finalize expected output of classification.
struct ContentClassificationResult {
  enum class Status { kUnknown };
  Status status = Status::kUnknown;
};

// Runs classification on the given input.
ContentClassificationResult RunContentClassification(
    const ContentClassificationInput& input);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
