// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/core/internal/scheduling_embedder.h"

#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

namespace {

using testing::ElementsAre;

using ComputePassagesEmbeddingsFuture =
    base::test::TestFuture<std::vector<std::string>,
                           std::vector<Embedding>,
                           uint64_t,
                           ComputeEmbeddingsStatus>;

std::vector<mojom::PassageEmbeddingsResultPtr> GenerateExpectedServiceOutput(
    const std::vector<std::string>& passages) {
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  for (size_t i = 0; i < passages.size(); ++i) {
    results.push_back(
        mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f}));
  }
  return results;
}

void IgnoreResults(std::vector<std::string>,
                   std::vector<Embedding>,
                   uint64_t,
                   ComputeEmbeddingsStatus) {}

}  // namespace

class GetEmbeddingsStub {
 public:
  MOCK_METHOD(void,
              GetEmbeddings,
              (std::vector<std::string> passages,
               PassagePriority priority,
               SchedulingEmbedder::GetEmbeddingsResultCallback callback));
};

class SchedulingEmbedderTest : public testing::Test {
 public:
  void SetUp() override {
    embedder_metadata_provider_ =
        std::make_unique<TestEmbedderMetadataProvider>();
  }

  void TearDown() override { embedder_metadata_provider_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<EmbedderMetadataProvider> embedder_metadata_provider_;
  GetEmbeddingsStub get_embeddings_stub_;
};

TEST_F(SchedulingEmbedderTest, InvokesService) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<std::string> requested_passages;
  std::optional<PassagePriority> passage_priority;
  SchedulingEmbedder::GetEmbeddingsResultCallback result_callback;

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        requested_passages = std::move(passages);
        passage_priority = priority;
        result_callback = std::move(callback);
      });

  Embedder::Job job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"},
      base::BindOnce(&IgnoreResults));

  EXPECT_THAT(requested_passages, ElementsAre("test passage 1"));
  EXPECT_EQ(passage_priority, PassagePriority::kPassive);
  ASSERT_FALSE(result_callback.is_null());
  // Run the callback to notify the SchedulingEmbedder of the processed output.
  std::move(result_callback)
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);
}

TEST_F(SchedulingEmbedderTest, TranslatesServiceOutput) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/2u,
      /*use_performance_scenario=*/false);

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        std::vector<mojom::PassageEmbeddingsResultPtr> results;
        results.push_back(mojom::PassageEmbeddingsResult::New(
            std::vector<float>{1.0f, 0.0f}));
        results.push_back(mojom::PassageEmbeddingsResult::New(
            std::vector<float>{0.0f, 1.0f}));
        std::move(callback).Run(std::move(results),
                                ComputeEmbeddingsStatus::kSuccess);
      });

  ComputePassagesEmbeddingsFuture future;
  Embedder::Job job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1", "test passage 2"},
      future.GetCallback());

  auto [passages, embeddings, received_job_id, status] = future.Take();
  EXPECT_THAT(passages, ElementsAre("test passage 1", "test passage 2"));
  ASSERT_EQ(embeddings.size(), 2u);
  EXPECT_THAT(embeddings[0].GetData(), ElementsAre(1.0f, 0.0f));
  EXPECT_THAT(embeddings[1].GetData(), ElementsAre(0.0f, 1.0f));
  EXPECT_EQ(job.id(), received_job_id);
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that a vector that can't be scaled to unit length fails the job
// instead of crashing or producing an invalid embedding.
TEST_F(SchedulingEmbedderTest, FailsJobOnEmbeddingThatCannotBeNormalized) {
  const float kNaN = std::numeric_limits<float>::quiet_NaN();
  const float kInfinity = std::numeric_limits<float>::infinity();
  const float kMax = std::numeric_limits<float>::max();
  const std::vector<std::vector<float>> kBadVectors = {
      {}, {0.0f, 0.0f}, {kNaN, 1.0f}, {kInfinity, 0.0f}, {kMax, 0.0f}};

  for (const std::vector<float>& bad_vector : kBadVectors) {
    SCOPED_TRACE(testing::PrintToString(bad_vector));
    auto embedder = std::make_unique<SchedulingEmbedder>(
        embedder_metadata_provider_.get(),
        base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                            base::Unretained(&get_embeddings_stub_)),
        /*max_jobs=*/4u,
        /*max_batch_size=*/1u,
        /*use_performance_scenario=*/false);

    EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
        .WillOnce(
            [&](std::vector<std::string> passages, PassagePriority priority,
                SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
              std::vector<mojom::PassageEmbeddingsResultPtr> results;
              results.push_back(
                  mojom::PassageEmbeddingsResult::New(bad_vector));
              std::move(callback).Run(std::move(results),
                                      ComputeEmbeddingsStatus::kSuccess);
            });

    ComputePassagesEmbeddingsFuture future;
    Embedder::Job job = embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"p1"}, future.GetCallback());

    auto [passages, embeddings, received_job_id, status] = future.Take();
    EXPECT_THAT(passages, ElementsAre("p1"));
    EXPECT_TRUE(embeddings.empty());
    EXPECT_EQ(job.id(), received_job_id);
    EXPECT_EQ(status, ComputeEmbeddingsStatus::kExecutionFailure);
    testing::Mock::VerifyAndClearExpectations(&get_embeddings_stub_);
  }
}

