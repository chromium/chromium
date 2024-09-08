// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

#include <sstream>

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId2);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInteractionTestUtilCustomEventType);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInteractionTestUtilCustomEventType2);

constexpr char kEmptyDocumentURL[] = "/empty.html";
constexpr char kDocumentWithTitle1URL[] = "/title1.html";
constexpr char kDocumentWithTitle2URL[] = "/title2.html";
constexpr char kDocumentWithLinksURL[] = "/links.html";

}  // namespace

class WebContentsInteractionTestUtilTest : public InProcessBrowserTest {
 public:
  WebContentsInteractionTestUtilTest() = default;
  ~WebContentsInteractionTestUtilTest() override = default;

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

  ui::InteractionSequence::Builder DefaultBuilder(
      Browser* context_browser = nullptr) {
    if (!context_browser) {
      context_browser = browser();
    }
    return std::move(
        ui::InteractionSequence::Builder()
            .SetContext(context_browser->window()->GetElementContext())
            // Because the state of the util needs to be checked immediately,
            // the start callbacks need to be immediate.
            .SetDefaultStepStartMode(
                ui::InteractionSequence::StepStartMode::kImmediate));
  }
};

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementCreatedForExistingPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Using this constructor hits all of the rest of the constructors, saving us
  // the hassle of writing three identical tests.
  auto util = WebContentsInteractionTestUtil::ForExistingTabInContext(
      browser()->window()->GetElementContext(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(element->IsA<TrackedElementWebContents>());
                        EXPECT_EQ(
                            util.get(),
                            element->AsA<TrackedElementWebContents>()->owner());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementCreatedForExistingWebContentsWithoutWebView) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForTabWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(element->IsA<TrackedElementWebContents>());
                        EXPECT_EQ(
                            util.get(),
                            element->AsA<TrackedElementWebContents>()->owner());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementRecreatedOnNavigate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                       .SetTransitionOnlyOnEvent(true)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest, LoadPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPage(url);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util->web_contents()->GetURL());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest, IsPageLoaded) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_FALSE(util->is_page_loaded());
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->is_page_loaded());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementRecreatedWithDifferentIdOnNavigate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->set_page_identifier(kWebContentsElementId2);
                             NavigateParams navigate_params(
                                 browser(), url, ui::PAGE_TRANSITION_TYPED);
                             Navigate(&navigate_params);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementRecreatedWithDifferentIdOnBackForward) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Do two navigations, then go back, then forward again.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithTitle1URL);
  const GURL url2 = embedded_test_server()->GetURL(kDocumentWithTitle2URL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Load the first page and make sure we wait for the page transition.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPage(url);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetMustRemainVisible(false)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util->web_contents()->GetURL());
                             // Load the second page and wait for it to finish
                             // loading.
                             util->set_page_identifier(kWebContentsElementId2);
                             util->LoadPage(url2);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url2, util->web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoBack(browser()));
                             util->set_page_identifier(kWebContentsElementId);
                             chrome::GoBack(browser(),
                                            WindowOpenDisposition::CURRENT_TAB);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util->web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoForward(browser()));
                             util->set_page_identifier(kWebContentsElementId2);
                             chrome::GoForward(
                                 browser(), WindowOpenDisposition::CURRENT_TAB);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
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

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementCreatedInFreshBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithTitle1URL);

  // Open a new browser, and immediately navigate to a new page. Even though the
  // original chrome://new-tab-page might not have finished loading, the
  // element should not be created until the new URL is loaded.
  Browser* browser2 = chrome::OpenEmptyWindow(
      browser()->profile(), /*should_trigger_session_restore=*/false);
  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser2, kWebContentsElementId);
  util->LoadPage(url);

  auto sequence =
      DefaultBuilder(browser2)
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetContext(ui::InteractionSequence::ContextMode::kAny)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(util->web_contents()->GetURL(), url);
                             EXPECT_EQ(
                                 util->web_contents()->GetLastCommittedURL(),
                                 url);
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest, EvaluateInt) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(1, util->Evaluate("() => 1").GetInt());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest, EvaluateString) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest, EvaluatePromise) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  constexpr char kPromiseScript[] =
      "() => new Promise((resolve) => setTimeout(resolve(123), 300))";
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnStateChangeOnCurrentCondition) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate("function() { window.value = 1; }");
                             WebContentsInteractionTestUtil::StateChange
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnStateChangeOnDelayedCondition) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                             WebContentsInteractionTestUtil::StateChange
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       StateChangeTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                             WebContentsInteractionTestUtil::StateChange
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       StateChangeOnPromise) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  constexpr base::TimeDelta kPollTime = base::Milliseconds(50);
  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.test_function = R"(() => {
                                 return new Promise(r => {
                                       setTimeout(() => r(1), 100);
                                     });
                                 })";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.polling_interval = kPollTime;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendStateChangeEventsForDifferentDataTypes) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);

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
    WebContentsInteractionTestUtil::StateChange state_change;
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
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
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
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) { check_elapsed(); }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnStateChangeOnAlreadyExists) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  // This is an element in the document.
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"a#title1"};

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Wait for an element which is already in the document to exist and
          // fire an event when it does (which should be almost immediate).
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kExists;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())

          // Once the event is received, verify synchronouosly that the element
          // does exist.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->Exists(kQuery));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnStateChangeOnExistsAfterDelay) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  // This is an element that is not (yet) in the document.
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"ul#foo"};

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Queue a method that will cause an element to be added to the
          // document in 300 ms, then wait for it to become present and fire an
          // event.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Inject JS that will add the element later.
                             util->Evaluate(
                                 R"(function () {
                                      setTimeout(
                                        function() {
                                          let el = document.createElement('ul');
                                          el.id = 'foo';
                                          document.body.appendChild(el);
                                        },
                                        300);
                                    })");

                             // Set up the waiter.
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kExists;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);

                             // Verify that the element doesn't exist yet.
                             EXPECT_FALSE(util->Exists(kQuery));
                           }))
                       .Build())

          // Once the event is received, verify synchronouosly that the element
          // does exist.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->Exists(kQuery));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// TODO(crbug.com/40285351): Resolve flakiness on ChromeOS and re-enable the
