// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/events.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/test/test_help_bubble.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier3);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier4);
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

class ScopedTestTutorialState : public user_education::ScopedTutorialState {
 public:
  explicit ScopedTestTutorialState(ui::test::TestElement* element)
      : user_education::ScopedTutorialState(element->context()),
        element_(element) {
    element_->Show();
  }
  ~ScopedTestTutorialState() override { element_->Hide(); }

 private:
  raw_ptr<ui::test::TestElement> element_;
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

  void ClearEventQueue(bool fast_forward = true) {
    if (fast_forward) {
      task_environment_.FastForwardUntilNoTasksRemain();
    } else {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TutorialTest, TutorialBuilder) {
  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  Tutorial::Builder builder;
  int current_progress = 0;

  // build a step with an ElementID
  auto step1 = Tutorial::Builder::BuildFromDescriptionStep(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleBodyText(IDS_OK),
      2, current_progress, false, false, IDS_TUTORIAL_CLOSE_TUTORIAL, &service);

  // build a step that names an element
  auto step2 = Tutorial::Builder::BuildFromDescriptionStep(
      TutorialDescription::HiddenStep::WaitForShown(kTestIdentifier1)
          .NameElement(kTestElementName1),
      2, current_progress, false, false, IDS_TUTORIAL_CLOSE_TUTORIAL, &service);

  // build a step with a named element
  auto step3 = Tutorial::Builder::BuildFromDescriptionStep(
      TutorialDescription::BubbleStep(kTestElementName1)
          .SetBubbleBodyText(IDS_OK),
      2, current_progress, false, false, IDS_TUTORIAL_CLOSE_TUTORIAL, &service);

  // transition event
  auto step4 = Tutorial::Builder::BuildFromDescriptionStep(
      TutorialDescription::HiddenStep::WaitForShowEvent(kTestIdentifier1), 2,
      current_progress, false, false, IDS_TUTORIAL_CLOSE_TUTORIAL, &service);

  // final bubble
  auto step5 = Tutorial::Builder::BuildFromDescriptionStep(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleBodyText(IDS_OK),
      2, current_progress, true, false, IDS_TUTORIAL_CLOSE_TUTORIAL, &service);

  builder.SetContext(kTestContext1)
      .AddStep(std::move(step1))
      .AddStep(std::move(step2))
      .AddStep(std::move(step3))
      .AddStep(std::move(step4))
      .AddStep(std::move(step5))
      .Build();
}

TEST_F(TutorialTest, RegisterTutorial) {
  std::unique_ptr<TutorialRegistry> registry =
      std::make_unique<TutorialRegistry>();

  {
    TutorialDescription description;
    description.steps.emplace_back(
        TutorialDescription::BubbleStep(kTestIdentifier1)
            .SetBubbleBodyText(IDS_OK));
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

  const auto step = TutorialDescription::BubbleStep(kTestIdentifier1)
                        .SetBubbleBodyText(IDS_OK);

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

  const auto step = TutorialDescription::BubbleStep(kTestIdentifier1)
                        .SetBubbleBodyText(IDS_OK);

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

  const auto step = TutorialDescription::BubbleStep(kTestIdentifier1)
                        .SetBubbleBodyText(IDS_OK);

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

TEST_F(TutorialTest, RegisterSameTutorialInMultipleRegistries) {
  std::unique_ptr<TutorialRegistry> registry1 =
      std::make_unique<TutorialRegistry>();
  std::unique_ptr<TutorialRegistry> registry2 =
      std::make_unique<TutorialRegistry>();

  const auto step = TutorialDescription::BubbleStep(kTestIdentifier1)
                        .SetBubbleBodyText(IDS_OK);

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
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  ClearEventQueue();
  EXPECT_TRUE(service.currently_displayed_bubble_for_testing());
  EXPECT_ASYNC_CALL_IN_SCOPE(
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
  ClearEventQueue();

  // Verify only the default next button is visible and click it.
  auto* bubble = service.currently_displayed_bubble_for_testing();
  ASSERT_TRUE(bubble);
  EXPECT_FALSE(HasButtonWithText(bubble, IDS_TUTORIAL_CLOSE_TUTORIAL));
  EXPECT_TRUE(HasButtonWithText(bubble, IDS_TUTORIAL_NEXT_BUTTON));
  EXPECT_EQ(&element_1, bubble->AsA<test::TestHelpBubble>()->anchor_element());
  ClickNextButton(bubble);
  ClearEventQueue();

  // Verify only the custom next button is visible and click it.
  bubble = service.currently_displayed_bubble_for_testing();
  ASSERT_TRUE(bubble);
  EXPECT_FALSE(HasButtonWithText(bubble, IDS_TUTORIAL_CLOSE_TUTORIAL));
  EXPECT_TRUE(HasButtonWithText(bubble, IDS_TUTORIAL_NEXT_BUTTON));
  EXPECT_EQ(&element_2, bubble->AsA<test::TestHelpBubble>()->anchor_element());
  EXPECT_CALL_IN_SCOPE(next, Run, ClickNextButton(bubble));
  ClearEventQueue();

  // Verify only the close button is visible and click it.
  bubble = service.currently_displayed_bubble_for_testing();
  ASSERT_TRUE(bubble);
  EXPECT_TRUE(HasButtonWithText(bubble, IDS_TUTORIAL_CLOSE_TUTORIAL));
  EXPECT_FALSE(HasButtonWithText(bubble, IDS_TUTORIAL_NEXT_BUTTON));
  EXPECT_ASYNC_CALL_IN_SCOPE(completed, Run, ClickCloseButton(bubble));
}

TEST_F(TutorialTest, StartTutorialAbortsExistingTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  EXPECT_ASYNC_CALL_IN_SCOPE(
      aborted, Run, service.StartTutorial(kTestTutorial1, element_1.context()));
  EXPECT_TRUE(service.IsRunningTutorial());
}

TEST_F(TutorialTest, StartTutorialCompletesExistingTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  EXPECT_ASYNC_CALL_IN_SCOPE(
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
      TutorialDescription::EventStep(kCustomEventType1, kTestIdentifier1));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      &element_1, kCustomEventType1);

  EXPECT_ASYNC_CALL_IN_SCOPE(
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
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK)
          .NameElement(kElementName1));
  description.steps.emplace_back(
      TutorialDescription::HiddenStep::WaitForShown(kElementName1)
          .NameElement(kElementName2));
  description.steps.emplace_back(
      TutorialDescription::HiddenStep::WaitForShown(kElementName2)
          .NameElements(base::BindRepeating(
              [](ui::InteractionSequence* sequence, ui::TrackedElement* el) {
                sequence->NameElement(el, std::string_view(kElementName3));
                return true;
              })));
  description.steps.emplace_back(TutorialDescription::BubbleStep(kElementName3)
                                     .SetBubbleTitleText(IDS_OK)
                                     .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());

  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

TEST_F(TutorialTest, TutorialWithExtendedProperties) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // Build and show test element.
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Configure extended properties.
  HelpBubbleParams::ExtendedProperties extended_properties;
  extended_properties.values().Set("string", "v1");
  extended_properties.values().Set("bool", true);
  extended_properties.values().Set("int", 1);

  // Build the tutorial `description`.
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleBodyText(IDS_OK)
          .SetExtendedProperties(extended_properties));

  // Register and start the tutorial.
  registry.AddTutorial(kTestTutorial1, std::move(description));
  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  ClearEventQueue();

  // Verify the bubble has been forwarded the extended properties.
  auto* bubble = service.currently_displayed_bubble_for_testing();
  ASSERT_TRUE(bubble);
  EXPECT_THAT(
      static_cast<test::TestHelpBubble*>(bubble)->params().extended_properties,
      extended_properties);

  // Close the bubble to complete the tutorial.
  EXPECT_ASYNC_CALL_IN_SCOPE(completed, Run, ClickCloseButton(bubble));
}

TEST_F(TutorialTest, SingleStepRestartTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());

  EXPECT_ASYNC_CALL_IN_SCOPE(
      restarted, Run,
      ClickRestartButton(service.currently_displayed_bubble_for_testing()));

  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

// Clicks restart tutorial a few times. Then closes the tutorial from the close
// button. Expects to call the restarted callback multiple times.
TEST_F(TutorialTest, SingleStepRestartTutorialCanRestartMultipleTimes) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  ClearEventQueue();

  const int restarted_times = 3;
  for (int i = 0; i < restarted_times; ++i) {
    EXPECT_ASYNC_CALL_IN_SCOPE(
        restarted, Run,
        ClickRestartButton(service.currently_displayed_bubble_for_testing()));
  }

  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

// Starts a tutorial with 3 steps, completes steps, then clicks restart tutorial
// then completes the tutorial again and closes it from the close button.
// Expects to call the completed callback.
TEST_F(TutorialTest, MultiStepRestartTutorialWithCloseOnComplete) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier3)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  ClearEventQueue();
  element_2.Show();
  ClearEventQueue();
  element_3.Show();
  ClearEventQueue();
  element_2.Hide();
  ClearEventQueue();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      restarted, Run,
      ClickRestartButton(service.currently_displayed_bubble_for_testing()));

  EXPECT_TRUE(service.IsRunningTutorial());
  element_2.Show();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      ClickCloseButton(service.currently_displayed_bubble_for_testing()));
}