// Verifies that only the job that owns a vector that can't be normalized fails,
// as soon as its batch returns: the jobs before it keep their results, and the
// jobs after it are resubmitted.
TEST_F(SchedulingEmbedderTest,
       FailsOnlyOwnerOfEmbeddingThatCannotBeNormalized) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/4u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 0 is submitted alone; jobs 1, 2 and 3 wait and then share a batch.
  Embedder::Job job0 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p0"}, base::BindOnce(&IgnoreResults));
  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, future1.GetCallback());
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2a", "p2b"}, future2.GetCallback());
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p3"}, future3.GetCallback());

  ASSERT_EQ(calls.size(), 1u);
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p0"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p1", "p2a", "p2b", "p3"));

  // The vector for p2b is bad.
  const float kNaN = std::numeric_limits<float>::quiet_NaN();
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{0.0f, 1.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{kNaN, 1.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 1.0f}));
  std::move(calls[1].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  // Job 1 keeps its result. Job 2 fails right away, without the result it got
  // for p2a. The result after the bad vector is dropped, so job 3 is
  // resubmitted alone.
  ASSERT_TRUE(future1.IsReady());
  auto [passages1, embeddings1, job_id1, status1] = future1.Take();
  ASSERT_EQ(embeddings1.size(), 1u);
  EXPECT_THAT(embeddings1[0].GetData(), ElementsAre(1.0f, 0.0f));
  EXPECT_EQ(status1, ComputeEmbeddingsStatus::kSuccess);
  ASSERT_TRUE(future2.IsReady());
  auto [passages2, embeddings2, job_id2, status2] = future2.Take();
  EXPECT_THAT(passages2, ElementsAre("p2a", "p2b"));
  EXPECT_TRUE(embeddings2.empty());
  EXPECT_EQ(status2, ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_FALSE(future3.IsReady());
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_THAT(calls[2].passages, ElementsAre("p3"));

  std::move(calls[2].callback)
      .Run(GenerateExpectedServiceOutput({"p3"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_TRUE(future3.IsReady());
  auto [passages3, embeddings3, job_id3, status3] = future3.Take();
  EXPECT_EQ(embeddings3.size(), 1u);
  EXPECT_EQ(status3, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(calls.size(), 3u);
}

// Verifies that a vector that can't be normalized after the result for the
// last passage, which only a misbehaving service would send, is ignored: the
// job keeps its result and nothing is resubmitted.
TEST_F(SchedulingEmbedderTest, IgnoresBadEmbeddingAfterLastPassage) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        const float kNaN = std::numeric_limits<float>::quiet_NaN();
        std::vector<mojom::PassageEmbeddingsResultPtr> results;
        results.push_back(mojom::PassageEmbeddingsResult::New(
            std::vector<float>{1.0f, 0.0f}));
        results.push_back(mojom::PassageEmbeddingsResult::New(
            std::vector<float>{kNaN, 1.0f}));
        std::move(callback).Run(std::move(results),
                                ComputeEmbeddingsStatus::kSuccess);
      });

  ComputePassagesEmbeddingsFuture future;
  Embedder::Job job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, future.GetCallback());

  auto [passages, embeddings, received_job_id, status] = future.Take();
  EXPECT_THAT(passages, ElementsAre("p1"));
  ASSERT_EQ(embeddings.size(), 1u);
  EXPECT_THAT(embeddings[0].GetData(), ElementsAre(1.0f, 0.0f));
  EXPECT_EQ(job.id(), received_job_id);
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that a vector that can't be normalized after the last passage of a
// batch fails no job, even if that passage's job has more passages waiting. The
// job keeps its results, and its remaining passage is submitted next.
TEST_F(SchedulingEmbedderTest, IgnoresBadEmbeddingAfterLastPassageOfSplitJob) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/2u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  ComputePassagesEmbeddingsFuture future;
  Embedder::Job job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1", "p2", "p3"}, future.GetCallback());
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_THAT(calls[0].passages, ElementsAre("p1", "p2"));

  // A third, bad vector comes back for a batch of two passages.
  const float kNaN = std::numeric_limits<float>::quiet_NaN();
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{0.0f, 1.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{kNaN, 1.0f}));
  std::move(calls[0].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  EXPECT_FALSE(future.IsReady());
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p3"));
  std::move(calls[1].callback)
      .Run(GenerateExpectedServiceOutput({"p3"}),
           ComputeEmbeddingsStatus::kSuccess);

  auto [passages, embeddings, received_job_id, status] = future.Take();
  ASSERT_EQ(embeddings.size(), 3u);
  EXPECT_THAT(embeddings[0].GetData(), ElementsAre(1.0f, 0.0f));
  EXPECT_THAT(embeddings[1].GetData(), ElementsAre(0.0f, 1.0f));
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kSuccess);
}

