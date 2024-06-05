// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_answerer.h"

namespace history_embeddings {

MockAnswerer::MockAnswerer() = default;
MockAnswerer::~MockAnswerer() = default;

int64_t MockAnswerer::GetModelVersion() {
  return 1;
}

void MockAnswerer::ComputeAnswer(std::string query,
                                 Context context,
                                 ComputeAnswerCallback callback) {
  std::move(callback).Run({ComputeAnswerStatus::SUCCESS, query,
                           std::string("This is the answer to query '") +
                               query + std::string("'.")});
}

}  // namespace history_embeddings
