// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"

#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/user_education/test_help_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier3);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType1);

const char kTestElementName1[] = "ELEMENT_NAME_1";

const ui::ElementContext kTestContext1(1);

std::unique_ptr<HelpBubbleFactoryRegistry>
CreateTestTutorialBubbleFactoryRegistry() {
  std::unique_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<HelpBubbleFactoryRegistry>();
  bubble_factory_registry->MaybeRegister<TestHelpBubbleFactory>();
  return bubble_factory_registry;
}

void ClickDismissButton(HelpBubble* bubble) {
  TestHelpBubble* help_bubble = static_cast<TestHelpBubble*>(bubble);
  help_bubble->SimulateDismiss();
}

void ClickCloseButton(HelpBubble* bubble) {
  LOG(INFO) << "BUBBLE: " << bubble;
  TestHelpBubble* help_bubble = static_cast<TestHelpBubble*>(bubble);
  int button_index = help_bubble->GetIndexOfButtonWithText(
      l10n_util::GetStringUTF16(IDS_TUTORIAL_CLOSE_TUTORIAL));
  LOG(INFO) << "BUBBLE: " << button_index;

  EXPECT_TRUE(button_index != TestHelpBubble::kNoButtonWithTextIndex);
  help_bubble->SimulateButtonPress(button_index);
}

void ClickRestartButton(HelpBubble* bubble) {
  TestHelpBubble* help_bubble = static_cast<TestHelpBubble*>(bubble);
  int button_index = help_bubble->GetIndexOfButtonWithText(
      l10n_util::GetStringUTF16(IDS_TUTORIAL_RESTART_TUTORIAL));

  EXPECT_TRUE(button_index != TestHelpBubble::kNoButtonWithTextIndex);
  help_bubble->SimulateButtonPress(button_index);
}

const TutorialIdentifier kTestTutorial1{"kTestTutorial1"};
}  // namespace

class TutorialTest : public testing::Test {};

TEST_F(TutorialTest, TutorialBuilder) {
  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  Tutorial::Builder builder;

  // build a step with an ElementID
  std::unique_ptr<ui::InteractionSequence::Step> step1 =
      Tutorial::StepBuilder()
          .SetAnchorElementID(kTestIdentifier1)
          .Build(&service);

  // build a step that names an element
  std::unique_ptr<ui::InteractionSequence::Step> step2 =
      Tutorial::StepBuilder()
          .SetAnchorElementID(kTestIdentifier1)
          .SetNameElementsCallback(
              base::BindRepeating([](ui::InteractionSequence* sequence,
                                     ui::TrackedElement* element) {
                sequence->NameElement(element, "TEST ELEMENT");
                return true;
              }))
          .Build(&service);

  // build a step with a named element
  std::unique_ptr<ui::InteractionSequence::Step> step3 =
      Tutorial::StepBuilder()
          .SetAnchorElementName(std::string(kTestElementName1))
          .Build(&service);

  // transition event
  std::unique_ptr<ui::InteractionSequence::Step> step4 =
      Tutorial::StepBuilder()
          .SetAnchorElementID(kTestIdentifier1)
          .SetTransitionOnlyOnEvent(true)
          .Build(&service);

  builder.SetContext(kTestContext1)
      .AddStep(std::move(step1))
      .AddStep(std::move(step2))
      .AddStep(std::move(step3))
      .AddStep(std::move(step4))
      .Build();
}

TEST_F(TutorialTest, TutorialRegistryRegistersTutorials) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  {
    TutorialDescription description;
    description.steps.emplace_back(TutorialDescription::Step(
        0, IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
        ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
        std::string(), HelpBubbleArrow::kNone));
    description.can_be_restarted = true;
    registry->AddTutorial(kTestTutorial1, std::move(description));
  }

  std::unique_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<HelpBubbleFactoryRegistry>();

  registry->GetTutorialIdentifiers();
}

