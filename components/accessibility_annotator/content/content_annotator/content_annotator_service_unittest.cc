// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/optional_ref.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/content_annotator/content_annotations_data.h"
#include "components/accessibility_annotator/core/content_annotator/content_classifier.h"
#include "components/accessibility_annotator/core/content_annotator/content_classifier_types.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/test_history_database.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/page_content_annotations/content/mock_page_content_services.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using ::page_content_annotations::MockPageEmbeddingsService;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Return;

MATCHER_P(EmbeddingDataEq, expected_data, "") {
  return ExplainMatchResult(ElementsAreArray(expected_data), arg.GetData(),
                            result_listener);
}

class ContentAnnotatorFeatureList {
 public:
  ContentAnnotatorFeatureList() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kContentAnnotator,
        {{features::kContentAnnotatorMaxPendingUrls.name, "2"}});
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
        page_content_annotations::PageEmbeddingsService&
            page_embeddings_service,
        AccessibilityAnnotatorBackend& accessibility_annotator_backend,
        history::HistoryService* history_service,
        passage_embeddings::Embedder* embedder,
        passage_embeddings::EmbedderMetadataProvider*
            embedder_metadata_provider,
        std::unique_ptr<ContentClassifier> content_classifier)
        : ContentAnnotatorService(page_content_annotations_service,
                                  page_content_extraction_service,
                                  optimization_guide_remote_model_executor,
                                  page_embeddings_service,
                                  accessibility_annotator_backend,
                                  history_service,
                                  embedder,
                                  embedder_metadata_provider,
                                  std::move(content_classifier)) {}
  };

  ContentAnnotatorServiceTest() = default;
  ~ContentAnnotatorServiceTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Initialize services here to ensure BrowserContext and TaskEnvironment are
    // ready.
    page_content_extraction_service_.emplace(
        /*os_crypt_async=*/nullptr, base::FilePath(),
        /*tracker=*/nullptr);

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    page_content_annotations_service_ =
        page_content_annotations::TestPageContentAnnotationsService::Create(
            &optimization_guide_model_provider_, history_service_.get());

    mock_remote_model_executor_ =
        std::make_unique<optimization_guide::MockRemoteModelExecutor>();

    mock_page_embeddings_service_ = std::make_unique<MockPageEmbeddingsService>(
        &page_content_extraction_service_.value());

    accessibility_annotator_backend_ =
        std::make_unique<AccessibilityAnnotatorBackendImpl>(
            /*history_service=*/nullptr,
            /*os_crypt_async=*/nullptr,
            temp_dir_.GetPath().Append(
                FILE_PATH_LITERAL("AccessibilityAnnotatorDatabase")));

    mock_embedder_ = std::make_unique<passage_embeddings::TestEmbedder>();

    mock_embedder_metadata_provider_ =
        std::make_unique<passage_embeddings::TestEmbedderMetadataProvider>();

    auto mock_classifier =
        std::make_unique<testing::StrictMock<MockContentClassifier>>();
    mock_classifier_ = mock_classifier.get();

    service_ = std::make_unique<TestContentAnnotatorService>(
        *page_content_annotations_service_, *page_content_extraction_service_,
        *mock_remote_model_executor_, *mock_page_embeddings_service_,
        *accessibility_annotator_backend_, history_service_.get(),
        mock_embedder_.get(), mock_embedder_metadata_provider_.get(),
        std::move(mock_classifier));
  }

  void TearDown() override {
    // Explicitly destroy services before the TestHarness tears down the
    // environment.
    mock_classifier_ = nullptr;
    service_.reset();
    mock_embedder_.reset();
    mock_embedder_metadata_provider_.reset();
    mock_page_embeddings_service_.reset();
    page_content_annotations_service_.reset();
    page_content_extraction_service_.reset();
    mock_remote_model_executor_.reset();
    accessibility_annotator_backend_.reset();

    // Ensure HistoryService tasks are complete before destroying it.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
    history_service_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  // Helper to trigger all necessary inputs for MaybeAnnotate.
  void TriggerClassification(const GURL& url, base::Time base_time) {
    // 0. Send URLVisited
    history::URLRow url_row(url);
    history::VisitRow visit_row;
    visit_row.visit_id = 1;
    visit_row.visit_time = base_time;
    history::VisitedURLInfo visited_url_info(url_row, visit_row);
    service_->OnURLVisited(nullptr, visited_url_info);

    // 1. Send PageContentAnnotated
    service_->OnPageContentAnnotated(
        page_content_annotations::HistoryVisit(base_time, url),
        page_content_annotations::PageContentAnnotationsResult::
            CreateContentVisibilityScoreResult(0.5f));

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
    std::unique_ptr<content::WebContents> web_contents =
        CreateTestWebContents();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents.get(), url);
    service_->OnPageContentExtracted(web_contents->GetPrimaryPage(),
                                     annotated_page_content);

    // 4. Send PageEmbeddingsAvailable
    EXPECT_CALL(*mock_page_embeddings_service_,
                GetEmbeddings(testing::Ref(web_contents->GetPrimaryPage())))
        .WillOnce(
            Return(std::vector<page_content_annotations::PassageEmbedding>{
                {{"Test Title",
                  page_content_annotations::EmbeddingPassageType::kTitle},
                 passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}}));
    service_->OnPageEmbeddingsAvailable(web_contents->GetPrimaryPage());
  }

  base::ScopedTempDir temp_dir_;
  ContentAnnotatorFeatureList feature_list_;
  std::unique_ptr<history::HistoryService> history_service_;
  optimization_guide::TestOptimizationGuideModelProvider
      optimization_guide_model_provider_;

  // Use optional/unique_ptr to control lifecycle
  std::optional<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  std::unique_ptr<optimization_guide::MockRemoteModelExecutor>
      mock_remote_model_executor_;
  std::unique_ptr<MockPageEmbeddingsService> mock_page_embeddings_service_;
  std::unique_ptr<AccessibilityAnnotatorBackend>
      accessibility_annotator_backend_;
  std::unique_ptr<passage_embeddings::TestEmbedder> mock_embedder_;
  std::unique_ptr<passage_embeddings::TestEmbedderMetadataProvider>
      mock_embedder_metadata_provider_;
  std::unique_ptr<TestContentAnnotatorService> service_;
  raw_ptr<testing::StrictMock<MockContentClassifier>> mock_classifier_;
};

