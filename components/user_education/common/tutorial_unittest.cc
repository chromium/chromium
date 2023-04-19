// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial.h"

#include <string>

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/test/test_help_bubble.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier3);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType1);

const char kTestElementName1[] = "ELEMENT_NAME_1";

const ui::ElementContext kTestContext1(1);

const TutorialIdentifier kTestTutorial1{"kTestTutorial1"};
const TutorialIdentifier kTestTutorial2{"kTestTutorial2"};
const TutorialIdentifier kTestTutorial3{"kTestTutorial3"};

const char kHistogramName1[] = "histogram 1";
const char kHistogramName2[] = "histogram 2";

class TestTutorialService : public TutorialService {
 public:
  TestTutorialService(TutorialRegistry* tutorial_registry,
                      HelpBubbleFactoryRegistry* help_bubble_factory_registry)
      : TutorialService(tutorial_registry, help_bubble_factory_registry) {}
  ~TestTutorialService() override = default;

  std::u16string GetBodyIconAltText(bool is_last_step) const override {
    return std::u16string();
  }
};

std::unique_ptr<HelpBubbleFactoryRegistry>
CreateTestTutorialBubbleFactoryRegistry() {
  auto bubble_factory_registry = std::make_unique<HelpBubbleFactoryRegistry>();
  bubble_factory_registry->MaybeRegister<test::TestHelpBubbleFactory>();
  return bubble_factory_registry;
}

void ClickDismissButton(HelpBubble* bubble) {
  auto* const help_bubble = static_cast<test::TestHelpBubble*>(bubble);
  help_bubble->SimulateDismiss();
}

void ClickCloseButton(HelpBubble* bubble) {
  auto* const help_bubble = static_cast<test::TestHelpBubble*>(bubble);
  int button_index = help_bubble->GetIndexOfButtonWithText(
      l10n_util::GetStringUTF16(IDS_TUTORIAL_CLOSE_TUTORIAL));
  EXPECT_TRUE(button_index != test::TestHelpBubble::kNoButtonWithTextIndex);
  help_bubble->SimulateButtonPress(button_index);
}

void ClickNextButton(HelpBubble* bubble) {
  auto* const help_bubble = static_cast<test::TestHelpBubble*>(bubble);
  int button_index = help_bubble->GetIndexOfButtonWithText(
      l10n_util::GetStringUTF16(IDS_TUTORIAL_NEXT_BUTTON));
  EXPECT_TRUE(button_index != test::TestHelpBubble::kNoButtonWithTextIndex);
  help_bubble->SimulateButtonPress(button_index);
}

void ClickRestartButton(HelpBubble* bubble) {
  auto* const help_bubble = static_cast<test::TestHelpBubble*>(bubble);
  int button_index = help_bubble->GetIndexOfButtonWithText(
      l10n_util::GetStringUTF16(IDS_TUTORIAL_RESTART_TUTORIAL));

  EXPECT_TRUE(button_index != test::TestHelpBubble::kNoButtonWithTextIndex);
  help_bubble->SimulateButtonPress(button_index);
}

bool HasButtonWithText(HelpBubble* bubble, int message_id) {
  return static_cast<test::TestHelpBubble*>(bubble)->GetIndexOfButtonWithText(
             l10n_util::GetStringUTF16(message_id)) !=
         test::TestHelpBubble::kNoButtonWithTextIndex;
}

}  // namespace

class TutorialTest : public testing::Test {
 public:
  TutorialTest() = default;
  ~TutorialTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TutorialTest, TutorialBuilder) {
  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

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

TEST_F(TutorialTest, RegisterTutorial) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  {
    TutorialDescription description;
    description.steps.emplace_back(
        0, IDS_OK, ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
        std::string(), HelpBubbleArrow::kNone);
    description.can_be_restarted = true;
    registry->AddTutorial(kTestTutorial1, std::move(description));
  }

  std::unique_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<HelpBubbleFactoryRegistry>();

  registry->GetTutorialIdentifiers();
}

