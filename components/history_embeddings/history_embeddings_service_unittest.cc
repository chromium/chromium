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
#include "base/strings/string_number_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/token.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/core/search_strings_update_listener.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/mock_answerer.h"
#include "components/history_embeddings/mock_intent_classifier.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

using passage_embeddings::ComputeEmbeddingsStatus;
using passage_embeddings::Embedding;

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
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
      passage_embeddings::Embedder* embedder,
      std::unique_ptr<Answerer> answerer,
      std::unique_ptr<IntentClassifier> intent_classfier)
      : HistoryEmbeddingsService(os_crypt_async,
                                 history_service,
                                 page_content_annotations_service,
                                 optimization_guide_decider,
                                 embedder_metadata_provider,
                                 embedder,
                                 std::move(answerer),
                                 std::move(intent_classfier)) {}

  using HistoryEmbeddingsService::Storage;

  using HistoryEmbeddingsService::OnPassagesEmbeddingsComputed;
  using HistoryEmbeddingsService::OnSearchCompleted;
  using HistoryEmbeddingsService::QueryIsFiltered;
  using HistoryEmbeddingsService::RebuildAbsentEmbeddings;

  using HistoryEmbeddingsService::answerer_;
  using HistoryEmbeddingsService::embedder_metadata_;
  using HistoryEmbeddingsService::intent_classifier_;
  using HistoryEmbeddingsService::storage_;
};

class HistoryEmbeddingsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    FeatureParameters feature_parameters = GetFeatureParameters();
    feature_parameters.search_passage_minimum_word_count = 3;
    feature_parameters.word_match_min_embedding_score = 0;
    feature_parameters.word_match_required_term_ratio = 0;
    feature_parameters.scroll_tags_enabled = true;
    SetFeatureParametersForTesting(feature_parameters);

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
        passage_embeddings_test_env_.embedder_metadata_provider(),
        passage_embeddings_test_env_.embedder(),
        std::make_unique<MockAnswerer>(),
        std::make_unique<MockIntentClassifier>());
    ASSERT_TRUE(service_->embedder_metadata_.IsValid());

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

  void OnPassagesEmbeddingsComputed(UrlData url_passages,
                                    std::vector<std::string> passages,
                                    std::vector<Embedding> passages_embeddings,
                                    ComputeEmbeddingsStatus status) {
    for (const std::string& passage : passages) {
      url_passages.passages.add_passages(passage);
      url_passages.embeddings.emplace_back(std::vector<float>{});
    }
    service_->OnPassagesEmbeddingsComputed(std::move(url_passages),
                                           std::move(passages),
                                           std::move(passages_embeddings),
                                           /*task_id=*/0, status);
  }

  void SetMetadataScoreThreshold(double threshold) {
    service_->embedder_metadata_.search_score_threshold = threshold;
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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir history_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider>
      optimization_guide_decider_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  passage_embeddings::TestEnvironment passage_embeddings_test_env_;
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
  UrlData url_passages(/*url_id=*/1, /*visit_id=*/1, base::Time::Now());
  std::vector<Embedding> passages_embeddings = {
      Embedding(std::vector<float>(768, 1.0f)),
      Embedding(std::vector<float>(768, 1.0f))};
  OnPassagesEmbeddingsComputed(url_passages, passages, passages_embeddings,
                               ComputeEmbeddingsStatus::kSuccess);
  url_passages.url_id = 2;
  url_passages.visit_id = 2;
  OnPassagesEmbeddingsComputed(url_passages, passages, passages_embeddings,
                               ComputeEmbeddingsStatus::kSuccess);
  url_passages.url_id = 3;
  url_passages.visit_id = 3;
  OnPassagesEmbeddingsComputed(url_passages, passages, passages_embeddings,
                               ComputeEmbeddingsStatus::kSuccess);

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
  service_->Search(nullptr, "", {}, 1, /*skip_answering=*/false,
                   future.GetRepeatingCallback());
  EXPECT_FALSE(future.Take().session_id.empty());
}

