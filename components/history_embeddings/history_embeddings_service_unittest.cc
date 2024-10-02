// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/token.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/core/search_strings_update_listener.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/mock_answerer.h"
#include "components/history_embeddings/mock_embedder.h"
#include "components/history_embeddings/mock_intent_classifier.h"
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

namespace {

base::FilePath GetTestFilePath(const std::string& file_name) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  return test_data_dir.AppendASCII("components/test/data/history_embeddings")
      .AppendASCII(file_name);
}

}  // namespace

class HistoryEmbeddingsServicePublic : public HistoryEmbeddingsService {
 public:
  HistoryEmbeddingsServicePublic(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      history::HistoryService* history_service,
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      std::unique_ptr<Embedder> embedder,
      std::unique_ptr<Answerer> answerer,
      std::unique_ptr<IntentClassifier> intent_classfier)
      : HistoryEmbeddingsService(os_crypt_async,
                                 history_service,
                                 page_content_annotations_service,
                                 optimization_guide_decider,
                                 std::move(embedder),
                                 std::move(answerer),
                                 std::move(intent_classfier)) {}

  using HistoryEmbeddingsService::Storage;

  using HistoryEmbeddingsService::OnPassagesEmbeddingsComputed;
  using HistoryEmbeddingsService::OnSearchCompleted;
  using HistoryEmbeddingsService::QueryIsFiltered;

  using HistoryEmbeddingsService::answerer_;
  using HistoryEmbeddingsService::embedder_metadata_;
  using HistoryEmbeddingsService::intent_classifier_;
  using HistoryEmbeddingsService::storage_;
};

class HistoryEmbeddingsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{kHistoryEmbeddings,
          {{"SearchPassageMinimumWordCount", "3"},
           {"WordMatchMinEmbeddingScore", "0"}}},
         {kHistoryEmbeddingsAnswers, {}},
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
        os_crypt_.get(), history_service_.get(),
        page_content_annotations_service_.get(),
        /*optimization_guide_decider=*/nullptr,
        std::make_unique<MockEmbedder>(), std::make_unique<MockAnswerer>(),
        std::make_unique<MockIntentClassifier>());

    ASSERT_TRUE(listener()->filter_words_hashes().empty());
    listener()->OnSearchStringsUpdate(
        GetTestFilePath("fake_search_strings_file"));
    task_environment_.RunUntilIdle();
    ASSERT_EQ(
        listener()->filter_words_hashes(),
        std::unordered_set<uint32_t>({3962775614, 4220142007, 430397466}));
  }

  void TearDown() override {
    if (service_) {
      service_->storage_.SynchronouslyResetForTest();
      service_->Shutdown();
    }
    listener()->ResetForTesting();
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
  IntentClassifier* GetIntentClassifier() {
    return service_->intent_classifier_.get();
  }

  SearchStringsUpdateListener* listener() {
    return SearchStringsUpdateListener::GetInstance();
  }

 protected:
  void AddTestHistoryPage(const std::string& url) {
    history_service_->AddPage(GURL(url), base::Time::Now() - base::Days(4), 0,
                              0, GURL(), history::RedirectList(),
                              ui::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED,
                              false);
  }

  base::test::ScopedFeatureList feature_list_;

  base::test::TaskEnvironment task_environment_;

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
  service_->Search(nullptr, "", {}, 1, future.GetRepeatingCallback());
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
  SearchResult initial_result;
  initial_result.query = "this is a question!?";
  service_->OnSearchCompleted(future.GetRepeatingCallback(),
                              std::move(initial_result), scored_url_rows);

  // No answer on initial search result.
  SearchResult first_result = future.Take();
  EXPECT_EQ(ComputeAnswerStatus::UNSPECIFIED,
            first_result.answerer_result.status);
  EXPECT_TRUE(first_result.AnswerText().empty());

  // Second result is published to indicate an answer is being attempted. The
  // answer should still be empty.
  SearchResult second_result = future.Take();
  EXPECT_EQ(second_result.answerer_result.status, ComputeAnswerStatus::LOADING);
  EXPECT_TRUE(second_result.AnswerText().empty());

  // Then the answerer responds and another result is published with answer.
  SearchResult final_result = future.Take();
  EXPECT_EQ(final_result.answerer_result.status, ComputeAnswerStatus::SUCCESS);
  EXPECT_FALSE(final_result.AnswerText().empty());
}

