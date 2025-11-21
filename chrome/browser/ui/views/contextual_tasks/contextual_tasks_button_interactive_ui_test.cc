// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
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
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

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
                       ShowContextualTaskPageAction) {
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