TEST_F(HistoryEmbeddingsServiceTest, SearchCallsCallbackWithAnswer) {
  OverrideVisibilityScoresForTesting({
      {"A passage with five words.", 1},
  });

  auto create_scored_url_row = [&](history::VisitID visit_id, float score,
                                   float word_match_score) {
    AddTestHistoryPage("http://answertest.com");
    ScoredUrlRow scored_url_row(
        ScoredUrl(1, visit_id, {}, score, word_match_score));
    scored_url_row.passages_embeddings.passages.add_passages(
        "A passage with five words.");
    scored_url_row.passages_embeddings.embeddings.emplace_back(
        std::vector<float>(768, 1.0f));
    scored_url_row.scores.push_back(score);
    return scored_url_row;
  };
  std::vector<ScoredUrlRow> scored_url_rows = {
      create_scored_url_row(1, 1, 0),
  };

  base::test::TestFuture<SearchResult> future;
  SearchResult initial_result;
  initial_result.count = 3;
  initial_result.query = "this is a question!?";
  service_->OnSearchCompleted(future.GetRepeatingCallback(),
                              std::move(initial_result), scored_url_rows);

  // No answer on initial search result.
  SearchResult first_result = future.Take();
  EXPECT_EQ(ComputeAnswerStatus::kUnspecified,
            first_result.answerer_result.status);
  EXPECT_TRUE(first_result.AnswerText().empty());

  // Second result is published to indicate an answer is being attempted. The
  // answer should still be empty.
  SearchResult second_result = future.Take();
  EXPECT_EQ(second_result.answerer_result.status,
            ComputeAnswerStatus::kLoading);
  EXPECT_TRUE(second_result.AnswerText().empty());

  // Then the answerer responds and another result is published with answer.
  SearchResult final_result = future.Take();
  EXPECT_EQ(final_result.answerer_result.status, ComputeAnswerStatus::kSuccess);
  EXPECT_FALSE(final_result.AnswerText().empty());

  // Citation with scroll directive pointing to passage text.
  EXPECT_EQ(final_result.answerer_result.text_directives.size(), 1u);
  EXPECT_EQ(final_result.answerer_result.text_directives[0],
            "A passage,five words.");
}

