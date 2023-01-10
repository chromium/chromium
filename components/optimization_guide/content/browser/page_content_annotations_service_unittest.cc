// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/content/browser/test_page_content_annotator.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ::testing::_;

const TemplateURLService::Initializer kTemplateURLData[] = {
    {"default-engine.com", "http://default-engine.com/search?q={searchTerms}",
     "Default"},
    {"non-default-engine.com", "http://non-default-engine.com?q={searchTerms}",
     "Not Default"},
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;

  MOCK_METHOD(void,
              AddContentModelAnnotationsForVisit,
              (const history::VisitContentModelAnnotations&, history::VisitID),
              (override));

  MOCK_METHOD(void,
              AddSearchMetadataForVisit,
              (const GURL&, const std::u16string&, history::VisitID),
              (override));
};

}  // namespace

class PageContentAnnotationsServiceTest : public testing::Test {
 public:
  PageContentAnnotationsServiceTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {
              {"write_to_history_service", "true"},
          }},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{features::kPreventLongRunningPredictionModels});
  }
  ~PageContentAnnotationsServiceTest() override = default;

  void SetUp() override {
    optimization_guide_model_provider_ =
        std::make_unique<TestOptimizationGuideModelProvider>();
    history_service_ =
        std::make_unique<testing::StrictMock<MockHistoryService>>();

    template_url_service_ = std::make_unique<TemplateURLService>(
        kTemplateURLData, std::size(kTemplateURLData));

    // Instantiate service.
    service_ = std::make_unique<PageContentAnnotationsService>(
        /*autocomplete_provider_client=*/nullptr, "en-US",
        optimization_guide_model_provider_.get(), history_service_.get(),
        template_url_service_.get(),
        /*zero_suggest_cache_service=*/nullptr,
        /*database_provider=*/nullptr,
        /*database_dir=*/base::FilePath(),
        /*optimization_guide_logger=*/nullptr,
        /*background_task_runner=*/nullptr);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    test_annotator_ = std::make_unique<TestPageContentAnnotator>();
    test_annotator_->UseVisibilityScores(/*model_info=*/absl::nullopt,
                                         {{"test", 0.5}});
    service_->OverridePageContentAnnotatorForTesting(test_annotator_.get());
#endif
  }

  // Simulates a visit to URL.
  void VisitURL(const GURL& url,
                const std::u16string& title,
                history::VisitID visit_id,
                bool is_synced_visit = false) {
    history::URLRow url_row(url);
    url_row.set_title(title);
    history::VisitRow new_visit;
    new_visit.visit_id = visit_id;
    new_visit.originator_cache_guid = is_synced_visit ? "otherdevice" : "";
    service_->OnURLVisited(history_service_.get(), url_row, new_visit);
  }

 protected:
  std::unique_ptr<MockHistoryService> history_service_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<TestPageContentAnnotator> test_annotator_;
  std::unique_ptr<PageContentAnnotationsService> service_;
};

TEST_F(PageContentAnnotationsServiceTest, ObserveLocalVisitNonSearch) {
  history::VisitID visit_id = 1;

  // Should not call history service at all.

  VisitURL(GURL("https://example.com"), u"test", visit_id,
           /*is_synced_visit=*/false);
}

TEST_F(PageContentAnnotationsServiceTest, ObserveSyncedVisitsNonSearch) {
  history::VisitID visit_id = 1;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  EXPECT_CALL(*history_service_,
              AddContentModelAnnotationsForVisit(_, visit_id));
#endif

  VisitURL(GURL("https://example.com"), u"test", visit_id,
           /*is_synced_visit=*/true);
}

TEST_F(PageContentAnnotationsServiceTest, ObserveLocalVisitsSearch) {
  history::VisitID visit_id = 1;

  EXPECT_CALL(*history_service_, AddSearchMetadataForVisit(_, _, visit_id));

  // Should not send for annotation.

  VisitURL(GURL("https://default-engine.com/search?q=test#frag"), u"Test Page",
           visit_id,
           /*is_synced_visit=*/false);
}

TEST_F(PageContentAnnotationsServiceTest, ObserveSyncedVisitsSearch) {
  history::VisitID visit_id = 1;

  EXPECT_CALL(*history_service_, AddSearchMetadataForVisit(_, _, visit_id));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  EXPECT_CALL(*history_service_,
              AddContentModelAnnotationsForVisit(_, visit_id));
#endif

  VisitURL(GURL("https://default-engine.com/search?q=test#frag"), u"Test Page",
           visit_id,
           /*is_synced_visit=*/true);
}

}  // namespace optimization_guide