TEST_F(ContentAnnotatorServiceTest, TestMaybeAnnotate_ClassificationTriggered) {
  GURL url("https://example.com/");
  base::Time base_time = base::Time::Now();

  EXPECT_CALL(*mock_classifier_, Classify(testing::_))
      .WillOnce(Return(ContentClassificationResult()));

  TriggerClassification(url, base_time);
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
                  Field(&ContentClassificationInput::visit_id,
                        Optional(static_cast<history::VisitID>(2))),
                  Field(&ContentClassificationInput::sensitivity_score,
                        Optional(testing::FloatEq(0.7f))),
                  Field(&ContentClassificationInput::navigation_timestamp,
                        Optional(nav_time2)),
                  Field(&ContentClassificationInput::adopted_language,
                        Optional(std::string("fr"))),
                  Field(&ContentClassificationInput::page_title,
                        Optional(std::string("Title 2"))),
                  Field(&ContentClassificationInput::annotated_page_content,
                        testing::Eq(apc2)),
                  Field(&ContentClassificationInput::page_title_embedding,
                        Optional(EmbeddingDataEq(
                            std::vector<float>{1.0f, 0.0f, 0.0f}))))))

      .WillOnce(Return(ContentClassificationResult()));

  // URL 1 shouldn't trigger classification because it's incomplete.
  EXPECT_CALL(*mock_classifier_,
              Classify(Field(&ContentClassificationInput::url, url1)))
      .Times(0);

  // 1. Send partial data for URL 1.
  history::URLRow url_row1(url1);
  history::VisitRow visit_row1;
  visit_row1.visit_id = 1;
  visit_row1.visit_time = base_time;
  history::VisitedURLInfo visited_url_info1(url_row1, visit_row1);
  service_->OnURLVisited(nullptr, visited_url_info1);

  service_->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(base_time, url1),
      page_content_annotations::PageContentAnnotationsResult::
          CreateContentVisibilityScoreResult(0.5f));

  // 2. Send all data for URL 2.
  history::URLRow url_row2(url2);
  history::VisitRow visit_row2;
  visit_row2.visit_id = 2;
  visit_row2.visit_time = nav_time2;
  history::VisitedURLInfo visited_url_info2(url_row2, visit_row2);
  service_->OnURLVisited(nullptr, visited_url_info2);

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

  EXPECT_CALL(*mock_page_embeddings_service_,
              GetEmbeddings(testing::Ref(web_contents2->GetPrimaryPage())))
      .WillOnce(Return(std::vector<page_content_annotations::PassageEmbedding>{
          {{"Title 2", page_content_annotations::EmbeddingPassageType::kTitle},
           passage_embeddings::Embedding({1.0f, 0.0f, 0.0f})}}));
  service_->OnPageEmbeddingsAvailable(web_contents2->GetPrimaryPage());
}