// Starts a tutorial with 3 steps, completes steps, then clicks restart tutorial
// then closes the tutorial on the first step. Expects to call the completed
// callback.
TEST_F(TutorialTest, MultiStepRestartTutorialWithDismissAfterRestart) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier3)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  ClearEventQueue();
  element_2.Show();
  ClearEventQueue();
  element_3.Show();
  ClearEventQueue();
  element_2.Hide();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      restarted, Run,
      ClickRestartButton(service.currently_displayed_bubble_for_testing()));
  ClearEventQueue();

  EXPECT_TRUE(service.IsRunningTutorial());
  EXPECT_TRUE(service.currently_displayed_bubble_for_testing() != nullptr);

  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      ClickDismissButton(service.currently_displayed_bubble_for_testing()));
}

// Starts a tutorial with 3 steps. Completes steps and clicks restart tutorial a
// few times. Then closes the tutorial on the first step. Expects to call the
// restarted callback multiple times.
TEST_F(TutorialTest, MultiStepRestartTutorialCanRestartMultipleTimes) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier3)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  ClearEventQueue();

  const int restarted_times = 3;
  for (int i = 0; i < restarted_times; ++i) {
    element_2.Show();
    ClearEventQueue();
    element_3.Show();
    ClearEventQueue();
    element_2.Hide();
    EXPECT_ASYNC_CALL_IN_SCOPE(
        restarted, Run,
        ClickRestartButton(service.currently_displayed_bubble_for_testing()));
    ClearEventQueue();
  }

  EXPECT_TRUE(service.IsRunningTutorial());
  EXPECT_TRUE(service.currently_displayed_bubble_for_testing() != nullptr);

  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      ClickDismissButton(service.currently_displayed_bubble_for_testing()));
}