TEST_F(SchedulingEmbedderTest, UserInitiatedJobTakesPriority) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    PassagePriority priority;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;

  const auto save_call_parameters =
      [&calls](std::vector<std::string> passages, PassagePriority priority,
               SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        calls.emplace_back(std::move(passages), priority, std::move(callback));
      };

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce(save_call_parameters)
      .WillOnce(save_call_parameters)
      .WillOnce(save_call_parameters);

  // Submit a passive priority job.
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1", "test passage 2"},
      base::BindOnce(&IgnoreResults));

  // Submit a user-initiated priority job. This will suspend the partially
  // completed passive priority job.
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUserInitiated, {"query"},
      base::BindOnce(&IgnoreResults));

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_THAT(calls.back().passages, ElementsAre("test passage 1"));
  EXPECT_EQ(calls.back().priority, PassagePriority::kPassive);

  // Running the callback should kick off the next round of processing.
  ASSERT_FALSE(calls.back().callback.is_null());
  std::move(calls.back().callback)
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls.back().passages, ElementsAre("query"));
  EXPECT_EQ(calls.back().priority, PassagePriority::kUserInitiated);

  ASSERT_FALSE(calls.back().callback.is_null());
  std::move(calls.back().callback)
      .Run(GenerateExpectedServiceOutput({"query"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(calls.size(), 3u);
  EXPECT_THAT(calls.back().passages, ElementsAre("test passage 2"));
  EXPECT_EQ(calls.back().priority, PassagePriority::kPassive);

  ASSERT_FALSE(calls.back().callback.is_null());
  std::move(calls.back().callback)
      .Run(GenerateExpectedServiceOutput({"test passage 2"}),
           ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that destroying the handle of a waiting job cancels it right away,
// and that a job whose handle is kept is unaffected.
TEST_F(SchedulingEmbedderTest, TryCancel) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<std::string> requested_passages;
  SchedulingEmbedder::GetEmbeddingsResultCallback result_callback;

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&requested_passages, &result_callback](
                    std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        requested_passages = std::move(passages);
        result_callback = std::move(callback);
      });

  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ComputePassagesEmbeddingsFuture future2;
  {
    Embedder::Job second_job = embedder->ComputePassagesEmbeddings(
        PassagePriority::kPassive, {"test passage 2"}, future2.GetCallback());
  }

  auto [passages2, embeddings2, job_id2, status2] = future2.Take();
  EXPECT_THAT(passages2, ElementsAre("test passage 2"));
  EXPECT_TRUE(embeddings2.empty());
  EXPECT_EQ(status2, ComputeEmbeddingsStatus::kCanceled);

  EXPECT_THAT(requested_passages, ElementsAre("test passage 1"));

  // Running the callback finishes the first job and should not kick off any
  // further processing. The latter is validated by the WillOnce expectation
  // above.
  ASSERT_FALSE(result_callback.is_null());
  std::move(result_callback)
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  auto [passages1, embeddings1, job_id1, status1] = future1.Take();
  EXPECT_EQ(job_id1, job1.id());
  EXPECT_EQ(embeddings1.size(), 1u);
  EXPECT_EQ(status1, ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that destroying the handle of a job whose first passage is being
// embedded calls back with kCanceled without waiting for the in-flight batch,
// and that the job's remaining passages are never submitted.
TEST_F(SchedulingEmbedderTest, TryCancelRunningJobCallsBackBeforeBatchReturns) {
  base::HistogramTester histogram_tester;
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<std::string> requested_passages;
  SchedulingEmbedder::GetEmbeddingsResultCallback result_callback;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        requested_passages = std::move(passages);
        result_callback = std::move(callback);
      });

  ComputePassagesEmbeddingsFuture future;
  std::optional<Embedder::Job> job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1", "p2", "p3"}, future.GetCallback());
  EXPECT_THAT(requested_passages, ElementsAre("p1"));
  const uint64_t job_id = job->id();
  job.reset();

  // Canceling posts the callback. Run the tasks posted so far to check that it
  // runs while "p1" is still being embedded.
  base::test::TestFuture<void> posted_tasks_ran;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, posted_tasks_ran.GetCallback());
  ASSERT_TRUE(posted_tasks_ran.Wait());
  ASSERT_TRUE(future.IsReady());
  auto [passages, embeddings, received_job_id, status] = future.Take();
  EXPECT_THAT(passages, ElementsAre("p1", "p2", "p3"));
  EXPECT_TRUE(embeddings.empty());
  EXPECT_EQ(received_job_id, job_id);
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kCanceled);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.ScheduledJobStatus.Passive",
      ComputeEmbeddingsStatus::kCanceled, 1);

  // The batch returns. The WillOnce expectation above validates that "p2" and
  // "p3" are never submitted.
  ASSERT_FALSE(result_callback.is_null());
  std::move(result_callback)
      .Run(GenerateExpectedServiceOutput({"p1"}),
           ComputeEmbeddingsStatus::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.ScheduledJobStatus.Passive",
      ComputeEmbeddingsStatus::kCanceled, 1);
  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobDuration",
                                    0);
}

// Verifies that a canceled job whose last passage is in the in-flight batch
// reports kCanceled, not kSuccess, when the batch succeeds.
TEST_F(SchedulingEmbedderTest, TryCancelRunningJobWhoseLastPassageIsInFlight) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  SchedulingEmbedder::GetEmbeddingsResultCallback result_callback;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        result_callback = std::move(callback);
      });

  ComputePassagesEmbeddingsFuture future;
  std::optional<Embedder::Job> job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, future.GetCallback());
  job.reset();

  ASSERT_FALSE(result_callback.is_null());
  std::move(result_callback)
      .Run(GenerateExpectedServiceOutput({"p1"}),
           ComputeEmbeddingsStatus::kSuccess);

  auto [passages, embeddings, job_id, status] = future.Take();
  EXPECT_THAT(passages, ElementsAre("p1"));
  EXPECT_TRUE(embeddings.empty());
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kCanceled);
}

// Verifies that a canceled job reports kCanceled, not the batch failure, when
// its in-flight batch fails.
TEST_F(SchedulingEmbedderTest, TryCancelRunningJobWhenBatchFails) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  SchedulingEmbedder::GetEmbeddingsResultCallback result_callback;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        result_callback = std::move(callback);
      });

  ComputePassagesEmbeddingsFuture future;
  std::optional<Embedder::Job> job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1", "p2"}, future.GetCallback());
  job.reset();

  // The WillOnce expectation above validates that "p2" is never submitted.
  ASSERT_FALSE(result_callback.is_null());
  std::move(result_callback)
      .Run({}, ComputeEmbeddingsStatus::kExecutionFailure);

  auto [passages, embeddings, job_id, status] = future.Take();
  EXPECT_THAT(passages, ElementsAre("p1", "p2"));
  EXPECT_TRUE(embeddings.empty());
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kCanceled);
}

// Verifies that once a running job is canceled, the next batch goes to the
// next job rather than to the canceled job's remaining passages.
TEST_F(SchedulingEmbedderTest, TryCancelRunningJobLetsOtherJobsRun) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  ComputePassagesEmbeddingsFuture future1;
  std::optional<Embedder::Job> job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1a", "p1b"}, future1.GetCallback());
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, future2.GetCallback());
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_THAT(calls[0].passages, ElementsAre("p1a"));
  job1.reset();

  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p1a"}),
           ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kCanceled);

  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p2"));
  std::move(calls[1].callback)
      .Run(GenerateExpectedServiceOutput({"p2"}),
           ComputeEmbeddingsStatus::kSuccess);

  auto [passages2, embeddings2, job_id2, status2] = future2.Take();
  EXPECT_EQ(embeddings2.size(), 1u);
  EXPECT_EQ(status2, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(calls.size(), 2u);
}

