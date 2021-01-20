// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_UNITTEST_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_UNITTEST_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/common/extension_builder.h"
#include "ui/base/pointer/touch_ui_controller.h"

class ScopedTestingLocalState;
class ToolbarActionsBar;
class ToolbarActionsModel;

namespace extensions {
class Extension;
}

// A cross-platform unit test for the ToolbarActionsBar that uses the
// TestToolbarActionsBarHelper to create the platform-specific containers.
// TODO(devlin): Since this *does* use the real platform containers, in theory,
// we can move all the BrowserActionsBarBrowserTests to be unittests. See about
// doing this.
class ToolbarActionsBarUnitTest : public BrowserWithTestWindowTest,
                                  public testing::WithParamInterface<bool> {
 public:
  ToolbarActionsBarUnitTest();
  ~ToolbarActionsBarUnitTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  // Activates the tab at the given |index| in the tab strip model.
  void ActivateTab(int index);

  // Creates an extension with the given |name| and |action_type|, adds it to
  // the associated extension service, and returns the created extension. (It's
  // safe to ignore the returned value.)
  scoped_refptr<const extensions::Extension> CreateAndAddExtension(
      const std::string& name,
      extensions::ExtensionBuilder::ActionType action_type);

  // Verifies that the toolbar is in order specified by |expected_names|, has
  // the total action count of |total_size|, and has the same |visible_count|.
  // This verifies that both the ToolbarActionsBar and the associated
  // (platform-specific) view is correct.
  // We use expected names (instead of ids) because they're much more readable
  // in a debug message. These aren't enforced to be unique, so don't make
  // duplicates.
  // If any of these is wrong, returns testing::AssertionFailure() with a
  // message.
  testing::AssertionResult VerifyToolbarOrder(
      const char* expected_names[],
      size_t total_size,
      size_t visible_count) WARN_UNUSED_RESULT;

  ToolbarActionsBar* toolbar_actions_bar() {
    return browser_action_test_util_->GetToolbarActionsBar();
  }
  ToolbarActionsBar* overflow_bar() {
    return overflow_browser_action_test_util_->GetToolbarActionsBar();
  }
  ToolbarActionsModel* toolbar_model() { return toolbar_model_; }
  ExtensionActionTestHelper* browser_action_test_util() {
    return browser_action_test_util_.get();
  }
  ExtensionActionTestHelper* overflow_browser_action_test_util() {
    return overflow_browser_action_test_util_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  // The associated ToolbarActionsModel (owned by the keyed service setup).
  ToolbarActionsModel* toolbar_model_ = nullptr;

  // A ExtensionActionTestHelper object constructed with the associated
  // ToolbarActionsBar.
  std::unique_ptr<ExtensionActionTestHelper> browser_action_test_util_;

  // The overflow container's ExtensionActionTestHelper.
  std::unique_ptr<ExtensionActionTestHelper> overflow_browser_action_test_util_;

  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_;

  // Local state for the browser process.
  std::unique_ptr<ScopedTestingLocalState> local_state_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarUnitTest);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_UNITTEST_H_