// test.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_StateChangeExistsTimeoutSendsEvent \
  DISABLED_StateChangeExistsTimeoutSendsEvent
#else
#define MAYBE_StateChangeExistsTimeoutSendsEvent \
  StateChangeExistsTimeoutSendsEvent
#endif
IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       MAYBE_StateChangeExistsTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  // This element is not yet present in the document.
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"ul#foo"};

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Queue a method that will cause an element to be added to the
          // document in 1000 ms, then wait for it to become present and fire an
          // event with only a 300ms timeout - the timeout event will be fired
          // instead.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Inject JS into the document that will add the
                             // element in 1000 ms.
                             util->Evaluate(
                                 R"(function () {
                                      setTimeout(
                                        function() {
                                          let el = document.createElement('ul');
                                          el.id = 'foo';
                                          document.body.appendChild(el);
                                        },
                                        1000);
                                    })");

                             // Set up a waiter that will only wait 300 ms.
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kExists;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.timeout = base::Milliseconds(300);
                             state_change.timeout_event =
                                 kInteractionTestUtilCustomEventType2;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())

          // Verify that the timeout event was sent. The sequence will fail if
          // the event is not received.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType2)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnStateChangeOnAlreadyDoesNotExist) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  // This element is not actually in the document.
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"a#title5"};

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Wait for an element which is already not in the document to not
          // exist and fire an event (which should be almost immediate).
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kDoesNotExist;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())

          // Once the event is received, ensure that the element is not present.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_FALSE(util->Exists(kQuery));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnStateChangeOnDoesNotExistAfterDelay) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  // This element is not initially present in the document.
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"ul#foo"};

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Add the element referenced above and then remove it in 300 ms.
          // In the interim, wait for the element to be removed and send an
          // event when it is.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // This adds the element and queues up a callback
                             // to remove it again in 300 ms.
                             util->Evaluate(
                                 R"(() => {
                                      let el = document.createElement('ul');
                                      el.id = 'foo';
                                      document.body.appendChild(el);
                                      setTimeout(
                                        function() {
                                          el.remove();
                                        },
                                        300);
                                    })");

                             // Wait for the element to be removed.
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kDoesNotExist;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);

                             // Verify that the element is currently present.
                             EXPECT_TRUE(util->Exists(kQuery));
                           }))
                       .Build())

          // Wait for the event on removal and verify synchronously that the
          // element is actually gone.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_FALSE(util->Exists(kQuery));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       StateChangeDoesNotExistTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  // This element is not initially in the document.
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"ul#foo"};

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          // Add the above-referenced element to the document, and then set up
          // a callback to remove it 1000 ms later. Wait only 300 ms for the
          // element to disappear (sending an event on timeout).
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Adds the element and then queues up its removal
                             // 1000 ms later.
                             util->Evaluate(
                                 R"(() => {
                                      let el = document.createElement('ul');
                                      el.id = 'foo';
                                      document.body.appendChild(el);
                                      setTimeout(
                                        function() {
                                          el.remove();
                                        },
                                        1000);
                                    })");

                             // Wait for the element to go away, but only 300
                             // ms. This should send the timeout event instead
                             // of the success event.
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kDoesNotExist;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.timeout = base::Milliseconds(300);
                             state_change.timeout_event =
                                 kInteractionTestUtilCustomEventType2;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())

          // Verify that the timeout event was sent. The sequence will fail if
          // the event is not received.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType2)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnStateChangeOnAlreadyExistsAndConditionTrue) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"a#title1"};
  const char kTestCondition[] = "el => (el.innerText == 'Go to title1')";

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kExistsAndConditionTrue;
                             state_change.test_function = kTestCondition;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                           kInteractionTestUtilCustomEventType)
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(
                            util->EvaluateAt(kQuery, kTestCondition).GetBool());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(
    WebContentsInteractionTestUtilTest,
    SendEventOnStateChangeOnExistsAndConditionTrueAfterDelay) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"h1#foo"};
  const char kTestCondition[] = "el => (el.innerText == 'bar')";

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      setTimeout(
                                        function() {
                                          let el = document.createElement('h1');
                                          el.id = 'foo';
                                          document.body.appendChild(el);
                                          setTimeout(
                                            function() {
                                              let el = document.querySelector(
                                                  'h1#foo');
                                              el.innerText = 'bar';
                                            },
                                            100);
                                        },
                                        300);
                                    })");
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kExistsAndConditionTrue;
                             state_change.where = kQuery;
                             state_change.test_function = kTestCondition;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                             EXPECT_FALSE(util->Exists(kQuery));
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                           kInteractionTestUtilCustomEventType)
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(
                            util->EvaluateAt(kQuery, kTestCondition).GetBool());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       StateChangeExistsAndConditionTrueTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"h1#foo"};
  const char kTestCondition[] = "el => (el.innerText == 'bar')";

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      let el = document.createElement('h1');
                                      el.id = 'foo';
                                      document.body.appendChild(el);
                                      setTimeout(
                                          function() {
                                            let el = document.querySelector(
                                                'h1#foo');
                                            el.innerText = 'bar';
                                          },
                                          1000);
                                    })");
                             WebContentsInteractionTestUtil::StateChange
                                 state_change;
                             state_change.type =
                                 WebContentsInteractionTestUtil::StateChange::
                                     Type::kExistsAndConditionTrue;
                             state_change.where = kQuery;
                             state_change.test_function = kTestCondition;
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ExecuteCatchesJavascriptError) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             std::string err;
                             util->Evaluate(
                                 "() => { throw new Error('an error'); }",
                                 &err);
                             EXPECT_FALSE(err.empty());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ExecuteCanChangePageState) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        // This is an artificial value that is not
                        // initially true.
                        const char kCheckFunction[] = "() => !!window.value";
                        EXPECT_FALSE(util->Evaluate(kCheckFunction).GetBool());

                        // Prepare to send an event when the condition
                        // becomes true.
                        WebContentsInteractionTestUtil::StateChange
                            state_change;
                        state_change.test_function = kCheckFunction;
                        state_change.event =
                            kInteractionTestUtilCustomEventType;
                        util->SendEventOnStateChange(state_change);

                        // Immediately set a truthy value.
                        util->Execute("() => { window.value = 1; }");
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ExecuteAtCanChangePageState) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebContentsInteractionTestUtil::DeepQuery kQuery = {"a#title1"};

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        // This is an artificial value that is not
                        // initially true.
                        const char kCheckFunction[] =
                            "el => (el.innerText === 'abcde')";

                        // Verify that the check function is false.
                        EXPECT_FALSE(
                            util->EvaluateAt(kQuery, kCheckFunction).GetBool());

                        // Set up a condition check for a text string that
                        // doesn't exist in the document.
                        WebContentsInteractionTestUtil::StateChange
                            state_change;
                        state_change.where = kQuery;
                        state_change.test_function = kCheckFunction;
                        state_change.event =
                            kInteractionTestUtilCustomEventType;
                        util->SendEventOnStateChange(state_change);

                        // Set the expected text using ExecuteAt().
                        // The check function should become true.
                        util->ExecuteAt(kQuery,
                                        "el => { el.innerText = 'abcde'; }");
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       NavigatePageFromScriptCreatesNewElement) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementRemovedOnMoveToNewBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_THAT(ui::ElementTracker::GetElementTracker()
                  ->GetAllMatchingElementsInAnyContext(kWebContentsElementId),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ElementRemovedOnPageClosed) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             browser()->tab_strip_model()->CloseSelectedTabs();
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       OpenPageInNewTabInactive) {
  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto* const model = browser()->tab_strip_model();
  const int count = model->GetTabCount();
  const int index = model->active_index();
  util->LoadPageInNewTab(url, false);
  EXPECT_EQ(count + 1, model->GetTabCount());
  EXPECT_EQ(index, model->active_index());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       OpenPageInNewTabActive) {
  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto* const model = browser()->tab_strip_model();
  const int count = model->GetTabCount();
  const int index = model->active_index();
  util->LoadPageInNewTab(url, true);
  EXPECT_EQ(count + 1, model->GetTabCount());
  EXPECT_EQ(index + 1, model->active_index());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ForNextTabInContext) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto util2 = WebContentsInteractionTestUtil::ForNextTabInContext(
      browser()->window()->GetElementContext(), kWebContentsElementId2);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPageInNewTab(url, false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ForNextTabInBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);
  Browser* const browser2 = CreateBrowser(browser()->profile());

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto util2 = WebContentsInteractionTestUtil::ForNextTabInBrowser(
      browser2, kWebContentsElementId2);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
      DefaultBuilder(browser2)
          .SetCompletedCallback(completed2.Get())
          .SetAbortedCallback(aborted2.Get())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed2, Run,
                       sequence2->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ForNextTabInAnyBrowserFreshBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);
  Browser* browser2 = nullptr;

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto util2 = WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(
      kWebContentsElementId2);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
      DefaultBuilder(browser2)
          .SetCompletedCallback(completed2.Get())
          .SetAbortedCallback(aborted2.Get())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed2, Run,
                       sequence2->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ForNextTabInAnyBrowserSameBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto util2 = WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(
      kWebContentsElementId2);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPageInNewTab(url, false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       MovePageToNewBrowserTriggersTabInAnyBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto util2 = WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(
      kWebContentsElementId2);

  auto get_element2 = [&]() {
    const auto result =
        ui::ElementTracker::GetElementTracker()
            ->GetAllMatchingElementsInAnyContext(kWebContentsElementId2);
    return result.empty() ? nullptr : result.front();
  };

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  auto* const element = get_element2();
  EXPECT_NE(nullptr, element);
  EXPECT_EQ(other_browser->window()->GetElementContext(), element->context());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       MovePageToNewBrowserTriggersNextTabInBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto util2 = WebContentsInteractionTestUtil::ForNextTabInBrowser(
      other_browser, kWebContentsElementId2);

  auto get_element2 = [&]() {
    const auto result =
        ui::ElementTracker::GetElementTracker()
            ->GetAllMatchingElementsInAnyContext(kWebContentsElementId2);
    return result.empty() ? nullptr : result.front();
  };

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebContentsElementId)
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
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  auto* const element = get_element2();
  EXPECT_NE(nullptr, element);
  EXPECT_EQ(other_browser->window()->GetElementContext(), element->context());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest, ExistsInWebUIPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery1{
      "settings-ui", "settings-main#main", "div#noSearchResults"};
  const WebContentsInteractionTestUtil::DeepQuery kQuery2{
      "settings-ui", "settings-main#foo", "div#noSearchResults"};

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  util->LoadPage(GURL("chrome://settings"));

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       EvaluateAtInWebUIPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery{
      "settings-ui", "settings-main#main", "div#noSearchResults"};

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  util->LoadPage(GURL("chrome://settings"));

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       EvaluateAtNotExistElement) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery{
      "settings-ui", "settings-main#main", "not-exists-element"};

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  util->LoadPage(GURL("chrome://settings"));

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             const auto result =
                                 util->EvaluateAt(kQuery, "(el, err) => !!el");
                             EXPECT_TRUE(result.is_bool());
                             EXPECT_FALSE(result.GetBool());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       ExistsInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery1{"#ref"};
  const WebContentsInteractionTestUtil::DeepQuery kQuery2{"#not-present"};

  // These queries check that we can properly escape quotes:
  const WebContentsInteractionTestUtil::DeepQuery kQuery3{"[id=\"ref\"]"};
  const WebContentsInteractionTestUtil::DeepQuery kQuery4{"[id='ref']"};

  // These queries check that we can return strings with quotes on failure:
  const WebContentsInteractionTestUtil::DeepQuery kQuery5{
      "[id=\"not-present\"]"};
  const WebContentsInteractionTestUtil::DeepQuery kQuery6{"[id='not-present']"};

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       EvaluateAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery{"#ref"};
  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnConditionStateChangeAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery{"#ref"};
  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->EvaluateAt(kQuery,
                                              R"(el => {
                                      el.innerText = '';
                                      setTimeout(() => el.innerText = 'foo',
                                                 300);
                                    })");
                             WebContentsInteractionTestUtil::StateChange change;
                             change.test_function = "el => el.innerText";
                             change.where = kQuery;
                             change.event = kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnExistsStateChangeAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery1{"#ref"};
  const WebContentsInteractionTestUtil::DeepQuery kQuery2{"#ref", "p#pp"};
  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                             WebContentsInteractionTestUtil::StateChange change;
                             change.where = kQuery2;
                             change.event = kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       SendEventOnExistsStateChangeContinueAcrossNavigation) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebContentsInteractionTestUtil::DeepQuery kQuery{"#ref"};
  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const GURL url1 = embedded_test_server()->GetURL(kDocumentWithTitle1URL);
  const GURL url2 = embedded_test_server()->GetURL(kDocumentWithLinksURL);

  // Navigate to the first page.
  util->LoadPage(url1);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Wait for an element that is only present on the
                             // second page, to be loaded below.
                             WebContentsInteractionTestUtil::StateChange change;
                             change.type = WebContentsInteractionTestUtil::
                                 StateChange::Type::kExists;
                             change.where = kQuery;
                             change.event = kInteractionTestUtilCustomEventType;
                             change.continue_across_navigation = true;
                             util->SendEventOnStateChange(change);

                             // Navigate to the second page.
                             util->LoadPage(url2);
                           }))
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebContentsElementId)
                       .SetMustBeVisibleAtStart(false)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This is a regression test for the case where we open a new tab in a way that
