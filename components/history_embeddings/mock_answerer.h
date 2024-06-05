// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_MOCK_ANSWERER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_MOCK_ANSWERER_H_

#include "components/history_embeddings/answerer.h"

namespace history_embeddings {

class MockAnswerer : public Answerer {
 public:
  MockAnswerer();
  ~MockAnswerer() override;

  // Answerer:
  int64_t GetModelVersion() override;
  void ComputeAnswer(std::string query,
                     Context context,
                     ComputeAnswerCallback callback) override;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_MOCK_ANSWERER_H_
