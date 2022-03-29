// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/user_education/interaction_sequence_browser_util.h"

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/user_education/user_education_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInteractionSequenceBrowserUtilTestId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(
    kInteractionSequenceBrowserUtilCustomEventId);
}  // namespace

class InteractionSequenceBrowserUtilInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  InteractionSequenceBrowserUtilInteractiveUiTest() = default;
  ~InteractionSequenceBrowserUtilInteractiveUiTest() override = default;
  InteractionSequenceBrowserUtilInteractiveUiTest(
      const InteractionSequenceBrowserUtilInteractiveUiTest&) = delete;
  void operator=(const InteractionSequenceBrowserUtilInteractiveUiTest&) =
      delete;

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

IN_PROC_BROWSER_TEST_F(
    InteractionSequenceBrowserUtilInteractiveUiTest,
    NavigateMenuAndBringUpDownloadsPageThenOpenMoreActionsMenu) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  InteractionSequenceBrowserUtil util(browser(),
                                      kInteractionSequenceBrowserUtilTestId);
  auto test_util = CreateInteractionTestUtil();
  const ui::ElementContext context = browser()->window()->GetElementContext();
  const InteractionSequenceBrowserUtil::DeepQuery kButtonQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-icon-button#moreActions"};
  const InteractionSequenceBrowserUtil::DeepQuery kDialogQuery = {
      "downloads-manager", "downloads-toolbar#toolbar",
      "cr-action-menu#moreActionsMenu", "dialog#dialog"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->PressButton(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(AppMenuModel::kDownloadsMenuItem)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util->SelectMenuItem(element);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             auto* const contents =
                                 element->AsA<TrackedElementWebPage>()
                                     ->owner()
                                     ->web_contents();
                             ASSERT_EQ(GURL("chrome://downloads/"),
                                       contents->GetURL());
                             ASSERT_NE(nullptr, contents->GetWebUI());
                             EXPECT_FALSE(
                                 util.EvaluateAt(kDialogQuery, "el => el.open")
                                     .GetBool());
                             util.EvaluateAt(kButtonQuery, "el => el.click()");
                             InteractionSequenceBrowserUtil::StateChange change;
                             change.where = kDialogQuery;
                             change.event =
                                 kInteractionSequenceBrowserUtilCustomEventId;
                             change.test_function = "el => el.open";
                             util.SendEventOnStateChange(change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionSequenceBrowserUtilCustomEventId)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
