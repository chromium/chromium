// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
}  // namespace

class ContextualTasksButtonInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageActionsMigration, {}},
         {contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "toolbar-permanent"}}},
         {features::kTabbedBrowserUseNewLayout, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }

  PrefService* GetPrefService() { return browser()->profile()->GetPrefs(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksButtonInteractiveTest,
                       ShowContextualTaskButton) {
  RunTestSequence(
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      Check([&] {
        return GetPrefService()->GetBoolean(prefs::kPinContextualTaskButton);
      }),
      Do([&] {
        GetPrefService()->SetBoolean(prefs::kPinContextualTaskButton, false);
      }),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton),
      Do([&] {
        GetPrefService()->SetBoolean(prefs::kPinContextualTaskButton, true);
      }),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksButtonInteractiveTest,
                       ToggleToolbarHeightSidePanel) {
  RunTestSequence(
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForShow(kSidePanelElementId),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForHide(kSidePanelElementId));
}

class ContextualTasksEphemeralButtonInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageActionsMigration, {}},
         {contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "toolbar-revisit"}}},
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

  auto CreateTaskForTab(int tab_index) {
    return Do([&, tab_index] {
      contextual_tasks::ContextualTask task =
          GetContextualTasksController()->CreateTask();
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      GetContextualTasksController()->AssociateTabWithTask(task.GetTaskId(),
                                                           session_id);
    });
  }

  auto RemoveTaskFromTab(int tab_index) {
    return Do([&, tab_index] {
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      std::optional<contextual_tasks::ContextualTask> task =
          GetContextualTasksController()->GetContextualTaskForTab(session_id);
      if (task.has_value()) {
        GetContextualTasksController()->DisassociateTabFromTask(
            task.value().GetTaskId(), session_id);
      }
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       EphemeralButtonUpdatesVisibility) {
  RunTestSequence(
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      SelectTab(kTabStripElementId, 1),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton),
      SelectTab(kTabStripElementId, 0),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       HideEphemeralButtonWhenNotAssociatedToTask) {
  RunTestSequence(
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      RemoveTaskFromTab(0),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       EphemeralButtonVisibilityIsPreservedAsSidePanelToggles) {
  RunTestSequence(
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0), SelectTab(kTabStripElementId, 0),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForShow(kSidePanelElementId),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForHide(kSidePanelElementId),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton));
}