TEST_F(ContentAnnotatorServiceTest,
       TestMaybeAnnotate_ClassificationNotTriggeredWhenIncomplete) {
  GURL url("https://example.com/");
  base::Time base_time = base::Time::Now();

  // Expect Classify NOT to be called because PageContentExtracted is missing.
  EXPECT_CALL(*mock_classifier_, Classify(testing::_)).Times(0);

  // 0. Send URLVisited
  history::URLRow url_row(url);
  history::VisitRow visit_row;
  visit_row.visit_id = 1;
  visit_row.visit_time = base_time;
  history::VisitedURLInfo visited_url_info(url_row, visit_row);
  service_->OnURLVisited(nullptr, visited_url_info);

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
        TriggerClassification(url, base_time);
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
  {
    // Case 5: is_in_target_language is nullopt -> true
    base::HistogramTester scoped_tester;
    ContentClassificationResult result_null_lang;
    result_null_lang.title_keyword_result =
        ContentClassificationResult::Result();
    result_null_lang.title_keyword_result->category = "test_category";
    result_null_lang.is_sensitive = false;
    result_null_lang.is_in_target_language = std::nullopt;
    trigger_classification_fn(result_null_lang);
    scoped_tester.ExpectUniqueSample(
        "AccessibilityAnnotator.FullAnnotationReached", true, 1);
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
      ContentAnnotatorMissingDependentInformation::kVisitIdMissing, 1);
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
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kPageTitleEmbeddingMissing,
      1);
  histogram_tester.ExpectTotalCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing", 6);
}