TEST_F(TutorialTest, SingleInteractionTutorialRuns) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier1, "", HelpBubbleArrow::kNone));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  EXPECT_TRUE(service.currently_displayed_bubble());
  EXPECT_CALL_IN_SCOPE(completed, Run,
                       ClickCloseButton(service.currently_displayed_bubble()));
}

TEST_F(TutorialTest, TutorialWithCustomEvent) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(TutorialDescription::Step(
      IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
      IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
      ui::InteractionSequence::StepType::kCustomEvent, kTestIdentifier1, "",
      HelpBubbleArrow::kNone, kCustomEventType1));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      &element_1, kCustomEventType1);

  EXPECT_CALL_IN_SCOPE(completed, Run,
                       ClickCloseButton(service.currently_displayed_bubble()));
}

TEST_F(TutorialTest, TutorialWithNamedElement) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  static constexpr char kElementName[] = "Element Name";

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(TutorialDescription::Step(
      IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
      IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
      ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
      std::string(), HelpBubbleArrow::kNone, ui::CustomElementEventType(),
      /* must_remain_visible =*/true,
      /* transition_only_on_event =*/false,
      base::BindLambdaForTesting(
          [](ui::InteractionSequence* sequence, ui::TrackedElement* element) {
            sequence->NameElement(element, base::StringPiece(kElementName));
            return true;
          })));
  description.steps.emplace_back(TutorialDescription::Step(
      IDS_TUTORIAL_TAB_GROUP_SUCCESS_TITLE,
      IDS_TUTORIAL_TAB_GROUP_SUCCESS_DESCRIPTION,
      ui::InteractionSequence::StepType::kShown, ui::ElementIdentifier(),
      kElementName, HelpBubbleArrow::kNone));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  EXPECT_CALL_IN_SCOPE(completed, Run,
                       ClickCloseButton(service.currently_displayed_bubble()));
}

TEST_F(TutorialTest, SingleStepRestartTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier1, "", HelpBubbleArrow::kNone));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  ClickRestartButton(service.currently_displayed_bubble());

  EXPECT_CALL_IN_SCOPE(completed, Run,
                       ClickCloseButton(service.currently_displayed_bubble()));
}

// Starts a tutorial with 3 steps, completes steps, then clicks restart tutorial
// then completes the tutorial again and closes it from the close button.
// Expects to call the completed callback.
TEST_F(TutorialTest, MultiStepRestartTutorialWithCloseOnComplete) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement element_3(kTestIdentifier3, kTestContext1);

  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier1, "", HelpBubbleArrow::kNone));
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier2, "", HelpBubbleArrow::kNone));
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier3, "", HelpBubbleArrow::kNone));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  element_2.Show();
  element_3.Show();

  element_2.Hide();

  ClickRestartButton(service.currently_displayed_bubble());

  EXPECT_TRUE(service.IsRunningTutorial());
  element_2.Show();

  EXPECT_CALL_IN_SCOPE(completed, Run,
                       ClickCloseButton(service.currently_displayed_bubble()));
}

// Starts a tutorial with 3 steps, completes steps, then clicks restart tutorial
// then closes the tutorial on the first step. Expects to call the completed
// callback.
TEST_F(TutorialTest, MultiStepRestartTutorialWithDismissAfterRestart) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement element_3(kTestIdentifier3, kTestContext1);

  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier1, "", HelpBubbleArrow::kNone));
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier2, "", HelpBubbleArrow::kNone));
  description.steps.emplace_back(
      TutorialDescription::Step(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP,
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier3, "", HelpBubbleArrow::kNone));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  element_2.Show();
  element_3.Show();

  element_2.Hide();

  ClickRestartButton(service.currently_displayed_bubble());

  EXPECT_TRUE(service.IsRunningTutorial());
  EXPECT_TRUE(service.currently_displayed_bubble() != nullptr);

  EXPECT_CALL_IN_SCOPE(
      completed, Run, ClickDismissButton(service.currently_displayed_bubble()));
}