// causes it not to have a URL; previously, it would not create an element
// because navigating_away_from_ was empty.
IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       CreatesElementForPageWithBlankURL) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kExistingTabElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabElementId);

  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  std::unique_ptr<WebContentsInteractionTestUtil> existing_tab =
      WebContentsInteractionTestUtil::ForExistingTabInBrowser(
          browser(), kExistingTabElementId);
  std::unique_ptr<WebContentsInteractionTestUtil> new_tab;

  auto sequence =
      DefaultBuilder()

          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          // Get the first tab and inject code to pop up a second window.
          // Because the second window is created using a javascript: URL, it
          // will not report a valid URL.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kExistingTabElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*, ui::TrackedElement*) {
                        new_tab =
                            WebContentsInteractionTestUtil::ForNextTabInBrowser(
                                browser(), kNewTabElementId);
                        // Cause a tab to come into being and do some stuff.
                        existing_tab->Evaluate(
                            "() => window.open('javascript:window.foo=1')");
                      }))
                  .Build())
          // Verify that the element for the second tab is still created,
          // despite it not having a URL.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kNewTabElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       PaintEventSentForExistingPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  ui::test::TestElement test_el(kDummyElementId,
                                browser()->window()->GetElementContext());
  test_el.Show();

  NavigateParams params(browser(),
                        embedded_test_server()->GetURL(kDocumentWithLinksURL),
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&params);

  // Using this constructor hits all of the rest of the constructors, saving us
  // the hassle of writing three identical tests.
  std::unique_ptr<WebContentsInteractionTestUtil> util;
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kDummyElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util = WebContentsInteractionTestUtil::
                                 ForExistingTabInContext(
                                     browser()->window()->GetElementContext(),
                                     kWebContentsElementId);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                TrackedElementWebContents::kFirstNonEmptyPaint)
                       .SetElementID(kWebContentsElementId)
                       .SetMustBeVisibleAtStart(false)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       PaintEventSentOnNavigate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
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
                       .SetTransitionOnlyOnEvent(true)
                       .SetElementID(kWebContentsElementId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                TrackedElementWebContents::kFirstNonEmptyPaint)
                       .SetElementID(kWebContentsElementId)
                       .SetMustBeVisibleAtStart(false)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilTest,
                       PaintEventSentOnNavigateInBackgroundThenActivate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  auto util2 = WebContentsInteractionTestUtil::ForNextTabInContext(
      browser()->window()->GetElementContext(), kWebContentsElementId2);

  auto sequence =
      DefaultBuilder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())

          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPageInNewTab(url, false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebContentsElementId2)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // An event won't actually be sent until the page
                             // is loaded in a WebView of non-empty size, so
                             // switch tabs.
                             browser()->tab_strip_model()->ActivateTabAt(1);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                TrackedElementWebContents::kFirstNonEmptyPaint)
                       .SetElementID(kWebContentsElementId2)
                       .SetMustBeVisibleAtStart(false)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