TEST_F(ContentAnnotatorServiceTest, TestMaybeAnnotate_FullAnnotationReached) {
  // 1. Enable features::kContentAnnotatorEnableFullAnnotation flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kContentAnnotator,
      {{"content_annotator_enable_full_annotation", "true"}});

  GURL url("https://example.com/proto");
  base::Time base_time = base::Time::Now();

  // 2. Mock Classify to return a result that satisfies the `reached_annotation`
  // condition.
  ContentClassificationResult classifier_result;
  classifier_result.title_keyword_result =
      ContentClassificationResult::Result();
  classifier_result.title_keyword_result->category = "test category";
  classifier_result.is_sensitive = false;
  classifier_result.is_in_target_language = true;

  EXPECT_CALL(*mock_classifier_, Classify(_))
      .WillOnce(Return(classifier_result));

  // 3. Capture the callback passed to ExecuteModel.
  base::OnceCallback<void(
      optimization_guide::OptimizationGuideModelExecutionResult,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>)>
      captured_callback;

  EXPECT_CALL(
      *mock_remote_model_executor_,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContentAnnotation,
          /*request_metadata=*/_,
          /*options=*/_,
          /*callback=*/_))
      .Times(1)
      .WillOnce([&captured_callback](
                    auto feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const auto& options, auto callback) {
        captured_callback = std::move(callback);
      });

  TriggerClassification(url, base_time);

  // 4. Simulate the model execution by running the captured callback.
  ASSERT_TRUE(captured_callback);
  optimization_guide::proto::ContentAnnotationResponse mock_response_proto;
  optimization_guide::proto::ContentAnnotation* content_annotation =
      mock_response_proto.mutable_content_annotation();
  content_annotation->set_description("Test description");
  content_annotation->set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  optimization_guide::proto::Order* order =
      content_annotation->mutable_structured_data()->add_orders();
  order->set_id("order_123");

  optimization_guide::proto::Any any_proto;
  any_proto.set_type_url(base::StrCat(
      {"type.googleapis.com/", mock_response_proto.GetTypeName()}));
  any_proto.set_value(mock_response_proto.SerializeAsString());

  optimization_guide::OptimizationGuideModelExecutionResult mock_result(
      base::ok(any_proto), /*execution_info=*/nullptr);

  ASSERT_NO_FATAL_FAILURE(std::move(captured_callback)
                              .Run(std::move(mock_result),
                                   /*log_entry=*/nullptr));

  // 6. Verify that the data is cached in the backend.
  base::optional_ref<const ContentAnnotationsData> cached_data =
      accessibility_annotator_backend_->GetContentAnnotationsCacheData(
          static_cast<history::VisitID>(1));
  ASSERT_TRUE(cached_data.has_value());
  EXPECT_EQ(cached_data->content_annotation.description(), "Test description");
  EXPECT_EQ(cached_data->content_annotation.status(),
            optimization_guide::proto::ContentAnnotation::CONFIRMED);
  ASSERT_EQ(cached_data->content_annotation.structured_data().orders_size(), 1);
  EXPECT_EQ(cached_data->content_annotation.structured_data().orders(0).id(),
            "order_123");
  EXPECT_EQ(cached_data->page_title, "Test Title");
  EXPECT_EQ(cached_data->url, url);
  EXPECT_EQ(cached_data->navigation_timestamp, base_time);

  base::DictValue expected_classifier_results;
  expected_classifier_results.Set("title_keyword_result", "test category");
  EXPECT_EQ(cached_data->classifier_results, expected_classifier_results);
}

TEST_F(ContentAnnotatorServiceTest,
       TestMaybeAnnotate_FullAnnotationNotTriggeredOnFailure) {
  // 1. Enable features::kContentAnnotatorEnableFullAnnotation flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kContentAnnotator,
      {{"content_annotator_enable_full_annotation", "true"}});

  GURL url("https://example.com");
  base::Time base_time = base::Time::Now();

  // 2. Mock Classify to return a result that does not trigger full annotation.
  ContentClassificationResult classifier_result;
  classifier_result.is_sensitive = false;
  classifier_result.is_in_target_language = true;

  EXPECT_CALL(*mock_classifier_, Classify(_))
      .WillOnce(Return(classifier_result));

  // 3. Expect ExecuteModel to NOT be called on the mock_remote_model_executor_.
  EXPECT_CALL(
      *mock_remote_model_executor_,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContentAnnotation,
          /*request_metadata=*/_,
          /*options=*/_,
          /*callback=*/_))
      .Times(0);

  TriggerClassification(url, base_time);
}

TEST_F(ContentAnnotatorServiceTest,
       TestMaybeAnnotate_FullAnnotationNotTriggeredWhenFlagDisabled) {
  // 1. Disable features::kContentAnnotatorEnableFullAnnotation flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kContentAnnotator,
      {{"content_annotator_enable_full_annotation", "false"}});

  GURL url("https://example.com");
  base::Time base_time = base::Time::Now();

  // 2. Mock Classify to return a result that would trigger full annotation.
  ContentClassificationResult classifier_result;
  classifier_result.title_keyword_result =
      ContentClassificationResult::Result();
  classifier_result.title_keyword_result->category = "test category";
  classifier_result.is_sensitive = false;
  classifier_result.is_in_target_language = true;

  EXPECT_CALL(*mock_classifier_, Classify(_))
      .WillOnce(Return(classifier_result));

  // 3. Expect ExecuteModel to NOT be called on the mock_remote_model_executor_.
  EXPECT_CALL(
      *mock_remote_model_executor_,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContentAnnotation,
          /*request_metadata=*/_,
          /*options=*/_,
          /*callback=*/_))
      .Times(0);

  TriggerClassification(url, base_time);
}

