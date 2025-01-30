// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/scheduling_embedder.h"

#include <memory>

#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/history_embeddings/embedder.h"
#include "components/history_embeddings/mock_embedder.h"
#include "components/history_embeddings/vector_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

using ComputePassagesEmbeddingsFuture =
    base::test::TestFuture<std::vector<std::string>,
                           std::vector<Embedding>,
                           SchedulingEmbedder::TaskId,
                           passage_embeddings::ComputeEmbeddingsStatus>;

class MockEmbedderWithDelay : public MockEmbedder {
 public:
  static constexpr base::TimeDelta kTimeout = base::Seconds(1);

  MockEmbedderWithDelay() = default;
  ~MockEmbedderWithDelay() override = default;

  // Embedder:
  void ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(passages),
                       ComputeEmbeddingsForPassages(passages),
                       passage_embeddings::ComputeEmbeddingsStatus::kSuccess),
        kTimeout);
  }
};

class SchedulingEmbedderTest : public testing::Test {
 protected:
  std::unique_ptr<SchedulingEmbedder> MakeEmbedder() {
    auto embedder = std::make_unique<SchedulingEmbedder>(
        std::make_unique<MockEmbedderWithDelay>(), 1u, false);
    embedder->SetOnEmbedderReady(
        base::BindOnce([](passage_embeddings::EmbedderMetadata metadata) {}));
    return embedder;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(SchedulingEmbedderTest, UserInitiatedJobTakesPriority) {
  auto embedder = MakeEmbedder();

  // Submit a passive priority task.
  ComputePassagesEmbeddingsFuture future_1;
  auto expected_task_id_1 = embedder->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kPassive,
      {"test passage 1", "test passage 2"}, future_1.GetCallback());

  // Submit a user-initiated priority task. This will suspend the partially
  // completed passive priority task.
  ComputePassagesEmbeddingsFuture future_2;
  auto expected_task_id_2 = embedder->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUserInitiated, {"query"},
      future_2.GetCallback());

  // The user-initiated priority task finishes first.
  EXPECT_FALSE(future_2.IsReady());
  auto [passages_2, embeddings_2, task_id_2, status_2] = future_2.Get();
  EXPECT_EQ(passages_2.size(), 1u);
  EXPECT_EQ(passages_2[0], "query");
  EXPECT_EQ(embeddings_2.size(), 1u);
  EXPECT_EQ(expected_task_id_2, task_id_2);
  EXPECT_EQ(status_2, passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  // The passive priority task finishes last.
  EXPECT_FALSE(future_1.IsReady());
  auto [passages_1, embeddings_1, task_id_1, status_1] = future_1.Get();
  EXPECT_EQ(passages_1.size(), 2u);
  EXPECT_EQ(passages_1[0], "test passage 1");
  EXPECT_EQ(passages_1[1], "test passage 2");
  EXPECT_EQ(embeddings_1.size(), 2u);
  EXPECT_EQ(task_id_1, expected_task_id_1);
  EXPECT_EQ(status_1, passage_embeddings::ComputeEmbeddingsStatus::kSuccess);
}

TEST_F(SchedulingEmbedderTest, JobCompletionRecordsHistograms) {
  auto embedder = MakeEmbedder();

  ComputePassagesEmbeddingsFuture future1;
  ComputePassagesEmbeddingsFuture future2;
  ComputePassagesEmbeddingsFuture future3;
  embedder->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kPassive, {"test passage 1"},
      future1.GetCallback());
  auto task_id = embedder->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUserInitiated,
      {"test passage 2a", "test passage 2b"}, future2.GetCallback());
  embedder->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kPassive, {"test passage 3"},
      future3.GetCallback());
  embedder->TryCancel(task_id);
  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());
  EXPECT_TRUE(future3.Wait());

  // Only two of the jobs completed normally; one was canceled.
  // So there will only be two duration samples but three counts are logged as
  // the jobs are all started.
  histogram_tester_.ExpectTotalCount("History.Embeddings.ScheduledJobDuration",
                                     2);
  histogram_tester_.ExpectTotalCount("History.Embeddings.ScheduledJobCount", 3);
  histogram_tester_.ExpectTotalCount("History.Embeddings.ScheduledPassageCount",
                                     3);

  histogram_tester_.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 0,
                                      1);
  histogram_tester_.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 1,
                                      1);
  histogram_tester_.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 2,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledPassageCount", 0, 1);
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledPassageCount", 1, 1);

  // When the third job is started, 1 + 2 = 3 passages are waiting in the
  // previous two jobs.
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledPassageCount", 3, 1);
}

}  // namespace history_embeddings