TEST_F(TutorialTest, RegisterMultipleTutorials) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  TutorialDescription::Step step(
      0, IDS_OK, ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
      std::string(), HelpBubbleArrow::kNone);

  TutorialDescription description1;
  description1.steps.push_back(step);
  description1.histograms = MakeTutorialHistograms<kHistogramName1>(1);

  TutorialDescription description2;
  description2.steps.push_back(step);
  description2.histograms = MakeTutorialHistograms<kHistogramName2>(1);

  registry->AddTutorial(kTestTutorial1, std::move(description1));
  registry->AddTutorial(kTestTutorial2, std::move(description2));
  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial1));
  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial2));
}

TEST_F(TutorialTest, RegisterSameTutorialTwice) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  TutorialDescription::Step step(
      0, IDS_OK, ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
      std::string(), HelpBubbleArrow::kNone);

  TutorialDescription description1;
  description1.steps.push_back(step);
  description1.histograms = MakeTutorialHistograms<kHistogramName1>(1);

  TutorialDescription description2;
  description2.steps.push_back(step);
  description2.histograms = MakeTutorialHistograms<kHistogramName1>(1);

  registry->AddTutorial(kTestTutorial1, std::move(description1));
  registry->AddTutorial(kTestTutorial1, std::move(description2));
  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial1));
}

TEST_F(TutorialTest, RegisterTutorialsWithAndWithoutHistograms) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  TutorialDescription::Step step(
      0, IDS_OK, ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
      std::string(), HelpBubbleArrow::kNone);

  TutorialDescription description1;
  description1.steps.push_back(step);

  TutorialDescription description2;
  description2.steps.push_back(step);
  description2.histograms = MakeTutorialHistograms<kHistogramName2>(1);

  TutorialDescription description3;
  description2.steps.push_back(step);

  registry->AddTutorial(kTestTutorial1, std::move(description1));
  registry->AddTutorial(kTestTutorial2, std::move(description2));
  registry->AddTutorial(kTestTutorial3, std::move(description3));
  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial1));
  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial2));
  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial3));
}

#if DCHECK_IS_ON()
#define MAYBE_RegisterDifferentTutorialsWithSameHistogram \
  RegisterDifferentTutorialsWithSameHistogram
#else
#define MAYBE_RegisterDifferentTutorialsWithSameHistogram \
  DISABLED_RegisterDifferentTutorialsWithSameHistogram
#endif

TEST_F(TutorialTest, MAYBE_RegisterDifferentTutorialsWithSameHistogram) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  TutorialDescription::Step step(
      0, IDS_OK, ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
      std::string(), HelpBubbleArrow::kNone);

  TutorialDescription description1;
  description1.steps.push_back(step);
  description1.histograms = MakeTutorialHistograms<kHistogramName1>(1);

  TutorialDescription description2;
  description2.steps.push_back(step);
  description2.histograms = MakeTutorialHistograms<kHistogramName1>(1);

  registry->AddTutorial(kTestTutorial1, std::move(description1));
  EXPECT_DCHECK_DEATH(
      registry->AddTutorial(kTestTutorial2, std::move(description2)));
}

TEST_F(TutorialTest, RegisterSameTutorialInMultipleRegistries) {
  std::unique_ptr<TutorialRegistry> registry1 =
      std::make_unique<TutorialRegistry>();
  std::unique_ptr<TutorialRegistry> registry2 =
      std::make_unique<TutorialRegistry>();

  TutorialDescription::Step step(
      0, IDS_OK, ui::InteractionSequence::StepType::kShown, kTestIdentifier1,
      std::string(), HelpBubbleArrow::kNone);

  TutorialDescription description1;
  description1.steps.push_back(step);
  description1.histograms = MakeTutorialHistograms<kHistogramName1>(1);

  TutorialDescription description2;
  description2.steps.push_back(step);
  description2.histograms = MakeTutorialHistograms<kHistogramName1>(1);

  registry1->AddTutorial(kTestTutorial1, std::move(description1));
  registry2->AddTutorial(kTestTutorial1, std::move(description2));
  EXPECT_TRUE(registry1->IsTutorialRegistered(kTestTutorial1));
  EXPECT_TRUE(registry2->IsTutorialRegistered(kTestTutorial1));
}