// Tests that a metadata update enables semantic classification and subsequent
// full annotations.
TEST_F(ContentAnnotatorServiceTest,
       TestMaybeAnnotate_FullAnnotationReachedAfterEmbedderMetadataUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kContentAnnotator,
      {{"content_annotator_enable_full_annotation", "true"}});

  GURL url("https://example.com/");
  base::Time base_time = base::Time::Now();
  base::HistogramTester histogram_tester;

  // 1. Classification fails before metadata update.
  EXPECT_CALL(*mock_classifier_, Classify(_))
      .WillOnce(Return(ContentClassificationResult()));
  EXPECT_CALL(*mock_remote_model_executor_, ExecuteModel).Times(0);

  TriggerClassification(url, base_time);
  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.FullAnnotationReached", false, 1);

  // 2. Metadata update enables semantic classification.
  service_->EmbedderMetadataUpdated(passage_embeddings::EmbedderMetadata(1, 2));

  // 3. Classification succeeds after metadata update.
  ContentClassificationResult result;
  result.semantic_match_result = ContentClassificationResult::Result();
  result.semantic_match_result->category = "semantic_cat";
  result.is_sensitive = false;
  result.is_in_target_language = true;

  EXPECT_CALL(*mock_classifier_, Classify(_)).WillOnce(Return(result));
  EXPECT_CALL(*mock_remote_model_executor_, ExecuteModel).Times(1);

  TriggerClassification(url, base_time);
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.FullAnnotationReached", true, 1);
}

TEST_F(ContentAnnotatorServiceTest, TestOnVisitsDeleted_CancelsInProgressWork) {
  // 1. Enable features::kContentAnnotatorEnableFullAnnotation flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kContentAnnotator,
      {{"content_annotator_enable_full_annotation", "true"}});

  GURL url("https://example.com/proto");
  base::Time base_time = base::Time::Now();

  // 2. Mock Classify to return a result that triggers full annotation.
  ContentClassificationResult classifier_result;
  classifier_result.title_keyword_result =
      ContentClassificationResult::Result();
  classifier_result.title_keyword_result->category = "test category";
  classifier_result.is_sensitive = false;
  classifier_result.is_in_target_language = true;

  EXPECT_CALL(*mock_classifier_, Classify(_))
      .WillOnce(Return(classifier_result));

  // 3. Capture the callback passed to ExecuteModel.
  base::OnceCallback<void(
      optimization_guide::OptimizationGuideModelExecutionResult,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>)>
      captured_callback;

  EXPECT_CALL(
      *mock_remote_model_executor_,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContentAnnotation,
          /*request_metadata=*/_,
          /*options=*/_,
          /*callback=*/_))
      .Times(1)
      .WillOnce([&captured_callback](
                    auto feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const auto& options, auto callback) {
        captured_callback = std::move(callback);
      });

  TriggerClassification(url, base_time);

  ASSERT_TRUE(captured_callback);

  // 4. Simulate history deletion for visit_id 1.
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange::Invalid(), /*is_from_expiration=*/false,
      history::DeletionInfo::Reason::kOther, history::URLRows(),
      /*deleted_visit_ids=*/{1}, /*favicon_urls=*/{},
      /*restrict_urls=*/std::nullopt);
  service_->OnHistoryDeletions(nullptr, deletion_info);

  // 5. Simulate the model execution by running the captured callback.
  optimization_guide::proto::ContentAnnotationResponse mock_response_proto;
  optimization_guide::proto::ContentAnnotation* content_annotation =
      mock_response_proto.mutable_content_annotation();
  content_annotation->set_description("Test description");
  content_annotation->set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);

  optimization_guide::proto::Any any_proto;
  any_proto.set_type_url(base::StrCat(
      {"type.googleapis.com/", mock_response_proto.GetTypeName()}));
  any_proto.set_value(mock_response_proto.SerializeAsString());

  optimization_guide::OptimizationGuideModelExecutionResult mock_result(
      base::ok(any_proto), /*execution_info=*/nullptr);

  ASSERT_NO_FATAL_FAILURE(std::move(captured_callback)
                              .Run(std::move(mock_result),
                                   /*log_entry=*/nullptr));

  // 6. Verify that the data is NOT cached in the backend.
  base::optional_ref<const ContentAnnotationsData> cached_data =
      accessibility_annotator_backend_->GetContentAnnotationsCacheData(
          static_cast<history::VisitID>(1));
  EXPECT_FALSE(cached_data.has_value());
}

