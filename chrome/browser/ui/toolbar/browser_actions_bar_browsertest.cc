// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace {

scoped_refptr<const extensions::Extension> CreateExtension(
    const std::string& name,
    bool has_browser_action) {
  extensions::ExtensionBuilder builder(name);
  if (has_browser_action)
    builder.SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION);
  return builder.Build();
}

}  // namespace

// BrowserActionsBarBrowserTest:

BrowserActionsBarBrowserTest::BrowserActionsBarBrowserTest()
    : toolbar_model_(nullptr) {}

BrowserActionsBarBrowserTest::~BrowserActionsBarBrowserTest() {
}

void BrowserActionsBarBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Note: The ScopedFeatureList needs to be instantiated before the rest of
  // set up happens.
  // This suite relies on behavior specific to ToolbarActionsBar. See
  // ExtensionsMenuViewBrowserTest and ExtensionsMenuViewUnitTest for new tests.
  feature_list_.InitAndDisableFeature(features::kExtensionsToolbarMenu);

  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  ToolbarActionsBar::disable_animations_for_testing_ = true;
}

void BrowserActionsBarBrowserTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();
  browser_actions_bar_ = ExtensionActionTestHelper::Create(browser());
  toolbar_model_ = ToolbarActionsModel::Get(profile());
}

void BrowserActionsBarBrowserTest::TearDownOnMainThread() {
  ToolbarActionsBar::disable_animations_for_testing_ = false;
  extensions::ExtensionBrowserTest::TearDownOnMainThread();
}

void BrowserActionsBarBrowserTest::LoadExtensions() {
  // Create three extensions with browser actions.
  extension_a_ = CreateExtension("alpha", true);
  extension_b_ = CreateExtension("beta", true);
  extension_c_ = CreateExtension("gamma", true);

  const extensions::Extension* extensions[] =
      { extension_a(), extension_b(), extension_c() };
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  // Add each, and verify that it is both correctly added to the extension
  // registry and to the browser actions container.
  for (size_t i = 0; i < base::size(extensions); ++i) {
    extension_service()->AddExtension(extensions[i]);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(extensions[i]->id())) <<
        extensions[i]->name();
    EXPECT_EQ(static_cast<int>(i + 1),
              browser_actions_bar_->NumberOfBrowserActions());
    EXPECT_TRUE(browser_actions_bar_->HasIcon(i));
    EXPECT_EQ(static_cast<int>(i + 1),
              browser_actions_bar()->VisibleBrowserActions());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest,
                       OverflowedBrowserActionPopupTest) {
  std::unique_ptr<ExtensionActionTestHelper> overflow_bar =
      browser_actions_bar()->CreateOverflowBar(browser());

  // Load up two extensions that have browser action popups.
  base::FilePath data_dir =
      test_data_dir_.AppendASCII("api_test").AppendASCII("browser_action");
  const extensions::Extension* first_extension =
      LoadExtension(data_dir.AppendASCII("open_popup"));
  ASSERT_TRUE(first_extension);
  const extensions::Extension* second_extension =
      LoadExtension(data_dir.AppendASCII("remove_popup"));
  ASSERT_TRUE(second_extension);

  // Verify state: two actions, in the order of [first, second].
  RunScheduledLayouts();
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(first_extension->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_extension->id(), browser_actions_bar()->GetExtensionId(1));

  // Do a little piping to get at the underlying ExtensionActionViewControllers.
  ToolbarActionsBar* main_tab = browser_actions_bar()->GetToolbarActionsBar();
  std::vector<ToolbarActionViewController*> toolbar_actions =
      main_tab->GetActions();
  ASSERT_EQ(2u, toolbar_actions.size());
  EXPECT_EQ(first_extension->id(), toolbar_actions[0]->GetId());
  EXPECT_EQ(second_extension->id(), toolbar_actions[1]->GetId());
  ExtensionActionViewController* first_controller_main =
      static_cast<ExtensionActionViewController*>(toolbar_actions[0]);
  ExtensionActionViewController* second_controller_main =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  ToolbarActionsBar* overflow_tab = overflow_bar->GetToolbarActionsBar();
  toolbar_actions = overflow_tab->GetActions();
  ExtensionActionViewController* second_controller_overflow =
      static_cast<ExtensionActionViewController*>(toolbar_actions[1]);

  toolbar_model()->SetVisibleIconCount(0);
  RunScheduledLayouts();
  overflow_bar->LayoutForOverflowBar();
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, overflow_bar->VisibleBrowserActions());

  // Neither should yet be showing a popup.
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(second_controller_main->IsShowingPopup());
  EXPECT_FALSE(second_controller_overflow->IsShowingPopup());

  // Click on the first extension's browser action. This should open a popup.
  overflow_bar->Press(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(overflow_bar->HasPopup());
  EXPECT_TRUE(second_controller_main->IsShowingPopup());
  EXPECT_FALSE(second_controller_overflow->IsShowingPopup());

  RunScheduledLayouts();
  overflow_bar->LayoutForOverflowBar();
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(1u, main_tab->GetIconCount());
  EXPECT_EQ(second_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(1, overflow_bar->VisibleBrowserActions());
  EXPECT_EQ(2u, overflow_tab->GetIconCount());
  EXPECT_EQ(first_controller_main->GetId(),
            overflow_bar->GetExtensionId(0));

  {
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());
    second_controller_main->HidePopup();
    observer.Wait();
  }

  RunScheduledLayouts();
  overflow_bar->LayoutForOverflowBar();
  EXPECT_FALSE(browser_actions_bar()->HasPopup());
  EXPECT_FALSE(overflow_bar->HasPopup());
  EXPECT_FALSE(second_controller_main->IsShowingPopup());
  EXPECT_FALSE(second_controller_overflow->IsShowingPopup());
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, overflow_bar->VisibleBrowserActions());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(first_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(second_controller_main->GetId(),
            browser_actions_bar()->GetExtensionId(1));
}
