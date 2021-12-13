// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);

const char kTestElementName1[] = "ELEMENT_NAME_1";

const ui::ElementContext kTestContext1(1);

std::unique_ptr<TutorialBubbleFactoryRegistry>
CreateTestTutorialBubbleFactoryRegistry() {
  std::unique_ptr<TutorialBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<TutorialBubbleFactoryRegistry>();
  std::unique_ptr<TestTutorialBubbleFactory> test_bubble_factory =
      std::make_unique<TestTutorialBubbleFactory>();
  bubble_factory_registry->RegisterBubbleFactory(
      std::move(test_bubble_factory));

  return bubble_factory_registry;
}

const TutorialIdentifier kTestTutorial1{"kTestTutorial1"};
}  // namespace

TEST(TutorialTest, TutorialBuilder) {
  std::unique_ptr<TutorialBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<TutorialBubbleFactoryRegistry>();

  std::unique_ptr<TutorialService> service =
      std::make_unique<TutorialService>();

  Tutorial::Builder builder;

  // build a step with an ElementID
  std::unique_ptr<ui::InteractionSequence::Step> step1 =
      Tutorial::StepBuilder()
          .SetAnchorElementID(kTestIdentifier1)
          .Build(service.get(), bubble_factory_registry.get());

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
          .Build(service.get(), bubble_factory_registry.get());

  // build a step with a named element
  std::unique_ptr<ui::InteractionSequence::Step> step3 =
      Tutorial::StepBuilder()
          .SetAnchorElementName(std::string(kTestElementName1))
          .Build(service.get(), bubble_factory_registry.get());

  // transition event
  std::unique_ptr<ui::InteractionSequence::Step> step4 =
      Tutorial::StepBuilder()
          .SetAnchorElementID(kTestIdentifier1)
          .SetTransitionOnlyOnEvent(true)
          .Build(service.get(), bubble_factory_registry.get());

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
        kTestIdentifier1, std::string(),
        TutorialDescription::Step::Arrow::NONE));
    registry->AddTutorial(kTestTutorial1, description);
  }

  std::unique_ptr<TutorialBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<TutorialBubbleFactoryRegistry>();

  registry->GetTutorialIdentifiers();
}

TEST(TutorialTest, SingleInteractionTutorialRuns) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  std::unique_ptr<TutorialBubbleFactoryRegistry> bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();

  std::unique_ptr<TutorialRegistry> tutorial_registry =
      std::make_unique<TutorialRegistry>();

  std::unique_ptr<TutorialService> service =
      std::make_unique<TutorialService>();
  service->SetOnCompleteTutorial(completed.Get());

  // build elements and keep them for triggering show/hide
  ui::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(TutorialDescription::Step(
      u"step 1 title", u"step 1 description",
      ui::InteractionSequence::StepType::kShown, kTestIdentifier1, "",
      TutorialDescription::Step::Arrow::NONE));

  tutorial_registry->AddTutorial(kTestTutorial1, description);

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      service->StartTutorial(kTestTutorial1, element_1.context(),
                             bubble_factory_registry.get(),
                             tutorial_registry.get()));
}
