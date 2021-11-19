// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
const char kTestElementName1[] = "ELEMENT_NAME_1";
const ui::ElementContext kTestContext1(1);

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
