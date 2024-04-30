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
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

class HistoryEmbeddingsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        kHistoryEmbeddings, {{"UseMlEmbedder", "false"}});

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
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

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

  size_t CountEmbeddingsRows(HistoryEmbeddingsService* service) {
    size_t result = 0;
    base::RunLoop loop;
    service->storage_.PostTaskWithThisObject(base::BindLambdaForTesting(
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

 protected:
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
};

TEST_F(HistoryEmbeddingsServiceTest, ConstructsAndInvalidatesWeakPtr) {
  auto service = std::make_unique<HistoryEmbeddingsService>(
      history_service_.get(), page_content_annotations_service_.get(),
      optimization_guide_model_provider_.get(), /*service_controller=*/nullptr);
  auto weak_ptr = service->AsWeakPtr();
  EXPECT_TRUE(weak_ptr);
  service.reset();
  EXPECT_FALSE(weak_ptr);
}

TEST_F(HistoryEmbeddingsServiceTest, OnHistoryDeletions) {
  auto add_page = [&](const std::string& url) {
    history_service_->AddPage(GURL(url), base::Time::Now() - base::Days(4), 0,
                              0, GURL(), history::RedirectList(),
                              ui::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED,
                              false);
  };
  add_page("http://test1.com");
  add_page("http://test2.com");
  add_page("http://test3.com");

  auto service = std::make_unique<HistoryEmbeddingsService>(
      history_service_.get(), page_content_annotations_service_.get(),
      /*model_provider=*/nullptr, /*service_controller=*/nullptr);

  // Add a fake set of passages for all visits.
  UrlPassages url_passages(/*url_id=*/1, /*visit_id=*/1, base::Time::Now());
  std::vector<std::string> passages = {"test passage 1", "test passage 2"};
  std::vector<Embedding> passages_embeddings = {
      Embedding(std::vector<float>(768, 1.0f)),
      Embedding(std::vector<float>(768, 1.0f))};
  service->OnPassagesEmbeddingsComputed(url_passages, passages,
                                        passages_embeddings);
  url_passages.url_id = 2;
  url_passages.visit_id = 2;
  service->OnPassagesEmbeddingsComputed(url_passages, passages,
                                        passages_embeddings);
  url_passages.url_id = 3;
  url_passages.visit_id = 3;
  service->OnPassagesEmbeddingsComputed(url_passages, passages,
                                        passages_embeddings);

  // Verify that we find all three passages initially.
  EXPECT_EQ(CountEmbeddingsRows(service.get()), 3U);

  // Verify that we can delete indivdiual URLs.
  history_service_->DeleteURLs({GURL("http://test2.com")});
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_EQ(CountEmbeddingsRows(service.get()), 2U);

  // Verify that we can delete all of History at once.
  base::CancelableTaskTracker tracker;
  history_service_->ExpireHistoryBetween(
      /*restrict_urls=*/{}, /*restrict_app_id=*/{},
      /*begin_time=*/base::Time(), /*end_time=*/base::Time(),
      /*user_initiated=*/true, base::BindLambdaForTesting([] {}), &tracker);
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  EXPECT_EQ(CountEmbeddingsRows(service.get()), 0U);
}

TEST_F(HistoryEmbeddingsServiceTest, SearchReportsHistograms) {
  base::HistogramTester histogram_tester;
  auto service = std::make_unique<HistoryEmbeddingsService>(
      history_service_.get(), page_content_annotations_service_.get(),
      /*model_provider=*/nullptr, /*service_controller=*/nullptr);

  base::test::TestFuture<SearchResult> future;
  OverrideVisibilityScoresForTesting({{"", 0.99}});
  service->Search("", {}, 1, future.GetCallback());
  EXPECT_TRUE(future.Take().empty());

  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.Completed",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("History.Embeddings.Search.UrlCount", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.Search.EmbeddingCount", 0, 1);
}

}  // namespace history_embeddings
