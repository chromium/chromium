// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"

class ExtensionsTabbedMenuViewInteractiveUITest
    : public ExtensionsToolbarUITest {
 public:
  ExtensionsTabbedMenuViewInteractiveUITest();
  ~ExtensionsTabbedMenuViewInteractiveUITest() override = default;
  ExtensionsTabbedMenuViewInteractiveUITest(
      const ExtensionsTabbedMenuViewInteractiveUITest&) = delete;
  const ExtensionsTabbedMenuViewInteractiveUITest& operator=(
      const ExtensionsTabbedMenuViewInteractiveUITest&) = delete;

  std::vector<InstalledExtensionMenuItemView*> installed_items() {
    return extensions_tabbed_menu_view()->GetInstalledItemsForTesting();
  }

  std::vector<SiteAccessMenuItemView*> requests_access_items() {
    return extensions_tabbed_menu_view()
        ->GetVisibleRequestsAccessItemsForTesting();
  }

  std::vector<SiteAccessMenuItemView*> has_access_items() {
    return extensions_tabbed_menu_view()->GetVisibleHasAccessItemsForTesting();
  }

  // This should only be called after the menu is visible.
  ExtensionsTabbedMenuView* extensions_tabbed_menu_view() {
    ExtensionsTabbedMenuView* menu =
        GetExtensionsToolbarContainer()
            ->GetExtensionsTabbedMenuCoordinatorForTesting()
            ->GetExtensionsTabbedMenuView();
    EXPECT_TRUE(menu);
    return menu;
  }

  // Asserts there is exactly one installed menu item and then returns it.
  InstalledExtensionMenuItemView* GetOnlyInstalledMenuItem();

  // Opens the tabbed menu in the installed tab.
  void ShowInstalledTabInMenu();
  // Opens the tabbed menu in the site access tab.
  void ShowSiteAccessTabInMenu();

  void ClickPrimaryButton(InstalledExtensionMenuItemView* item);
  void ClickPinButton(InstalledExtensionMenuItemView* installed_item);
  void RightClickExtensionInToolbar(ToolbarActionView* extension);

  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ExtensionsTabbedMenuViewInteractiveUITest::ShowUi(
    const std::string& name) {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // The extensions menu can appear offscreen on Linux, so verifying bounds
  // makes the tests flaky.
  set_should_verify_dialog_bounds(false);
#endif

  ShowInstalledTabInMenu();
  ASSERT_TRUE(extensions_tabbed_menu_view());
}

bool ExtensionsTabbedMenuViewInteractiveUITest::VerifyUi() {
  return ExtensionsToolbarUITest::VerifyUi();
}

ExtensionsTabbedMenuViewInteractiveUITest::
    ExtensionsTabbedMenuViewInteractiveUITest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

InstalledExtensionMenuItemView*
ExtensionsTabbedMenuViewInteractiveUITest::GetOnlyInstalledMenuItem() {
  std::vector<InstalledExtensionMenuItemView*> items = installed_items();
  if (items.size() != 1) {
    ADD_FAILURE() << "Not exactly one item; size is: " << items.size();
    return nullptr;
  }
  return *items.begin();
}

void ExtensionsTabbedMenuViewInteractiveUITest::ShowInstalledTabInMenu() {
  ClickButton(GetExtensionsToolbarContainer()->GetExtensionsButton());
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewInteractiveUITest::ShowSiteAccessTabInMenu() {
  ClickButton(GetExtensionsToolbarContainer()
                  ->GetExtensionsToolbarControls()
                  ->site_access_button_for_testing());
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewInteractiveUITest::RightClickExtensionInToolbar(
    ToolbarActionView* extension) {
  ui::MouseEvent click_down_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_RIGHT_MOUSE_BUTTON, 0);
  ui::MouseEvent click_up_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                gfx::Point(), base::TimeTicks(),
                                ui::EF_RIGHT_MOUSE_BUTTON, 0);
  extension->OnMouseEvent(&click_down_event);
  extension->OnMouseEvent(&click_up_event);
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewInteractiveUITest::ClickPrimaryButton(
    InstalledExtensionMenuItemView* item) {
  ClickButton(item->primary_action_button_for_testing());
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewInteractiveUITest::ClickPinButton(
    InstalledExtensionMenuItemView* installed_item) {
  ClickButton(installed_item->pin_button_for_testing());
  WaitForAnimation();
}

IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       InvokeUi_default) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       InvocationSourceMetrics) {
  base::HistogramTester histogram_tester;
  LoadTestExtension("extensions/uitest/extension_with_action_and_command");
  ShowInstalledTabInMenu();

  constexpr char kHistogramName[] = "Extensions.Toolbar.InvocationSource";
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  ClickPrimaryButton(GetOnlyInstalledMenuItem());
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName, ToolbarActionViewController::InvocationSource::kMenuEntry,
      1);

  // TODO(crbug.com/1070305): Add a test for command invocation.
}

IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       Toolbar_ContextMenuKeepsActionPoppedOut) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  EXPECT_EQ(extensions_container->GetPoppedOutAction(), nullptr);
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  ClickPrimaryButton(GetOnlyInstalledMenuItem());

  // Verify a popped-out action appears after triggering the extension primary
  // button.
  auto visible_actions = GetVisibleToolbarActionViews();
  EXPECT_NE(extensions_container->GetPoppedOutAction(), nullptr);
  EXPECT_EQ(extensions_container->GetExtensionWithOpenContextMenuForTesting(),
            absl::nullopt);
  ASSERT_EQ(visible_actions.size(), 1u);
  EXPECT_EQ(visible_actions[0]->view_controller(),
            extensions_container->GetPoppedOutAction());

  RightClickExtensionInToolbar(extensions_container->GetViewForId(
      extensions_container->GetPoppedOutAction()->GetId()));
  extensions_container->HideActivePopup();

  // Verify the popped-out action is still open after opening the context menu.
  visible_actions = GetVisibleToolbarActionViews();
  ASSERT_EQ(visible_actions.size(), 1u);
  EXPECT_EQ(extensions_container->GetPoppedOutAction(), nullptr);
  EXPECT_NE(extensions_container->GetExtensionWithOpenContextMenuForTesting(),
            absl::nullopt);
  EXPECT_EQ(visible_actions[0]->view_controller()->GetId(),
            extensions_container->GetExtensionWithOpenContextMenuForTesting());
}

IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       Toolbar_RemoveExtensionShowingPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  // Trigger popup via the extensions menu.
  ClickPrimaryButton(GetOnlyInstalledMenuItem());

  ExtensionsContainer* const extensions_container =
      GetExtensionsToolbarContainer();
  ToolbarActionViewController* action =
      extensions_container->GetPoppedOutAction();
  ASSERT_NE(action, nullptr);
  ASSERT_EQ(GetVisibleToolbarActionViews().size(), 1u);

  DisableExtension(action->GetId());

  EXPECT_EQ(extensions_container->GetPoppedOutAction(), nullptr);
  EXPECT_EQ(GetVisibleToolbarActionViews().size(), 0u);
}

// Test for crbug.com/1099456.
IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       Toolbar_RemoveMultipleExtensionsWhileShowingPopup) {
  const auto& id1 = LoadTestExtension("extensions/simple_with_popup")->id();
  const auto& id2 = LoadTestExtension("extensions/uitest/window_open")->id();
  ShowUi("");
  VerifyUi();

  // Trigger popup of one extension via the extensions menu.
  ASSERT_EQ(installed_items().size(), 2u);
  ClickPrimaryButton(installed_items()[0]);

  ExtensionsContainer* const extensions_container =
      GetExtensionsToolbarContainer();
  ASSERT_NE(extensions_container->GetPoppedOutAction(), nullptr);

  DisableExtension(id1);
  DisableExtension(id2);

  EXPECT_EQ(extensions_container->GetPoppedOutAction(), nullptr);
}

IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       InstalledTab_TriggerPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  EXPECT_EQ(extensions_container->GetPoppedOutAction(), nullptr);
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  ClickPrimaryButton(GetOnlyInstalledMenuItem());

  // Verify a popped-out action appears after triggering the extension primary
  // button.
  auto visible_actions = GetVisibleToolbarActionViews();
  EXPECT_NE(extensions_container->GetPoppedOutAction(), nullptr);
  ASSERT_EQ(visible_actions.size(), 1u);
  EXPECT_EQ(visible_actions[0]->view_controller(),
            extensions_container->GetPoppedOutAction());

  extensions_container->HideActivePopup();

  WaitForAnimation();

  // After dismissing the popup there should no longer be a popped-out action
  // and the icon should no longer be visible in the extensions container.
  EXPECT_EQ(extensions_container->GetPoppedOutAction(), nullptr);
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());
}

// TODO(emiliapaz): Add a tests that check triggering extension closes the menu.

IN_PROC_BROWSER_TEST_F(ExtensionsTabbedMenuViewInteractiveUITest,
                       InstalledTab_CreatesOneInstalledMenuItemPerExtension) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");
  ShowUi("");
  VerifyUi();
  EXPECT_EQ(installed_items().size(), 2u);
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(
    ExtensionsTabbedMenuViewInteractiveUITest,
    InstalledTab_PinnedExtensionShowsCorrectContextMenuPinOption) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowInstalledTabInMenu();

  ASSERT_TRUE(VerifyUi());
  ASSERT_EQ(installed_items().size(), 1u);

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  // Pin extension from menu.
  ClickPinButton(GetOnlyInstalledMenuItem());

  // Verify extension is pinned but not stored as the popped out action.
  auto visible_actions = GetVisibleToolbarActionViews();
  ASSERT_EQ(visible_actions.size(), 1u);
  EXPECT_EQ(extensions_container->GetPoppedOutAction(), nullptr);

  // Verify the context menu option is to unpin the extension.
  auto* context_menu = static_cast<ui::SimpleMenuModel*>(
      visible_actions[0]->view_controller()->GetContextMenu(
          extensions::ExtensionContextMenuModel::ContextMenuSource::
              kToolbarAction));
  absl::optional<size_t> visibility_index = context_menu->GetIndexOfCommandId(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
  ASSERT_TRUE(visibility_index.has_value());
  std::u16string visibility_label =
      context_menu->GetLabelAt(visibility_index.value());
  EXPECT_EQ(visibility_label, u"Unpin");

  // Trigger the pinned extension.
  ToolbarActionView* pinned_extension =
      extensions_container->GetViewForId(extensions()[0]->id());
  ClickButton(pinned_extension);
  WaitForAnimation();

  // Verify extension has a popped out action.
  EXPECT_NE(extensions_container->GetPoppedOutAction(), nullptr);
}

IN_PROC_BROWSER_TEST_F(
    ExtensionsTabbedMenuViewInteractiveUITest,
    InstalledTab_UnpinnedExtensionShowsCorrectContextMenuPinOption) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowInstalledTabInMenu();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  ClickPrimaryButton(GetOnlyInstalledMenuItem());

  // Verify extension is visible and there is a popped out action.
  auto visible_actions = GetVisibleToolbarActionViews();
  ASSERT_EQ(visible_actions.size(), 1u);
  EXPECT_NE(extensions_container->GetPoppedOutAction(), nullptr);

  // Verify the context menu option is to unpin the extension.
  ui::SimpleMenuModel* context_menu = static_cast<ui::SimpleMenuModel*>(
      visible_actions[0]->view_controller()->GetContextMenu(
          extensions::ExtensionContextMenuModel::ContextMenuSource::
              kToolbarAction));
  absl::optional<size_t> visibility_index = context_menu->GetIndexOfCommandId(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
  ASSERT_TRUE(visibility_index.has_value());
  std::u16string visibility_label =
      context_menu->GetLabelAt(visibility_index.value());
  EXPECT_EQ(visibility_label, u"Pin");
}

