// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_test.h"

class ProjectsPanelStateControllerBrowserTest : public InProcessBrowserTest {
 public:
  ProjectsPanelStateControllerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(tab_groups::kProjectsPanel);
  }

 protected:
  ProjectsPanelStateController* controller() {
    return ProjectsPanelStateController::From(browser());
  }

  glic::GlicTestEnvironment glic_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProjectsPanelStateControllerBrowserTest,
                       CanShowGeminiThreads) {
  // Initial state should be true because GlicTestEnvironment defaults to
  // force_signin_and_glic_capability = true and fre_status = kCompleted.
  EXPECT_TRUE(controller()->CanShowGeminiThreads());

  int call_count = 0;
  auto subscription =
      controller()->RegisterOnThreadEligibilityChanged(base::BindRepeating(
          [](int* call_count, ProjectsPanelStateController*) {
            (*call_count)++;
          },
          &call_count));

  // Disable Glic capability. Displaying Gemini chats should no longer be
  // allowed.
  glic::SetGlicCapability(browser()->profile(), false);
  EXPECT_FALSE(controller()->CanShowGeminiThreads());
  EXPECT_EQ(1, call_count);

  // Enable Glic capability. Displaying Gemini chats should be allowed again.
  glic::SetGlicCapability(browser()->profile(), true);
  EXPECT_TRUE(controller()->CanShowGeminiThreads());
  EXPECT_EQ(2, call_count);
}
