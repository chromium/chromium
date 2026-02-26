// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using ::testing::Field;
using ::testing::Optional;
using ::testing::Return;

class ContentAnnotatorFeatureList {
 public:
  ContentAnnotatorFeatureList() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kContentAnnotator, {{kContentAnnotatorMaxPendingUrls.name, "2"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MockContentClassifier : public ContentClassifier {
 public:
  MockContentClassifier() = default;
  ~MockContentClassifier() override = default;

  MOCK_METHOD(ContentClassificationResult,
              Classify,
              (const ContentClassificationInput&),
              (const, override));
};

// Inherit from RenderViewHostTestHarness to provide WebContents and Page
// objects.
class ContentAnnotatorServiceTest : public content::RenderViewHostTestHarness {
 public:
  // A test-only class to allow for construction of the service.
  class TestContentAnnotatorService : public ContentAnnotatorService {
   public:
    TestContentAnnotatorService(
        page_content_annotations::PageContentAnnotationsService&
            page_content_annotations_service,
        page_content_annotations::PageContentExtractionService&
            page_content_extraction_service,
        optimization_guide::RemoteModelExecutor&
            optimization_guide_remote_model_executor,
        std::unique_ptr<ContentClassifier> content_classifier)
        : ContentAnnotatorService(page_content_annotations_service,
                                  page_content_extraction_service,
                                  optimization_guide_remote_model_executor,
                                  std::move(content_classifier)) {}
  };

  ContentAnnotatorServiceTest() = default;
  ~ContentAnnotatorServiceTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Initialize services here to ensure BrowserContext and TaskEnvironment are
    // ready.
    page_content_extraction_service_.emplace(
        /*os_crypt_async=*/nullptr, base::FilePath(),
        /*tracker=*/nullptr);

    page_content_annotations_service_ =
        page_content_annotations::TestPageContentAnnotationsService::Create(
            &optimization_guide_model_provider_, &history_service_);

    mock_remote_model_executor_ =
        std::make_unique<optimization_guide::MockRemoteModelExecutor>();

    auto mock_classifier =
        std::make_unique<testing::StrictMock<MockContentClassifier>>();
    mock_classifier_ = mock_classifier.get();

    service_ = std::make_unique<TestContentAnnotatorService>(
        *page_content_annotations_service_, *page_content_extraction_service_,
        *mock_remote_model_executor_, std::move(mock_classifier));
  }

  void TearDown() override {
    // Explicitly destroy services before the TestHarness tears down the
    // environment.
    mock_classifier_ = nullptr;
    service_.reset();
    page_content_annotations_service_.reset();
    page_content_extraction_service_.reset();
    mock_remote_model_executor_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  ContentAnnotatorFeatureList feature_list_;
  history::HistoryService history_service_;
  optimization_guide::TestOptimizationGuideModelProvider
      optimization_guide_model_provider_;

  // Use optional/unique_ptr to control lifecycle
  std::optional<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  std::unique_ptr<optimization_guide::MockRemoteModelExecutor>
      mock_remote_model_executor_;
  std::unique_ptr<ContentAnnotatorService> service_;
  raw_ptr<testing::StrictMock<MockContentClassifier>> mock_classifier_;
};

TEST_F(ContentAnnotatorServiceTest, TestMaybeAnnotate_ClassificationTriggered) {
  GURL url("https://example.com/");
  base::Time base_time = base::Time::Now();

  // 1. Send PageContentAnnotated
  auto result = page_content_annotations::PageContentAnnotationsResult::
      CreateContentVisibilityScoreResult(0.5f);
  service_->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(base_time, url), result);

  // 2. Send LanguageDetermined
  translate::LanguageDetectionDetails details;
  details.url = url;
  details.adopted_language = "en";
  service_->OnLanguageDetermined(details);

  // 3. Send PageContentExtracted
  scoped_refptr<page_content_annotations::RefCountedAnnotatedPageContent>
      annotated_page_content = base::MakeRefCounted<
          page_content_annotations::RefCountedAnnotatedPageContent>();
  annotated_page_content->data.mutable_main_frame_data()->set_title(
      "Test Title");

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents.get(),
                                                             url);

  // Expect Classify to be called when the final piece of data arrives.
  EXPECT_CALL(*mock_classifier_, Classify(testing::_))
      .WillOnce(Return(ContentClassificationResult()));

  service_->OnPageContentExtracted(web_contents->GetPrimaryPage(),
                                   annotated_page_content);
}

TEST_F(ContentAnnotatorServiceTest, TestMaybeAnnotate_TwoUrlsOnlyOneCompletes) {
  GURL url1("https://example1.com/");
  GURL url2("https://example2.com/");
  base::Time base_time = base::Time::Now();
  base::Time nav_time2 = base_time + base::Minutes(1);

  scoped_refptr<page_content_annotations::RefCountedAnnotatedPageContent> apc2 =
      base::MakeRefCounted<
          page_content_annotations::RefCountedAnnotatedPageContent>();
  apc2->data.mutable_main_frame_data()->set_title("Title 2");

  // Expect Classify to be called only for URL 2 with correct data
  EXPECT_CALL(*mock_classifier_,
              Classify(testing::AllOf(
                  Field(&ContentClassificationInput::url, url2),
                  Field(&ContentClassificationInput::sensitivity_score,
                        Optional(testing::FloatEq(0.7f))),
                  Field(&ContentClassificationInput::navigation_timestamp,
                        Optional(nav_time2)),
                  Field(&ContentClassificationInput::adopted_language,
                        Optional(std::string("fr"))),
                  Field(&ContentClassificationInput::page_title,
                        Optional(std::string("Title 2"))),
                  Field(&ContentClassificationInput::annotated_page_content,
                        testing::Eq(apc2)))))
      .WillOnce(Return(ContentClassificationResult()));

  // URL 1 shouldn't trigger classification because it's incomplete.
  EXPECT_CALL(*mock_classifier_,
              Classify(Field(&ContentClassificationInput::url, url1)))
      .Times(0);

  // 1. Send partial data for URL 1.
  service_->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(base_time, url1),
      page_content_annotations::PageContentAnnotationsResult::
          CreateContentVisibilityScoreResult(0.5f));