// Verify that when the final bubble of a tutorial is forced to close without
// being dismissed by the user (e.g. because its anchor element disappears, or
// it's programmatically closed) the tutorial ends.
TEST_F(TutorialTest, BubbleClosingProgrammaticallyOnlyEndsTutorialOnLastStep) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

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
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.can_be_restarted = true;
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  ClearEventQueue();
  element_2.Show();
  ClearEventQueue();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      service.currently_displayed_bubble_for_testing()->Close());
}

TEST_F(TutorialTest, TimeoutBeforeFirstBubble) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  ui::test::TestElement el(kTestIdentifier1, kTestContext1);

  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, el.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  EXPECT_FALSE(service.currently_displayed_bubble_for_testing());
  EXPECT_ASYNC_CALL_IN_SCOPE(aborted, Run, ClearEventQueue());
}

TEST_F(TutorialTest, TimeoutBetweenBubbles) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  ui::test::TestElement el1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement el2(kTestIdentifier1, kTestContext1);
  el1.Show();

  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK)
          .AbortIfVisibilityLost(false));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, el1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  ClearEventQueue();

  // This closes the bubble but does not advance the tutorial.
  el1.Hide();
  ClearEventQueue(/*fast_forward=*/false);
  EXPECT_FALSE(service.currently_displayed_bubble_for_testing());
  EXPECT_ASYNC_CALL_IN_SCOPE(aborted, Run, ClearEventQueue());
}