TEST_F(HistoryEmbeddingsServiceTest, SearchReportsHistograms) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<SearchResult> future;
  OverrideVisibilityScoresForTesting({{"", 0.99}});
  service_->Search(nullptr, "", {}, 1, future.GetRepeatingCallback());
  EXPECT_TRUE(future.Take().scored_url_rows.empty());

  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.Completed",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.UrlCount", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.Search.EmbeddingCount", 0, 1);
}

TEST_F(HistoryEmbeddingsServiceTest, SearchIncrementsSessionIdSequenceNumber) {
  base::test::TestFuture<SearchResult> future;
  base::Token old_token;
  base::Token token;

  // Specifying null produces a new random session_id with sequence number 0.
  service_->Search(/*previous_search_result=*/nullptr, "", {}, 1,
                   future.GetRepeatingCallback());
  token = *base::Token::FromString(future.Take().session_id);
  EXPECT_NE(token.high(), 0u);
  EXPECT_EQ(token.low() & HistoryEmbeddingsService::kSessionIdSequenceBitMask,
            0u);

  // Likewise for first new result when previous result was empty.
  SearchResult result;
  service_->Search(&result, "", {}, 1, future.GetRepeatingCallback());
  result = future.Take();
  token = *base::Token::FromString(result.session_id);
  EXPECT_NE(token.high(), 0u);
  EXPECT_EQ(token.low() & HistoryEmbeddingsService::kSessionIdSequenceBitMask,
            0u);

  // Random bits are preserved as sequence bits are incremented.
  for (size_t i = 1; i <= HistoryEmbeddingsService::kSessionIdSequenceBitMask;
       i++) {
    old_token = token;
    service_->Search(&result, "", {}, 1, future.GetRepeatingCallback());
    result = future.Take();
    token = *base::Token::FromString(result.session_id);
    EXPECT_EQ(token.high(), old_token.high());
    EXPECT_EQ(
        token.low() & ~HistoryEmbeddingsService::kSessionIdSequenceBitMask,
        old_token.low() & ~HistoryEmbeddingsService::kSessionIdSequenceBitMask);
    EXPECT_EQ(token.low() & HistoryEmbeddingsService::kSessionIdSequenceBitMask,
              i);

    // Skip most of the loop for test efficiency.
    if (i == 5) {
      i += HistoryEmbeddingsService::kSessionIdSequenceBitMask - 10;
      result.session_id =
          base::Token(token.high(),
                      (token.low() &
                       ~HistoryEmbeddingsService::kSessionIdSequenceBitMask) |
                          i)
              .ToString();
    }
  }
  old_token = token;

  // Additional increments simply overflow into the next higher bits.
  old_token = base::Token(old_token.high(), old_token.low() + 1);
  service_->Search(&result, "", {}, 1, future.GetRepeatingCallback());
  result = future.Take();
  token = *base::Token::FromString(result.session_id);
  EXPECT_EQ(old_token, token);

  old_token = base::Token(old_token.high(), old_token.low() + 1);
  service_->Search(&result, "", {}, 1, future.GetRepeatingCallback());
  result = future.Take();
  token = *base::Token::FromString(result.session_id);
  EXPECT_EQ(old_token, token);
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
        kHistoryEmbeddings, {{"SearchPassageMinimumWordCount", "3"},
                             {"SearchScoreThreshold", "0.5"},
                             {"WordMatchMinEmbeddingScore", "0"}});
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
  service_->Search(nullptr, "test query", {}, 3, future.GetRepeatingCallback());
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