// Verifies that the results for a canceled job in a batch shared with other
// jobs are skipped, so the other jobs get the right embeddings.
TEST_F(SchedulingEmbedderTest, TryCancelRunningJobInMultiJobBatch) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/3u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 0 is submitted alone; jobs 1 and 2 wait and then share a batch.
  Embedder::Job job0 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p0"}, base::BindOnce(&IgnoreResults));
  ComputePassagesEmbeddingsFuture future1;
  std::optional<Embedder::Job> job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1a", "p1b"}, future1.GetCallback());
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, future2.GetCallback());

  ASSERT_EQ(calls.size(), 1u);
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p0"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p1a", "p1b", "p2"));

  job1.reset();

  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{0.0f, 1.0f}));
  std::move(calls[1].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  auto [passages1, embeddings1, job_id1, status1] = future1.Take();
  EXPECT_TRUE(embeddings1.empty());
  EXPECT_EQ(status1, ComputeEmbeddingsStatus::kCanceled);

  // Job 2 got the third result, so both results for job 1 were skipped.
  ASSERT_TRUE(future2.IsReady());
  auto [passages2, embeddings2, job_id2, status2] = future2.Take();
  ASSERT_EQ(embeddings2.size(), 1u);
  EXPECT_THAT(embeddings2[0].GetData(), ElementsAre(0.0f, 1.0f));
  EXPECT_EQ(status2, ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that the callback of a job that completes when its batch returns
// can cancel the next job in the same batch. The canceled job's result is
// skipped, the job after it gets its own result, and the canceled job's status
// is recorded once.
TEST_F(SchedulingEmbedderTest,
       CompletedJobCallbackCanCancelAnotherJobInSameBatch) {
  base::HistogramTester histogram_tester;
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/3u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 0 is submitted alone; jobs 1, 2 and 3 wait and then share a batch.
  Embedder::Job job0 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p0"}, base::BindOnce(&IgnoreResults));
  std::optional<Embedder::Job> job2;
  std::optional<ComputeEmbeddingsStatus> status1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"},
      base::BindLambdaForTesting([&](std::vector<std::string> passages,
                                     std::vector<Embedding> embeddings,
                                     uint64_t job_id,
                                     ComputeEmbeddingsStatus status) {
        status1 = status;
        job2.reset();
      }));
  ComputePassagesEmbeddingsFuture future2;
  job2 = embedder->ComputePassagesEmbeddings(PassagePriority::kPassive, {"p2"},
                                             future2.GetCallback());
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p3"}, future3.GetCallback());

  ASSERT_EQ(calls.size(), 1u);
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p0"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p1", "p2", "p3"));

  // Job 1 completes first, and its callback cancels job 2.
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{0.0f, 1.0f}));
  std::move(calls[1].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  EXPECT_EQ(status1, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_FALSE(job2.has_value());
  // Job 3 got the third result, so job 2's result was skipped.
  ASSERT_TRUE(future3.IsReady());
  auto [passages3, embeddings3, job_id3, status3] = future3.Take();
  ASSERT_EQ(embeddings3.size(), 1u);
  EXPECT_THAT(embeddings3[0].GetData(), ElementsAre(0.0f, 1.0f));
  EXPECT_EQ(status3, ComputeEmbeddingsStatus::kSuccess);
  auto [passages2, embeddings2, job_id2, status2] = future2.Take();
  EXPECT_TRUE(embeddings2.empty());
  EXPECT_EQ(status2, ComputeEmbeddingsStatus::kCanceled);
  // "p2" is never submitted again.
  EXPECT_EQ(calls.size(), 2u);
  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobStatus", 4);
  histogram_tester.ExpectBucketCount(
      "History.Embeddings.ScheduledJobStatus.Passive",
      ComputeEmbeddingsStatus::kCanceled, 1);
}

// Verifies that a canceled job stops counting toward the job limit right away,
// even while some of its passages are being embedded.
TEST_F(SchedulingEmbedderTest, CanceledRunningJobDoesNotCountTowardMaxJobs) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/2u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  std::optional<Embedder::Job> job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, base::BindOnce(&IgnoreResults));
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, future2.GetCallback());
  ASSERT_EQ(calls.size(), 1u);
  job1.reset();

  // "p1" is still being embedded, but only job 2 counts toward the limit, so
  // job 3 is accepted rather than dropped.
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p3"}, future3.GetCallback());
  EXPECT_FALSE(future3.IsReady());

  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p1"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p2"));
  std::move(calls[1].callback)
      .Run(GenerateExpectedServiceOutput({"p2"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_THAT(calls[2].passages, ElementsAre("p3"));
  std::move(calls[2].callback)
      .Run(GenerateExpectedServiceOutput({"p3"}),
           ComputeEmbeddingsStatus::kSuccess);

  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that when a batch whose first job was canceled returns nothing, the
// next job isn't failed in its place. It goes back to the queue and is
// resubmitted.
TEST_F(SchedulingEmbedderTest,
       TryCancelFirstJobOfFailedBatchDoesNotFailNextJob) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/2u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 0 is submitted alone; jobs 1 and 2 wait and then share a batch.
  Embedder::Job job0 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p0"}, base::BindOnce(&IgnoreResults));
  ComputePassagesEmbeddingsFuture future1;
  std::optional<Embedder::Job> job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, future1.GetCallback());
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, future2.GetCallback());

  ASSERT_EQ(calls.size(), 1u);
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p0"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p1", "p2"));

  job1.reset();
  std::move(calls[1].callback)
      .Run({}, ComputeEmbeddingsStatus::kExecutionFailure);

  EXPECT_FALSE(future2.IsReady());
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_THAT(calls[2].passages, ElementsAre("p2"));
  std::move(calls[2].callback)
      .Run(GenerateExpectedServiceOutput({"p2"}),
           ComputeEmbeddingsStatus::kSuccess);

  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kCanceled);
  auto [passages2, embeddings2, job_id2, status2] = future2.Take();
  EXPECT_EQ(embeddings2.size(), 1u);
  EXPECT_EQ(status2, ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that a vector that can't be normalized in a canceled job's slice
// fails no other job. The job before it keeps its result, and the job after it
// goes back to the queue and is resubmitted.
TEST_F(SchedulingEmbedderTest, BadEmbeddingOfCanceledJobFailsNoOtherJob) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/3u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 0 is submitted alone; jobs 1, 2 and 3 wait and then share a batch.
  Embedder::Job job0 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p0"}, base::BindOnce(&IgnoreResults));
  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, future1.GetCallback());
  ComputePassagesEmbeddingsFuture future2;
  std::optional<Embedder::Job> job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, future2.GetCallback());
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p3"}, future3.GetCallback());

  ASSERT_EQ(calls.size(), 1u);
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p0"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p1", "p2", "p3"));

  // Job 2 is canceled, and the vector for "p2" is bad.
  job2.reset();
  const float kNaN = std::numeric_limits<float>::quiet_NaN();
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{kNaN, 1.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{0.0f, 1.0f}));
  std::move(calls[1].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  // Job 1 keeps its result. Job 3 isn't failed in job 2's place. The result
  // after the bad vector is dropped, so job 3 is resubmitted alone.
  ASSERT_TRUE(future1.IsReady());
  auto [passages1, embeddings1, job_id1, status1] = future1.Take();
  ASSERT_EQ(embeddings1.size(), 1u);
  EXPECT_THAT(embeddings1[0].GetData(), ElementsAre(1.0f, 0.0f));
  EXPECT_EQ(status1, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_FALSE(future3.IsReady());
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_THAT(calls[2].passages, ElementsAre("p3"));

  std::move(calls[2].callback)
      .Run(GenerateExpectedServiceOutput({"p3"}),
           ComputeEmbeddingsStatus::kSuccess);
  auto [passages3, embeddings3, job_id3, status3] = future3.Take();
  EXPECT_EQ(embeddings3.size(), 1u);
  EXPECT_EQ(status3, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kCanceled);
}

// Verifies that when a canceled job's slice comes before a vector that can't be
// normalized, the job that owns the bad vector fails right away, and the job
// after it is resubmitted.
TEST_F(SchedulingEmbedderTest, BadEmbeddingAfterCanceledJobFailsItsOwner) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/4u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 0 is submitted alone; jobs 1, 2 and 3 wait and then share a batch.
  Embedder::Job job0 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p0"}, base::BindOnce(&IgnoreResults));
  ComputePassagesEmbeddingsFuture future1;
  std::optional<Embedder::Job> job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1a", "p1b"}, future1.GetCallback());
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, future2.GetCallback());
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p3"}, future3.GetCallback());

  ASSERT_EQ(calls.size(), 1u);
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p0"}),
           ComputeEmbeddingsStatus::kSuccess);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p1a", "p1b", "p2", "p3"));

  // Job 1 is canceled, and the vector for "p2" is bad.
  job1.reset();
  const float kNaN = std::numeric_limits<float>::quiet_NaN();
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f, 0.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{kNaN, 1.0f}));
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{0.0f, 1.0f}));
  std::move(calls[1].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  // Job 2 owns the bad vector and fails right away. Job 3 is resubmitted alone.
  ASSERT_TRUE(future2.IsReady());
  auto [passages2, embeddings2, job_id2, status2] = future2.Take();
  EXPECT_TRUE(embeddings2.empty());
  EXPECT_EQ(status2, ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_FALSE(future3.IsReady());
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_THAT(calls[2].passages, ElementsAre("p3"));

  std::move(calls[2].callback)
      .Run(GenerateExpectedServiceOutput({"p3"}),
           ComputeEmbeddingsStatus::kSuccess);
  auto [passages3, embeddings3, job_id3, status3] = future3.Take();
  EXPECT_EQ(embeddings3.size(), 1u);
  EXPECT_EQ(status3, ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kCanceled);
}

TEST_F(SchedulingEmbedderTest, RecordsHistograms) {
  base::HistogramTester histogram_tester;
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  const auto record_callback =
      [&callbacks](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      };
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce(record_callback)
      .WillOnce(record_callback);

  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ComputePassagesEmbeddingsFuture future2;
  std::optional<Embedder::Job> job_to_cancel =
      embedder->ComputePassagesEmbeddings(
          PassagePriority::kUserInitiated,
          {"test passage 2a", "test passage 2b"}, future2.GetCallback());

  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());

  job_to_cancel.reset();

  ASSERT_EQ(callbacks.size(), 1u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(callbacks.size(), 2u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 3"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future2.Wait());
  ASSERT_TRUE(future3.Wait());

  // Only the two "passive priority" jobs successfully completed; the "user
  // initiate priority" one was canceled. So only two duration histogram samples
  // are logged, but three counts histograms samples and three status histogram
  // samples are logged as the all jobs were enqueued and completed in some way.
  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobDuration",
                                    2);
  histogram_tester.ExpectTotalCount(
      "History.Embeddings.ScheduledJobDuration.Passive", 2);

  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobStatus", 3);
  histogram_tester.ExpectTotalCount(
      "History.Embeddings.ScheduledJobStatus.Passive", 2);
  histogram_tester.ExpectBucketCount(
      "History.Embeddings.ScheduledJobStatus.Passive",
      ComputeEmbeddingsStatus::kSuccess, 2);
  histogram_tester.ExpectTotalCount(
      "History.Embeddings.ScheduledJobStatus.UserInitiated", 1);
  histogram_tester.ExpectBucketCount(
      "History.Embeddings.ScheduledJobStatus.UserInitiated",
      ComputeEmbeddingsStatus::kCanceled, 1);

  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobCount", 3);
  histogram_tester.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 0,
                                     1);
  histogram_tester.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 1,
                                     1);
  histogram_tester.ExpectBucketCount("History.Embeddings.ScheduledJobCount", 2,
                                     1);

  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledPassageCount",
                                    3);
  histogram_tester.ExpectBucketCount("History.Embeddings.ScheduledPassageCount",
                                     0, 1);
  histogram_tester.ExpectBucketCount("History.Embeddings.ScheduledPassageCount",
                                     1, 1);
  // When the third job is enqueued, 1 + 2 = 3 passages are waiting in the
  // previous two jobs.
  histogram_tester.ExpectBucketCount("History.Embeddings.ScheduledPassageCount",
                                     3, 1);
}

