// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_

#include <string>
#include <vector>

#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

class QueryClassifier {
 public:
  QueryClassifier();
  QueryClassifier(const QueryClassifier&) = delete;
  QueryClassifier& operator=(const QueryClassifier&) = delete;
  ~QueryClassifier();

  QueryIntentType Classify(const std::u16string& query);

 private:
  void InitializeStopWords();
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