TEST_F(ContentAnnotatorServiceTest, TestOnURLVisited_PopulatesVisitId) {
  base::HistogramTester histogram_tester;
  GURL url1("https://example1.com/");
  GURL url2("https://example2.com/");
  GURL url3("https://example3.com/");
  base::Time base_time = base::Time::Now();

  // 1. Call OnURLVisited for URL1
  history::URLRow url_row1(url1);
  history::VisitRow visit_row1;
  visit_row1.visit_id = 101;
  visit_row1.visit_time = base_time;
  history::VisitedURLInfo visited_url_info1(url_row1, visit_row1);
  service_->OnURLVisited(nullptr, visited_url_info1);

  // 2. Add URL2 to cache
  translate::LanguageDetectionDetails details2;
  details2.url = url2;
  details2.adopted_language = "en";
  service_->OnLanguageDetermined(details2);

  // 3. Add URL3 to cache. This should trigger overflow and evict URL1.
  translate::LanguageDetectionDetails details3;
  details3.url = url3;
  details3.adopted_language = "en";
  service_->OnLanguageDetermined(details3);

  // URL1 should be evicted. It should have visit_id and navigation_timestamp.
  // So those should NOT be reported as missing.
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kVisitIdMissing, 0);
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kNavigationTimestampMissing,
      0);

  // It should report other missing fields.
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kSensitivityScoreMissing, 1);
}

TEST_F(ContentAnnotatorServiceTest, TestOnURLVisited_IgnoresSyncedVisits) {
  base::HistogramTester histogram_tester;
  GURL url1("https://example1.com/");
  GURL url2("https://example2.com/");
  GURL url3("https://example3.com/");
  GURL url4("https://example4.com/");
  base::Time base_time = base::Time::Now();

  // 1. Call OnURLVisited for URL1 with originator_cache_guid (synced visit)
  history::URLRow url_row1(url1);
  history::VisitRow visit_row1;
  visit_row1.visit_id = 101;
  visit_row1.visit_time = base_time;
  visit_row1.originator_cache_guid = "remote_device";
  history::VisitedURLInfo visited_url_info1(url_row1, visit_row1);
  service_->OnURLVisited(nullptr, visited_url_info1);

  // 2. Add URL2 to cache
  translate::LanguageDetectionDetails details2;
  details2.url = url2;
  details2.adopted_language = "en";
  service_->OnLanguageDetermined(details2);

  // 3. Add URL3 to cache. If URL1 was ignored, cache now has {URL2, URL3}.
  translate::LanguageDetectionDetails details3;
  details3.url = url3;
  details3.adopted_language = "en";
  service_->OnLanguageDetermined(details3);

  // 4. Add URL4 to cache. This should trigger overflow.
  // If URL1 was ignored, URL2 is evicted (lacks visit_id).
  // If URL1 was NOT ignored, URL1 was evicted earlier (has visit_id).
  translate::LanguageDetectionDetails details4;
  details4.url = url4;
  details4.adopted_language = "en";
  service_->OnLanguageDetermined(details4);

  // If URL1 was ignored, URL2 was evicted, and it lacks visit_id.
  // So kVisitIdMissing should be logged.
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kVisitIdMissing, 1);

  // Confirm URL1 was not evicted as it shouldn't be added to cache in the
  // first place. If it was evicted, it would log kAdoptedLanguageMissing since
  // it only had visit_id and timestamp.
  histogram_tester.ExpectBucketCount(
      "AccessibilityAnnotator.ContentAnnotator.DependentInformationMissing",
      ContentAnnotatorMissingDependentInformation::kAdoptedLanguageMissing, 0);
}

}  // namespace accessibility_annotator
