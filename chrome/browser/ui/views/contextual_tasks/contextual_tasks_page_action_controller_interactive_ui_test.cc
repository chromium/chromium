// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
}  // namespace

class ContextualTasksPageActionControllerInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageActionsMigration, {}},
         {contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "page-action-revisit"}}},
         {features::kTabbedBrowserUseNewLayout, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestURL() {
    return embedded_test_server()->GetURL("example.com", "/title1.html");
  }

  contextual_tasks::ContextualTasksContextController*
  GetContextualTasksController() {
    return contextual_tasks::ContextualTasksContextControllerFactory::
        GetForProfile(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPageActionControllerInteractiveTest,
                       DISABLED_ShowContextualTaskPageAction) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  EnsureNotPresent(kContextualTasksPageActionElementId),
                  NavigateWebContents(kFirstTab, GetTestURL()),
                  Do([&] { GetContextualTasksController()->CreateTask(); }),
                  WaitForShow(kContextualTasksPageActionElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPageActionControllerInteractiveTest,
                       HideContextualTaskPageAction) {
  contextual_tasks::ContextualTask task =
      GetContextualTasksController()->CreateTask();
  RunTestSequence(
      InstrumentTab(kFirstTab),
      WaitForShow(kContextualTasksPageActionElementId),
      Do([&] { GetContextualTasksController()->DeleteTask(task.GetTaskId()); }),
      WaitForHide(kContextualTasksPageActionElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPageActionControllerInteractiveTest,
                       ToggleContextualTasksSidePanel) {
  contextual_tasks::ContextualTask task =
      GetContextualTasksController()->CreateTask();
  RunTestSequence(
      InstrumentTab(kFirstTab),
      NavigateWebContents(kFirstTab, embedded_test_server()->GetURL(
                                         "example.com", "/title1.html")),
      WaitForShow(kContextualTasksPageActionElementId),
      // Pressing the page action should open the toolbar height side panel
      PressButton(kContextualTasksPageActionElementId),
      WaitForShow(kSidePanelElementId),
      // // Pressing the button again should close the toolbar height side panel
      PressButton(kContextualTasksPageActionElementId),
      WaitForHide(kSidePanelElementId));
}
