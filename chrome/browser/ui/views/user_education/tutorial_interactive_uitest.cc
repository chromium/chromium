// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "chrome/browser/ui/views/user_education/help_bubble_factory_views.h"
#include "chrome/browser/ui/views/user_education/help_bubble_view.h"
#include "chrome/browser/ui/views/user_education/user_education_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
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

class TutorialInteractiveUitest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    GetTutorialService()->tutorial_registry()->AddTutorial(
        kTestTutorialId, GetDefaultTutorialDescription());
  }

  void TearDownOnMainThread() override {
    auto* const service = GetTutorialService();
    service->AbortTutorial();
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
    TutorialDescription::Step step1(0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                    ui::InteractionSequence::StepType::kShown,
                                    kAppMenuButtonElementId, std::string(),
                                    HelpBubbleArrow::kTopRight);
    description.steps.emplace_back(step1);

    TutorialDescription::Step step2(
        0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
        ui::InteractionSequence::StepType::kCustomEvent,
        ui::ElementIdentifier(), std::string(), HelpBubbleArrow::kTopCenter,
        kCustomEventType1);
    description.steps.emplace_back(step2);

    TutorialDescription::Step step3(
        0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
        ui::InteractionSequence::StepType::kActivated, kAppMenuButtonElementId,
        std::string(), HelpBubbleArrow::kTopRight);
    description.steps.emplace_back(step3);

    return description;
  }
};

IN_PROC_BROWSER_TEST_F(TutorialInteractiveUitest, SampleTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  const bool started = GetTutorialService()->StartTutorial(
      kTestTutorialId, browser()->window()->GetElementContext(),
      completed.Get(), aborted.Get());
  EXPECT_TRUE(started);

  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      GetElement(kTabStripElementId), kCustomEventType1);

  auto test_util = CreateInteractionTestUtil();

  test_util->PressButton(GetElement(kAppMenuButtonElementId));

  // Simulate click on close button.
  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          static_cast<HelpBubbleViews*>(
              GetTutorialService()->currently_displayed_bubble())
              ->bubble_view()
              ->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kKeyboard));
}