TEST_F(TutorialTest, SingleInteractionTutorialRuns) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  EXPECT_TRUE(service.currently_displayed_bubble_for_testing());
  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

TEST_F(TutorialTest, MultipleInteractionTutorialRuns) {
  UNCALLED_MOCK_CALLBACK(TutorialDescription::NextButtonCallback, next);
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // Build and show test elements.
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement element_3(kTestIdentifier3, kTestContext1);
  element_1.Show();
  element_2.Show();
  element_3.Show();

  // Configure `next` callback to trigger `kCustomEventType1` on current anchor.
  ON_CALL(next, Run).WillByDefault(
      testing::Invoke([](ui::TrackedElement* current_anchor) {
        ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
            current_anchor, kCustomEventType1);
      }));

  // Build the tutorial `description`.
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleBodyText(IDS_OK)
          .AddDefaultNextButton());
  description.steps.emplace_back(TutorialDescription::EventStep(
      kHelpBubbleNextButtonClickedEvent, kTestIdentifier1));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleBodyText(IDS_OK)
          .AddCustomNextButton(next.Get()));
  description.steps.emplace_back(
      TutorialDescription::EventStep(kCustomEventType1, kTestIdentifier2));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier3)
          .SetBubbleBodyText(IDS_OK)
          // Should no-op for last step.
          .AddCustomNextButton(base::DoNothing())
          .AddDefaultNextButton());

  // Register and start the tutorial.
  registry.AddTutorial(kTestTutorial1, std::move(description));
  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  // Verify only the default next button is visible and click it.
  auto* bubble = service.currently_displayed_bubble_for_testing();
  ASSERT_TRUE(bubble);
  EXPECT_FALSE(HasButtonWithText(bubble, IDS_TUTORIAL_CLOSE_TUTORIAL));
  EXPECT_TRUE(HasButtonWithText(bubble, IDS_TUTORIAL_NEXT_BUTTON));
  ClickNextButton(bubble);

  // Verify only the custom next button is visible and click it.
  bubble = service.currently_displayed_bubble_for_testing();
  ASSERT_TRUE(bubble);
  EXPECT_FALSE(HasButtonWithText(bubble, IDS_TUTORIAL_CLOSE_TUTORIAL));
  EXPECT_TRUE(HasButtonWithText(bubble, IDS_TUTORIAL_NEXT_BUTTON));
  EXPECT_CALL_IN_SCOPE(next, Run, ClickNextButton(bubble));

  // Verify only the close button is visible and click it.
  bubble = service.currently_displayed_bubble_for_testing();
  ASSERT_TRUE(bubble);
  EXPECT_TRUE(HasButtonWithText(bubble, IDS_TUTORIAL_CLOSE_TUTORIAL));
  EXPECT_FALSE(HasButtonWithText(bubble, IDS_TUTORIAL_NEXT_BUTTON));
  EXPECT_CALL_IN_SCOPE(completed, Run, ClickCloseButton(bubble));
}

TEST_F(TutorialTest, StartTutorialAbortsExistingTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, aborted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description. This has two steps, the second of which
  // will not
  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier2, "", HelpBubbleArrow::kNone);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get());
  EXPECT_CALL_IN_SCOPE(
      aborted, Run, service.StartTutorial(kTestTutorial1, element_1.context()));
  EXPECT_TRUE(service.IsRunningTutorial());
}

TEST_F(TutorialTest, StartTutorialCompletesExistingTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, aborted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description. This has two steps, the second of which
  // will not
  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get());
  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      service.StartTutorial(kTestTutorial1, element_1.context()));
  EXPECT_TRUE(service.IsRunningTutorial());
}

