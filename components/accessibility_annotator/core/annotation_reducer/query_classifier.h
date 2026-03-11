// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_

#include <string_view>

#include "base/functional/callback.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

// A callback that classifies the intent of a given query.
using QueryClassifier =
    base::RepeatingCallback<QueryIntentType(std::u16string_view)>;

// Creates a default instance of the query classifier, which orchestrates
// multiple underlying classifiers.
QueryClassifier CreateQueryClassifier();

namespace internal {

// Creates a query classifier that uses keyword matching with regular
// expressions.
QueryClassifier CreateRegExpQueryClassifier();

// Creates a query classifier that uses Gemini within Model Execution
// Service.
QueryClassifier CreateGeminiClassifier();

}  // namespace internal

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