TEST_F(SchedulingEmbedderTest, SkipsHistogramsForGemma) {
  base::HistogramTester histogram_tester;
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false,
      /*execute_for_gemma=*/true);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  const auto record_callback =
      [&callbacks](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      };
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings).WillOnce(record_callback);

  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ASSERT_EQ(callbacks.size(), 1u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_TRUE(future1.Wait());

  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobDuration",
                                    0);
  histogram_tester.ExpectTotalCount(
      "History.Embeddings.ScheduledJobDuration.Passive", 0);

  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobStatus", 0);
  histogram_tester.ExpectTotalCount(
      "History.Embeddings.ScheduledJobStatus.Passive", 0);

  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledJobCount", 0);

  histogram_tester.ExpectTotalCount("History.Embeddings.ScheduledPassageCount",
                                    0);
}

TEST_F(SchedulingEmbedderTest, JobCanceledDuringQueueLimitCallback) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/2u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      });

  // Enqueue job1, `in_progress` is true since it's the only job.
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"},
      base::BindOnce(&IgnoreResults));

  // Enqueue job2, `in_progress` is false.
  std::optional<Embedder::Job> job2;
  bool callback_run = false;
  job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 2"},
      base::BindLambdaForTesting([&](std::vector<std::string> passages,
                                     std::vector<Embedding> embeddings,
                                     uint64_t job_id,
                                     ComputeEmbeddingsStatus status) {
        EXPECT_EQ(status, ComputeEmbeddingsStatus::kCanceled);
        job2.reset();
        callback_run = true;
      }));

  // Enqueue job3, exceeding queue limit. job2 should be synchronously canceled,
  // triggering its callback to reset job2.
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUrgent, {"test passage 3"},
      base::BindOnce(&IgnoreResults));

  EXPECT_TRUE(callback_run);
}