TEST_F(TutorialTest, TutorialWithCustomEvent) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(
      IDS_OK, IDS_OK, ui::InteractionSequence::StepType::kCustomEvent,
      kTestIdentifier1, "", HelpBubbleArrow::kNone, kCustomEventType1);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      &element_1, kCustomEventType1);

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

TEST_F(TutorialTest, TutorialWithNamedElement) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  static constexpr char kElementName1[] = "Element Name 1";
  static constexpr char kElementName2[] = "Element Name 2";
  static constexpr char kElementName3[] = "Element Name 3";

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // Build elements and keep them for triggering show/hide.
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial description.
  TutorialDescription description;
  description.steps.emplace_back(
      IDS_OK, IDS_OK, ui::InteractionSequence::StepType::kShown,
      kTestIdentifier1, std::string(), HelpBubbleArrow::kNone,
      ui::CustomElementEventType(),
      /* must_remain_visible =*/true,
      /* transition_only_on_event =*/false,
      base::BindLambdaForTesting(
          [](ui::InteractionSequence* sequence, ui::TrackedElement* element) {
            sequence->NameElement(element, base::StringPiece(kElementName1));
            return true;
          }));
  description.steps.emplace_back(
      TutorialDescription::HiddenStep::WaitForShown(kElementName1)
          .NameElement(kElementName2));
  description.steps.emplace_back(
      TutorialDescription::HiddenStep::WaitForShown(kElementName2)
          .NameElements(base::BindRepeating(
              [](ui::InteractionSequence* sequence, ui::TrackedElement* el) {
                sequence->NameElement(el, base::StringPiece(kElementName3));
                return true;
              })));
  description.steps.emplace_back(
      IDS_OK, IDS_OK, ui::InteractionSequence::StepType::kShown,
      ui::ElementIdentifier(), kElementName3, HelpBubbleArrow::kNone);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

TEST_F(TutorialTest, SingleStepRestartTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  ClickRestartButton(service.currently_displayed_bubble_for_testing());

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

// Starts a tutorial with 3 steps, completes steps, then clicks restart tutorial
// then completes the tutorial again and closes it from the close button.
// Expects to call the completed callback.
TEST_F(TutorialTest, MultiStepRestartTutorialWithCloseOnComplete) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement element_3(kTestIdentifier3, kTestContext1);

  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier2, "", HelpBubbleArrow::kNone);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier3, "", HelpBubbleArrow::kNone);
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  element_2.Show();
  element_3.Show();

  element_2.Hide();

  ClickRestartButton(service.currently_displayed_bubble_for_testing());

  EXPECT_TRUE(service.IsRunningTutorial());
  element_2.Show();

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

// Starts a tutorial with 3 steps, completes steps, then clicks restart tutorial
// then closes the tutorial on the first step. Expects to call the completed
// callback.
TEST_F(TutorialTest, MultiStepRestartTutorialWithDismissAfterRestart) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement element_3(kTestIdentifier3, kTestContext1);

  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier2, "", HelpBubbleArrow::kNone);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier3, "", HelpBubbleArrow::kNone);
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  element_2.Show();
  element_3.Show();

  element_2.Hide();

  ClickRestartButton(service.currently_displayed_bubble_for_testing());

  EXPECT_TRUE(service.IsRunningTutorial());
  EXPECT_TRUE(service.currently_displayed_bubble_for_testing() != nullptr);

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      ClickDismissButton(service.currently_displayed_bubble_for_testing()));
}

// Verify that when the final bubble of a tutorial is forced to close without
// being dismissed by the user (e.g. because its anchor element disappears, or
// it's programmatically closed) the tutorial ends.
TEST_F(TutorialTest, BubbleClosingProgrammaticallyOnlyEndsTutorialOnLastStep) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // build elements and keep them for triggering show/hide
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);

  element_1.Show();

  // Build the tutorial Description
  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier2, "", HelpBubbleArrow::kNone);
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get());
  service.currently_displayed_bubble_for_testing()->Close();
  element_2.Show();
  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      service.currently_displayed_bubble_for_testing()->Close());
}