TEST_F(HistoryEmbeddingsServiceTest, FilterWordsHashes) {
  AddTestHistoryPage("http://test1.com");
  OnPassagesEmbeddingsComputed(UrlPassages(1, 1, base::Time::Now()),
                               {"passage1", "passage2", "passage3", "passage4"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::SUCCESS);
  OverrideVisibilityScoresForTesting({
      {"query without terms", 0.99},
      {"query with inexact spe'cial in the middle", 0.99},
      {"query with non-ASCII ∅ character but no terms", 0.99},
      {"the word 'special' has its hash filtered", 0.99},
      {"the phrase 'something something' is also hash filtered", 0.99},
      {"this    Hello,   World!   is also hash filtered", 0.99},
      {"Hello | World is also filtered due to trimmed empty removal", 0.99},
      {"hellow orld is not filtered since its hash differs", 0.99},
  });
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "query without terms", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query without terms");
    EXPECT_GT(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "query with inexact spe'cial in the middle", {},
                     3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with inexact spe'cial in the middle");
    EXPECT_GT(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "query with non-ASCII ∅ character but no terms",
                     {}, 3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with non-ASCII ∅ character but no terms");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "the word 'special' has its hash filtered", {}, 3,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "the word 'special' has its hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr,
                     "the phrase 'something something' is also hash filtered",
                     {}, 3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query,
              "the phrase 'something something' is also hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "this    Hello,   World!   is also hash filtered",
                     {}, 3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "this    Hello,   World!   is also hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(
        nullptr, "Hello | World is also filtered due to trimmed empty removal",
        {}, 3, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query,
              "Hello | World is also filtered due to trimmed empty removal");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr,
                     "hellow orld is not filtered since its hash differs", {},
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

TEST_F(HistoryEmbeddingsServiceTest, IntentClassifierMocked) {
  EXPECT_EQ(GetIntentClassifier()->GetModelVersion(), 1);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    GetIntentClassifier()->ComputeQueryIntent(
        "can this query be answered, please and thank you?",
        future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::SUCCESS);
    EXPECT_EQ(is_query_answerable, true);
  }
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    GetIntentClassifier()->ComputeQueryIntent("any other query",
                                              future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::SUCCESS);
    EXPECT_EQ(is_query_answerable, false);
  }
}

TEST_F(HistoryEmbeddingsServiceTest, StopWordsExcludedFromQueryTerms) {
  SearchParams search_params;
  bool filtered = service_->QueryIsFiltered(
      "the stop and words, the, and, and, and and.", search_params);
  EXPECT_EQ(filtered, false);
  EXPECT_EQ(search_params.query_terms.size(), 2u);
  // Hash for "the" is 2374167618; hash for "and" is 754760635. These are stop
  // words in `fake_search_strings_file` test proto.
  EXPECT_EQ(search_params.query_terms,
            std::vector<std::string>({"stop", "words"}));
}

TEST_F(HistoryEmbeddingsServiceTest, SearchDoesNotWordMatchBoostLongQueries) {
  AddTestHistoryPage("http://test1.com");
  OverrideVisibilityScoresForTesting({
      {"boosted test query", 0.99},
      {"this very long test query isn't boosted", 0.99},
      {"test passage 1", 0.99},
      {"test passage 2", 0.99},
  });
  OnPassagesEmbeddingsComputed(UrlPassages(1, 1, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::SUCCESS);
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr, "boosted test query",
                     {}, 1, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // The word "test" in "boosted test query" boosts the score slightly.
    EXPECT_LT(std::ranges::max(row.scores), row.scored_url.score);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr,
                     "this very long test query isn't boosted", {}, 1,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // The word "test" makes no difference in the long query because
    // there are enough terms to disable word match boosting.
    EXPECT_FLOAT_EQ(std::ranges::max(row.scores), row.scored_url.score);
  }
}

}  // namespace history_embeddings
