// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

namespace {

// Tests the User Education tutorial for Send Tab to Self in desktop Chrome.
class SendTabToSelfTutorialInteractiveUiTest : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    MaybeRegisterChromeTutorials(*GetTutorialService()->tutorial_registry());
  }

  void TearDownOnMainThread() override {
    auto* const service = GetTutorialService();
    service->CancelTutorialIfRunning();
    service->tutorial_registry()->RemoveTutorialForTesting(
        kSendTabToSelfTutorialId);
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  user_education::TutorialService* GetTutorialService() {
    return &UserEducationServiceFactory::GetForBrowserContext(
                browser()->GetProfile())
                ->tutorial_service();
  }

  // Starts the Send Tab to Self tutorial with optional callbacks.
  auto StartTutorial(user_education::TutorialService::CompletedCallback
                         completed = base::DoNothing(),
                     user_education::TutorialService::AbortedCallback aborted =
                         base::DoNothing()) {
    return Do([this, completed = std::move(completed),
               aborted = std::move(aborted)]() mutable {
      GetTutorialService()->StartTutorial(
          kSendTabToSelfTutorialId,
          BrowserElements::From(browser())->GetContext(), std::move(completed),
          std::move(aborted));
    });
  }

  // Cancels any running tutorial and waits for the bubble to hide.
  auto CancelTutorial() {
    return Steps(
        Do([this]() { GetTutorialService()->CancelTutorialIfRunning(); }),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  // Verifies that the help bubble anchors to the specified tab index.
  auto CheckHelpBubbleAnchor(int tab_index) {
    return CheckView(
        user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
        [this, tab_index](user_education::HelpBubbleView* bubble) {
          auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser());
          tabs::TabInterface* const tab =
              browser()->tab_strip_model()->GetTabAtIndex(tab_index);
          return tab && bubble->GetAnchorView() ==
                            browser_view->tab_strip_view()->GetTabAnchorView(
                                tab->GetHandle());
        });
  }

  // Verifies that the help bubble anchors to the specified element identifier.
  auto CheckHelpBubbleAnchor(ui::ElementIdentifier id) {
    return CheckView(
        user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
        [this, id](user_education::HelpBubbleView* bubble) {
          auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser());
          auto* const view =
              views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                  id,
                  views::ElementTrackerViews::GetContextForView(browser_view));
          return bubble->GetAnchorView() == view;
        });
  }

  // Verifies the help bubble body string resource.
  auto CheckHelpBubbleBodyText(int string_id) {
    return CheckViewProperty(
        user_education::HelpBubbleView::kBodyTextIdForTesting,
        &views::Label::GetText, l10n_util::GetStringUTF16(string_id));
  }

  // Verifies the help bubble title string resource.
  auto CheckHelpBubbleTitleText(int string_id) {
    return CheckViewProperty(
        user_education::HelpBubbleView::kTitleTextIdForTesting,
        &views::Label::GetText, l10n_util::GetStringUTF16(string_id));
  }

  // Injects a dummy view with the specified element identifier for testing.
  auto AddDummyElement(ui::ElementIdentifier id) {
    return Do([this, id]() {
      auto* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser());
      views::View* const view = browser_view->contents_web_view()->AddChildView(
          std::make_unique<views::View>());
      view->SetProperty(views::kElementIdentifierKey, id);
    });
  }

  // Generates the test sequence verifying tutorial anchoring to active tabs.
  auto TestAnchorsToActiveTabSequence() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);

    return Steps(
        AddInstrumentedTab(kSecondTabId, GURL(chrome::kChromeUINewTabURL)),
        Check([this]() {
          return browser()->tab_strip_model()->active_index() == 1;
        }),
        StartTutorial(),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckHelpBubbleAnchor(1), CancelTutorial(),
        SelectTab(kTabStripElementId, 0), Check([this]() {
          return browser()->tab_strip_model()->active_index() == 0;
        }),
        StartTutorial(),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckHelpBubbleAnchor(0), CancelTutorial());
  }

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       AnchorsToActiveTab) {
  RunTestSequence(TestAnchorsToActiveTabSequence());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest, TutorialSteps) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL(completed, Run).Times(1);

  RunTestSequence(
      // Step 1: Start tutorial and verify bubble on active tab.
      StartTutorial(completed.Get(), aborted.Get()),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(0),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY),

      // Step 2: Show the Send Tab to Self menu item view.
      AddDummyElement(kTabSendTabToSelfMenuItem),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(kTabSendTabToSelfMenuItem),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_2_BODY),

      // Step 3: Show the ToastView.
      AddDummyElement(toasts::ToastView::kToastViewId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(toasts::ToastView::kToastViewId),
      CheckHelpBubbleTitleText(IDS_TUTORIAL_SEND_TAB_TO_SELF_SUCCESS_TITLE),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_SUCCESS_BODY),

      // Complete tutorial by clicking default button.
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Check([this]() { return !GetTutorialService()->IsRunningTutorial(); }),
      Do([this]() {
        histogram_tester_.ExpectUniqueSample(
            "Tutorial.SendTabToSelf.Completion", 1, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       TutorialDismissed) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL(aborted, Run).Times(1);

  RunTestSequence(
      // Step 1: Start tutorial and verify bubble on active tab.
      StartTutorial(completed.Get(), aborted.Get()),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(0),

      // Dismiss the tutorial via the close button on the help bubble.
      PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Check([this]() { return !GetTutorialService()->IsRunningTutorial(); }),
      Do([this]() {
        histogram_tester_.ExpectUniqueSample(
            "Tutorial.SendTabToSelf.Completion", 0, 1);
      }));
}

// Tests the Send Tab to Self tutorial with the vertical tab strip enabled.
class SendTabToSelfVerticalTabsInteractiveUiTest
    : public VerticalTabsBrowserTestMixin<
          SendTabToSelfTutorialInteractiveUiTest> {};

IN_PROC_BROWSER_TEST_F(SendTabToSelfVerticalTabsInteractiveUiTest,
                       AnchorsToActiveTabView) {
  RunTestSequence(TestAnchorsToActiveTabSequence());
}

}  // namespace

}  // namespace send_tab_to_self