  // 2. Send all data for URL 2.
  service_->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(nav_time2, url2),
      page_content_annotations::PageContentAnnotationsResult::
          CreateContentVisibilityScoreResult(0.3f));

  translate::LanguageDetectionDetails details2;
  details2.url = url2;
  details2.adopted_language = "fr";
  service_->OnLanguageDetermined(details2);

  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents2.get(), url2);

  service_->OnPageContentExtracted(web_contents2->GetPrimaryPage(), apc2);
}

TEST_F(ContentAnnotatorServiceTest,
       TestMaybeAnnotate_ClassificationNotTriggeredWhenIncomplete) {
  GURL url("https://example.com/");
  base::Time base_time = base::Time::Now();

  // Expect Classify NOT to be called because PageContentExtracted is missing.
  EXPECT_CALL(*mock_classifier_, Classify(testing::_)).Times(0);

  // 1. Send PageContentAnnotated
  auto result = page_content_annotations::PageContentAnnotationsResult::
      CreateContentVisibilityScoreResult(0.5f);
  service_->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(base_time, url), result);

  // 2. Send LanguageDetermined
  translate::LanguageDetectionDetails details;
  details.url = url;
  details.adopted_language = "en";
  service_->OnLanguageDetermined(details);
}

TEST_F(ContentAnnotatorServiceTest,
       TestMaybeAnnotate_FullAnnotationReachedHistogram) {
  GURL url("https://example.com/");
  base::Time base_time = base::Time::Now();

  // Helper to trigger classification
  auto trigger_classification_fn =
      [&](ContentClassificationResult mock_result) {
        EXPECT_CALL(*mock_classifier_, Classify(testing::_))
            .WillOnce(Return(mock_result));

        service_->OnPageContentAnnotated(
            page_content_annotations::HistoryVisit(base_time, url),
            page_content_annotations::PageContentAnnotationsResult::
                CreateContentVisibilityScoreResult(0.5f));

        translate::LanguageDetectionDetails details;
        details.url = url;
        details.adopted_language = "en";
        service_->OnLanguageDetermined(details);

        scoped_refptr<page_content_annotations::RefCountedAnnotatedPageContent>
            apc = base::MakeRefCounted<
                page_content_annotations::RefCountedAnnotatedPageContent>();
        apc->data.mutable_main_frame_data()->set_title("Title");
        std::unique_ptr<content::WebContents> web_contents =
            CreateTestWebContents();
        content::NavigationSimulator::NavigateAndCommitFromBrowser(
            web_contents.get(), url);
        service_->OnPageContentExtracted(web_contents->GetPrimaryPage(), apc);
      };

  {
    // Case 1: No results -> false
    base::HistogramTester scoped_tester;
    trigger_classification_fn(ContentClassificationResult());
    scoped_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.FullAnnotationReached", false, 1);
  }
  {
    // Case 2: Title keyword result has category -> true
    base::HistogramTester scoped_tester;
    ContentClassificationResult result_with_title;
    result_with_title.title_keyword_result =
        ContentClassificationResult::Result();
    result_with_title.title_keyword_result->category = "test_category";
    result_with_title.is_sensitive = false;
    result_with_title.is_in_target_language = true;
    trigger_classification_fn(result_with_title);
    scoped_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.FullAnnotationReached", true, 1);
  }
  {
    // Case 3: URL match result has category -> true
    base::HistogramTester scoped_tester;
    ContentClassificationResult result_with_url;
    result_with_url.url_match_result = ContentClassificationResult::Result();
    result_with_url.url_match_result->category = "test_category";
    result_with_url.is_sensitive = false;
    result_with_url.is_in_target_language = true;
    trigger_classification_fn(result_with_url);
    scoped_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.FullAnnotationReached", true, 1);
  }
  {
    // Case 4: Result does not pass either classifier (no category) -> false
    base::HistogramTester scoped_tester;
    ContentClassificationResult result_no_match;
    result_no_match.is_sensitive = false;
    result_no_match.is_in_target_language = true;
    trigger_classification_fn(result_no_match);
    scoped_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.FullAnnotationReached", false, 1);
  }
}

