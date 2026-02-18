// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include "base/files/file_path.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class ContentAnnotatorFeatureList {
 public:
  ContentAnnotatorFeatureList() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kContentAnnotator, {{"kContentAnnotatorMaxPendingUrls", "2"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ContentAnnotatorServiceTest : public testing::Test {
 public:
  ContentAnnotatorServiceTest()
      : page_content_extraction_service_(
            /*os_crypt_async=*/nullptr,
            base::FilePath(),
            /*tracker=*/nullptr),
        page_content_annotations_service_(
            page_content_annotations::TestPageContentAnnotationsService::Create(
                &optimization_guide_model_provider_,
                &history_service_)),
        service_(*page_content_annotations_service_,
                 page_content_extraction_service_,
                 mock_remote_model_executor_) {}
  ~ContentAnnotatorServiceTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  ContentAnnotatorFeatureList feature_list_;
  history::HistoryService history_service_;
  optimization_guide::TestOptimizationGuideModelProvider
      optimization_guide_model_provider_;
  page_content_annotations::PageContentExtractionService
      page_content_extraction_service_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  optimization_guide::MockRemoteModelExecutor mock_remote_model_executor_;
  ContentAnnotatorService service_;
};

// TODO(crbug.com/463734845): Remove/replace these tests with meaningful tests
// once the service has more public functionality that can be tested.
TEST_F(ContentAnnotatorServiceTest, OnPageContentAnnotatedSucceeds) {
  GURL url1("https://example.com/1");
  base::Time base_time = base::Time::Now();

  auto result = page_content_annotations::PageContentAnnotationsResult::
      CreateContentVisibilityScoreResult(0.5f);

  ASSERT_NO_FATAL_FAILURE(service_.OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(base_time, url1), result));
}

TEST_F(ContentAnnotatorServiceTest, OnLanguageDeterminedSucceeds) {
  GURL url1("https://example.com/1");
  translate::LanguageDetectionDetails details;
  details.url = url1;
  details.adopted_language = "en";

  ASSERT_NO_FATAL_FAILURE(service_.OnLanguageDetermined(details));
}

}  // namespace accessibility_annotator