TEST_F(TutorialTest, NoTimeoutIfBubbleShowing) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(TutorialService::RestartedCallback, restarted);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  ui::test::TestElement el1(kTestIdentifier1, kTestContext1);
  ui::test::TestElement el2(kTestIdentifier1, kTestContext1);
  el1.Show();

  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier2)
          .SetBubbleTitleText(IDS_OK)
          .SetBubbleBodyText(IDS_OK));
  registry.AddTutorial(kTestTutorial1, std::move(description));

  service.StartTutorial(kTestTutorial1, el1.context(), completed.Get(),
                        aborted.Get(), restarted.Get());
  ClearEventQueue();

  // Since there is a bubble, there is no timeout.
  EXPECT_TRUE(service.currently_displayed_bubble_for_testing());
  ClearEventQueue();

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

TEST_F(TutorialTest, SetupTemporaryStateCallback) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);

  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // Build and show test element.
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Create another test element which will be shown during the tutorial.
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);
  ASSERT_FALSE(element_2.IsVisible());

  // Build the tutorial `description`.
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleBodyText(IDS_OK));
  description.temporary_state_callback = base::BindRepeating(
      [](ui::test::TestElement* element, ui::ElementContext context)
          -> std::unique_ptr<user_education::ScopedTutorialState> {
        return base::WrapUnique(new ScopedTestTutorialState(element));
      },
      base::Unretained(&element_2));

  // Register and start the tutorial.
  registry.AddTutorial(kTestTutorial1, std::move(description));
  service.StartTutorial(kTestTutorial1, element_1.context(), completed.Get());
  ClearEventQueue();

  auto* bubble = service.currently_displayed_bubble_for_testing();
  // Verify that the element is shown when the tutorial is active.
  ASSERT_TRUE(element_2.IsVisible());
  // Close the bubble to complete the tutorial.
  EXPECT_ASYNC_CALL_IN_SCOPE(completed, Run, ClickCloseButton(bubble));
  // Verify that the element is hidden when the tutorial is completed.
  ASSERT_FALSE(element_2.IsVisible());
}

TEST_F(TutorialTest, CleanupTemporaryStateOnAbort) {
  const auto bubble_factory_registry =
      CreateTestTutorialBubbleFactoryRegistry();
  TutorialRegistry registry;
  TestTutorialService service(&registry, bubble_factory_registry.get());

  // Build and show test element.
  ui::test::TestElement element_1(kTestIdentifier1, kTestContext1);
  element_1.Show();

  // Create another test element which will be shown during the tutorial.
  ui::test::TestElement element_2(kTestIdentifier2, kTestContext1);
  ASSERT_FALSE(element_2.IsVisible());

  // Build the tutorial `description`.
  TutorialDescription description;
  description.steps.emplace_back(
      TutorialDescription::BubbleStep(kTestIdentifier1)
          .SetBubbleBodyText(IDS_OK));
  description.temporary_state_callback = base::BindRepeating(
      [](ui::test::TestElement* element, ui::ElementContext context)
          -> std::unique_ptr<user_education::ScopedTutorialState> {
        return base::WrapUnique(new ScopedTestTutorialState(element));
      },
      base::Unretained(&element_2));

  // Register and start the tutorial.
  registry.AddTutorial(kTestTutorial1, std::move(description));
  service.StartTutorial(kTestTutorial1, element_1.context());
  ClearEventQueue();

  // Verify that the bubble is shown to the user.
  EXPECT_TRUE(service.currently_displayed_bubble_for_testing());
  // Verify that the element is shown when the tutorial is active.
  ASSERT_TRUE(element_2.IsVisible());

  // Verify the tutorial is aborted when the anchor visibility is lost.
  element_1.Hide();
  ClearEventQueue();
  EXPECT_FALSE(service.currently_displayed_bubble_for_testing());
  EXPECT_FALSE(service.IsRunningTutorial());
  // Verify that the state is reset when the tutorial is aborted.
  ASSERT_FALSE(element_2.IsVisible());
}

