// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_MOCK_INTENT_CLASSIFIER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_MOCK_INTENT_CLASSIFIER_H_

#include "components/history_embeddings/intent_classifier.h"

namespace history_embeddings {

class MockIntentClassifier : public IntentClassifier {
 public:
  MockIntentClassifier();
  ~MockIntentClassifier() override;

  // IntentClassifier:
  int64_t GetModelVersion() override;
  void ComputeQueryIntent(std::string query,
                          ComputeQueryIntentCallback callback) override;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_MOCK_INTENT_CLASSIFIER_H_
