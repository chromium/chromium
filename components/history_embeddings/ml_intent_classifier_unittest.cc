// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_intent_classifier.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"

namespace history_embeddings {

class HistoryEmbeddingsMlIntentClassifierTest : public testing::Test {
 public:
  void SetUp() override {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, ComputeQueryIntent) {
  MlIntentClassifier intent_classifier(nullptr);
  EXPECT_EQ(intent_classifier.GetModelVersion(), 0);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::SUCCESS);
    EXPECT_EQ(is_query_answerable, true);
  }
}

}  // namespace history_embeddings
