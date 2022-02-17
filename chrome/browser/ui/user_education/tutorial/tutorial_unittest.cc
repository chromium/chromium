// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/user_education/test_help_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
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

const TutorialIdentifier kTestTutorial1{"kTestTutorial1"};
}  // namespace

TEST(TutorialTest, TutorialBuilder) {
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

TEST(TutorialTest, TutorialRegistryRegistersTutorials) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  {
    TutorialDescription description;
    description.steps.emplace_back(TutorialDescription::Step(
        u"title", u"description", ui::InteractionSequence::StepType::kShown,
        kTestIdentifier1, std::string(), HelpBubbleArrow::kNone));
    registry->AddTutorial(kTestTutorial1, std::move(description));
  }

  std::unique_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<HelpBubbleFactoryRegistry>();

  registry->GetTutorialIdentifiers();
}

TEST(TutorialTest, SingleInteractionTutorialRuns) {
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
      TutorialDescription::Step(u"step 1 title", u"step 1 description",
                                ui::InteractionSequence::StepType::kShown,
                                kTestIdentifier1, "", HelpBubbleArrow::kNone));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      service.StartTutorial(kTestTutorial1, element_1.context(),
                            completed.Get()));
}

TEST(TutorialTest, TutorialWithCustomEvent) {
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
      u"step 1 title", u"step 1 description",
      ui::InteractionSequence::StepType::kCustomEvent, kTestIdentifier1, "",
      HelpBubbleArrow::kNone, kCustomEventType1));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          &element_1, kCustomEventType1));
}

TEST(TutorialTest, BuildAndRunTutorial) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  static constexpr char kElementName[] = "Element Name";

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();

  Tutorial::Builder builder;
  builder.SetCompletedCallback(completed.Get());
  builder.SetAbortedCallback(aborted.Get());
  builder.SetContext(element.context());
  builder.AddStep(Tutorial::StepBuilder()
                      .SetStepType(ui::InteractionSequence::StepType::kShown)
                      .SetAnchorElementID(element.identifier())
                      .SetNameElementsCallback(base::BindLambdaForTesting(
                          [](ui::InteractionSequence* sequence,
                             ui::TrackedElement* element) {
                            sequence->NameElement(element, kElementName);
                            return true;
                          }))
                      .SetArrow(HelpBubbleArrow::kTopCenter)
                      .SetBodyText(u"Bubble 1")
                      .SetProgress(std::make_pair(0, 2))
                      .Build(&service));
  builder.AddStep(
      Tutorial::StepBuilder()
          .SetStepType(ui::InteractionSequence::StepType::kActivated)
          .SetAnchorElementName(kElementName)
          .SetArrow(HelpBubbleArrow::kLeftCenter)
          .SetProgress(std::make_pair(1, 2))
          .Build(&service));

  auto tutorial = builder.Build();

  tutorial->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element.Activate());
}