TEST_F(ContentAnnotatorServiceTest,
       GetOrCreateJoinEntry_CacheOverflowLogsMissingFields) {
  base::HistogramTester histogram_tester;
  GURL url1("https://example1.com/");
  GURL url2("https://example2.com/");
  GURL url3("https://example3.com/");
  base::Time base_time = base::Time::Now();
  base::Time nav_time2 = base_time + base::Minutes(1);

  // 1. Add URL1
  translate::LanguageDetectionDetails details;
  details.url = url1;
  details.adopted_language = "en";
  service_->OnLanguageDetermined(details);

  // 2. Add URL2
  translate::LanguageDetectionDetails details2;
  details2.url = url2;
  details2.adopted_language = "en";
  service_->OnLanguageDetermined(details2);
  // Doesn't trigger overflow as URL2 is already in the cache.
  service_->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(base_time, url2),
      page_content_annotations::PageContentAnnotationsResult::
          CreateContentVisibilityScoreResult(0.5f));

  // 3. Add URL3. This should trigger the overflow check for URL1 (the LRU).
  service_->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(nav_time2, url3),
      page_content_annotations::PageContentAnnotationsResult::
          CreateContentVisibilityScoreResult(0.5f));

  // URL1 is missing everything except adopted_language.
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kSensitivityScoreMissing, 1);
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kNavigationTimestampMissing,
      1);
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kAdoptedLanguageMissing, 0);
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kPageTitleMissing, 1);
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kAnnotatedPageContentMissing,
      1);
  histogram_tester.ExpectTotalCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing", 4);
}

}  // namespace accessibility_annotator