// Verifies that when the job queue is full, a new job with the same priority as
// the worst job in the queue is dropped, favoring the existing (older) job.
TEST_F(SchedulingEmbedderTest, LimitsJobCount) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/2u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  const auto record_callback =
      [&callbacks](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      };
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce(record_callback)
      .WillOnce(record_callback);

  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 2"}, future2.GetCallback());

  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());

  ASSERT_EQ(callbacks.size(), 1u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(callbacks.size(), 2u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 2"}),
           ComputeEmbeddingsStatus::kSuccess);

  // New job is dropped when the limit is reached and it doesn't have better
  // priority.
  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kCanceled);
}

TEST_F(SchedulingEmbedderTest, LimitsJobCountRespectsPriority) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/2u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  const auto record_callback =
      [&callbacks](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      };
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce(record_callback)
      .WillOnce(record_callback);

  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUrgent, {"test passage 2"}, future2.GetCallback());

  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());

  ASSERT_EQ(callbacks.size(), 1u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(callbacks.size(), 2u);
  ASSERT_FALSE(callbacks.back().is_null());
  // We expect future2 (Urgent) to be processed, not future3 (Passive).
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 2"}),
           ComputeEmbeddingsStatus::kSuccess);

  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kCanceled);
}

TEST_F(SchedulingEmbedderTest, LimitsJobCountDisplacesLowPriority) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/2u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  const auto record_callback =
      [&callbacks](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      };
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce(record_callback)
      .WillOnce(record_callback);

  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 2"}, future2.GetCallback());

  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUrgent, {"test passage 3"}, future3.GetCallback());

  ASSERT_EQ(callbacks.size(), 1u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(callbacks.size(), 2u);
  ASSERT_FALSE(callbacks.back().is_null());
  // We expect future3 (Urgent) to be processed, while future2 (Passive) was
  // displaced.
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 3"}),
           ComputeEmbeddingsStatus::kSuccess);

  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kCanceled);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that when the job queue is full and we must displace a job to make
// room for a higher-priority job, we displace the oldest of the tied worst jobs
// (FIFO).
TEST_F(SchedulingEmbedderTest, LimitsJobCountDisplacesOldestOfTiedWorst) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/3u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  const auto record_callback =
      [&callbacks](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      };
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce(record_callback)
      .WillOnce(record_callback)
      .WillOnce(record_callback);

  // Job 1 (Passive) starts immediately and is in_progress.
  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  // Job 2 (Passive, older) is enqueued.
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 2"}, future2.GetCallback());

  // Job 3 (Passive, newer) is enqueued. Queue is now full (size 3).
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());

  // Job 4 (Urgent) arrives. It should displace the older Passive job (Job 2)
  // instead of the newer one (Job 3) due to FIFO eviction.
  ComputePassagesEmbeddingsFuture future4;
  Embedder::Job job4 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUrgent, {"test passage 4"}, future4.GetCallback());

  ASSERT_EQ(callbacks.size(), 1u);
  ASSERT_FALSE(callbacks.back().is_null());
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(callbacks.size(), 2u);
  ASSERT_FALSE(callbacks.back().is_null());
  // We expect future4 (Urgent) to be processed next.
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 4"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(callbacks.size(), 3u);
  ASSERT_FALSE(callbacks.back().is_null());
  // We expect future3 (newer Passive) to be processed last.
  std::move(callbacks.back())
      .Run(GenerateExpectedServiceOutput({"test passage 3"}),
           ComputeEmbeddingsStatus::kSuccess);

  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kCanceled);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future4.Take()), ComputeEmbeddingsStatus::kSuccess);
}

