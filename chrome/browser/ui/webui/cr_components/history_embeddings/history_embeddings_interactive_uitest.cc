// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/history_embeddings/mock_embedder.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "content/public/test/browser_test.h"

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistoryTabId);
#endif

}  // namespace

class HistoryEmbeddingsInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{history_embeddings::kHistoryEmbeddings, {{}}},
         {page_content_annotations::features::kPageContentAnnotations, {{}}}},
        /*disabled_features=*/{});

    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    HistoryEmbeddingsServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return HistoryEmbeddingsServiceFactory::
              BuildServiceInstanceForBrowserContextForTesting(
                  context, std::make_unique<history_embeddings::MockEmbedder>(),
                  /*answerer=*/nullptr, /*intent_classfier=*/nullptr);
        }));

    InteractiveBrowserTest::SetUpOnMainThread();
  }

 protected:
  history_embeddings::HistoryEmbeddingsService* service() {
    return HistoryEmbeddingsServiceFactory::GetForProfile(browser()->profile());
  }

  base::RepeatingCallback<void(history_embeddings::UrlPassages)>&
  callback_for_tests() {
    return service()->callback_for_tests_;
  }

  page_content_annotations::PageContentAnnotationsService*
  page_content_annotations_service() {
    return PageContentAnnotationsServiceFactory::GetForProfile(
        browser()->profile());
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
    page_content_annotations_service()->OverridePageContentAnnotatorForTesting(
        &page_content_annotator_);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  page_content_annotations::TestPageContentAnnotator page_content_annotator_;
};

// Opening the feedback dialog on CrOS & LaCrOS open a system level dialog,
// which cannot be easily tested here. Instead, LaCrOS has a separate feedback
// browser test which gives some coverage.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsInteractiveTest, FeedbackDialog) {
  optimization_guide::EnableSigninAndModelExecutionCapability(
      browser()->profile());
  browser()->profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kHistorySearch),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));

  // Setup a search result so that the WebUI can show the results with the
  // thumbs up/down UI.
  OverrideVisibilityScoresForTesting({
      {"A a B C b a 2 D", 0.99},
  });
  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<history_embeddings::UrlPassages> store_future;
  callback_for_tests() = store_future.GetRepeatingCallback();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/inner_text/test1.html")));
  EXPECT_TRUE(store_future.Wait());

  // Load the History page and click on the thumbs down icon to verify that
  // the feedback dialog appears.
  const DeepQuery kThumbsDownElement = {"history-app", "cr-history-embeddings",
                                        "cr-feedback-buttons", "#thumbsDown"};
  RunTestSequence(
      InstrumentTab(kHistoryTabId),
      NavigateWebContents(kHistoryTabId,
                          GURL("chrome://history/?q=A+B+C+D+e+f+g")),
      WaitForElementToRender(kHistoryTabId, kThumbsDownElement),
      MoveMouseTo(kHistoryTabId, kThumbsDownElement), ClickMouse(),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}
#endif
