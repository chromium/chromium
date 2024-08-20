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
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace history_embeddings {

using optimization_guide::OptimizationGuideModelExecutor;

class HistoryEmbeddingsServicePublic : public HistoryEmbeddingsService {
 public:
  HistoryEmbeddingsServicePublic(
      history::HistoryService* history_service,
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service,
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      PassageEmbeddingsServiceController* service_controller,
      os_crypt_async::OSCryptAsync* os_crypt_async,
      OptimizationGuideModelExecutor* optimization_guide_model_executor)
      : HistoryEmbeddingsService(history_service,
                                 page_content_annotations_service,
                                 optimization_guide_model_provider,
                                 optimization_guide_decider,
                                 service_controller,
                                 os_crypt_async,
                                 optimization_guide_model_executor) {}

  using HistoryEmbeddingsService::Storage;

  using HistoryEmbeddingsService::OnPassagesEmbeddingsComputed;
  using HistoryEmbeddingsService::OnSearchCompleted;

  using HistoryEmbeddingsService::answerer_;
  using HistoryEmbeddingsService::embedder_metadata_;
  using HistoryEmbeddingsService::storage_;
};

class HistoryEmbeddingsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{kHistoryEmbeddings,
          {{"UseMlEmbedder", "false"},
           {"SearchPassageMinimumWordCount", "3"},
           {"UseMlAnswerer", "false"},
           {"EnableAnswers", "true"},
           {"FilterTerms", "term1,term2,Filter Phrase,TeRm3"},
           {"FilterHashes", "3962775614,4220142007,430397466"}}},
#if BUILDFLAG(IS_CHROMEOS)
         {chromeos::features::kFeatureManagementHistoryEmbedding, {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});

    CHECK(history_dir_.CreateUniqueTempDir());

    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    CHECK(history_service_);
    os_crypt_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);

    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();

    page_content_annotations_service_ =
        page_content_annotations::TestPageContentAnnotationsService::Create(
            optimization_guide_model_provider_.get(), history_service_.get());
    CHECK(page_content_annotations_service_);

    service_ = std::make_unique<HistoryEmbeddingsServicePublic>(
        history_service_.get(), page_content_annotations_service_.get(),
        optimization_guide_model_provider_.get(),
        optimization_guide_decider_.get(),
        /*service_controller=*/nullptr, os_crypt_.get(),
        /*optimization_guide_model_executor=*/nullptr);
  }

  void TearDown() override {
    if (service_) {
      service_->storage_.SynchronouslyResetForTest();
      service_->Shutdown();
    }
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
        [&](HistoryEmbeddingsServicePublic::Storage* storage) {
          std::unique_ptr<SqlDatabase::UrlDataIterator> iterator =
              storage->sql_database.MakeUrlDataIterator({});
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
    for (const std::string& passage : passages) {
      url_passages.passages.add_passages(passage);
    }
    service_->OnPassagesEmbeddingsComputed(
        /*embedding_cache=*/{}, std::move(url_passages), std::move(passages),
        std::move(passages_embeddings), status);
  }

  void SetMetadataScoreThreshold(double threshold) {
    service_->embedder_metadata_->search_score_threshold = threshold;
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
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider>
      optimization_guide_decider_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  page_content_annotations::TestPageContentAnnotator page_content_annotator_;
  std::unique_ptr<HistoryEmbeddingsServicePublic> service_;
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
  std::vector<std::string> passages = {"test passage 1", "test passage 2"};
  UrlPassages url_passages(/*url_id=*/1, /*visit_id=*/1, base::Time::Now());
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

TEST_F(HistoryEmbeddingsServiceTest, SearchSetsValidSessionId) {
  // Arbitrary constructed search results have no ID.
  SearchResult unfilled_result;
  EXPECT_TRUE(unfilled_result.session_id.empty());

  // Search results created by service search have new valid ID.
  base::test::TestFuture<SearchResult> future;
  service_->Search("", {}, 1, future.GetRepeatingCallback());
  EXPECT_FALSE(future.Take().session_id.empty());
}

TEST_F(HistoryEmbeddingsServiceTest, SearchCallsCallbackWithAnswer) {
  OverrideVisibilityScoresForTesting({
      {"passage with answer", 1},
  });

  auto create_scored_url_row = [&](history::VisitID visit_id, float score) {
    AddTestHistoryPage("http://answertest.com");
    ScoredUrlRow scored_url_row(ScoredUrl(1, visit_id, {}, score));
    scored_url_row.passages_embeddings.url_passages.passages.add_passages(
        "passage with answer");
    scored_url_row.passages_embeddings.url_embeddings.embeddings.emplace_back(
        std::vector<float>(768, 1.0f));
    scored_url_row.scores.push_back(score);
    return scored_url_row;
  };
  std::vector<ScoredUrlRow> scored_url_rows = {
      create_scored_url_row(1, 1),
  };

  base::test::TestFuture<SearchResult> future;
  service_->OnSearchCompleted(future.GetRepeatingCallback(), {},
                              scored_url_rows);

  // No answer on initial search result.
  SearchResult first_result = future.Take();
  EXPECT_TRUE(first_result.AnswerText().empty());

  // Then the answerer responds and another result is published with answer.
  SearchResult second_result = future.Take();
  EXPECT_FALSE(second_result.AnswerText().empty());
}

TEST_F(HistoryEmbeddingsServiceTest, SearchReportsHistograms) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<SearchResult> future;
  OverrideVisibilityScoresForTesting({{"", 0.99}});
  service_->Search("", {}, 1, future.GetRepeatingCallback());
  EXPECT_TRUE(future.Take().scored_url_rows.empty());

  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.Completed",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.UrlCount", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.Search.EmbeddingCount", 0, 1);
}

TEST_F(HistoryEmbeddingsServiceTest, SearchUsesCorrectThresholds) {
  OverrideVisibilityScoresForTesting({
      {"passage", 1},
  });

  auto create_scored_url_row = [&](history::VisitID visit_id, float score) {
    AddTestHistoryPage("http://test.com");
    ScoredUrlRow scored_url_row(ScoredUrl(1, visit_id, {}, score));
    scored_url_row.passages_embeddings.url_passages.passages.add_passages(
        "passage");
    scored_url_row.passages_embeddings.url_embeddings.embeddings.emplace_back(
        std::vector<float>(768, 1.0f));
    scored_url_row.scores.push_back(score);
    return scored_url_row;
  };
  std::vector<ScoredUrlRow> scored_url_rows = {
      create_scored_url_row(1, 1),
      create_scored_url_row(2, .8),
      create_scored_url_row(3, .6),
      create_scored_url_row(4, .4),
  };

  // Note, the block scopes are to cleanly separate searches since answers
  // come in late with repeated callbacks.
  {
    // Should default to .9 when neither the feature param nor metadata
    // thresholds are set.
    base::test::TestFuture<SearchResult> future;
    service_->OnSearchCompleted(future.GetRepeatingCallback(), {},
                                scored_url_rows);
    SearchResult result = future.Take();
    ASSERT_EQ(result.scored_url_rows.size(), 1u);
    EXPECT_EQ(result.scored_url_rows[0].scored_url.visit_id, 1);
  }

  {
    // Should use the metadata threshold when it's set.
    base::test::TestFuture<SearchResult> future;
    SetMetadataScoreThreshold(0.7);
    service_->OnSearchCompleted(future.GetRepeatingCallback(), {},
                                scored_url_rows);
    SearchResult result = future.Take();
    ASSERT_EQ(result.scored_url_rows.size(), 2u);
    EXPECT_EQ(result.scored_url_rows[0].scored_url.visit_id, 1);
    EXPECT_EQ(result.scored_url_rows[1].scored_url.visit_id, 2);
  }

  {
    // Should use the feature param threshold when it's set, even if the
    // metadata is also set.
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        kHistoryEmbeddings, {
                                {"UseMlEmbedder", "false"},
                                {"SearchPassageMinimumWordCount", "3"},
                                {"SearchScoreThreshold", "0.5"},
                            });
    base::test::TestFuture<SearchResult> future;
    service_->OnSearchCompleted(future.GetRepeatingCallback(), {},
                                scored_url_rows);
    SearchResult result = future.Take();
    ASSERT_EQ(result.scored_url_rows.size(), 3u);
    EXPECT_EQ(result.scored_url_rows[0].scored_url.visit_id, 1);
    EXPECT_EQ(result.scored_url_rows[1].scored_url.visit_id, 2);
    EXPECT_EQ(result.scored_url_rows[2].scored_url.visit_id, 3);
  }
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
  service_->Search("test query", {}, 3, future.GetRepeatingCallback());
  SearchResult result = future.Take();

  EXPECT_EQ(result.query, "test query");
  EXPECT_EQ(result.time_range_start, std::nullopt);
  EXPECT_EQ(result.count, 3u);

  EXPECT_EQ(result.scored_url_rows.size(), 2u);
  EXPECT_EQ(result.scored_url_rows[0].scored_url.url_id, 3);
  EXPECT_EQ(result.scored_url_rows[1].scored_url.url_id, 1);
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

TEST_F(HistoryEmbeddingsServiceTest, StaticHashVerificationTest) {
  EXPECT_EQ(history_embeddings::HashString("special"), 3962775614u);
  EXPECT_EQ(history_embeddings::HashString("something something"), 4220142007u);
  EXPECT_EQ(history_embeddings::HashString("hello world"), 430397466u);
}

TEST_F(HistoryEmbeddingsServiceTest, FilterTerms) {
  AddTestHistoryPage("http://test1.com");
  OnPassagesEmbeddingsComputed(UrlPassages(1, 1, base::Time::Now()),
                               {"term1", "term2", "Filter Phrase", "TeRm3"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::SUCCESS);
  OverrideVisibilityScoresForTesting({
      {"term1", 0.99},
      {"term2", 0.99},
      {"Filter Phrase", 0.99},
      {"TeRm3", 0.99},
      {"query without terms", 0.99},
      {"term1 in query", 0.99},
      {"query ending with term2", 0.99},
      {"query ending with tErM2", 0.99},
      {"query containing filTer phrAse", 0.99},
      {"query containing thefilter phrase-and-more", 0.99},
      {"query containing the filterphrase inexactly", 0.99},
      {"query with term3 in the middle", 0.99},
      {"query with TERM3 in the middle", 0.99},
      {"query with inexact te'rm3 in the middle", 0.99},
      {"query with 'term3', surrounded by punctuation", 0.99},
      {"query with non-ASCII ∅ character but no terms", 0.99},
      {"the word 'special' has its hash filtered", 0.99},
      {"the phrase 'something something' is also hash filtered", 0.99},
      {"this    Hello,   World!   is also hash filtered", 0.99},
      {"Hello | World is also filtered due to trimmed empty removal", 0.99},
      {"hellow orld is not filtered since its hash differs", 0.99},
  });
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query without terms", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query without terms");
    EXPECT_GT(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("term1 in query", {}, 3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "term1 in query");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query ending with term2", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query ending with term2");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query ending with tErM2", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query ending with tErM2");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query containing filTer phrAse", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query containing filTer phrAse");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query containing thefilter phrase-and-more", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query containing thefilter phrase-and-more");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query containing the filterphrase inexactly", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query containing the filterphrase inexactly");
    EXPECT_GT(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query with term3 in the middle", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with term3 in the middle");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query with TERM3 in the middle", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with TERM3 in the middle");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query with inexact te'rm3 in the middle", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with inexact te'rm3 in the middle");
    EXPECT_GT(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query with 'term3', surrounded by punctuation", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with 'term3', surrounded by punctuation");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("query with non-ASCII ∅ character but no terms", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with non-ASCII ∅ character but no terms");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("the word 'special' has its hash filtered", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "the word 'special' has its hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("the phrase 'something something' is also hash filtered",
                     {}, 3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query,
              "the phrase 'something something' is also hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("this    Hello,   World!   is also hash filtered", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "this    Hello,   World!   is also hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(
        "Hello | World is also filtered due to trimmed empty removal", {}, 3,
        future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query,
              "Hello | World is also filtered due to trimmed empty removal");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search("hellow orld is not filtered since its hash differs", {},
                     3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query,
              "hellow orld is not filtered since its hash differs");
    EXPECT_GT(result.count, 0u);
  }
}

TEST_F(HistoryEmbeddingsServiceTest, AnswerMocked) {
  auto* answerer = GetAnswerer();
  EXPECT_EQ(answerer->GetModelVersion(), 1);
  base::test::TestFuture<AnswererResult> future;
  answerer->ComputeAnswer("test query", Answerer::Context("1"),
                          future.GetCallback());
  AnswererResult result = future.Take();

  EXPECT_EQ(result.status, ComputeAnswerStatus::SUCCESS);
  EXPECT_EQ(result.query, "test query");
  EXPECT_EQ(result.answer.text(), "This is the answer to query 'test query'.");
}

}  // namespace history_embeddings
