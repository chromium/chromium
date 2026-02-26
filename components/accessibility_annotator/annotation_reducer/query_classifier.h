// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_

#include <string>
#include <vector>

#include "components/accessibility_annotator/annotation_reducer/query_intent_type.h"

namespace annotation_reducer {

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

}  // namespace annotation_reducer

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
