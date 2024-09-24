// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/start_tutorial_in_page.h"

#include <optional>

#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_registry.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kStartingPageURL[] = "chrome://whats-new/";
const char kTargetPageURL[] = "chrome://settings/";
const ui::ElementIdentifier kHelpBubbleAnchorId =
    kWebUIIPHDemoElementIdentifier;
const char kTestTutorialId[] = "Test Tutorial";

// Gets a partially-filled params block with default values. You will still
// need to specify `target_url` and `callback`.
StartTutorialInPage::Params GetDefaultParams() {
  StartTutorialInPage::Params params;
  params.tutorial_id = kTestTutorialId;
  return params;
}

}  // namespace

using user_education::HelpBubbleArrow;
using user_education::TutorialDescription;
using BubbleStep = user_education::TutorialDescription::BubbleStep;
using EventStep = user_education::TutorialDescription::EventStep;
using HiddenStep = user_education::TutorialDescription::HiddenStep;

class StartTutorialInPageBrowserTest : public InteractiveBrowserTest {
 public:
  StartTutorialInPageBrowserTest() = default;
  ~StartTutorialInPageBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    GetTutorialService()->tutorial_registry()->AddTutorial(
        kTestTutorialId, GetDefaultTutorialDescription());
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(kStartingPageURL)));
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    auto* const service = GetTutorialService();
    service->CancelTutorialIfRunning();
    service->tutorial_registry()->RemoveTutorialForTesting(kTestTutorialId);
  }

 protected:
  user_education::TutorialDescription GetDefaultTutorialDescription() {
    TutorialDescription test_description;
    test_description.steps = {
        BubbleStep(kHelpBubbleAnchorId)
            .SetBubbleBodyText(IDS_OK)
            .SetBubbleArrow(HelpBubbleArrow::kTopRight),
        BubbleStep(kHelpBubbleAnchorId)
            .SetBubbleBodyText(IDS_OK)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter),
        HiddenStep::WaitForHidden(kHelpBubbleAnchorId),
        BubbleStep(kTopContainerElementId).SetBubbleBodyText(IDS_OK)};
    return test_description;
  }

  user_education::TutorialService* GetTutorialService() {
    return static_cast<user_education::FeaturePromoControllerCommon*>(
               browser()->window()->GetFeaturePromoControllerForTesting())
        ->tutorial_service_for_testing();
  }

  bool VerifyPage(Browser* browser, const char* pageURL) {
    auto url = browser->tab_strip_model()->GetActiveWebContents()->GetURL();
    return url == GURL(pageURL);
  }
};

IN_PROC_BROWSER_TEST_F(StartTutorialInPageBrowserTest,
                       StartTutorialInSamePage) {
  using user_education::TutorialService;

  base::MockCallback<StartTutorialInPage::Callback> tutorial_triggered;
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  TutorialService& tutorial_service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
          ->tutorial_service();

  base::WeakPtr<StartTutorialInPage> handle;
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(kStartingPageURL,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_CALL(tutorial_triggered, Run).WillOnce([&](TutorialService* service) {
    EXPECT_EQ(service, &tutorial_service);
    EXPECT_TRUE(service->IsRunningTutorial(kTestTutorialId));
    quit_closure.Run();
  });

  auto params = GetDefaultParams();
  params.callback = tutorial_triggered.Get();
  handle = StartTutorialInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);
  run_loop.Run();

  EXPECT_TRUE(GetTutorialService()->IsRunningTutorial(kTestTutorialId));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(kStartingPageURL,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());

  // Cancelling tutorial will clean up handle.
  tutorial_service.CancelTutorialIfRunning(kTestTutorialId);
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(StartTutorialInPageBrowserTest, StartTutorialInNewTab) {
  using user_education::TutorialService;

  base::MockCallback<StartTutorialInPage::Callback> tutorial_triggered;
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  TutorialService& tutorial_service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
          ->tutorial_service();

  base::WeakPtr<StartTutorialInPage> handle;
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(kStartingPageURL,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_CALL(tutorial_triggered, Run).WillOnce([&](TutorialService* service) {
    EXPECT_EQ(service, &tutorial_service);
    EXPECT_TRUE(service->IsRunningTutorial(kTestTutorialId));
    quit_closure.Run();
  });

  auto params = GetDefaultParams();
  params.target_url = GURL(kTargetPageURL);
  params.callback = tutorial_triggered.Get();
  handle = StartTutorialInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);
  run_loop.Run();

  EXPECT_TRUE(GetTutorialService()->IsRunningTutorial(kTestTutorialId));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(kStartingPageURL,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(kTargetPageURL,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());

  // Cancelling tutorial will clean up handle.
  tutorial_service.CancelTutorialIfRunning(kTestTutorialId);
  ASSERT_FALSE(handle);
}

IN_PROC_BROWSER_TEST_F(StartTutorialInPageBrowserTest,
                       StartTutorialInSamePageWithNavigation) {
  using user_education::TutorialService;

  base::MockCallback<StartTutorialInPage::Callback> tutorial_triggered;
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  TutorialService& tutorial_service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
          ->tutorial_service();

  base::WeakPtr<StartTutorialInPage> handle;
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(kStartingPageURL,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_CALL(tutorial_triggered, Run).WillOnce([&](TutorialService* service) {
    EXPECT_EQ(service, &tutorial_service);
    EXPECT_TRUE(service->IsRunningTutorial(kTestTutorialId));
    quit_closure.Run();
  });

  auto params = GetDefaultParams();
  params.target_url = GURL(kTargetPageURL);
  params.overwrite_active_tab = true;
  params.callback = tutorial_triggered.Get();
  handle = StartTutorialInPage::Start(browser(), std::move(params));

  ASSERT_NE(nullptr, handle);
  run_loop.Run();

  EXPECT_TRUE(GetTutorialService()->IsRunningTutorial(kTestTutorialId));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(kTargetPageURL,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());

  // Cancelling tutorial will clean up handle.
  tutorial_service.CancelTutorialIfRunning(kTestTutorialId);
  ASSERT_FALSE(handle);
}