// Test where the parameter is a bitfield describing choices the test will make
// at each branch.
class ConditionalTutorialTest : public ui::test::InteractiveTestT<TutorialTest>,
                                public testing::WithParamInterface<int> {
 public:
  ConditionalTutorialTest() = default;
  ~ConditionalTutorialTest() override = default;

  void SetUp() override {
    InteractiveTestT<TutorialTest>::SetUp();
    EXPECT_CALL(completed_, Run).Times(1);
    first_anchor_.Show();
  }

 protected:
  using BubbleStep = TutorialDescription::BubbleStep;
  using IfStep = TutorialDescription::If;

  // Gets whether the `n`th branch should be active.
  bool GetBranchValue(int n) const { return 0 != (GetParam() & (1 << n)); }

  // Gets the condition function for the `n`th branch.
  TutorialDescription::ConditionalCallback Branch(int n) const {
    const bool result = GetBranchValue(n);
    return base::BindLambdaForTesting(
        [result](const ui::TrackedElement* el) { return result; });
  }

  template <typename... Args>
  auto StartTutorial(Args... steps) {
    tutorial_registry_.AddTutorial(
        kTestTutorial1, TutorialDescription::Create<kHistogramName1>(steps...));
    return Do([this]() {
      tutorial_service_.StartTutorial(kTestTutorial1, first_anchor_.context(),
                                      completed_.Get(), aborted_.Get());
    });
  }

  // Closes a help bubble. The bubble must already be visible.
  auto CloseHelpBubble() {
    return Steps(WithElement(test::TestHelpBubble::kElementId,
                             [](ui::TrackedElement* el) {
                               el->AsA<test::TestHelpBubbleElement>()
                                   ->bubble()
                                   ->SimulateDismiss();
                             }),
                 WaitForHide(test::TestHelpBubble::kElementId));
  }

  auto VerifyHelpBubble(std::map<int, int> expected_strings,
                        std::optional<std::pair<int, int>> progress) {
    const int id = expected_strings.size() == 1U
                       ? expected_strings.begin()->second
                       : expected_strings[GetParam()];
    return Steps(
        std::move(CheckElement(
                      test::TestHelpBubble::kElementId,
                      [](ui::TrackedElement* el) {
                        return el->AsA<test::TestHelpBubbleElement>()
                            ->bubble()
                            ->params()
                            .body_text;
                      },
                      l10n_util::GetStringUTF16(id))
                      .FormatDescription("%s - Body text must match.")),
        std::move(CheckElement(
                      test::TestHelpBubble::kElementId,
                      [](ui::TrackedElement* el) {
                        return el->AsA<test::TestHelpBubbleElement>()
                            ->bubble()
                            ->params()
                            .buttons.empty();
                      },
                      progress.has_value())
                      .FormatDescription(
                          "%s - Only final bubble should have buttons.")),
        std::move(
            CheckElement(
                test::TestHelpBubble::kElementId,
                [](ui::TrackedElement* el) {
                  return el->AsA<test::TestHelpBubbleElement>()
                      ->bubble()
                      ->params()
                      .progress;
                },
                progress)
                .SetMustRemainVisible(false)
                .FormatDescription("%s - Progress indicators should match.")));
  }

  TutorialRegistry tutorial_registry_;
  std::unique_ptr<HelpBubbleFactoryRegistry> help_bubble_registry_ =
      CreateTestTutorialBubbleFactoryRegistry();
  TestTutorialService tutorial_service_{&tutorial_registry_,
                                        help_bubble_registry_.get()};
  base::test::ScopedRunLoopTimeout timeout_{FROM_HERE, base::Seconds(30)};
  testing::StrictMock<base::MockCallback<TutorialService::CompletedCallback>>
      completed_;
  testing::StrictMock<base::MockCallback<TutorialService::AbortedCallback>>
      aborted_;
  ui::test::TestElement first_anchor_{kTestIdentifier1, kTestContext1};
};

using ConditionalTutorialTest1 = ConditionalTutorialTest;
INSTANTIATE_TEST_SUITE_P(, ConditionalTutorialTest1, testing::Range(0, 2));

