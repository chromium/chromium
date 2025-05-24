// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/internal/scheduling_embedder.h"

#include <initializer_list>
#include <memory>
#include <optional>
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

namespace {

using testing::ElementsAre;
using testing::Invoke;

using ComputePassagesEmbeddingsFuture =
    base::test::TestFuture<std::vector<std::string>,
                           std::vector<Embedding>,
                           SchedulingEmbedder::TaskId,
                           ComputeEmbeddingsStatus>;

std::vector<mojom::PassageEmbeddingsResultPtr> GenerateExpectedServiceOutput(
    std::initializer_list<std::string> passages) {
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  for (size_t i = 0; i < passages.size(); ++i) {
    results.push_back(
        mojom::PassageEmbeddingsResult::New(std::vector<float>{1.0f}));
  }
  return results;
}

void IgnoreResults(std::vector<std::string>,
                   std::vector<Embedding>,
                   SchedulingEmbedder::TaskId,
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
      .WillOnce(Invoke(
          [&](std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            requested_passages = std::move(passages);
            passage_priority = priority;
            result_callback = std::move(callback);
          }));

  embedder->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                      {"test passage 1"},
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
      .WillOnce(
          Invoke([](std::vector<std::string> passages, PassagePriority priority,
                    SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            std::vector<mojom::PassageEmbeddingsResultPtr> results;
            results.push_back(mojom::PassageEmbeddingsResult::New(
                std::vector<float>{1.0f, 0.0f}));
            results.push_back(mojom::PassageEmbeddingsResult::New(
                std::vector<float>{0.0f, 1.0f}));
            std::move(callback).Run(std::move(results),
                                    ComputeEmbeddingsStatus::kSuccess);
          }));

  ComputePassagesEmbeddingsFuture future;
  Embedder::TaskId task_id = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1", "test passage 2"},
      future.GetCallback());

  auto [passages, embeddings, received_task_id, status] = future.Take();
  EXPECT_THAT(passages, ElementsAre("test passage 1", "test passage 2"));
  ASSERT_EQ(embeddings.size(), 2u);
  EXPECT_THAT(embeddings[0].GetData(), ElementsAre(1.0f, 0.0f));
  EXPECT_THAT(embeddings[1].GetData(), ElementsAre(0.0f, 1.0f));
  EXPECT_EQ(task_id, received_task_id);
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
      .WillOnce(Invoke(save_call_parameters))
      .WillOnce(Invoke(save_call_parameters))
      .WillOnce(Invoke(save_call_parameters));

  // Submit a passive priority task.
  embedder->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                      {"test passage 1", "test passage 2"},
                                      base::BindOnce(&IgnoreResults));

  // Submit a user-initiated priority task. This will suspend the partially
  // completed passive priority task.
  embedder->ComputePassagesEmbeddings(PassagePriority::kUserInitiated,
                                      {"query"},
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
      .WillOnce(Invoke(
          [&requested_passages, &result_callback](
              std::vector<std::string> passages, PassagePriority priority,
              SchedulingEmbedder::GetEmbeddingsResultCallback callback) {
            requested_passages = std::move(passages);
            result_callback = std::move(callback);
          }));

  embedder->ComputePassagesEmbeddings(PassagePriority::kPassive,
                                      {"test passage 1"},
                                      base::BindOnce(&IgnoreResults));

  Embedder::TaskId second_task_id = embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 2"},
      base::BindOnce(&IgnoreResults));

  embedder->TryCancel(second_task_id);

  EXPECT_THAT(requested_passages, ElementsAre("test passage 1"));

  // Running the callback should not kick off any further processing. This is
  // validated by the WillOnce expectation above.
  ASSERT_FALSE(result_callback.is_null());
  std::move(result_callback)
      .Run(GenerateExpectedServiceOutput({"test passage 1"}),
           ComputeEmbeddingsStatus::kSuccess);
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
      .WillOnce(Invoke(record_callback))
      .WillOnce(Invoke(record_callback));

  ComputePassagesEmbeddingsFuture future1;
  embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ComputePassagesEmbeddingsFuture future2;
  Embedder::TaskId task_id = embedder->ComputePassagesEmbeddings(
      PassagePriority::kUserInitiated, {"test passage 2a", "test passage 2b"},
      future2.GetCallback());

  ComputePassagesEmbeddingsFuture future3;
  embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());
  embedder->TryCancel(task_id);

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
      .WillOnce(Invoke(record_callback))
      .WillOnce(Invoke(record_callback));

  ComputePassagesEmbeddingsFuture future1;
  embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 1"}, future1.GetCallback());

  ComputePassagesEmbeddingsFuture future2;
  embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 2"}, future2.GetCallback());

  ComputePassagesEmbeddingsFuture future3;
  embedder->ComputePassagesEmbeddings(
      PassagePriority::kPassive, {"test passage 3"}, future3.GetCallback());

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

  // Final job interrupts the job at back of line when the limit is reached.
  EXPECT_EQ(std::get<3>(future1.Take()), ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(std::get<3>(future2.Take()), ComputeEmbeddingsStatus::kCanceled);
  EXPECT_EQ(std::get<3>(future3.Take()), ComputeEmbeddingsStatus::kSuccess);
}

}  // namespace passage_embeddings
