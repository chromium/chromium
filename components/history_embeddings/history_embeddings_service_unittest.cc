// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace history_embeddings {

class HistoryEmbeddingsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{kHistoryEmbeddings,
          {{"UseMlEmbedder", "false"}, {"SearchPassageMinimumWordCount", "3"}}},
#if BUILDFLAG(IS_CHROMEOS)
         {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});

    OSCryptMocker::SetUp();

    CHECK(history_dir_.CreateUniqueTempDir());

    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    CHECK(history_service_);

    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();

    page_content_annotations_service_ =
        page_content_annotations::TestPageContentAnnotationsService::Create(
            optimization_guide_model_provider_.get(), history_service_.get());
    CHECK(page_content_annotations_service_);

    service_ = std::make_unique<HistoryEmbeddingsService>(
        history_service_.get(), page_content_annotations_service_.get(),
        optimization_guide_model_provider_.get(),
        /*service_controller=*/nullptr);
  }

  void TearDown() override {
    if (service_) {
      service_->storage_.SynchronouslyResetForTest();
      service_->Shutdown();
    }
    OSCryptMocker::TearDown();
  }

  void OverrideVisibilityScoresForTesting(
      const base::flat_map<std::string, double>& visibility_scores_for_input) {
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(
                base::FilePath(FILE_PATH_LITERAL("visibility_model")))
            .SetVersion(123)
            .Build();
    CHECK(model_info);
    page_content_annotator_.UseVisibilityScores(*model_info,
                                                visibility_scores_for_input);
    page_content_annotations_service_->OverridePageContentAnnotatorForTesting(
        &page_content_annotator_);
  }

  size_t CountEmbeddingsRows() {
    size_t result = 0;
    base::RunLoop loop;
    service_->storage_.PostTaskWithThisObject(base::BindLambdaForTesting(
        [&](HistoryEmbeddingsService::Storage* storage) {
          std::unique_ptr<SqlDatabase::EmbeddingsIterator> iterator =
              storage->sql_database.MakeEmbeddingsIterator({});
          if (!iterator) {
            return;
          }
          while (iterator->Next()) {
            result++;
          }

          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  void OnPassagesEmbeddingsComputed(UrlPassages url_passages,
                                    std::vector<std::string> passages,
                                    std::vector<Embedding> passages_embeddings,
                                    ComputeEmbeddingsStatus status) {
    service_->OnPassagesEmbeddingsComputed(
        std::move(url_passages), std::move(passages),
        std::move(passages_embeddings), status);
  }

  Answerer* GetAnswerer() { return service_->answerer_.get(); }

 protected:
  void AddTestHistoryPage(const std::string& url) {
    history_service_->AddPage(GURL(url), base::Time::Now() - base::Days(4), 0,
                              0, GURL(), history::RedirectList(),
                              ui::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED,
                              false);
  }

  base::test::ScopedFeatureList feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  page_content_annotations::TestPageContentAnnotator page_content_annotator_;
  std::unique_ptr<HistoryEmbeddingsService> service_;
};

TEST_F(HistoryEmbeddingsServiceTest, ConstructsAndInvalidatesWeakPtr) {
  auto weak_ptr = service_->AsWeakPtr();
  EXPECT_TRUE(weak_ptr);
  // This is required to synchronously reset storage on separate sequence.
  TearDown();
  service_.reset();
  EXPECT_FALSE(weak_ptr);
}

TEST_F(HistoryEmbeddingsServiceTest, OnHistoryDeletions) {
  AddTestHistoryPage("http://test1.com");
  AddTestHistoryPage("http://test2.com");
  AddTestHistoryPage("http://test3.com");

  // Add a fake set of passages for all visits.
  UrlPassages url_passages(/*url_id=*/1, /*visit_id=*/1, base::Time::Now());
  std::vector<std::string> passages = {"test passage 1", "test passage 2"};
  std::vector<Embedding> passages_embeddings = {
      Embedding(std::vector<float>(768, 1.0f)),
      Embedding(std::vector<float>(768, 1.0f))};
  OnPassagesEmbeddingsComputed(url_passages, passages, passages_embeddings,
                               ComputeEmbeddingsStatus::SUCCESS);
  url_passages.url_id = 2;
  url_passages.visit_id = 2;
  OnPassagesEmbeddingsComputed(url_passages, passages, passages_embeddings,
                               ComputeEmbeddingsStatus::SUCCESS);
  url_passages.url_id = 3;
  url_passages.visit_id = 3;
  OnPassagesEmbeddingsComputed(url_passages, passages, passages_embeddings,
                               ComputeEmbeddingsStatus::SUCCESS);

  // Verify that we find all three passages initially.
  EXPECT_EQ(CountEmbeddingsRows(), 3U);

  // Verify that we can delete indivdiual URLs.
  history_service_->DeleteURLs({GURL("http://test2.com")});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_EQ(CountEmbeddingsRows(), 2U);

  // Verify that we can delete all of History at once.
  base::CancelableTaskTracker tracker;
  history_service_->ExpireHistoryBetween(
      /*restrict_urls=*/{}, /*restrict_app_id=*/{},
      /*begin_time=*/base::Time(), /*end_time=*/base::Time(),
      /*user_initiated=*/true, base::BindLambdaForTesting([] {}), &tracker);
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_EQ(CountEmbeddingsRows(), 0U);
}

TEST_F(HistoryEmbeddingsServiceTest, SearchReportsHistograms) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<SearchResult> future;
  OverrideVisibilityScoresForTesting({{"", 0.99}});
  service_->Search("", {}, 1, future.GetCallback());
  EXPECT_TRUE(future.Take().scored_url_rows.empty());

  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.Completed",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.UrlCount", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.Search.EmbeddingCount", 0, 1);
}

TEST_F(HistoryEmbeddingsServiceTest, SearchFiltersLowScoringResults) {
  // Put results in to be found.
  AddTestHistoryPage("http://test1.com");
  AddTestHistoryPage("http://test2.com");
  AddTestHistoryPage("http://test3.com");
  OnPassagesEmbeddingsComputed(UrlPassages(1, 1, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::SUCCESS);
  OnPassagesEmbeddingsComputed(UrlPassages(2, 2, base::Time::Now()),
                               {"test passage 3", "test passage 4"},
                               {Embedding(std::vector<float>(768, -1.0f)),
                                Embedding(std::vector<float>(768, -1.0f))},
                               ComputeEmbeddingsStatus::SUCCESS);
  OnPassagesEmbeddingsComputed(UrlPassages(3, 3, base::Time::Now()),
                               {"test passage 5", "test passage 6"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::SUCCESS);

  // Search
  base::test::TestFuture<SearchResult> future;
  OverrideVisibilityScoresForTesting({
      {"test query", 0.99},
      {"test passage 1", 0.99},
      {"test passage 2", 0.99},
      {"test passage 3", 0.99},
      {"test passage 4", 0.99},
      {"test passage 5", 0.99},
      {"test passage 6", 0.99},
  });
  service_->Search("test query", {}, 3, future.GetCallback());
  SearchResult result = future.Take();

  EXPECT_EQ(result.query, "test query");
  EXPECT_EQ(result.time_range_start, std::nullopt);
  EXPECT_EQ(result.count, 3u);

  EXPECT_EQ(result.scored_url_rows.size(), 2u);
  EXPECT_EQ(result.scored_url_rows[0].scored_url.url_id, 1);
  EXPECT_EQ(result.scored_url_rows[1].scored_url.url_id, 3);
}

TEST_F(HistoryEmbeddingsServiceTest, CountWords) {
  extern size_t CountWords(const std::string& s);
  EXPECT_EQ(0u, CountWords(""));
  EXPECT_EQ(0u, CountWords(" "));
  EXPECT_EQ(1u, CountWords("a"));
  EXPECT_EQ(1u, CountWords(" a"));
  EXPECT_EQ(1u, CountWords("a "));
  EXPECT_EQ(1u, CountWords(" a "));
  EXPECT_EQ(1u, CountWords("  a  "));
  EXPECT_EQ(2u, CountWords("  a  b"));
  EXPECT_EQ(2u, CountWords("  a  b "));
  EXPECT_EQ(2u, CountWords("a  bc"));
  EXPECT_EQ(3u, CountWords("a  bc d"));
  EXPECT_EQ(3u, CountWords("a  bc  def "));
}

TEST_F(HistoryEmbeddingsServiceTest, AnswerMocked) {
  auto* answerer = GetAnswerer();
  EXPECT_EQ(answerer->GetModelVersion(), 1);
  base::test::TestFuture<AnswererResult> future;
  answerer->ComputeAnswer("test query", {}, future.GetCallback());
  AnswererResult result = future.Take();

  EXPECT_EQ(result.status, ComputeAnswerStatus::SUCCESS);
  EXPECT_EQ(result.query, "test query");
  EXPECT_EQ(result.answer, "This is the answer to query 'test query'.");
}

}  // namespace history_embeddings