TEST_F(TutorialTest, TimeoutBeforeFirstBubble) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  ui::test::TestElement el(kTestIdentifier1, kTestContext1);

  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, el.context(), completed.Get(),
                        aborted.Get());
  EXPECT_FALSE(service.currently_displayed_bubble_for_testing());
  EXPECT_CALL_IN_SCOPE(aborted, Run,
                       task_environment_.FastForwardUntilNoTasksRemain());
}

TEST_F(TutorialTest, TimeoutBetweenBubbles) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  ui::test::TestElement el1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement el2(kTestIdentifier1, kTestContext1);
  el1.Show();

  TutorialDescription description;
  description.steps.emplace_back(
      IDS_OK, IDS_OK, ui::InteractionSequence::StepType::kShown,
      kTestIdentifier1, "", HelpBubbleArrow::kNone,
      ui::CustomElementEventType(), /* must_remain_visible */ false);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier2, "", HelpBubbleArrow::kNone);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, el1.context(), completed.Get(),
                        aborted.Get());

  // This closes the bubble but does not advance the tutorial.
  el1.Hide();
  EXPECT_FALSE(service.currently_displayed_bubble_for_testing());
  EXPECT_CALL_IN_SCOPE(aborted, Run,
                       task_environment_.FastForwardUntilNoTasksRemain());
}

TEST_F(TutorialTest, NoTimeoutIfBubbleShowing) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  ui::test::TestElement el1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement el2(kTestIdentifier1, kTestContext1);
  el1.Show();

  TutorialDescription description;
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier1, "", HelpBubbleArrow::kNone);
  description.steps.emplace_back(IDS_OK, IDS_OK,
                                 ui::InteractionSequence::StepType::kShown,
                                 kTestIdentifier2, "", HelpBubbleArrow::kNone);
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, el1.context(), completed.Get(),
                        aborted.Get());

  // Since there is a bubble, there is no timeout.
  EXPECT_TRUE(service.currently_displayed_bubble_for_testing());
  task_environment_.FastForwardUntilNoTasksRemain();

  // When we exit and destroy the service, the callback will be called.
  EXPECT_CALL(aborted, Run).Times(1);
}

TEST_F(TutorialTest, RegisterTutorialWithCreate) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  {
    auto description = TutorialDescription::Create<kHistogramName1>(
        TutorialDescription::BubbleStep(kTestIdentifier1)
            .SetBubbleBodyText(IDS_OK));
    description.can_be_restarted = true;
    EXPECT_TRUE(description.steps.size() == 1);

    registry->AddTutorial(kTestTutorial1, std::move(description));
  }

  std::unique_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<HelpBubbleFactoryRegistry>();

  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial1));
}

TEST_F(TutorialTest, RegisterTutorialWithCreateFromVector) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  {
    TutorialDescription::Step first_step =
        TutorialDescription::BubbleStep(kTestIdentifier1)
            .SetBubbleBodyText(IDS_OK);

    std::vector<TutorialDescription::Step> next_steps = {
        TutorialDescription::BubbleStep(kTestIdentifier2)
            .SetBubbleBodyText(IDS_OK),
        TutorialDescription::BubbleStep(kTestIdentifier3)
            .SetBubbleBodyText(IDS_OK)};

    auto description =
        TutorialDescription::Create<kHistogramName1>(first_step, next_steps);
    description.can_be_restarted = true;
    EXPECT_TRUE(description.steps.size() == 3);

    registry->AddTutorial(kTestTutorial1, std::move(description));
  }

  std::unique_ptr<HelpBubbleFactoryRegistry> bubble_factory_registry =
      std::make_unique<HelpBubbleFactoryRegistry>();

  EXPECT_TRUE(registry->IsTutorialRegistered(kTestTutorial1));
}

}  // namespace user_education