TEST_P(ConditionalTutorialTest1, ConditionalAtStartOfTutorial) {
  ui::test::TestElement el2(kTestIdentifier2, kTestContext1);

  RunTestSequenceInContext(
      first_anchor_.context(),
      StartTutorial(
          IfStep(kTestIdentifier1, Branch(0))
              .Then(BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_OK))
              .Else(BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_CANCEL)),
          BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_CLEAR)),
      VerifyHelpBubble({{0, IDS_CANCEL}, {1, IDS_OK}}, std::make_pair(1, 1)),
      Do([&]() { el2.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{-1, IDS_CLEAR}}, std::nullopt), CloseHelpBubble());
}

TEST_P(ConditionalTutorialTest1, ConditionalInMiddleOfTutorial) {
  ui::test::TestElement el2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement el3(kTestIdentifier3, kTestContext1);

  RunTestSequenceInContext(
      first_anchor_.context(),
      StartTutorial(
          BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_DONE),
          IfStep(kTestIdentifier2, Branch(0))
              .Then(BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_OK))
              .Else(BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_CANCEL)),
          BubbleStep(kTestIdentifier3).SetBubbleBodyText(IDS_CLEAR)),
      VerifyHelpBubble({{-1, IDS_DONE}}, std::make_pair(1, 2)),
      Do([&]() { el2.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{0, IDS_CANCEL}, {1, IDS_OK}}, std::make_pair(2, 2)),
      Do([&]() { el3.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{-1, IDS_CLEAR}}, std::nullopt), CloseHelpBubble());
}

TEST_P(ConditionalTutorialTest1, ConditionalAtEndOfTutorial) {
  ui::test::TestElement el2(kTestIdentifier2, kTestContext1);

  RunTestSequenceInContext(
      first_anchor_.context(),
      StartTutorial(
          BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_DONE),
          IfStep(kTestIdentifier2, Branch(0))
              .Then(BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_OK))
              .Else(
                  BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_CANCEL))),
      VerifyHelpBubble({{-1, IDS_DONE}}, std::make_pair(1, 1)),
      Do([&]() { el2.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{0, IDS_CANCEL}, {1, IDS_OK}}, std::nullopt),
      CloseHelpBubble());
}

TEST_P(ConditionalTutorialTest1, ConditionalAtEndOfTutorialUnevenSteps) {
  ui::test::TestElement el2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement el3(kTestIdentifier3, kTestContext1);

  RunTestSequenceInContext(
      first_anchor_.context(),
      StartTutorial(
          BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_DONE),
          IfStep(kTestIdentifier2, Branch(0))
              .Then(BubbleStep(kTestIdentifier3).SetBubbleBodyText(IDS_OK))
              .Else(
                  BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_CLEAR),
                  BubbleStep(kTestIdentifier3).SetBubbleBodyText(IDS_CANCEL))),
      VerifyHelpBubble({{-1, IDS_DONE}}, std::make_pair(1, 2)),
      Do([&]() { el2.Show(); }),
      If([this]() { return !GetBranchValue(0); },
         Steps(std::move(WaitForShow(test::TestHelpBubble::kElementId)
                             .SetTransitionOnlyOnEvent(true)),
               VerifyHelpBubble({{-1, IDS_CLEAR}}, std::make_pair(2, 2)))),
      Do([&]() { el3.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{0, IDS_CANCEL}, {1, IDS_OK}}, std::nullopt),
      CloseHelpBubble());
}

TEST_P(ConditionalTutorialTest1, OptionalStep) {
  ui::test::TestElement el2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement el3(kTestIdentifier3, kTestContext1);

  RunTestSequenceInContext(
      first_anchor_.context(),
      StartTutorial(
          BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_DONE),
          IfStep(kTestIdentifier2, Branch(0))
              .Then(BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_OK)),
          BubbleStep(kTestIdentifier3).SetBubbleBodyText(IDS_CLEAR)),
      VerifyHelpBubble({{-1, IDS_DONE}}, std::make_pair(1, 2)),
      If([this]() { return GetBranchValue(0); },
         Steps(Do([&]() { el2.Show(); }),
               std::move(WaitForShow(test::TestHelpBubble::kElementId)
                             .SetTransitionOnlyOnEvent(true)),
               VerifyHelpBubble({{1, IDS_OK}}, std::make_pair(2, 2)))),
      Do([&]() { el3.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{-1, IDS_CLEAR}}, std::nullopt), CloseHelpBubble());
}