class WebContentsInteractionTestUtilInteractiveTest
    : public InteractiveBrowserTest {
 public:
  WebContentsInteractionTestUtilInteractiveTest() = default;
  ~WebContentsInteractionTestUtilInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

// TODO(crbug.com/40268930): flaky on Mac - see comments below for the likely
// culprit line.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TrackWebContentsAcrossReplace \
  DISABLED_TrackWebContentsAcrossReplace
#else
#define MAYBE_TrackWebContentsAcrossReplace TrackWebContentsAcrossReplace
#endif
IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilInteractiveTest,
                       MAYBE_TrackWebContentsAcrossReplace) {
  const GURL url1 = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  const GURL url2 = embedded_test_server()->GetURL(kEmptyDocumentURL);
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url1),
      AddInstrumentedTab(kWebContentsElementId2, url2),
      SelectTab(kTabStripElementId, 1),
      // This has to be done on a fresh message loop.
      Do(base::BindLambdaForTesting([&]() {
        // Discard the first tab. This triggers a replacement.
        content::WebContents* original_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        std::unique_ptr<content::WebContents> new_contents =
            content::WebContents::Create(
                content::WebContents::CreateParams(browser()->profile()));
        new_contents->GetController().CopyStateFrom(
            &original_contents->GetController(), true);
        browser()->tab_strip_model()->DiscardWebContentsAt(
            0, std::move(new_contents));
      })),
      WaitForHide(kWebContentsElementId),
      // This has to be done on a fresh message loop.
      // For some reason, this does not reliably trigger page
      // reload on Mac (see crbug.com/1447298).
      SelectTab(kTabStripElementId, 0), WaitForShow(kWebContentsElementId));
}