TEST_F(HistoryEmbeddingsServiceTest, SearchReportsHistograms) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<SearchResult> future;
  OverrideVisibilityScoresForTesting({{"", 0.99}});
  service_->Search(nullptr, "", {}, 1, /*skip_answering=*/false,
                   future.GetRepeatingCallback());
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
                   /*skip_answering=*/false, future.GetRepeatingCallback());
  token = *base::Token::FromString(future.Take().session_id);
  EXPECT_NE(token.high(), 0u);
  EXPECT_EQ(token.low() & HistoryEmbeddingsService::kSessionIdSequenceBitMask,
            0u);

  // Likewise for first new result when previous result was empty.
  SearchResult result;
  service_->Search(&result, "", {}, 1, /*skip_answering=*/false,
                   future.GetRepeatingCallback());
  result = future.Take();
  token = *base::Token::FromString(result.session_id);
  EXPECT_NE(token.high(), 0u);
  EXPECT_EQ(token.low() & HistoryEmbeddingsService::kSessionIdSequenceBitMask,
            0u);

  // Random bits are preserved as sequence bits are incremented.
  for (size_t i = 1; i <= HistoryEmbeddingsService::kSessionIdSequenceBitMask;
       i++) {
    old_token = token;
    service_->Search(&result, "", {}, 1, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
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
  service_->Search(&result, "", {}, 1, /*skip_answering=*/false,
                   future.GetRepeatingCallback());
  result = future.Take();
  token = *base::Token::FromString(result.session_id);
  EXPECT_EQ(old_token, token);

  old_token = base::Token(old_token.high(), old_token.low() + 1);
  service_->Search(&result, "", {}, 1, /*skip_answering=*/false,
                   future.GetRepeatingCallback());
  result = future.Take();
  token = *base::Token::FromString(result.session_id);
  EXPECT_EQ(old_token, token);
}

TEST_F(HistoryEmbeddingsServiceTest, SearchUsesCorrectThresholds) {
  OverrideVisibilityScoresForTesting({
      {"passage", 1},
  });

  auto create_scored_url_row = [&](history::VisitID visit_id, float score,
                                   float word_match_score) {
    AddTestHistoryPage("http://test.com");
    ScoredUrlRow scored_url_row(
        ScoredUrl(1, visit_id, {}, score, word_match_score));
    scored_url_row.passages_embeddings.passages.add_passages("passage");
    scored_url_row.passages_embeddings.embeddings.emplace_back(
        std::vector<float>(768, 1.0f));
    scored_url_row.scores.push_back(score);
    return scored_url_row;
  };
  std::vector<ScoredUrlRow> scored_url_rows = {
      create_scored_url_row(1, 1, 0),
      create_scored_url_row(2, .8, 0),
      create_scored_url_row(3, .6, 0),
      create_scored_url_row(4, .4, 0),
  };
  SearchResult input_result;
  input_result.count = 3;

  // Note, the block scopes are to cleanly separate searches since answers
  // come in late with repeated callbacks.
  {
    // Should default to .9 when neither the feature param nor metadata
    // thresholds are set.
    base::test::TestFuture<SearchResult> future;
    service_->OnSearchCompleted(future.GetRepeatingCallback(),
                                input_result.Clone(), scored_url_rows);
    SearchResult result = future.Take();
    ASSERT_EQ(result.scored_url_rows.size(), 1u);
    EXPECT_EQ(result.scored_url_rows[0].scored_url.visit_id, 1);
  }

  {
    // Should use the metadata threshold when it's set.
    base::test::TestFuture<SearchResult> future;
    SetMetadataScoreThreshold(0.7);
    service_->OnSearchCompleted(future.GetRepeatingCallback(),
                                input_result.Clone(), scored_url_rows);
    SearchResult result = future.Take();
    ASSERT_EQ(result.scored_url_rows.size(), 2u);
    EXPECT_EQ(result.scored_url_rows[0].scored_url.visit_id, 1);
    EXPECT_EQ(result.scored_url_rows[1].scored_url.visit_id, 2);
  }

  {
    // Should use the feature param threshold when it's set, even if the
    // metadata is also set.
    FeatureParameters feature_parameters = GetFeatureParameters();
    feature_parameters.search_passage_minimum_word_count = 3;
    feature_parameters.word_match_min_embedding_score = 0;
    feature_parameters.search_score_threshold = 0.5;
    SetFeatureParametersForTesting(feature_parameters);
    base::test::TestFuture<SearchResult> future;
    service_->OnSearchCompleted(future.GetRepeatingCallback(),
                                input_result.Clone(), scored_url_rows);
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
  OnPassagesEmbeddingsComputed(UrlData(1, 1, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(2, 2, base::Time::Now()),
                               {"test passage 3", "test passage 4"},
                               {Embedding(std::vector<float>(768, -1.0f)),
                                Embedding(std::vector<float>(768, -1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(3, 3, base::Time::Now()),
                               {"test passage 5", "test passage 6"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);

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
  service_->Search(nullptr, "test query", {}, 3, /*skip_answering=*/false,
                   future.GetRepeatingCallback());
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
  OnPassagesEmbeddingsComputed(UrlData(1, 1, base::Time::Now()),
                               {"passage1", "passage2", "passage3", "passage4"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
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
                     /*skip_answering=*/false, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query without terms");
    EXPECT_GT(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "query with inexact spe'cial in the middle", {},
                     3, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with inexact spe'cial in the middle");
    EXPECT_GT(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "query with non-ASCII ∅ character but no terms",
                     {}, 3, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "query with non-ASCII ∅ character but no terms");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "the word 'special' has its hash filtered", {}, 3,
                     /*skip_answering=*/false, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "the word 'special' has its hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(
        nullptr, "the phrase 'something something' is also hash filtered", {},
        3, /*skip_answering=*/false, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query,
              "the phrase 'something something' is also hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(nullptr, "this    Hello,   World!   is also hash filtered",
                     {}, 3, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query, "this    Hello,   World!   is also hash filtered");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(
        nullptr, "Hello | World is also filtered due to trimmed empty removal",
        {}, 3, /*skip_answering=*/false, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_EQ(result.query,
              "Hello | World is also filtered due to trimmed empty removal");
    EXPECT_EQ(result.count, 0u);
  }
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(
        nullptr, "hellow orld is not filtered since its hash differs", {}, 3,
        /*skip_answering=*/false, future.GetRepeatingCallback());
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

  EXPECT_EQ(result.status, ComputeAnswerStatus::kSuccess);
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
  OnPassagesEmbeddingsComputed(UrlData(1, 1, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  {
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr, "boosted test query",
                     {}, 1, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // The word "test" in "boosted test query" boosts the score slightly.
    EXPECT_LT(std::ranges::max(row.scores), row.scored_url.score);
  }
  {
    // Default configuration allows ten terms in query before switching off
    // word match boosting.
    base::test::TestFuture<SearchResult> future;
    service_->Search(
        /*previous_search_result=*/nullptr,
        "this very very very very very long test query isn't boosted", {}, 1,
        /*skip_answering=*/false, future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // The word "test" makes no difference in the long query because
    // there are enough terms to disable word match boosting.
    EXPECT_FLOAT_EQ(std::ranges::max(row.scores), row.scored_url.score);
  }
}

TEST_F(HistoryEmbeddingsServiceTest, NoWordMatchBoostForLowTermCountRatio) {
  auto set_ratio = [](float ratio) {
    FeatureParameters feature_parameters = GetFeatureParameters();
    feature_parameters.search_passage_minimum_word_count = 3;
    feature_parameters.search_score_threshold = 0.5;
    feature_parameters.word_match_min_embedding_score = 0;
    feature_parameters.word_match_max_term_count = 4;
    feature_parameters.word_match_required_term_ratio = ratio;
    SetFeatureParametersForTesting(feature_parameters);
  };
  AddTestHistoryPage("http://test1.com");
  OverrideVisibilityScoresForTesting({
      {"boosted test query", 0.99},
      {"test passage one", 0.99},
      {"test passage two", 0.99},
  });
  OnPassagesEmbeddingsComputed(UrlData(1, 1, base::Time::Now()),
                               {"test passage one", "test passage two"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  {
    set_ratio(0.3f);
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr, "boosted test query",
                     {}, 1, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // The word "test" in "boosted test query" boosts the score slightly
    // because the ratio threshold is met: 0.3 < 0.333.
    EXPECT_LT(std::ranges::max(row.scores), row.scored_url.score);
  }
  {
    set_ratio(0.5f);
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr, "boosted test query",
                     {}, 1, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // The word "test" in "boosted test query" does not affect the
    // score because only one of three query terms is found, and 0.333 < 0.5.
    EXPECT_EQ(std::ranges::max(row.scores), row.scored_url.score);
  }
  {
    set_ratio(1.0f);
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr,
                     "test passage one more", {}, 1, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // No boost because 0.75 < 1.0.
    EXPECT_EQ(std::ranges::max(row.scores), row.scored_url.score);
  }
  {
    set_ratio(1.0f);
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr, "test passage one", {},
                     1, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // Boost because all terms are found.
    EXPECT_LT(std::ranges::max(row.scores), row.scored_url.score);
  }
  {
    set_ratio(1.0f);
    base::test::TestFuture<SearchResult> future;
    service_->Search(/*previous_search_result=*/nullptr, "test passage one two",
                     {}, 1, /*skip_answering=*/false,
                     future.GetRepeatingCallback());
    SearchResult result = future.Take();
    EXPECT_EQ(result.scored_url_rows.size(), 1u);
    const ScoredUrlRow& row = result.scored_url_rows[0];
    // Boost because all terms are found. This variant confirms counting
    // is done across all passages.
    EXPECT_LT(std::ranges::max(row.scores), row.scored_url.score);
  }
}

TEST_F(HistoryEmbeddingsServiceTest, WordMatchBoostAddsLowScoredResultItems) {
  // These parameter override values make it easy to have one embedding
  // exceed the threshold and another to fall below the threshold. Due
  // to how the mock embedder works, all 1's will score the square root of
  // the output size, sqrt(768) ~= 27.7128, so setting the threshold
  // just below this value and using a shorter embedding will differentiate.
  ScopedFeatureParametersForTesting params;
  params.Get().search_score_threshold = 27.7;
  params.Get().search_word_match_score_threshold = 0.01f;

  base::HistogramTester histogram_tester;
  AddTestHistoryPage("http://test1.com");
  AddTestHistoryPage("http://test2.com");
  AddTestHistoryPage("http://test3.com");
  OverrideVisibilityScoresForTesting({
      {"boosted test query", 0.99},
      {"test passage 1", 0.99},
      {"test passage 2", 0.99},
  });
  OnPassagesEmbeddingsComputed(UrlData(1, 1, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(2, 2, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 0.9f)),
                                Embedding(std::vector<float>(768, 0.9f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(3, 3, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 0.9f)),
                                Embedding(std::vector<float>(768, 0.9f))},
                               ComputeEmbeddingsStatus::kSuccess);

  base::test::TestFuture<SearchResult> future;
  service_->Search(/*previous_search_result=*/nullptr, "boosted test query", {},
                   2, /*skip_answering=*/false, future.GetRepeatingCallback());
  SearchResult result = future.Take();
  EXPECT_EQ(result.scored_url_rows.size(), 2u);
  EXPECT_GT(result.scored_url_rows[0].scored_url.score,
            GetFeatureParameters().search_score_threshold);
  EXPECT_LT(result.scored_url_rows[1].scored_url.score,
            GetFeatureParameters().search_score_threshold);
  EXPECT_GT(result.scored_url_rows[1].scored_url.word_match_score,
            GetFeatureParameters().search_word_match_score_threshold);

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumUrlsAddedByWordMatch", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.NumUrlsKeptByWordMatch", 1, 1);
}

TEST_F(HistoryEmbeddingsServiceTest, GetUrlData) {
  base::Time now = base::Time::Now();
  AddTestHistoryPage("http://test1.com");
  OnPassagesEmbeddingsComputed(UrlData(1, 1, now),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  {
    base::test::TestFuture<std::optional<UrlData>> future;
    service_->GetUrlData(1, future.GetCallback());
    auto url_data = future.Take();
    EXPECT_EQ(url_data->url_id, 1);
    EXPECT_EQ(url_data->visit_id, 1);
    EXPECT_EQ(url_data->visit_time, now);
    EXPECT_EQ(url_data->embeddings.size(), 2u);
    EXPECT_EQ(url_data->passages.passages_size(), 2);

    const auto& passages = url_data->passages.passages();
    EXPECT_EQ(passages[0], "test passage 1");
    EXPECT_EQ(passages[1], "test passage 2");

    // Note the word count gets set when storing the embedding with its passage.
    const auto& embeddings = url_data->embeddings;
    EXPECT_EQ(embeddings[0], Embedding(std::vector<float>(768, 1.0f), 3));
    EXPECT_EQ(embeddings[1], Embedding(std::vector<float>(768, 1.0f), 3));
  }
  {
    base::test::TestFuture<std::optional<UrlData>> future;
    service_->GetUrlData(2, future.GetCallback());
    auto url_data = future.Take();
    EXPECT_EQ(url_data, std::nullopt);
  }
}

TEST_F(HistoryEmbeddingsServiceTest, GetUrlDataInTimeRange) {
  base::Time now = base::Time::Now();
  AddTestHistoryPage("http://test1.com");
  OnPassagesEmbeddingsComputed(UrlData(1, 1, now + base::Seconds(1)),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(2, 2, now + base::Hours(1)),
                               {"test passage 3", "test passage 4"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(3, 3, now + base::Minutes(1)),
                               {"test passage 5", "test passage 6"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(4, 4, now),
                               {"test passage 7", "test passage 8"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  {
    base::test::TestFuture<std::vector<UrlData>> future;
    service_->GetUrlDataInTimeRange(now, now + base::Days(1), 8, 0,
                                    future.GetCallback());
    const auto url_datas = future.Take();
    {
      // The first is the earliest due to ordering by visit_time.
      const auto& url_data = url_datas.front();
      EXPECT_EQ(url_data.url_id, 4);
      EXPECT_EQ(url_data.visit_id, 4);
      EXPECT_EQ(url_data.visit_time, now);
      EXPECT_EQ(url_data.embeddings.size(), 2u);
      EXPECT_EQ(url_data.passages.passages_size(), 2);

      const auto& passages = url_data.passages.passages();
      EXPECT_EQ(passages[0], "test passage 7");
      EXPECT_EQ(passages[1], "test passage 8");
      const auto& embeddings = url_data.embeddings;
      EXPECT_EQ(embeddings[0], Embedding(std::vector<float>(768, 1.0f), 3));
      EXPECT_EQ(embeddings[1], Embedding(std::vector<float>(768, 1.0f), 3));
    }
    {
      // The last is the latest due to ordering by visit_time.
      const auto& url_data = url_datas.back();
      EXPECT_EQ(url_data.url_id, 2);
      EXPECT_EQ(url_data.visit_id, 2);
      EXPECT_EQ(url_data.visit_time, now + base::Hours(1));
      EXPECT_EQ(url_data.embeddings.size(), 2u);
      EXPECT_EQ(url_data.passages.passages_size(), 2);

      const auto& passages = url_data.passages.passages();
      EXPECT_EQ(passages[0], "test passage 3");
      EXPECT_EQ(passages[1], "test passage 4");
      const auto& embeddings = url_data.embeddings;
      EXPECT_EQ(embeddings[0], Embedding(std::vector<float>(768, 1.0f), 3));
      EXPECT_EQ(embeddings[1], Embedding(std::vector<float>(768, 1.0f), 3));
    }
  }
  {
    base::test::TestFuture<std::vector<UrlData>> future;
    // Inclusive lower bound; exclusive upper bound.
    service_->GetUrlDataInTimeRange(now + base::Minutes(1),
                                    now + base::Hours(1), 8, 0,
                                    future.GetCallback());
    const auto url_datas = future.Take();
    EXPECT_EQ(url_datas.front().url_id, 3);
    EXPECT_EQ(url_datas.back().url_id, 3);
  }
  {
    base::test::TestFuture<std::vector<UrlData>> future;
    // Check limit and offset.
    service_->GetUrlDataInTimeRange(now, now + base::Days(1), 2, 1,
                                    future.GetCallback());
    const auto url_datas = future.Take();
    EXPECT_EQ(url_datas.size(), 2u);
    EXPECT_EQ(url_datas.front().url_id, 1);
    EXPECT_EQ(url_datas.back().url_id, 3);
  }
}

namespace {

class AddSyncedVisitTask : public history::HistoryDBTask {
 public:
  AddSyncedVisitTask(base::RunLoop* run_loop,
                     const GURL& url,
                     const history::VisitRow& visit)
      : run_loop_(run_loop), url_(url), visit_(visit) {}

  AddSyncedVisitTask(const AddSyncedVisitTask&) = delete;
  AddSyncedVisitTask& operator=(const AddSyncedVisitTask&) = delete;

  ~AddSyncedVisitTask() override = default;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    history::VisitID visit_id = backend->AddSyncedVisit(
        url_, u"Title", /*hidden=*/false, visit_, std::nullopt, std::nullopt);
    EXPECT_NE(visit_id, history::kInvalidVisitID);
    return true;
  }

  void DoneRunOnMainThread() override { run_loop_->QuitWhenIdle(); }

 private:
  raw_ptr<base::RunLoop> run_loop_;

  GURL url_;
  history::VisitRow visit_;
};

}  // namespace

TEST_F(HistoryEmbeddingsServiceTest, SearchGetsIfUrlIsKnownToSync) {
  AddTestHistoryPage("http://not-synced.com");
  AddTestHistoryPage("http://synced.com");

  // Add a synced visit, as it would be created by HISTORY sync. The API to do
  // this isn't exposed in HistoryService (only HistoryBackend).
  {
    history::VisitRow visit;
    visit.visit_time = base::Time::Now() - base::Days(2);
    visit.originator_cache_guid = "some_originator";
    visit.transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
        ui::PAGE_TRANSITION_CHAIN_END);
    visit.is_known_to_sync = true;

    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service_->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<AddSyncedVisitTask>(&run_loop,
                                             GURL("http://synced.com"), visit),
        &tracker);
    run_loop.Run();
  }

  OnPassagesEmbeddingsComputed(UrlData(1, 1, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OnPassagesEmbeddingsComputed(UrlData(2, 2, base::Time::Now()),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 0.9f)),
                                Embedding(std::vector<float>(768, 0.9f))},
                               ComputeEmbeddingsStatus::kSuccess);

  base::test::TestFuture<SearchResult> future;
  OverrideVisibilityScoresForTesting({{"my query", 0.99}});
  service_->Search(nullptr, "my query", {}, 3, /*skip_answering=*/false,
                   future.GetRepeatingCallback());
  SearchResult result = future.Take();

  EXPECT_EQ(result.scored_url_rows.size(), 2u);
  EXPECT_EQ(result.scored_url_rows[0].scored_url.url_id, 1);
  EXPECT_EQ(result.scored_url_rows[0].is_url_known_to_sync, false);
  EXPECT_EQ(result.scored_url_rows[1].scored_url.url_id, 2);
  EXPECT_EQ(result.scored_url_rows[1].is_url_known_to_sync, true);
}

TEST_F(HistoryEmbeddingsServiceTest, CancelPreviousSearches) {
  base::Time now = base::Time::Now();
  AddTestHistoryPage("http://test1.com");
  OnPassagesEmbeddingsComputed(UrlData(1, 1, now),
                               {"test passage 1", "test passage 2"},
                               {Embedding(std::vector<float>(768, 1.0f)),
                                Embedding(std::vector<float>(768, 1.0f))},
                               ComputeEmbeddingsStatus::kSuccess);
  OverrideVisibilityScoresForTesting({
      {"test passage 1", 0.99},
      {"test passage 2", 0.99},
  });
  // Service uses the default .9 score threshold when neither the feature param
  // nor the metadata thresholds are set.
  SetMetadataScoreThreshold(0.01);

  base::test::TestFuture<SearchResult> future1;
  service_->Search(nullptr, "passage", {}, 3, /*skip_answering=*/true,
                   future1.GetRepeatingCallback());

  base::test::TestFuture<SearchResult> future2;
  service_->Search(nullptr, "passage", {}, 3, /*skip_answering=*/true,
                   future2.GetRepeatingCallback());

  base::test::TestFuture<SearchResult> future3;
  service_->Search(nullptr, "passage", {}, 3, /*skip_answering=*/true,
                   future3.GetRepeatingCallback());

  base::test::TestFuture<SearchResult> future4;
  service_->Search(nullptr, "passage", {}, 3, /*skip_answering=*/true,
                   future4.GetRepeatingCallback());

  // The first query is skipped.
  SearchResult result1 = future1.Take();
  EXPECT_FALSE(result1.session_id.empty());
  EXPECT_EQ(result1.query, "passage");
  ASSERT_EQ(result1.scored_url_rows.size(), 0u);

  // The second query is skipped.
  SearchResult result2 = future2.Take();
  EXPECT_FALSE(result2.session_id.empty());
  EXPECT_EQ(result2.query, "passage");
  ASSERT_EQ(result2.scored_url_rows.size(), 0u);

  // The third query is skipped.
  SearchResult result3 = future3.Take();
  EXPECT_FALSE(result3.session_id.empty());
  EXPECT_EQ(result3.query, "passage");
  ASSERT_EQ(result3.scored_url_rows.size(), 0u);

  // The last query is processed.
  SearchResult result4 = future4.Take();
  EXPECT_FALSE(result4.session_id.empty());
  EXPECT_EQ(result4.query, "passage");
  ASSERT_EQ(result4.scored_url_rows.size(), 1u);
  EXPECT_EQ(result4.scored_url_rows[0].scored_url.url_id, 1);
  EXPECT_EQ(result4.scored_url_rows[0].scored_url.visit_id, 1);
  EXPECT_EQ(result4.scored_url_rows[0].scored_url.visit_time, now);
}

TEST_F(HistoryEmbeddingsServiceTest, UseDatabaseBeforeEmbedder) {
  base::test::TestFuture<UrlData> store_future;
  service_->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  base::Time now = base::Time::Now();
  AddTestHistoryPage("http://test1.com");

  FeatureParameters feature_parameters = GetFeatureParameters();
  feature_parameters.erase_non_ascii_characters = true;
  SetFeatureParametersForTesting(feature_parameters);

  {
    base::HistogramTester histogram_tester;
    service_->ComputeAndStorePassageEmbeddings(
        /*url_id=*/1,
        /*visit_id=*/1,
        /*visit_time=*/now + base::Seconds(1),
        {
            "test passage 1",
            "test passage ß",
            "ßßß",
            "",
        });

    UrlData url_data = store_future.Take();
    ASSERT_EQ(url_data.passages.passages_size(), 4);
    ASSERT_EQ(url_data.embeddings.size(), 4u);
    ASSERT_EQ(url_data.passages.passages(0), "test passage 1");
    ASSERT_EQ(url_data.embeddings[0].Dimensions(), 768u);
    ASSERT_EQ(url_data.passages.passages(1), "test passage ß");
    ASSERT_EQ(url_data.embeddings[1].Dimensions(), 768u);
    ASSERT_EQ(url_data.passages.passages(2), "ßßß");
    ASSERT_EQ(url_data.embeddings[2].Dimensions(), 768u);
    ASSERT_EQ(url_data.passages.passages(3), "");
    ASSERT_EQ(url_data.embeddings[3].Dimensions(), 768u);

    // The cache wasn't used because there was no existing data.
    histogram_tester.ExpectTotalCount(
        "History.Embeddings.DatabaseCachedPassageTryCount", 1);
    histogram_tester.ExpectBucketCount(
        "History.Embeddings.DatabaseCachedPassageTryCount", 4, 1);
    histogram_tester.ExpectTotalCount(
        "History.Embeddings.DatabaseCachedPassageHitCount", 1);
    histogram_tester.ExpectBucketCount(
        "History.Embeddings.DatabaseCachedPassageHitCount", 0, 1);
  }
  {
    base::HistogramTester histogram_tester;
    service_->ComputeAndStorePassageEmbeddings(
        /*url_id=*/1,
        /*visit_id=*/2,
        /*visit_time=*/now + base::Minutes(1),
        {
            "test passage 1",
            "test passage ßßß",
            "ßßß",
            "",
        });

    UrlData url_data = store_future.Take();
    ASSERT_EQ(url_data.passages.passages_size(), 4);
    ASSERT_EQ(url_data.embeddings.size(), 4u);
    ASSERT_EQ(url_data.passages.passages(0), "test passage 1");
    ASSERT_EQ(url_data.embeddings[0].Dimensions(), 768u);
    ASSERT_EQ(url_data.passages.passages(1), "test passage ßßß");
    ASSERT_EQ(url_data.embeddings[1].Dimensions(), 768u);
    ASSERT_EQ(url_data.passages.passages(2), "ßßß");
    ASSERT_EQ(url_data.embeddings[2].Dimensions(), 768u);
    ASSERT_EQ(url_data.passages.passages(3), "");
    ASSERT_EQ(url_data.embeddings[3].Dimensions(), 768u);

    // The cache was used because there was existing data.
    histogram_tester.ExpectTotalCount(
        "History.Embeddings.DatabaseCachedPassageTryCount", 1);
    histogram_tester.ExpectBucketCount(
        "History.Embeddings.DatabaseCachedPassageTryCount", 4, 1);
    histogram_tester.ExpectTotalCount(
        "History.Embeddings.DatabaseCachedPassageHitCount", 1);
    histogram_tester.ExpectBucketCount(
        "History.Embeddings.DatabaseCachedPassageHitCount", 3, 1);
  }
}

TEST_F(HistoryEmbeddingsServiceTest, RebuildAbsentEmbeddings) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<UrlData> store_future;
  service_->SetPassagesStoredCallbackForTesting(
      store_future.GetRepeatingCallback());

  FeatureParameters feature_parameters = GetFeatureParameters();
  feature_parameters.erase_non_ascii_characters = true;
  SetFeatureParametersForTesting(feature_parameters);

  UrlData existing_url_data_1(1, 1, base::Time::Now());
  existing_url_data_1.passages.add_passages("test passage 1");
  existing_url_data_1.passages.add_passages("test passage ßßß");
  existing_url_data_1.passages.add_passages("ßßß");
  existing_url_data_1.passages.add_passages("");
  service_->RebuildAbsentEmbeddings({existing_url_data_1});

  UrlData url_data = store_future.Take();
  ASSERT_EQ(url_data.passages.passages_size(), 4);
  ASSERT_EQ(url_data.embeddings.size(), 4u);
  ASSERT_EQ(url_data.passages.passages(0), "test passage 1");
  ASSERT_EQ(url_data.embeddings[0].Dimensions(), 768u);
  ASSERT_EQ(url_data.passages.passages(1), "test passage ßßß");
  ASSERT_EQ(url_data.embeddings[1].Dimensions(), 768u);
  ASSERT_EQ(url_data.passages.passages(2), "ßßß");
  ASSERT_EQ(url_data.embeddings[2].Dimensions(), 768u);
  ASSERT_EQ(url_data.passages.passages(3), "");
  ASSERT_EQ(url_data.embeddings[3].Dimensions(), 768u);
}

}  // namespace history_embeddings
