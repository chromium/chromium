// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/time_format.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<KeyedService> BuildTestHistoryEmbeddingsService(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  CHECK(history_service);
  auto* page_content_annotations_service =
      PageContentAnnotationsServiceFactory::GetForProfile(profile);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return std::make_unique<history_embeddings::HistoryEmbeddingsService>(
      history_service, page_content_annotations_service,
      optimization_guide_keyed_service, nullptr);
}

std::unique_ptr<KeyedService> BuildTestPageContentAnnotationsService(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return page_content_annotations::TestPageContentAnnotationsService::Create(
      optimization_guide_keyed_service, history_service);
}

std::unique_ptr<KeyedService> BuildTestOptimizationGuideKeyedService(
    content::BrowserContext* browser_context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

class HistoryEmbeddingsHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{history_embeddings::kHistoryEmbeddings,
                               {{"UseMlEmbedder", "false"}}},
#if BUILDFLAG(IS_CHROMEOS)
                              {chromeos::features::
                                   kFeatureManagementHistoryEmbedding,
                               {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    MockOptimizationGuideKeyedService::InitializeWithExistingTestLocalState();

    profile_ = profile_manager_->CreateTestingProfile(
        "History Embeddings Test User",
        {
            {HistoryServiceFactory::GetInstance(),
             HistoryServiceFactory::GetDefaultFactory()},
            {HistoryEmbeddingsServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestHistoryEmbeddingsService)},
            {PageContentAnnotationsServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestPageContentAnnotationsService)},
            {OptimizationGuideKeyedServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestOptimizationGuideKeyedService)},
        });

    handler_ = std::make_unique<HistoryEmbeddingsHandler>(
        mojo::PendingReceiver<history_embeddings::mojom::PageHandler>(),
        profile_->GetWeakPtr());
  }

  void TearDown() override { handler_.reset(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<HistoryEmbeddingsHandler> handler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  base::HistogramTester histogram_tester_;
};

TEST_F(HistoryEmbeddingsHandlerTest, Searches) {
  auto query = history_embeddings::mojom::SearchQuery::New();
  query->query = "search query for empty result";
  base::test::TestFuture<history_embeddings::mojom::SearchResultPtr> future;
  handler_->Search(std::move(query), future.GetCallback());
  auto result = future.Take();
  ASSERT_EQ(result->items.size(), 0u);
}

TEST_F(HistoryEmbeddingsHandlerTest, FormatsMojoResults) {
  history_embeddings::ScoredUrlRow scored_url_row(
      history_embeddings::ScoredUrl(0, 0, {}, .5, 0u, {}));
  scored_url_row.row = history::URLRow{GURL{"https://google.com"}};
  scored_url_row.row.set_title(u"my title");
  scored_url_row.row.set_last_visit(base::Time::Now() - base::Hours(1));
  history_embeddings::SearchResult embeddings_result;
  embeddings_result.scored_url_rows = {scored_url_row};

  auto query = history_embeddings::mojom::SearchQuery::New();
  query->query = "search query";
  base::test::TestFuture<history_embeddings::mojom::SearchResultPtr> future;
  handler_->OnReceivedSearchResult(future.GetCallback(), embeddings_result);

  auto mojo_results = future.Take();
  ASSERT_EQ(mojo_results->items.size(), 1u);
  EXPECT_EQ(mojo_results->items[0]->title, "my title");
  EXPECT_EQ(mojo_results->items[0]->url.spec(), "https://google.com/");
  EXPECT_EQ(mojo_results->items[0]->relative_time,
            base::UTF16ToUTF8(ui::TimeFormat::Simple(
                ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
                base::Time::Now() - scored_url_row.row.last_visit())));
  EXPECT_EQ(mojo_results->items[0]->last_url_visit_timestamp,
            scored_url_row.row.last_visit().InMillisecondsFSinceUnixEpoch());
  EXPECT_EQ(mojo_results->items[0]->url_for_display, "google.com");
}

TEST_F(HistoryEmbeddingsHandlerTest, RecordsMetrics) {
  handler_->RecordSearchResultsMetrics(false, false);
  histogram_tester().ExpectBucketCount(
      "History.Embeddings.UserActions",
      HistoryEmbeddingsUserActions::kEmbeddingsSearch, 1);
  histogram_tester().ExpectBucketCount(
      "History.Embeddings.UserActions",
      HistoryEmbeddingsUserActions::kEmbeddingsNonEmptyResultsShown, 0);
  histogram_tester().ExpectBucketCount(
      "History.Embeddings.UserActions",
      HistoryEmbeddingsUserActions::kEmbeddingsResultClicked, 0);

  handler_->RecordSearchResultsMetrics(true, true);
  histogram_tester().ExpectBucketCount(
      "History.Embeddings.UserActions",
      HistoryEmbeddingsUserActions::kEmbeddingsSearch, 2);
  histogram_tester().ExpectBucketCount(
      "History.Embeddings.UserActions",
      HistoryEmbeddingsUserActions::kEmbeddingsNonEmptyResultsShown, 1);
  histogram_tester().ExpectBucketCount(
      "History.Embeddings.UserActions",
      HistoryEmbeddingsUserActions::kEmbeddingsResultClicked, 1);
}
