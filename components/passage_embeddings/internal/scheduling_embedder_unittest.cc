// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/internal/scheduling_embedder.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

namespace {

using ComputePassagesEmbeddingsFuture =
    base::test::TestFuture<std::vector<std::string>,
                           std::vector<Embedding>,
                           SchedulingEmbedder::TaskId,
                           ComputeEmbeddingsStatus>;

void GetEmbeddings(std::vector<std::string> passages,
                   PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::string> passages,
             SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            std::vector<mojom::PassageEmbeddingsResultPtr> results;
            for (const std::string& passage : passages) {
              results.push_back(mojom::PassageEmbeddingsResult::New());
              results.back()->embeddings =
                  std::vector<float>(kEmbeddingsModelOutputSize, 1.0);
              results.back()->passage = passage;
            }
            std::move(callback).Run(std::move(results),
                                    ComputeEmbeddingsStatus::kSuccess);
          },
          std::move(passages), std::move(callback)),
      base::Seconds(1));
}

}  // namespace

class SchedulingEmbedderPublic : public SchedulingEmbedder {
 public:
  SchedulingEmbedderPublic(EmbedderMetadataProvider* embedder_metadata_provider,
                           GetEmbeddingsCallback get_embeddings_callback,
                           size_t max_jobs,
                           size_t scheduled_max_batch_size,
                           bool use_performance_scenario)
      : SchedulingEmbedder(embedder_metadata_provider,
                           get_embeddings_callback,
                           max_jobs,
                           scheduled_max_batch_size,
                           use_performance_scenario) {}

  using SchedulingEmbedder::embedder_metadata_;
  using SchedulingEmbedder::GetEmbeddingsCallback;
  using SchedulingEmbedder::GetEmbeddingsResultCallback;
};

class SchedulingEmbedderTest : public testing::Test {
 public:
  void SetUp() override {
    embedder_metadata_provider_ =
        std::make_unique<TestEmbedderMetadataProvider>();
    embedder_ = std::make_unique<SchedulingEmbedderPublic>(
        /*embedder_metadata_provider=*/embedder_metadata_provider_.get(),
        /*get_embeddings_callback=*/base::BindRepeating(&GetEmbeddings),
        /*max_jobs=*/4u,
        /*max_batch_size=*/1u,
        /*use_performance_scenario=*/false);
    ASSERT_TRUE(embedder_->embedder_metadata_.IsValid());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<EmbedderMetadataProvider> embedder_metadata_provider_;
  std::unique_ptr<SchedulingEmbedderPublic> embedder_;
};

TEST_F(SchedulingEmbedderTest, UserInitiatedJobTakesPriority) {
  // Submit a passive priority task.
  ComputePassagesEmbeddingsFuture future_1;
  auto expected_task_id_1 = embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1", "test passage 2"},
      future_1.GetCallback());

  // Submit a user-initiated priority task. This will suspend the partially
  // completed passive priority task.
  ComputePassagesEmbeddingsFuture future_2;
  auto expected_task_id_2 = embedder_->ComputePassagesEmbeddings(
      PassagePriority::kUserInitiated, {"query"}, future_2.GetCallback());

  // The user-initiated priority task finishes first.
  EXPECT_FALSE(future_2.IsReady());
  auto [passages_2, embeddings_2, task_id_2, status_2] = future_2.Get();
  EXPECT_EQ(passages_2.size(), 1u);
  EXPECT_EQ(passages_2[0], "query");
  EXPECT_EQ(embeddings_2.size(), 1u);
  EXPECT_EQ(expected_task_id_2, task_id_2);
  EXPECT_EQ(status_2, ComputeEmbeddingsStatus::kSuccess);

  // The passive priority task finishes last.
  EXPECT_FALSE(future_1.IsReady());
  auto [passages_1, embeddings_1, task_id_1, status_1] = future_1.Get();
  EXPECT_EQ(passages_1.size(), 2u);
  EXPECT_EQ(passages_1[0], "test passage 1");
  EXPECT_EQ(passages_1[1], "test passage 2");
  EXPECT_EQ(embeddings_1.size(), 2u);
  EXPECT_EQ(task_id_1, expected_task_id_1);
  EXPECT_EQ(status_1, ComputeEmbeddingsStatus::kSuccess);
}

TEST_F(SchedulingEmbedderTest, RecordsHistograms) {
  ComputePassagesEmbeddingsFuture future1;
  ComputePassagesEmbeddingsFuture future2;
  ComputePassagesEmbeddingsFuture future3;
  embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());
  auto task_id = embedder_->ComputePassagesEmbeddings(
      PassagePriority::kUserInitiated, {"test passage 2a", "test passage 2b"},
      future2.GetCallback());
  embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());
  embedder_->TryCancel(task_id);
  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());
  EXPECT_TRUE(future3.Wait());

  // Only the two "passive priority" jobs successfully completed; the "user
  // initiate priority" one was canceled. So only two duration histogram samples
  // are logged, but three counts histograms samples and three status histogram
  // samples are logged as the all jobs were enqueued and completed in some way.
  histogram_tester_.ExpectTotalCount("History.Embeddings.ScheduledJobDuration",
                                     2);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.ScheduledJobDuration.Passive", 2);

  histogram_tester_.ExpectTotalCount("History.Embeddings.ScheduledJobStatus",
                                     3);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.ScheduledJobStatus.Passive", 2);
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledJobStatus.Passive",
      ComputeEmbeddingsStatus::kSuccess, 2);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.ScheduledJobStatus.UserInitiated", 1);
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledJobStatus.UserInitiated",
      ComputeEmbeddingsStatus::kCanceled, 1);

  histogram_tester_.ExpectTotalCount("History.Embeddings.ScheduledJobCount", 3);
  histogram_tester_.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 0,
                                      1);
  histogram_tester_.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 1,
                                      1);
  histogram_tester_.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 2,
                                      1);

  histogram_tester_.ExpectTotalCount("History.Embeddings.ScheduledPassageCount",
                                     3);
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledPassageCount", 0, 1);
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledPassageCount", 1, 1);
  // When the third job is enqueued, 1 + 2 = 3 passages are waiting in the
  // previous two jobs.
  histogram_tester_.ExpectBucketCount(
      "History.Embeddings.ScheduledPassageCount", 3, 1);
}

TEST_F(SchedulingEmbedderTest, LimitsJobCount) {
  ComputePassagesEmbeddingsFuture future1;
  ComputePassagesEmbeddingsFuture future2;
  ComputePassagesEmbeddingsFuture future3;
  ComputePassagesEmbeddingsFuture future4;
  ComputePassagesEmbeddingsFuture future5;

  embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());
  embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 2"}, future2.GetCallback());
  embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());
  embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 4"}, future4.GetCallback());
  embedder_->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 5"}, future5.GetCallback());

  // Final job interrupts the job at back of line when limit (4) is reached.
  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future4.Take()), ComputeEmbeddingsStatus::kCanceled);
  EXPECT_EQ(std::get<3>(future5.Take()), ComputeEmbeddingsStatus::kSuccess);
}

}  // namespace passage_embeddings
