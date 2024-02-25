// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
constexpr char kTestTutorialId[] = "TutorialInteractiveUitest Tutorial";
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType1);
}  // namespace

using user_education::FeaturePromoControllerCommon;
using user_education::HelpBubbleArrow;
using user_education::HelpBubbleViews;
using user_education::TutorialDescription;
using user_education::TutorialService;

class TutorialInteractiveUitest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    GetTutorialService()->tutorial_registry()->AddTutorial(
        kTestTutorialId, GetDefaultTutorialDescription());
  }

  void TearDownOnMainThread() override {
    auto* const service = GetTutorialService();
    service->CancelTutorialIfRunning();
    service->tutorial_registry()->RemoveTutorialForTesting(kTestTutorialId);
  }

 protected:
  TutorialService* GetTutorialService() {
    return static_cast<FeaturePromoControllerCommon*>(
               browser()->window()->GetFeaturePromoController())
        ->tutorial_service_for_testing();
  }

  ui::TrackedElement* GetElement(ui::ElementIdentifier id) {
    return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        id, browser()->window()->GetElementContext());
  }

  TutorialDescription GetDefaultTutorialDescription() {
    TutorialDescription description;
    description.steps.emplace_back(
        TutorialDescription::BubbleStep(kToolbarAppMenuButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kTopRight));
    description.steps.emplace_back(
        TutorialDescription::EventStep(kCustomEventType1));
    description.steps.emplace_back(
        TutorialDescription::HiddenStep::WaitForActivated(
            kToolbarAppMenuButtonElementId));
    description.steps.emplace_back(
        TutorialDescription::BubbleStep(kToolbarAppMenuButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kTopRight));
    return description;
  }
};

IN_PROC_BROWSER_TEST_F(TutorialInteractiveUitest, SampleTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  GetTutorialService()->StartTutorial(kTestTutorialId,
                                      browser()->window()->GetElementContext(),
                                      completed.Get(), aborted.Get());
  EXPECT_TRUE(GetTutorialService()->IsRunningTutorial());

  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      GetElement(kTabStripElementId), kCustomEventType1);

  InteractionTestUtilBrowser test_util;
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.PressButton(GetElement(kToolbarAppMenuButtonElementId)));

  // Simulate click on close button.
  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          static_cast<HelpBubbleViews*>(
              GetTutorialService()->currently_displayed_bubble_for_testing())
              ->bubble_view()
              ->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kKeyboard));
}

class WebUITutorialInteractiveUitest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    GetTutorialService()->tutorial_registry()->AddTutorial(
        kTestTutorialId, GetDefaultTutorialDescription());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    auto* const service = GetTutorialService();
    service->CancelTutorialIfRunning();
    service->tutorial_registry()->RemoveTutorialForTesting(kTestTutorialId);
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  auto CheckWebUIHelpBubbleIsShowing(bool showing) {
    return InAnyContext(CheckElement(
        NewTabPageUI::kCustomizeChromeButtonElementId,
        [](ui::TrackedElement* el) {
          return el->AsA<user_education::TrackedElementWebUI>()
              ->handler()
              ->IsHelpBubbleShowingForTesting(el->identifier());
        },
        showing));
  }

  auto StartTutorial(ui::ElementIdentifier page_id) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHelpBubbleShownEvent);
    StateChange help_bubble_shown;
    help_bubble_shown.where = {"ntp-app", "help-bubble"};
    help_bubble_shown.type = StateChange::Type::kExists;
    help_bubble_shown.event = kHelpBubbleShownEvent;

    auto steps =
        Steps(Do([this]() {
                auto* const service = GetTutorialService();
                service->StartTutorial(
                    kTestTutorialId, browser()->window()->GetElementContext());
              }),
              WaitForStateChange(page_id, help_bubble_shown), FlushEvents());
    AddDescription(steps, "StartTutorial( %s )");
    return steps;
  }

  auto CancelTutorial(ui::ElementIdentifier page_id) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHelpBubbleHiddenEvent);
    StateChange help_bubble_hidden;
    help_bubble_hidden.type = StateChange::Type::kDoesNotExist;
    help_bubble_hidden.where = {"ntp-app", "help-bubble"};
    help_bubble_hidden.event = kHelpBubbleHiddenEvent;

    auto steps =
        Steps(Do([this]() {
                auto* const service = GetTutorialService();
                service->CancelTutorialIfRunning(kTestTutorialId);
              }),
              WaitForStateChange(page_id, help_bubble_hidden), FlushEvents());
    AddDescription(steps, "CancelTutorial( %s )");
    return steps;
  }

 protected:
  TutorialService* GetTutorialService() {
    return static_cast<FeaturePromoControllerCommon*>(
               browser()->window()->GetFeaturePromoController())
        ->tutorial_service_for_testing();
  }

  TutorialDescription GetDefaultTutorialDescription() {
    TutorialDescription description;
    description.steps.emplace_back(
        TutorialDescription::BubbleStep(
            NewTabPageUI::kCustomizeChromeButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kTopRight)
            .InAnyContext());
    description.steps.emplace_back(
        TutorialDescription::EventStep(kCustomEventType1));
    description.steps.emplace_back(
        TutorialDescription::BubbleStep(kToolbarAppMenuButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP));
    return description;
  }
};

// Regression test for crbug.com/1425161.
IN_PROC_BROWSER_TEST_F(WebUITutorialInteractiveUitest,
                       CloseTabWithTutorialBubble) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageId);
  constexpr char kTabCloseButtonId[] = "Tab Close Button";
  RunTestSequence(
      AddInstrumentedTab(kNewTabPageId, GURL(chrome::kChromeUINewTabPageURL)),
      StartTutorial(kNewTabPageId),
      NameViewRelative(kTabStripElementId, kTabCloseButtonId,
                       [](TabStrip* tab_strip) {
                         return tab_strip->tab_at(1)->close_button().get();
                       }),
      PressButton(kTabCloseButtonId), WaitForHide(kNewTabPageId));
}

// Regression test for a possible cause of crbug.com/1474307.
IN_PROC_BROWSER_TEST_F(WebUITutorialInteractiveUitest,
                       CancelTutorialClosesBubble) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageId);

  RunTestSequence(
      AddInstrumentedTab(kNewTabPageId, GURL("chrome://new-tab-page")),
      StartTutorial(kNewTabPageId), CheckWebUIHelpBubbleIsShowing(true),
      CancelTutorial(kNewTabPageId), CheckWebUIHelpBubbleIsShowing(false),
      StartTutorial(kNewTabPageId), CheckWebUIHelpBubbleIsShowing(true));
}

// Regression test for a possible cause of crbug.com/1474307.
IN_PROC_BROWSER_TEST_F(WebUITutorialInteractiveUitest,
                       StartTutorialTwiceInARow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageId);
  RunTestSequence(
      AddInstrumentedTab(kNewTabPageId, GURL("chrome://new-tab-page")),
      StartTutorial(kNewTabPageId), CheckWebUIHelpBubbleIsShowing(true),
      // This should cancel the previous tutorial and close the help bubble so
      // that the new tutorial can start immediately.
      StartTutorial(kNewTabPageId), CheckWebUIHelpBubbleIsShowing(true));
}
