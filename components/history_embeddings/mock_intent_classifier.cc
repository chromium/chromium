// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_intent_classifier.h"

#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/history_embeddings/history_embeddings_features.h"

namespace history_embeddings {

MockIntentClassifier::MockIntentClassifier() = default;
MockIntentClassifier::~MockIntentClassifier() = default;

int64_t MockIntentClassifier::GetModelVersion() {
  return 1;
}

void MockIntentClassifier::ComputeQueryIntent(
    std::string query,
    ComputeQueryIntentCallback callback) {
  std::vector<std::string> query_intent_indicating_words({
      "can ",
      "could ",
      "do ",
      "does ",
      "how ",
      "should ",
      "what ",
      "when ",
      "where ",
      "whether ",
      "which ",
      "who ",
      "whose ",
      "why ",
      "would ",
  });
  query = base::ToLowerASCII(query);
  bool is_query_answerable =
      query.ends_with('?') ||
      std::ranges::any_of(
          query_intent_indicating_words,
          [&](std::string_view start) { return query.starts_with(start); });
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ComputeIntentStatus::SUCCESS,
                     is_query_answerable),
      base::Milliseconds(kMockIntentClassifierDelayMS.Get()));
}

}  // namespace history_embeddings
