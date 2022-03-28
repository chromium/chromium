// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/interaction_sequence_browser_util.h"

#include <sstream>

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInteractionSequenceBrowserUtilTestId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInteractionSequenceBrowserUtilTestId2);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInteractionTestUtilCustomEventType);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInteractionTestUtilCustomEventType2);

}  // namespace

class InteractionSequenceBrowserUtilTest : public InProcessBrowserTest {
 public:
  InteractionSequenceBrowserUtilTest() = default;
  ~InteractionSequenceBrowserUtilTest() override = default;
  InteractionSequenceBrowserUtilTest(
      const InteractionSequenceBrowserUtilTest&) = delete;
  void operator=(const InteractionSequenceBrowserUtilTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       GetBrowserFromContext) {
  Browser* const other_browser = CreateBrowser(browser()->profile());
  EXPECT_EQ(browser(), InteractionSequenceBrowserUtil::GetBrowserFromContext(
                           browser()->window()->GetElementContext()));
  EXPECT_EQ(other_browser,
            InteractionSequenceBrowserUtil::GetBrowserFromContext(
                other_browser->window()->GetElementContext()));
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementCreatedForExistingPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Using this constructor hits all of the rest of the constructors, saving us
  // the hassle of writing three identical tests.
  InteractionSequenceBrowserUtil util(browser()->window()->GetElementContext(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kInteractionSequenceBrowserUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(element->IsA<TrackedElementWebPage>());
                        EXPECT_EQ(
                            &util,
                            element->AsA<TrackedElementWebPage>()->owner());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementRecreatedOnNavigate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             NavigateParams navigate_params(
                                 browser(), url, ui::PAGE_TRANSITION_TYPED);
                             Navigate(&navigate_params);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, LoadPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kInteractionSequenceBrowserUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) { util.LoadPage(url); }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  DCHECK_EQ(url, util.web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, IsPageLoaded) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util.is_page_loaded());
                             NavigateParams navigate_params(
                                 browser(), url, ui::PAGE_TRANSITION_TYPED);
                             Navigate(&navigate_params);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_FALSE(util.is_page_loaded());
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util.is_page_loaded());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementRecreatedWithDifferentIdOnNavigate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util.set_page_identifier(
                                 kInteractionSequenceBrowserUtilTestId2);
                             NavigateParams navigate_params(
                                 browser(), url, ui::PAGE_TRANSITION_TYPED);
                             Navigate(&navigate_params);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementRecreatedWithDifferentIdOnBackForward) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Do two navigations, then go back, then forward again.
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);

  // Load the first page and make sure we wait for the page transition.
  util.LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetTransitionOnlyOnEvent(true)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util.web_contents()->GetURL());
                             // Load the second page and wait for it to finish
                             // loading.
                             util.set_page_identifier(
                                 kInteractionSequenceBrowserUtilTestId2);
                             util.LoadPage(url2);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url2, util.web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoBack(browser()));
                             util.set_page_identifier(
                                 kInteractionSequenceBrowserUtilTestId);
                             chrome::GoBack(browser(),
                                            WindowOpenDisposition::CURRENT_TAB);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util.web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoForward(browser()));
                             util.set_page_identifier(
                                 kInteractionSequenceBrowserUtilTestId2);
                             chrome::GoForward(
                                 browser(), WindowOpenDisposition::CURRENT_TAB);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url2, util.web_contents()->GetURL());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, EvaluateInt) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(1, util.Evaluate("1").GetInt());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, EvaluateString) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kInteractionSequenceBrowserUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            std::string("The quick brown fox"),
                            util.Evaluate("'The quick brown fox'").GetString());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       SendEventOnStateChangeOnCurrentCondition) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util.Evaluate("window.value = 1");
                             InteractionSequenceBrowserUtil::StateChange
                                 state_change;
                             state_change.test_script = "window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util.SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       SendEventOnStateChangeOnDelayedCondition) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util.Evaluate(
                                 "window.value = 0; setTimeout(function() { "
                                 "window.value = 1; }, 300);");
                             InteractionSequenceBrowserUtil::StateChange
                                 state_change;
                             state_change.test_script = "window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util.SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       StateChangeTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util.Evaluate(
                                 "window.value = 0; setTimeout(function() { "
                                 "window.value = 1; }, 1000);");
                             InteractionSequenceBrowserUtil::StateChange
                                 state_change;
                             state_change.test_script = "window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.timeout = base::Milliseconds(300);
                             state_change.timeout_event =
                                 kInteractionTestUtilCustomEventType2;
                             util.SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType2)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       SendStateChangeEventsForDifferentDataTypes) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);

  // Poll significantly faster than the value in the page is expected to
  // change; this allows us to verify that the value changes after a non-zero
  // amount of time.
  constexpr base::TimeDelta kPollTime = base::Milliseconds(50);
  constexpr int kScriptDelayMs = 150;

  base::ElapsedTimer timer;
  base::TimeDelta last;

  // Sets window.value to an initial value, and then some time later, sets it
  // to a final value.
  const auto post_and_listen = [&](const char* initial, const char* final) {
    std::ostringstream oss;
    oss << "window.value = " << initial
        << "; setTimeout(function() { window.value = " << final << "; }, "
        << kScriptDelayMs << ");";
    util.Evaluate(oss.str());
    InteractionSequenceBrowserUtil::StateChange state_change;
    state_change.test_script = "window.value";
    state_change.event = kInteractionTestUtilCustomEventType;
    state_change.polling_interval = kPollTime;
    util.SendEventOnStateChange(state_change);
  };

  // Verifies that multiple polling intervals have passed before the condition
  // we were watching becomes true.
  const auto check_elapsed = [&]() {
    auto next = timer.Elapsed();
    EXPECT_GT(next, last + 2 * kPollTime);
    last = next;
  };

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             last = timer.Elapsed();
                             // Integers:
                             post_and_listen("0", "1");
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             // Booleans:
                             post_and_listen("false", "true");
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             // Strings:
                             post_and_listen("''", "'foo'");
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             // Doubles:
                             post_and_listen("0.0", "6.1");
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             // Dictionaries:
                             post_and_listen("null", "{ foo: 'bar' }");
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                           kInteractionTestUtilCustomEventType)
                  .SetElementID(kInteractionSequenceBrowserUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) { check_elapsed(); }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       NavigatePageFromScriptCreatesNewElement) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             std::ostringstream oss;
                             oss << "window.location = '" << url.spec() << "'";
                             util.Evaluate(oss.str());
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementRemovedOnMoveToNewBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kInteractionSequenceBrowserUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        chrome::MoveTabsToExistingWindow(
                            browser(), other_browser,
                            {browser()->tab_strip_model()->active_index()});
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_THAT(ui::ElementTracker::GetElementTracker()
                  ->GetAllMatchingElementsInAnyContext(
                      kInteractionSequenceBrowserUtilTestId),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementRemovedOnPageClosed) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             browser()->tab_strip_model()->CloseSelectedTabs();
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
