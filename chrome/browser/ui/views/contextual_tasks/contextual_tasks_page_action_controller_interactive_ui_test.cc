// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
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

  contextual_tasks::ContextualTasksService* GetContextualTasksService() {
    return contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
        browser()->profile());
  }

  auto CreateTaskForTab(int tab_index) {
    return Do([&, tab_index] {
      contextual_tasks::ContextualTask task =
          GetContextualTasksService()->CreateTask();
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      GetContextualTasksService()->AssociateTabWithTask(task.GetTaskId(),
                                                        session_id);
    });
  }

  auto RemoveTaskFromTab(int tab_index) {
    return Do([&, tab_index] {
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      std::optional<contextual_tasks::ContextualTask> task =
          GetContextualTasksService()->GetContextualTaskForTab(session_id);
      if (task.has_value()) {
        GetContextualTasksService()->DisassociateTabFromTask(
            task.value().GetTaskId(), session_id);
      }
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPageActionControllerInteractiveTest,
                       ShowContextualTaskPageAction) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  EnsureNotPresent(kContextualTasksPageActionElementId),
                  NavigateWebContents(kFirstTab, GetTestURL()),
                  CreateTaskForTab(0),
                  WaitForShow(kContextualTasksPageActionElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPageActionControllerInteractiveTest,
                       HideContextualTaskPageAction) {
  RunTestSequence(InstrumentTab(kFirstTab), CreateTaskForTab(0),
                  WaitForShow(kContextualTasksPageActionElementId),
                  RemoveTaskFromTab(0),
                  WaitForHide(kContextualTasksPageActionElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPageActionControllerInteractiveTest,
                       ToggleContextualTasksSidePanel) {
  RunTestSequence(
      InstrumentTab(kFirstTab), NavigateWebContents(kFirstTab, GetTestURL()),
      CreateTaskForTab(0), WaitForShow(kContextualTasksPageActionElementId),
      // Pressing the page action should open the toolbar height side panel
      PressButton(kContextualTasksPageActionElementId),
      WaitForShow(kSidePanelElementId),
      // Pressing the button again should close the toolbar height side panel
      // but the page action chip should remain visible
      PressButton(kContextualTasksPageActionElementId),
      WaitForHide(kSidePanelElementId),
      EnsurePresent(kContextualTasksPageActionElementId));
}