// TODO(crbug.com/1296893): This test is flaky.
IN_PROC_BROWSER_TEST_F(
    ExtensionsTabbedMenuViewInteractiveUITest,
    DISABLED_SiteAccessTab_ExtensionInCorrectSectionAfterContextMenuChangesPermissions) {
  ASSERT_TRUE(embedded_test_server()->Start());

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
           "name": "All Urls Extension",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["<all_urls>"]
         })");
  AppendExtension(
      extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
          test_dir.UnpackedPath()));
  ASSERT_EQ(1u, extensions().size());

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Verify site access button is visible, and trigger it to open site access
  // tab in the extension menu so we can test the UI when permissions change.
  auto* site_access_button = GetExtensionsToolbarContainer()
                                 ->GetExtensionsToolbarControls()
                                 ->site_access_button_for_testing();
  EXPECT_TRUE(site_access_button);
  ShowSiteAccessTabInMenu();

  // Extension with <all_urls> permission has site access by default (except for
  // forbidden websites such as chrome:-scheme), and it should be in the has
  // access section.
  ASSERT_EQ(has_access_items().size(), 1u);
  ASSERT_EQ(requests_access_items().size(), 0u);
  EXPECT_EQ((*has_access_items().begin())
                ->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"All Urls Extension");

  // Allow the site to access the extension only on click by changing the
  // extension permissions to run on click using the context menu.
  auto* context_menu = static_cast<extensions::ExtensionContextMenuModel*>(
      GetExtensionsToolbarContainer()
          ->GetActionForId(extensions()[0]->id())
          ->GetContextMenu(extensions::ExtensionContextMenuModel::
                               ContextMenuSource::kMenuItem));
  ASSERT_TRUE(context_menu);
  {
    extensions::PermissionsManagerWaiter waiter(
        extensions::PermissionsManager::Get(profile()));
    context_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK,
        /*event_flags=*/0);
    waiter.WaitForExtensionPermissionsUpdate();
  }

  // Verify the extension is in the requests access section.
  ASSERT_EQ(has_access_items().size(), 0u);
  ASSERT_EQ(requests_access_items().size(), 1u);
  EXPECT_EQ((*requests_access_items().begin())
                ->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"All Urls Extension");

  // Allow the site to access the extension by changing the extension
  // permissions to run on site using the context menu.
  {
    extensions::PermissionsManagerWaiter waiter(
        extensions::PermissionsManager::Get(profile()));
    context_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
        /*event_flags=*/0);
    waiter.WaitForExtensionPermissionsUpdate();
  }

  // Verify the extension is in the has access section.
  ASSERT_EQ(has_access_items().size(), 1u);
  ASSERT_EQ(requests_access_items().size(), 0u);
  EXPECT_EQ((*has_access_items().begin())
                ->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"All Urls Extension");
}

class ActivateWithReloadExtensionsTabbedMenuInteractiveUITest
    : public ExtensionsTabbedMenuViewInteractiveUITest,
      public ::testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(ActivateWithReloadExtensionsTabbedMenuInteractiveUITest,
                       ActivateWithReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadTestExtension("extensions/blocked_actions/content_scripts");
  auto extension = extensions().back();
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html")));

  ShowUi("");
  VerifyUi();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);

  EXPECT_TRUE(action_runner->WantsToRun(extension.get()));

  ClickPrimaryButton(GetOnlyInstalledMenuItem());

  auto* const action_bubble =
      GetExtensionsToolbarContainer()
          ->GetAnchoredWidgetForExtensionForTesting(extensions()[0]->id())
          ->widget_delegate()
          ->AsDialogDelegate();
  ASSERT_TRUE(action_bubble);

  const bool accept_reload_dialog = GetParam();
  if (accept_reload_dialog) {
    content::TestNavigationObserver observer(web_contents);
    action_bubble->AcceptDialog();
    EXPECT_TRUE(web_contents->IsLoading());
    // Wait for reload to finish.
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    // After reload the extension should be allowed to run.
    EXPECT_FALSE(action_runner->WantsToRun(extension.get()));
  } else {
    action_bubble->CancelDialog();
    EXPECT_FALSE(web_contents->IsLoading());
    EXPECT_TRUE(action_runner->WantsToRun(extension.get()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AcceptDialog,
    ActivateWithReloadExtensionsTabbedMenuInteractiveUITest,
    testing::Values(true));

INSTANTIATE_TEST_SUITE_P(
    CancelDialog,
    ActivateWithReloadExtensionsTabbedMenuInteractiveUITest,
    testing::Values(false));
