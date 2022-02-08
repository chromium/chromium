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
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"

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

  views::View* GetView(ui::ElementIdentifier id) {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        id, browser()->window()->GetElementContext());
  }

  TutorialDescription GetDefaultTutorialDescription() {
    TutorialDescription description;
    TutorialDescription::Step step1(absl::nullopt, u"Here's the app menu.",
                                    ui::InteractionSequence::StepType::kShown,
                                    kAppMenuButtonElementId, std::string(),
                                    HelpBubbleArrow::kTopRight);
    description.steps.emplace_back(step1);

    TutorialDescription::Step step2(
        absl::nullopt, u"A thing happened. Now click the app menu.",
        ui::InteractionSequence::StepType::kCustomEvent,
        ui::ElementIdentifier(), std::string(), HelpBubbleArrow::kTopCenter,
        kCustomEventType1);
    description.steps.emplace_back(step2);

    TutorialDescription::Step step3(
        absl::nullopt, u"App menu clicked! Congratulations!",
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

  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kCustomEventType1, GetView(kTabStripElementId));

  EXPECT_CALL_IN_SCOPE(completed, Run, {
    // Simulate app menu button press.
    auto* const button =
        static_cast<views::Button*>(GetView(kAppMenuButtonElementId));
    button->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE,
                                      ui::EF_NONE, ui::EventTimeForNow()));
    button->OnKeyReleased(ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SPACE,
                                       ui::EF_NONE, ui::EventTimeForNow()));
  });
}
