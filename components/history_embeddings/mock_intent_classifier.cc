// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_intent_classifier.h"

#include "base/task/sequenced_task_runner.h"

namespace history_embeddings {

MockIntentClassifier::MockIntentClassifier() = default;
MockIntentClassifier::~MockIntentClassifier() = default;

int64_t MockIntentClassifier::GetModelVersion() {
  return 1;
}

void MockIntentClassifier::ComputeQueryIntent(
    std::string query,
    ComputeQueryIntentCallback callback) {
  bool is_query_answerable =
      query == "can this query be answered, please and thank you?";
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ComputeIntentStatus::SUCCESS,
                     is_query_answerable));
}

}  // namespace history_embeddings
