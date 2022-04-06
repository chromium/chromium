// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_sequence_browser_util.h"

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
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  auto util = InteractionSequenceBrowserUtil::ForExistingTabInContext(
      browser()->window()->GetElementContext(),
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
                            util.get(),
                            element->AsA<TrackedElementWebPage>()->owner());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementCreatedForExistingWebContentsWithoutBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Using this constructor hits all of the rest of the constructors, saving us
  // the hassle of writing three identical tests.
  auto util = InteractionSequenceBrowserUtil::ForWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0),
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
                            util.get(),
                            element->AsA<TrackedElementWebPage>()->owner());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ElementCreatedForExistingWebContentsWithBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Using this constructor hits all of the rest of the constructors, saving us
  // the hassle of writing three identical tests.
  auto util = InteractionSequenceBrowserUtil::ForWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      kInteractionSequenceBrowserUtilTestId, browser());
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
                            util.get(),
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             util->LoadPage(url);
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
  DCHECK_EQ(url, util->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, IsPageLoaded) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             EXPECT_TRUE(util->is_page_loaded());
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
                             EXPECT_FALSE(util->is_page_loaded());
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->is_page_loaded());
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             util->set_page_identifier(
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);

  // Load the first page and make sure we wait for the page transition.
  util->LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util->web_contents()->GetURL());
                             // Load the second page and wait for it to finish
                             // loading.
                             util->set_page_identifier(
                                 kInteractionSequenceBrowserUtilTestId2);
                             util->LoadPage(url2);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url2, util->web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoBack(browser()));
                             util->set_page_identifier(
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
                             EXPECT_EQ(url, util->web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoForward(browser()));
                             util->set_page_identifier(
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
                             EXPECT_EQ(url2, util->web_contents()->GetURL());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, EvaluateInt) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             EXPECT_EQ(1, util->Evaluate("() => 1").GetInt());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, EvaluateString) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             EXPECT_EQ(
                                 std::string("The quick brown fox"),
                                 util->Evaluate("() => 'The quick brown fox'")
                                     .GetString());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, EvaluatePromise) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  constexpr char kPromiseScript[] =
      "() => new Promise((resolve) => setTimeout(resolve(123), 300))";
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
                             EXPECT_EQ(123,
                                       util->Evaluate(kPromiseScript).GetInt());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       SendEventOnStateChangeOnCurrentCondition) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             util->Evaluate("function() { window.value = 1; }");
                             InteractionSequenceBrowserUtil::StateChange
                                 state_change;
                             state_change.test_function = "() => window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             util->Evaluate(
                                 R"(function () {
                                      window.value = 0;
                                      setTimeout(
                                        function() { window.value = 1; },
                                        300);
                                    })");
                             InteractionSequenceBrowserUtil::StateChange
                                 state_change;
                             state_change.test_function = "() => window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             util->Evaluate(
                                 R"(function () {
                                      window.value = 0;
                                      setTimeout(
                                        function() { window.value = 1; },
                                        1000);
                                    })");
                             InteractionSequenceBrowserUtil::StateChange
                                 state_change;
                             state_change.test_function = "() => window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.timeout = base::Milliseconds(300);
                             state_change.timeout_event =
                                 kInteractionTestUtilCustomEventType2;
                             util->SendEventOnStateChange(state_change);
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);

  // Poll significantly faster than the value in the page is expected to
  // change; this allows us to verify that the value changes after a non-zero
  // amount of time.
  constexpr base::TimeDelta kPollTime = base::Milliseconds(50);
  constexpr int kScriptDelayMs = 150;

  base::ElapsedTimer timer;
  base::TimeDelta last;

  // Sets window.value to an initial value, and then some time later, sets it
  // to a final value.
  const auto post_and_listen = [&](base::Value initial, base::Value final) {
    std::string script = content::JsReplace(
        R"(function() {
             window.value = $1;
             setTimeout(function() { window.value = $2; }, $3);
           })",
        std::move(initial), std::move(final), kScriptDelayMs);
    util->Evaluate(script);
    InteractionSequenceBrowserUtil::StateChange state_change;
    state_change.test_function = "() => window.value";
    state_change.event = kInteractionTestUtilCustomEventType;
    state_change.polling_interval = kPollTime;
    util->SendEventOnStateChange(state_change);
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
                             post_and_listen(base::Value(0), base::Value(1));
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
                             post_and_listen(base::Value(false),
                                             base::Value(true));
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
                             post_and_listen(base::Value(""),
                                             base::Value("foo"));
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
                             post_and_listen(base::Value(0.0),
                                             base::Value(6.1));
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
                             base::Value::List list;
                             list.Append(false);
                             post_and_listen(base::Value(),
                                             base::Value(std::move(list)));
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
                             base::Value::Dict dict;
                             dict.Set("foo", "bar");
                             post_and_listen(base::Value(),
                                             base::Value(std::move(dict)));
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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
                             util->Evaluate(content::JsReplace(
                                 "function() { window.location = $1; }",
                                 url.spec()));
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
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

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       OpenPageInNewTabInactive) {
  const GURL url = embedded_test_server()->GetURL("/empty.html");

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto* const model = browser()->tab_strip_model();
  const int count = model->GetTabCount();
  const int index = model->active_index();
  util->LoadPageInNewTab(url, false);
  EXPECT_EQ(count + 1, model->GetTabCount());
  EXPECT_EQ(index, model->active_index());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       OpenPageInNewTabActive) {
  const GURL url = embedded_test_server()->GetURL("/empty.html");

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto* const model = browser()->tab_strip_model();
  const int count = model->GetTabCount();
  const int index = model->active_index();
  util->LoadPageInNewTab(url, true);
  EXPECT_EQ(count + 1, model->GetTabCount());
  EXPECT_EQ(index + 1, model->active_index());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ForNextTabInContext) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto util2 = InteractionSequenceBrowserUtil::ForNextTabInContext(
      browser()->window()->GetElementContext(),
      kInteractionSequenceBrowserUtilTestId2);

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
                             util->LoadPageInNewTab(url, false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ForNextTabInBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");
  Browser* const browser2 = CreateBrowser(browser()->profile());

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto util2 = InteractionSequenceBrowserUtil::ForNextTabInBrowser(
      browser2, kInteractionSequenceBrowserUtilTestId2);

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
                             util2->LoadPageInNewTab(url, true);
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());

  // Wait for the element in the other browser to appear.
  // TODO(dfried): when we support cross-context sequences, these can be
  // combined.

  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback,
                         completed2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted2);

  auto sequence2 =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed2.Get())
          .SetAbortedCallback(aborted2.Get())
          .SetContext(browser2->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed2, Run,
                       sequence2->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ForNextTabInAnyBrowserFreshBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");
  Browser* browser2 = nullptr;

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto util2 = InteractionSequenceBrowserUtil::ForNextTabInAnyBrowser(
      kInteractionSequenceBrowserUtilTestId2);

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
                             // Open a completely new browser, we'll detect it
                             // opened and capture its first tab.
                             browser2 = CreateBrowser(browser()->profile());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());

  // Wait for the element in the other browser to appear.
  // TODO(dfried): when we support cross-context sequences, these can be
  // combined.

  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback,
                         completed2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted2);

  auto sequence2 =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed2.Get())
          .SetAbortedCallback(aborted2.Get())
          .SetContext(browser2->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed2, Run,
                       sequence2->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ForNextTabInAnyBrowserSameBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL("/empty.html");

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto util2 = InteractionSequenceBrowserUtil::ForNextTabInAnyBrowser(
      kInteractionSequenceBrowserUtilTestId2);

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
                             util->LoadPageInNewTab(url, false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       MovePageToNewBrowserTriggersTabInAnyBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto util2 = InteractionSequenceBrowserUtil::ForNextTabInAnyBrowser(
      kInteractionSequenceBrowserUtilTestId2);

  auto get_element2 = [&]() {
    const auto result = ui::ElementTracker::GetElementTracker()
                            ->GetAllMatchingElementsInAnyContext(
                                kInteractionSequenceBrowserUtilTestId2);
    return result.empty() ? nullptr : result.front();
  };

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
                        EXPECT_EQ(nullptr, get_element2());
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
  auto* const element = get_element2();
  EXPECT_NE(nullptr, element);
  EXPECT_EQ(other_browser->window()->GetElementContext(), element->context());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       MovePageToNewBrowserTriggersNextTabInBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  auto util2 = InteractionSequenceBrowserUtil::ForNextTabInBrowser(
      other_browser, kInteractionSequenceBrowserUtilTestId2);

  auto get_element2 = [&]() {
    const auto result = ui::ElementTracker::GetElementTracker()
                            ->GetAllMatchingElementsInAnyContext(
                                kInteractionSequenceBrowserUtilTestId2);
    return result.empty() ? nullptr : result.front();
  };

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
                        EXPECT_EQ(nullptr, get_element2());
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
  auto* const element = get_element2();
  EXPECT_NE(nullptr, element);
  EXPECT_EQ(other_browser->window()->GetElementContext(), element->context());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest, ExistsInWebUIPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const InteractionSequenceBrowserUtil::DeepQuery kQuery1{
      "settings-ui", "settings-main#main", "div#noSearchResults"};
  const InteractionSequenceBrowserUtil::DeepQuery kQuery2{
      "settings-ui", "settings-main#foo", "div#noSearchResults"};

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  util->LoadPage(GURL("chrome://settings"));

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->Exists(kQuery1));
                             std::string failed;
                             EXPECT_FALSE(util->Exists(kQuery2, &failed));
                             EXPECT_EQ(kQuery2[1], failed);
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       EvaluateAtInWebUIPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const InteractionSequenceBrowserUtil::DeepQuery kQuery{
      "settings-ui", "settings-main#main", "div#noSearchResults"};

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  util->LoadPage(GURL("chrome://settings"));

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
                             const auto result =
                                 util->EvaluateAt(kQuery, "el => el.innerText");
                             EXPECT_TRUE(result.is_string());
                             EXPECT_FALSE(result.GetString().empty());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       ExistsInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const InteractionSequenceBrowserUtil::DeepQuery kQuery1{"#ref"};
  const InteractionSequenceBrowserUtil::DeepQuery kQuery2{"#not-present"};

  // These queries check that we can properly escape quotes:
  const InteractionSequenceBrowserUtil::DeepQuery kQuery3{"[id=\"ref\"]"};
  const InteractionSequenceBrowserUtil::DeepQuery kQuery4{"[id='ref']"};

  // These queries check that we can return strings with quotes on failure:
  const InteractionSequenceBrowserUtil::DeepQuery kQuery5{
      "[id=\"not-present\"]"};
  const InteractionSequenceBrowserUtil::DeepQuery kQuery6{"[id='not-present']"};

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  const GURL url = embedded_test_server()->GetURL("/links.html");
  util->LoadPage(url);

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
                             // Using DeepQuery.
                             EXPECT_TRUE(util->Exists(kQuery1));
                             std::string failed;
                             EXPECT_FALSE(util->Exists(kQuery2, &failed));
                             EXPECT_EQ(kQuery2[0], failed);
                             EXPECT_TRUE(util->Exists(kQuery3));
                             EXPECT_TRUE(util->Exists(kQuery4));
                             EXPECT_FALSE(util->Exists(kQuery5, &failed));
                             EXPECT_EQ(kQuery5[0], failed);
                             EXPECT_FALSE(util->Exists(kQuery6, &failed));
                             EXPECT_EQ(kQuery6[0], failed);

                             // Using the simple string selector version.
                             EXPECT_TRUE(util->Exists(kQuery1[0]));
                             EXPECT_FALSE(util->Exists(kQuery2[0]));
                             EXPECT_TRUE(util->Exists(kQuery3[0]));
                             EXPECT_TRUE(util->Exists(kQuery4[0]));
                             EXPECT_FALSE(util->Exists(kQuery5[0]));
                             EXPECT_FALSE(util->Exists(kQuery6[0]));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       EvaluateAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const InteractionSequenceBrowserUtil::DeepQuery kQuery{"#ref"};
  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  const GURL url = embedded_test_server()->GetURL("/links.html");
  util->LoadPage(url);

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
                             // Test evaluate at with a DeepQuery.
                             auto result =
                                 util->EvaluateAt(kQuery, "el => el.innerText");
                             EXPECT_TRUE(result.is_string());
                             EXPECT_EQ("ref link", result.GetString());

                             // Test evaluate at with a plain string selector.
                             result = util->EvaluateAt(kQuery[0],
                                                       "el => el.innerText");
                             EXPECT_TRUE(result.is_string());
                             EXPECT_EQ("ref link", result.GetString());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(InteractionSequenceBrowserUtilTest,
                       SendEventOnConditionStateChangeAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const InteractionSequenceBrowserUtil::DeepQuery kQuery{"#ref"};
  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  const GURL url = embedded_test_server()->GetURL("/links.html");
  util->LoadPage(url);

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
                             util->EvaluateAt(kQuery,
                                              R"(el => {
                                      el.innerText = '';
                                      setTimeout(() => el.innerText = 'foo',
                                                 300);
                                    })");
                             InteractionSequenceBrowserUtil::StateChange change;
                             change.test_function = "el => el.innerText";
                             change.where = kQuery;
                             change.event = kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(change);
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
                       SendEventOnExistsStateChangeAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const InteractionSequenceBrowserUtil::DeepQuery kQuery1{"#ref"};
  const InteractionSequenceBrowserUtil::DeepQuery kQuery2{"#ref", "p#pp"};
  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  const GURL url = embedded_test_server()->GetURL("/links.html");
  util->LoadPage(url);

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
                             util->EvaluateAt(kQuery1,
                                              R"(el => {
                                                el.innerText = '';
                                                setTimeout(() =>
                                                    el.innerHTML =
                                                        '<p id="pp">foo</p>',
                                                 300);
                                                })");
                             InteractionSequenceBrowserUtil::StateChange change;
                             change.where = kQuery2;
                             change.event = kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(change);
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