TEST_P(ConditionalTutorialTest1, WaitForAnyOf) {
  ui::test::TestElement el2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement el3(kTestIdentifier3, kTestContext1);
  ui::test::TestElement el4(kTestIdentifier4, kTestContext1);

  RunTestSequenceInContext(
      first_anchor_.context(),
      StartTutorial(
          BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_DONE),
          TutorialDescription::WaitForAnyOf(kTestIdentifier2)
              .Or(kTestIdentifier3),
          IfStep(kTestIdentifier2)
              .Then(BubbleStep(kTestIdentifier2).SetBubbleBodyText(IDS_OK))
              .Else(BubbleStep(kTestIdentifier3).SetBubbleBodyText(IDS_CANCEL)),
          BubbleStep(kTestIdentifier4).SetBubbleBodyText(IDS_CLEAR)),
      VerifyHelpBubble({{-1, IDS_DONE}}, std::make_pair(1, 2)),
      If([this]() { return GetBranchValue(0); },
         Steps(Do([&]() { el2.Show(); }),
               std::move(WaitForShow(test::TestHelpBubble::kElementId)
                             .SetTransitionOnlyOnEvent(true)),
               VerifyHelpBubble({{-1, IDS_OK}}, std::make_pair(2, 2))),
         Steps(Do([&]() { el3.Show(); }),
               std::move(WaitForShow(test::TestHelpBubble::kElementId)
                             .SetTransitionOnlyOnEvent(true)),
               VerifyHelpBubble({{-1, IDS_CANCEL}}, std::make_pair(2, 2)))),
      Do([&]() { el4.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{-1, IDS_CLEAR}}, std::nullopt), CloseHelpBubble());
}

using ConditionalTutorialTest2 = ConditionalTutorialTest;
INSTANTIATE_TEST_SUITE_P(, ConditionalTutorialTest2, testing::Range(0, 4));

TEST_P(ConditionalTutorialTest2, NestedConditionals) {
  ui::test::TestElement el2(kTestIdentifier2, kTestContext1);
  ui::test::TestElement el3(kTestIdentifier3, kTestContext1);

  RunTestSequenceInContext(
      first_anchor_.context(),
      StartTutorial(
          IfStep(kTestIdentifier1, Branch(0))
              .Then(BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_OK),
                    IfStep(kTestIdentifier2, Branch(1))
                        .Then(BubbleStep(kTestIdentifier2)
                                  .SetBubbleBodyText(IDS_DONE))
                        .Else(BubbleStep(kTestIdentifier2)
                                  .SetBubbleBodyText(IDS_CLEAR)))
              .Else(BubbleStep(kTestIdentifier1).SetBubbleBodyText(IDS_CANCEL),
                    IfStep(kTestIdentifier2, Branch(1))
                        .Then(BubbleStep(kTestIdentifier2)
                                  .SetBubbleBodyText(IDS_ADD))
                        .Else(BubbleStep(kTestIdentifier2)
                                  .SetBubbleBodyText(IDS_REMOVE))),
          BubbleStep(kTestIdentifier3).SetBubbleBodyText(IDS_SAVE)),
      VerifyHelpBubble(
          {{0, IDS_CANCEL}, {1, IDS_OK}, {2, IDS_CANCEL}, {3, IDS_OK}},
          std::make_pair(1, 2)),
      Do([&]() { el2.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble(
          {{0, IDS_REMOVE}, {1, IDS_CLEAR}, {2, IDS_ADD}, {3, IDS_DONE}},
          std::make_pair(2, 2)),
      Do([&]() { el3.Show(); }),
      WaitForShow(test::TestHelpBubble::kElementId)
          .SetTransitionOnlyOnEvent(true),
      VerifyHelpBubble({{-1, IDS_SAVE}}, std::nullopt), CloseHelpBubble());
}

}  // namespace user_education
