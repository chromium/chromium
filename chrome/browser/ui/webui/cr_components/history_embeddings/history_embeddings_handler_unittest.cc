// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/i18n/time_formatting.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/history_embeddings/mock_embedder.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/time_format.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

class MockPage : public history_embeddings::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<history_embeddings::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<history_embeddings::mojom::Page> receiver_{this};

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              SearchResultChanged,
              (history_embeddings::mojom::SearchResultPtr));
};

}  // namespace

std::unique_ptr<KeyedService> BuildTestHistoryEmbeddingsService(
    content::BrowserContext* browser_context) {
  return HistoryEmbeddingsServiceFactory::
      BuildServiceInstanceForBrowserContextForTesting(
          browser_context, std::make_unique<history_embeddings::MockEmbedder>(),
          /*answerer=*/nullptr,
          /*intent_classfier=*/nullptr);
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

class HistoryEmbeddingsHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{history_embeddings::kHistoryEmbeddings, {}},
                              {history_embeddings::kHistoryEmbeddingsAnswers,
                               {}},
                              {feature_engagement::kIPHHistorySearchFeature,
                               {}},
#if BUILDFLAG(IS_CHROMEOS)
                              {chromeos::features::
                                   kFeatureManagementHistoryEmbedding,
                               {{}}}
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});
    MockOptimizationGuideKeyedService::InitializeWithExistingTestLocalState();

    TestingProfile* profile_ = profile_manager()->CreateTestingProfile(
        "History Embeddings Test User",
        {
            TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
                HistoryServiceFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                HistoryEmbeddingsServiceFactory::GetInstance(),
                base::BindRepeating(&BuildTestHistoryEmbeddingsService)},
            TestingProfile::TestingFactory{
                PageContentAnnotationsServiceFactory::GetInstance(),
                base::BindRepeating(&BuildTestPageContentAnnotationsService)},
            TestingProfile::TestingFactory{
                OptimizationGuideKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&BuildTestOptimizationGuideKeyedService)},
        });

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());
    browser()->tab_strip_model()->AppendWebContents(std::move(web_contents_),
                                                    true);

    static_cast<TestBrowserWindow*>(window())->SetFeaturePromoController(
        std::make_unique<user_education::test::MockFeaturePromoController>());

    handler_ = std::make_unique<HistoryEmbeddingsHandler>(
        mojo::PendingReceiver<history_embeddings::mojom::PageHandler>(),
        profile_->GetWeakPtr(), web_ui());
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void TearDown() override {
    browser()->tab_strip_model()->CloseAllTabs();
    web_contents_.reset();
    handler_.reset();
    MockOptimizationGuideKeyedService::ResetForTesting();
    BrowserWithTestWindowTest::TearDown();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  user_education::test::MockFeaturePromoController* mock_promo_controller() {
    return static_cast<user_education::test::MockFeaturePromoController*>(
        static_cast<TestBrowserWindow*>(window())
            ->GetFeaturePromoControllerForTesting());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<HistoryEmbeddingsHandler> handler_;
  testing::NiceMock<MockPage> page_;
  base::HistogramTester histogram_tester_;
};

TEST_F(HistoryEmbeddingsHandlerTest, Searches) {
  auto query = history_embeddings::mojom::SearchQuery::New();
  query->query = "search query for empty result";
  base::test::TestFuture<history_embeddings::mojom::SearchResultPtr> future;
  EXPECT_CALL(page_, SearchResultChanged)
      .WillOnce(base::test::InvokeFuture(future));
  handler_->Search(std::move(query));
  auto result = future.Take();
  ASSERT_EQ(result->items.size(), 0u);
}

TEST_F(HistoryEmbeddingsHandlerTest, FormatsMojoResults) {
  history_embeddings::ScoredUrlRow scored_url_row(
      history_embeddings::ScoredUrl(0, 0, {}, .5));
  scored_url_row.row = history::URLRow{GURL{"https://google.com"}};
  scored_url_row.row.set_title(u"my title");
  scored_url_row.row.set_last_visit(base::Time::Now() - base::Hours(1));
  history_embeddings::ScoredUrlRow other_scored_url_row = scored_url_row;
  other_scored_url_row.row = history::URLRow(GURL("http://other.com"));

  history_embeddings::SearchResult embeddings_result;
  embeddings_result.scored_url_rows = {
      scored_url_row,
      other_scored_url_row,
  };
  embeddings_result.query = "search query";
  embeddings_result.answerer_result.status =
      history_embeddings::ComputeAnswerStatus::SUCCESS;
  embeddings_result.answerer_result.answer.set_text("the answer");
  embeddings_result.answerer_result.url = "http://other.com";
  embeddings_result.answerer_result.text_directives = {"text fragment"};

  base::test::TestFuture<history_embeddings::mojom::SearchResultPtr> future;
  EXPECT_CALL(page_, SearchResultChanged)
      .WillOnce(base::test::InvokeFuture(future));
  handler_->PublishResultToPageForTesting(std::move(embeddings_result));

  auto mojo_result = future.Take();
  EXPECT_EQ(mojo_result->query, "search query");
  EXPECT_EQ(mojo_result->answer_status,
            history_embeddings::mojom::AnswerStatus::kSuccess);
  EXPECT_EQ(mojo_result->answer, "the answer");
  ASSERT_EQ(mojo_result->items.size(), 2u);
  EXPECT_EQ(mojo_result->items[0]->title, "my title");
  EXPECT_EQ(mojo_result->items[0]->url.spec(), "https://google.com/");
  EXPECT_EQ(mojo_result->items[0]->relative_time,
            base::UTF16ToUTF8(ui::TimeFormat::Simple(
                ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
                base::Time::Now() - scored_url_row.row.last_visit())));
  EXPECT_EQ(mojo_result->items[0]->short_date_time,
            base::UTF16ToUTF8(
                base::TimeFormatShortDate(scored_url_row.row.last_visit())));
  EXPECT_EQ(mojo_result->items[0]->last_url_visit_timestamp,
            scored_url_row.row.last_visit().InMillisecondsFSinceUnixEpoch());
  EXPECT_EQ(mojo_result->items[0]->url_for_display, "google.com");
  EXPECT_EQ(mojo_result->items[0]->answer_data.is_null(), true);
  EXPECT_EQ(mojo_result->items[1]->url.spec(), "http://other.com/");
  EXPECT_EQ(mojo_result->items[1]->url_for_display, "other.com");
  EXPECT_EQ(mojo_result->items[1]->answer_data.is_null(), false);
  EXPECT_EQ(mojo_result->items[1]->answer_data->answer_text_directives.size(),
            1u);
  EXPECT_EQ(mojo_result->items[1]->answer_data->answer_text_directives[0],
            "text fragment");
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

TEST_F(HistoryEmbeddingsHandlerTest, ShowsPromo) {
  EXPECT_CALL(*mock_promo_controller(),
              MaybeShowPromo(user_education::test::MatchFeaturePromoParams(
                  feature_engagement::kIPHHistorySearchFeature)))
      .Times(1);
  handler_->MaybeShowFeaturePromo();
}
