// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

// The result of a query classification, containing the identified intent and
// any required words extracted from the query.
struct ClassifiedQuery {
  explicit ClassifiedQuery(QueryIntentType intent,
                           std::vector<std::u16string> filter_words = {});
  ClassifiedQuery(const ClassifiedQuery&);
  ClassifiedQuery& operator=(const ClassifiedQuery&);
  ClassifiedQuery(ClassifiedQuery&&);
  ClassifiedQuery& operator=(ClassifiedQuery&&);
  ~ClassifiedQuery();

  bool operator==(const ClassifiedQuery& other) const = default;

  QueryIntentType intent = QueryIntentType::kUnknown;

  // Words extracted from the query that are used to filter the search results.
  // For example, in the query "home address in San Diego", "san" and "diego"
  // would be extracted as filter words.
  std::vector<std::u16string> filter_words;
};

// A callback that classifies the intent of a given query.
using QueryClassifier =
    base::RepeatingCallback<ClassifiedQuery(std::u16string_view)>;

// Creates a default instance of the query classifier, which orchestrates
// multiple underlying classifiers.
QueryClassifier CreateQueryClassifier();

namespace internal {

// Checks if `needle` exists in `haystack` as a standalone phrase.
// A "standalone phrase" is defined as satisfying BOTH of the following:
// 1. At the very start of the `haystack` OR preceded by a whitespace character.
// 2. At the very end of the `haystack` OR followed by a whitespace character.
//
// Returns true if `needle` is found with valid word boundaries, false
// otherwise. Returns true if `needle` is empty.
bool ContainsStandalonePhrase(std::u16string_view haystack,
                              std::u16string_view needle);

// Creates a query classifier that uses keyword matching.
QueryClassifier CreateKeywordQueryClassifier();

// Creates a query classifier that uses Gemini within Model Execution
// Service.
QueryClassifier CreateGeminiClassifier();

}  // namespace internal

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