TEST_F(SchedulingEmbedderTest, LimitsJobCountDisplacesOldestLowPriority) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/3u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<SchedulingEmbedder::GetEmbeddingsResultCallback> callbacks;
  const auto record_callback =
      [&callbacks](std::vector<std::string> passages, PassagePriority priority,
                   SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        callbacks.push_back(std::move(callback));
      };
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(record_callback);

  // Job 1 is active.
  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  // Job 2 is oldest pending (priority kLatent).
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kLatent, {"test passage 2"}, future2.GetCallback());

  // Job 3 is newer pending (priority kLatent).
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kLatent, {"test passage 3"}, future3.GetCallback());

  // Job 3b is newest pending (priority kLatent).
  // This should reject Job 3b immediately (tied priority with worst pending
  // when at capacity 3).
  ComputePassagesEmbeddingsFuture future3b;
  Embedder::Job job3b = embedder->ComputePassagesEmbeddings(
      PassagePriority::kLatent, {"test passage 3b"}, future3b.GetCallback());
  EXPECT_EQ(std::get<3>(future3b.Take()), ComputeEmbeddingsStatus::kCanceled);

  // Now add a priority kPassive job. This should displace the oldest kLatent
  // job (Job 2), while keeping Job 3.
  ComputePassagesEmbeddingsFuture future4;
  Embedder::Job job4 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 4"}, future4.GetCallback());

  // Job 2 should be canceled now.
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kCanceled);

  // Job 1 completes.
  ASSERT_EQ(callbacks.size(), 1u);
  std::move(callbacks[0])
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);

  // Job 4 should be next (higher priority than Job 3).
  ASSERT_EQ(callbacks.size(), 2u);
  std::move(callbacks[1])
      .Run(GenerateExpectedServiceOutput({"test passage 4"}),
           ComputeEmbeddingsStatus::kSuccess);

  // Job 3 should be next.
  ASSERT_EQ(callbacks.size(), 3u);
  std::move(callbacks[2])
      .Run(GenerateExpectedServiceOutput({"test passage 3"}),
           ComputeEmbeddingsStatus::kSuccess);

  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future4.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kSuccess);
}

// Verifies that if a batch is partially fulfilled by the service:
// 1. Fully completed jobs are finished.
// 2. Partially completed jobs are returned to the pending queue and correctly
//    resume (requesting only remaining passages) in the next batch.
// 3. Unreached jobs are returned to the pending queue.
// 4. All remaining jobs preserve their original relative scheduling order.
TEST_F(SchedulingEmbedderTest, PartialJobCompletionResumesAndPreservesOrder) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/10u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 1: 1 passage. This will be submitted immediately.
  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"passage 1"}, future1.GetCallback());
  ASSERT_EQ(calls.size(), 1u);

  // While Job 1 is in-flight, enqueue more jobs.
  // Job 2: 2 passages.
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"passage 2a", "passage 2b"},
      future2.GetCallback());
  // Job 3: 1 passage.
  ComputePassagesEmbeddingsFuture future3;
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"passage 3"}, future3.GetCallback());

  // Finish Job 1. This triggers batching of Job 2 and Job 3.
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);
  EXPECT_TRUE(future1.IsReady());

  // Batch 2 should contain Job 2 and Job 3.
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages,
              ElementsAre("passage 2a", "passage 2b", "passage 3"));

  // Service returns ONLY 1 embedding for this batch (partial Job 2).
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{2.1f, 0.0f}));
  std::move(calls[1].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  EXPECT_FALSE(future2.IsReady());
  EXPECT_FALSE(future3.IsReady());

  // Batch 3 should include Job 2 (resuming) and Job 3.
  ASSERT_EQ(calls.size(), 3u);
  EXPECT_THAT(calls[2].passages, ElementsAre("passage 2b", "passage 3"));

  // Service returns remaining results.
  std::vector<mojom::PassageEmbeddingsResultPtr> results2;
  results2.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{2.2f, 0.0f}));
  results2.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{3.0f, 0.0f}));
  std::move(calls[2].callback)
      .Run(std::move(results2), ComputeEmbeddingsStatus::kSuccess);

  // All jobs finished.
  auto [p2, e2, id2, s2] = future2.Take();
  EXPECT_THAT(e2[0].GetData(), ElementsAre(1.0f, 0.0f));
  EXPECT_THAT(e2[1].GetData(), ElementsAre(1.0f, 0.0f));
  auto [p3, e3, id3, s3] = future3.Take();
  EXPECT_THAT(e3[0].GetData(), ElementsAre(1.0f, 0.0f));
}

// Verifies that a single job with more passages than max_batch_size is
// correctly split across multiple batches and resumed.
TEST_F(SchedulingEmbedderTest, JobSpansMultipleBatchesDueToSize) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/2u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 1: 3 passages. Since max_batch_size is 2, it must be split.
  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1", "p2", "p3"}, future1.GetCallback());

  // Batch 1 should contain the first 2 passages.
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_THAT(calls[0].passages, ElementsAre("p1", "p2"));

  // Finish Batch 1.
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p1", "p2"}),
           ComputeEmbeddingsStatus::kSuccess);

  // Job 1 is not finished yet.
  EXPECT_FALSE(future1.IsReady());

  // Batch 2 should contain the remaining passage.
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p3"));

  // Finish Batch 2.
  std::move(calls[1].callback)
      .Run(GenerateExpectedServiceOutput({"p3"}),
           ComputeEmbeddingsStatus::kSuccess);

  // Job 1 should now be finished with all 3 embeddings.
  EXPECT_TRUE(future1.IsReady());
  auto [passages, embeddings, task_id, status] = future1.Take();
  EXPECT_EQ(embeddings.size(), 3u);
}

// Verifies that a job is failed if it makes no progress after a partial
// completion, preventing infinite loops if a specific passage consistently
// causes the service to fail.
TEST_F(SchedulingEmbedderTest, FailsJobOnZeroEmbeddingsAfterPartialProgress) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/10u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back({std::move(passages), std::move(callback)});
          });

  // Job 1: 2 passages.
  ComputePassagesEmbeddingsFuture future1;
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1", "p2"}, future1.GetCallback());

  // Batch 1 should contain both passages.
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_THAT(calls[0].passages, ElementsAre("p1", "p2"));

  // Service returns ONLY 1 embedding (p1 done, p2 pending).
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(
      mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f}));
  std::move(calls[0].callback)
      .Run(std::move(results), ComputeEmbeddingsStatus::kSuccess);

  // Job 1 is not finished yet.
  EXPECT_FALSE(future1.IsReady());

  // Batch 2 should contain the remaining passage.
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_THAT(calls[1].passages, ElementsAre("p2"));

  // Service returns ZERO embeddings this time (e.g., consistent failure on p2).
  std::move(calls[1].callback)
      .Run({}, ComputeEmbeddingsStatus::kExecutionFailure);

  // Job 1 should now be finished with an error, PREVENTING an infinite loop.
  EXPECT_TRUE(future1.IsReady());
  auto [passages, embeddings, task_id, status] = future1.Take();
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kExecutionFailure);
  // On failure, the embeddings vector is cleared as per Embedder interface.
  EXPECT_EQ(embeddings.size(), 0u);
}

// Verifies that if the service returns kSuccess but zero embeddings for the
// lead job in a batch, the job is failed with kExecutionFailure to maintain
// the invariant that success implies all embeddings were provided.
TEST_F(SchedulingEmbedderTest, FailsJobOnSuccessWithNoData) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/10u,
      /*use_performance_scenario=*/false);

  SchedulingEmbedder::GetEmbeddingsResultCallback active_callback;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        active_callback = std::move(callback);
      });

  ComputePassagesEmbeddingsFuture future;
  Embedder::Job job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, future.GetCallback());

  // Service returns kSuccess but an empty results vector.
  std::move(active_callback).Run({}, ComputeEmbeddingsStatus::kSuccess);

  EXPECT_TRUE(future.IsReady());
  auto [passages, embeddings, task_id, status] = future.Take();
  EXPECT_EQ(status, ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_EQ(embeddings.size(), 0u);
}

// Verifies that ReprioritizeTasks correctly updates the priority of jobs
// whether they are currently pending or active (in-flight).
TEST_F(SchedulingEmbedderTest, ReprioritizeTasks) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  struct Call {
    std::vector<std::string> passages;
    PassagePriority priority;
    SchedulingEmbedder::GetEmbeddingsResultCallback callback;
  };
  std::vector<Call> calls;

  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            calls.push_back(
                {std::move(passages), priority, std::move(callback)});
          });

  // Task 1: Passive. Submitted immediately.
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, base::BindOnce(&IgnoreResults));
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].priority, PassagePriority::kPassive);

  // Task 2: Latent. Stays pending because Task 1 is active.
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kLatent, {"p2"}, base::BindOnce(&IgnoreResults));

  // Now Task 1 is ACTIVE, Task 2 is PENDING.
  // Reprioritize both to Urgent.
  job1.Reprioritize(PassagePriority::kUrgent);
  job2.Reprioritize(PassagePriority::kUrgent);

  // Finish Task 1.
  std::move(calls[0].callback)
      .Run(GenerateExpectedServiceOutput({"p1"}),
           ComputeEmbeddingsStatus::kSuccess);

  // Task 2 should now be submitted with its NEW priority (Urgent).
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[1].priority, PassagePriority::kUrgent);
  EXPECT_THAT(calls[1].passages, ElementsAre("p2"));
}

TEST_F(SchedulingEmbedderTest, ReprioritizeTasksPreservesFifoOrder) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/4u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  std::vector<std::vector<std::string>> submitted_passages;
  SchedulingEmbedder::GetEmbeddingsResultCallback active_callback;
  bool is_first = true;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillRepeatedly(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            submitted_passages.push_back(passages);
            if (is_first) {
              is_first = false;
              active_callback = std::move(callback);
            } else {
              std::move(callback).Run(GenerateExpectedServiceOutput(passages),
                                      ComputeEmbeddingsStatus::kSuccess);
            }
          });

  // Active job to block processing of pending jobs.
  Embedder::Job active_job = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUrgent, {"active"}, base::BindOnce(&IgnoreResults));

  // Job 1 (oldest pending): priority kLatent.
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kLatent, {"p1"}, base::BindOnce(&IgnoreResults));

  // Job 2 (newer pending): priority kPassive.
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, base::BindOnce(&IgnoreResults));

  // Reprioritize Job 1 from kLatent to kPassive. Since Job 1 was submitted
  // before Job 2, it should execute before Job 2 within kPassive.
  job1.Reprioritize(PassagePriority::kPassive);

  // Finish active job so pending jobs run.
  std::move(active_callback)
      .Run(GenerateExpectedServiceOutput({"active"}),
           ComputeEmbeddingsStatus::kSuccess);

  ASSERT_EQ(submitted_passages.size(), 3u);
  EXPECT_THAT(submitted_passages[1], ElementsAre("p1"));
  EXPECT_THAT(submitted_passages[2], ElementsAre("p2"));
}

// Verifies that jobs in the active (in-flight) batch are correctly accounted
// for in the total job limit, but are protected from being displaced by new
// higher-priority jobs. Only pending jobs should be displaced.
TEST_F(SchedulingEmbedderTest, ActiveJobsCountTowardsLimitButProtected) {
  auto embedder = std::make_unique<SchedulingEmbedder>(
      embedder_metadata_provider_.get(),
      base::BindRepeating(&GetEmbeddingsStub::GetEmbeddings,
                          base::Unretained(&get_embeddings_stub_)),
      /*max_jobs=*/2u,
      /*max_batch_size=*/1u,
      /*use_performance_scenario=*/false);

  SchedulingEmbedder::GetEmbeddingsResultCallback active_callback;
  EXPECT_CALL(get_embeddings_stub_, GetEmbeddings)
      .WillOnce([&](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
        active_callback = std::move(callback);
      });

  // Job 1: 1 passage. Becomes ACTIVE.
  Embedder::Job job1 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p1"}, base::BindOnce(&IgnoreResults));
  ASSERT_FALSE(active_callback.is_null());

  // Job 2: 1 passage. Becomes PENDING.
  ComputePassagesEmbeddingsFuture future2;
  Embedder::Job job2 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"p2"}, future2.GetCallback());

  // Queue is now full (1 active + 1 pending).
  // Job 3: Higher priority than both, but the active job is protected.
  Embedder::Job job3 = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUrgent, {"p3"}, base::BindOnce(&IgnoreResults));

  // Since active jobs are protected, the pending job (Job 2) is displaced.
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kCanceled);
}

}  // namespace passage_embeddings
