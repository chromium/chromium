// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/extensions/extension_install_ui_default.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/any_widget_observer.h"

using ::testing::ElementsAre;

class ExtensionsMenuViewBrowserTest : public ExtensionsToolbarBrowserTest {
 public:
  enum class ExtensionRemovalMethod {
    kDisable,
    kUninstall,
    kBlocklist,
    kTerminate,
  };

  static std::vector<ExtensionsMenuItemView*> GetExtensionsMenuItemViews() {
    return ExtensionsMenuView::GetExtensionsMenuViewForTesting()
        ->extensions_menu_items_for_testing();
  }

  void ShowUi(const std::string& name) override {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    // The extensions menu can appear offscreen on Linux, so verifying bounds
    // makes the tests flaky.
    set_should_verify_dialog_bounds(false);
#endif
    ui_test_name_ = name;

    if (name == "ReloadPageBubble") {
      ClickExtensionsMenuButton();
      TriggerSingleExtensionButton();
    } else if (ui_test_name_ == "UninstallDialog_Accept" ||
               ui_test_name_ == "UninstallDialog_Cancel") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();

      LoadTestExtension("extensions/uitest/long_name");
      LoadTestExtension("extensions/uitest/window_open");

      // Without the uninstall dialog the icon should now be invisible.
      EXPECT_FALSE(container->IsActionVisibleOnToolbar(
          container->GetActionForId(extensions()[0]->id())));
      EXPECT_FALSE(
          container->GetViewForId(extensions()[0]->id())->GetVisible());

      // Trigger uninstall dialog.
      views::NamedWidgetShownWaiter waiter(
          views::test::AnyWidgetTestPasskey{},
          "ExtensionUninstallDialogDelegateView");
      extensions::ExtensionContextMenuModel menu_model(
          extensions()[0].get(), browser(),
          extensions::ExtensionContextMenuModel::PINNED, nullptr,
          false /* can_show_icon_in_toolbar */);
      menu_model.ExecuteCommand(
          extensions::ExtensionContextMenuModel::UNINSTALL, 0);
      ASSERT_TRUE(waiter.WaitIfNeededAndGet());
    } else if (ui_test_name_ == "InstallDialog") {
      LoadTestExtension("extensions/uitest/long_name");
      LoadTestExtension("extensions/uitest/window_open");

      // Trigger post-install dialog.
      ExtensionInstallUIDefault::ShowPlatformBubble(extensions()[0], browser(),
                                                    SkBitmap());
    } else {
      ClickExtensionsMenuButton();
      ASSERT_TRUE(ExtensionsMenuView::GetExtensionsMenuViewForTesting());
    }

    // Wait for any pending animations to finish so that correct pinned
    // extensions and dialogs are actually showing.
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  }

  bool VerifyUi() override {
    EXPECT_TRUE(ExtensionsToolbarBrowserTest::VerifyUi());

    if (ui_test_name_ == "ReloadPageBubble") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      // Clicking the extension should close the extensions menu, pop out the
      // extension, and display the "reload this page" bubble.
      EXPECT_TRUE(container->GetAnchoredWidgetForExtensionForTesting(
          extensions()[0]->id()));
      EXPECT_FALSE(container->GetPoppedOutAction());
      EXPECT_FALSE(ExtensionsMenuView::IsShowing());
    } else if (ui_test_name_ == "UninstallDialog_Accept" ||
               ui_test_name_ == "UninstallDialog_Cancel" ||
               ui_test_name_ == "InstallDialog") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      EXPECT_TRUE(container->IsActionVisibleOnToolbar(
          container->GetActionForId(extensions()[0]->id())));
      EXPECT_TRUE(container->GetViewForId(extensions()[0]->id())->GetVisible());
    }

    return true;
  }

  void DismissUi() override {
    if (ui_test_name_ == "UninstallDialog_Accept" ||
        ui_test_name_ == "UninstallDialog_Cancel") {
      DismissUninstallDialog();
      return;
    }

    if (ui_test_name_ == "InstallDialog") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      views::BubbleDialogDelegate* const install_bubble =
          container->GetViewForId(extensions()[0]->id())
              ->GetProperty(views::kAnchoredDialogKey);
      ASSERT_TRUE(install_bubble);
      install_bubble->GetWidget()->Close();
      return;
    }

    // Use default implementation for other tests.
    ExtensionsToolbarBrowserTest::DismissUi();
  }

  void DismissUninstallDialog() {
    ExtensionsToolbarContainer* const container =
        GetExtensionsToolbarContainer();
    // Accept or cancel the dialog.
    views::BubbleDialogDelegate* const uninstall_bubble =
        container->GetViewForId(extensions()[0]->id())
            ->GetProperty(views::kAnchoredDialogKey);
    ASSERT_TRUE(uninstall_bubble);
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        uninstall_bubble->GetWidget());
    if (ui_test_name_ == "UninstallDialog_Accept") {
      uninstall_bubble->AcceptDialog();
    } else {
      uninstall_bubble->CancelDialog();
    }
    destroyed_waiter.Wait();

    if (ui_test_name_ == "UninstallDialog_Accept") {
      // Accepting the dialog should remove the item from the container and the
      // ExtensionRegistry.
      EXPECT_EQ(nullptr, container->GetActionForId(extensions()[0]->id()));
      EXPECT_EQ(nullptr, extensions::ExtensionRegistry::Get(profile())
                             ->GetInstalledExtension(extensions()[0]->id()));
    } else {
      // After dismissal the icon should become invisible.
      // Wait for animations to finish.
      views::test::WaitForAnimatingLayoutManager(
          GetExtensionsToolbarContainer());

      // The extension should still be present in the ExtensionRegistry (not
      // uninstalled) when the uninstall dialog is dismissed.
      EXPECT_NE(nullptr, extensions::ExtensionRegistry::Get(profile())
                             ->GetInstalledExtension(extensions()[0]->id()));
      // Without the uninstall dialog present the icon should now be
      // invisible.
      EXPECT_FALSE(container->IsActionVisibleOnToolbar(
          container->GetActionForId(extensions()[0]->id())));
      EXPECT_FALSE(
          container->GetViewForId(extensions()[0]->id())->GetVisible());
    }
  }

  void TriggerSingleExtensionButton() {
    ASSERT_EQ(1u, GetExtensionsMenuItemViews().size());
    TriggerExtensionButton(0u);
  }

  void TriggerExtensionButton(size_t item_index) {
    auto menu_items = GetExtensionsMenuItemViews();
    ASSERT_LT(item_index, menu_items.size());

    ui::MouseEvent click_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                               gfx::Point(), base::TimeTicks(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
    menu_items[item_index]
        ->primary_action_button_for_testing()
        ->button_controller()
        ->OnMouseReleased(click_event);

    // Wait for animations to finish.
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  }

  void RightClickExtensionInToolbar(ToolbarActionView* extension) {
    ui::MouseEvent click_down_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_RIGHT_MOUSE_BUTTON, 0);
    ui::MouseEvent click_up_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_RIGHT_MOUSE_BUTTON, 0);
    extension->OnMouseEvent(&click_down_event);
    extension->OnMouseEvent(&click_up_event);
  }

  void ClickExtensionsMenuButton(Browser* browser) {
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->GetExtensionsButton()
        ->OnMousePressed(click_event);
  }

  void ClickExtensionsMenuButton() { ClickExtensionsMenuButton(browser()); }

  void RemoveExtension(ExtensionRemovalMethod method,
                       const std::string& extension_id) {
    extensions::ExtensionService* const extension_service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    switch (method) {
      case ExtensionRemovalMethod::kDisable:
        extension_service->DisableExtension(
            extension_id, extensions::disable_reason::DISABLE_USER_ACTION);
        break;
      case ExtensionRemovalMethod::kUninstall:
        extension_service->UninstallExtension(
            extension_id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
        break;
      case ExtensionRemovalMethod::kBlocklist:
        extension_service->BlocklistExtensionForTest(extension_id);
        break;
      case ExtensionRemovalMethod::kTerminate:
        extension_service->TerminateExtension(extension_id);
        break;
    }

    // Removing an extension can result in the container changing visibility.
    // Allow it to finish laying out appropriately.
    auto* container = GetExtensionsToolbarContainer();
    container->GetWidget()->LayoutRootViewIfNecessary();
  }

  void VerifyContainerVisibility(ExtensionRemovalMethod method,
                                 bool expected_visibility) {
    // An empty container should not be shown.
    EXPECT_FALSE(GetExtensionsToolbarContainer()->GetVisible());

    // Loading the first extension should show the button (and container).
    LoadTestExtension("extensions/uitest/long_name");
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());

    // Add another extension so we can make sure that removing some don't change
    // the visibility.
    LoadTestExtension("extensions/uitest/window_open");

    // Remove 1/2 extensions, should still be drawn.
    RemoveExtension(method, extensions()[0]->id());
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());

    // Removing the last extension. All actions now have the same state.
    RemoveExtension(method, extensions()[1]->id());
    EXPECT_EQ(expected_visibility, GetExtensionsToolbarContainer()->IsDrawn());
  }

  std::string ui_test_name_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_default) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvisibleWithoutExtension_Disable) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kDisable, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvisibleWithoutExtension_Uninstall) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kUninstall, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvisibleWithoutExtension_Blocklist) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kBlocklist, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvisibleWithoutExtension_Terminate) {
  // TODO(pbos): Keep the container visible when extensions are terminated
  // (crash). This lets users find and restart them. Then update this test
  // expectation to be kept visible by terminated extensions. Also update the
  // test name to reflect that the container should be visible with only
  // terminated extensions.
  VerifyContainerVisibility(ExtensionRemovalMethod::kTerminate, false);
}

// Invokes the UI shown when a user has to reload a page in order to run an
// extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvokeUi_ReloadPageBubble) {
  ASSERT_TRUE(embedded_test_server()->Start());
  extensions::TestExtensionDir test_dir;
  // Load an extension that injects scripts at "document_start", which requires
  // reloading the page to inject if permissions are withheld.
  test_dir.WriteManifest(
      R"({
           "name": "Runs Script Everywhere",
           "description": "An extension that runs script everywhere",
           "manifest_version": 2,
           "version": "0.1",
           "content_scripts": [{
             "matches": ["*://*/*"],
             "js": ["script.js"],
             "run_at": "document_start"
           }]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                     "console.log('injected!');");

  AppendExtension(
      extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
          test_dir.UnpackedPath()));
  ASSERT_EQ(1u, extensions().size());
  ASSERT_TRUE(extensions().front());

  extensions::ScriptingPermissionsModifier(profile(), extensions().front())
      .SetWithholdHostPermissions(true);

  // Navigate to a page the extension wants to run on.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  {
    content::TestNavigationObserver observer(tab);
    GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ExtensionsMenuButtonHighlight) {
  LoadTestExtension("extensions/uitest/window_open");
  ClickExtensionsMenuButton();
  EXPECT_EQ(BrowserView::GetBrowserViewForBrowser(browser())
                ->toolbar()
                ->GetExtensionsButton()
                ->GetInkDrop()
                ->GetTargetInkDropState(),
            views::InkDropState::ACTIVATED);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, TriggerPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  TriggerSingleExtensionButton();

  // After triggering an extension with a popup, there should a popped-out
  // action and show the view.
  auto visible_icons = GetVisibleToolbarActionViews();
  EXPECT_NE(nullptr, extensions_container->GetPoppedOutAction());
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(extensions_container->GetPoppedOutAction(),
            visible_icons[0]->view_controller());

  extensions_container->HideActivePopup();

  // Wait for animations to finish.
  views::test::WaitForAnimatingLayoutManager(extensions_container);

  // After dismissing the popup there should no longer be a popped-out action
  // and the icon should no longer be visible in the extensions container.
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ContextMenuKeepsExtensionPoppedOut) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  TriggerSingleExtensionButton();

  // After triggering an extension with a popup, there should a popped-out
  // action and show the view.
  auto visible_icons = GetVisibleToolbarActionViews();
  EXPECT_NE(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_EQ(base::nullopt,
            extensions_container->GetExtensionWithOpenContextMenuForTesting());
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(extensions_container->GetPoppedOutAction(),
            visible_icons[0]->view_controller());

  RightClickExtensionInToolbar(extensions_container->GetViewForId(
      extensions_container->GetPoppedOutAction()->GetId()));
  extensions_container->HideActivePopup();

  // Wait for animations to finish.
  views::test::WaitForAnimatingLayoutManager(extensions_container);

  visible_icons = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_NE(base::nullopt,
            extensions_container->GetExtensionWithOpenContextMenuForTesting());
  EXPECT_EQ(extensions_container->GetExtensionWithOpenContextMenuForTesting(),
            visible_icons[0]->view_controller()->GetId());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       RemoveExtensionShowingPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();
  TriggerSingleExtensionButton();

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  ToolbarActionViewController* action =
      extensions_container->GetPoppedOutAction();
  ASSERT_NE(nullptr, action);
  ASSERT_EQ(1u, GetVisibleToolbarActionViews().size());

  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->DisableExtension(action->GetId(),
                         extensions::disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());
}

// Test for crbug.com/1099456.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       RemoveMultipleExtensionsWhileShowingPopup) {
  auto& id1 = LoadTestExtension("extensions/simple_with_popup")->id();
  auto& id2 = LoadTestExtension("extensions/uitest/window_open")->id();
  ShowUi("");
  VerifyUi();
  TriggerExtensionButton(0u);

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  ASSERT_NE(nullptr, extensions_container->GetPoppedOutAction());

  auto* extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();

  extension_service->DisableExtension(
      id1, extensions::disable_reason::DISABLE_USER_ACTION);
  extension_service->DisableExtension(
      id2, extensions::disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       TriggeringExtensionClosesMenu) {
  LoadTestExtension("extensions/trigger_actions/browser_action");
  ShowUi("");
  VerifyUi();

  EXPECT_TRUE(ExtensionsMenuView::IsShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      ExtensionsMenuView::GetExtensionsMenuViewForTesting()->GetWidget());
  TriggerSingleExtensionButton();

  destroyed_waiter.Wait();

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();

  // This test should not use a popped-out action, as we want to make sure that
  // the menu closes on its own and not because a popup dialog replaces it.
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());

  EXPECT_FALSE(ExtensionsMenuView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       CreatesOneMenuItemPerExtension) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");
  ShowUi("");
  VerifyUi();
  EXPECT_EQ(2u, extensions().size());
  EXPECT_EQ(extensions().size(), GetExtensionsMenuItemViews().size());
  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       PinningDisabledInIncognito) {
  LoadTestExtension("extensions/uitest/window_open", true);
  SetUpIncognitoBrowser();

  // Make sure the pinning item is disabled for context menus in the Incognito
  // browser.
  extensions::ExtensionContextMenuModel menu(
      extensions()[0].get(), incognito_browser(),
      extensions::ExtensionContextMenuModel::PINNED, nullptr,
      true /* can_show_icon_in_toolbar */);
  EXPECT_FALSE(menu.IsCommandIdEnabled(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY));

  // Show menu and verify that the in-menu pin button is disabled too.
  ClickExtensionsMenuButton(incognito_browser());

  ASSERT_TRUE(VerifyUi());
  ASSERT_EQ(1u, GetExtensionsMenuItemViews().size());
  EXPECT_EQ(views::Button::STATE_DISABLED, GetExtensionsMenuItemViews()
                                               .front()
                                               ->pin_button_for_testing()
                                               ->GetState());

  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       PinnedExtensionShowsCorrectContextMenuPinOption) {
  LoadTestExtension("extensions/simple_with_popup");

  ClickExtensionsMenuButton();
  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  // Pin extension from menu.
  ASSERT_TRUE(VerifyUi());
  ASSERT_EQ(1u, GetExtensionsMenuItemViews().size());
  ui::MouseEvent click_pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                     gfx::Point(), base::TimeTicks(),
                                     ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent click_released_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                      gfx::Point(), base::TimeTicks(),
                                      ui::EF_LEFT_MOUSE_BUTTON, 0);
  GetExtensionsMenuItemViews()
      .front()
      ->pin_button_for_testing()
      ->OnMousePressed(click_pressed_event);
  GetExtensionsMenuItemViews()
      .front()
      ->pin_button_for_testing()
      ->OnMouseReleased(click_released_event);

  // Wait for any pending animations to finish so that correct pinned
  // extensions and dialogs are actually showing.
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());

  // Verify extension is pinned but not stored as the popped out action.
  auto visible_icons = GetVisibleToolbarActionViews();
  visible_icons = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(nullptr, extensions_container->GetPoppedOutAction());

  // Trigger the pinned extension.
  ToolbarActionView* pinned_extension =
      extensions_container->GetViewForId(extensions()[0]->id());
  pinned_extension->OnMouseEvent(&click_pressed_event);
  pinned_extension->OnMouseEvent(&click_released_event);

  // Wait for any pending animations to finish so that correct pinned
  // extensions and dialogs are actually showing.
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());

  EXPECT_NE(nullptr, extensions_container->GetPoppedOutAction());

  // Verify the context menu option is to unpin the extension.
  ui::SimpleMenuModel* context_menu = static_cast<ui::SimpleMenuModel*>(
      extensions_container->GetActionForId(extensions()[0]->id())
          ->GetContextMenu());
  int visibility_index = context_menu->GetIndexOfCommandId(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
  ASSERT_GE(visibility_index, 0);
  base::string16 visibility_label = context_menu->GetLabelAt(visibility_index);
  EXPECT_EQ(base::UTF16ToUTF8(visibility_label), "Unpin");
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       UnpinnedExtensionShowsCorrectContextMenuPinOption) {
  LoadTestExtension("extensions/simple_with_popup");

  ClickExtensionsMenuButton();
  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  TriggerSingleExtensionButton();

  // Wait for any pending animations to finish so that correct pinned
  // extensions and dialogs are actually showing.
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());

  // Verify extension is visible and tbere is a popped out action.
  auto visible_icons = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_NE(nullptr, extensions_container->GetPoppedOutAction());

  // Verify the context menu option is to unpin the extension.
  ui::SimpleMenuModel* context_menu = static_cast<ui::SimpleMenuModel*>(
      extensions_container->GetActionForId(extensions()[0]->id())
          ->GetContextMenu());
  int visibility_index = context_menu->GetIndexOfCommandId(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
  ASSERT_GE(visibility_index, 0);
  base::string16 visibility_label = context_menu->GetLabelAt(visibility_index);
  EXPECT_EQ(base::UTF16ToUTF8(visibility_label), "Pin");
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ManageExtensionsOpensExtensionsPage) {
  // Ensure the menu is visible by adding an extension.
  LoadTestExtension("extensions/trigger_actions/browser_action");
  ShowUi("");
  VerifyUi();

  EXPECT_TRUE(ExtensionsMenuView::IsShowing());

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  ExtensionsMenuView::GetExtensionsMenuViewForTesting()
      ->manage_extensions_button_for_testing()
      ->button_controller()
      ->OnMouseReleased(click_event);

  // Clicking the Manage Extensions button should open chrome://extensions.
  EXPECT_EQ(
      chrome::kChromeUIExtensionsURL,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

// Tests that clicking on the context menu button of an extension item opens the
// context menu.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ClickingContextMenuButton) {
  LoadTestExtension("extensions/uitest/window_open");
  ClickExtensionsMenuButton();

  auto menu_items = GetExtensionsMenuItemViews();
  ASSERT_EQ(1u, menu_items.size());
  ExtensionsMenuItemView* item_view = menu_items[0];
  EXPECT_FALSE(item_view->IsContextMenuRunning());

  views::ImageButton* context_menu_button =
      menu_items[0]->context_menu_button_for_testing();
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  context_menu_button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                               gfx::Point(), base::TimeTicks(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  context_menu_button->OnMouseReleased(release_event);

  EXPECT_TRUE(item_view->IsContextMenuRunning());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_InstallDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvokeUi_UninstallDialog_Accept) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       InvokeUi_UninstallDialog_Cancel) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvocationSourceMetrics) {
  base::HistogramTester histogram_tester;
  LoadTestExtension("extensions/uitest/extension_with_action_and_command");
  ClickExtensionsMenuButton();

  constexpr char kHistogramName[] = "Extensions.Toolbar.InvocationSource";
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  TriggerSingleExtensionButton();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName, ToolbarActionViewController::InvocationSource::kMenuEntry,
      1);

  // TODO(devlin): Add a test for command invocation once
  // https://crbug.com/1070305 is fixed.
}

namespace {
constexpr char kExtensionAId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
constexpr char kExtensionBId[] = "mockepjebcnmhmhcahfddgfcdgkdifnc";
constexpr char kExtensionCId[] = "dpfmafkdlbmopmcepgpjkpldjbghdibm";

bool TestShouldEnableToolbarMenuExperiment() {
  std::string test_name =
      testing::UnitTest::GetInstance()->current_test_info()->name();
  // This PRE_PRE_ step sets up pre-migration extension prefs. The experiment
  // triggers migration so it needs to be off during pre-condition setup.
  return test_name.find(
             "PRE_PRE_PostExtensionMigrationChangesPersistAfterRestart") ==
         std::string::npos;
}

}  // namespace

class ExtensionsToolbarMigrationBrowserTest
    : public ExtensionsToolbarBrowserTest {
 protected:
  ExtensionsToolbarMigrationBrowserTest()
      : ExtensionsToolbarBrowserTest(TestShouldEnableToolbarMenuExperiment()) {}

  void ShowUi(const std::string& name) override {
    // Intentionally empty, this tests UI in the toolbar.
  }

 private:
  extensions::ScopedInstallVerifierBypassForTest ignore_install_verification_;
};

// Add and verify extensions with extensions toolbar menu feature turned off.
// TODO(corising): Remove this series of tests and the |enable_flag| parameter
// from initialization of ExtensionsToolbarBrowserTest once the extensions
// toolbar menu experiment has been launched for a couple milestones.
IN_PROC_BROWSER_TEST_F(
    ExtensionsToolbarMigrationBrowserTest,
    PRE_PRE_PostExtensionMigrationChangesPersistAfterRestart) {
  // Add three extensions.
  LoadTestExtension("extensions/good.crx");
  LoadTestExtension("extensions/trivial_extension/extension.crx");
  LoadTestExtension("extensions/page_action.crx");

  // Verify all extensions have been added.
  EXPECT_EQ(3u, extensions().size());
  EXPECT_EQ(extensions()[0]->id(), kExtensionAId);
  EXPECT_EQ(extensions()[1]->id(), kExtensionBId);
  EXPECT_EQ(extensions()[2]->id(), kExtensionCId);

  BrowserActionsContainer* browser_actions =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->browser_actions();

  // Hide the last extension and verify that only the first two of the three are
  // visible.
  ToolbarActionsModel::Get(profile())->SetActionVisibility(kExtensionCId,
                                                           false);
  EXPECT_TRUE(browser_actions->GetViewForId(kExtensionAId)->GetVisible());
  EXPECT_TRUE(browser_actions->GetViewForId(kExtensionBId)->GetVisible());
  EXPECT_FALSE(browser_actions->GetViewForId(kExtensionCId)->GetVisible());
}

// Test visible extensions migrate to pinned extensions after Chrome restart and
// that any further changes are reflected in the extension prefs.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarMigrationBrowserTest,
                       PRE_PostExtensionMigrationChangesPersistAfterRestart) {
  // Wait for any animations to finish.
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());

  auto* toolbar_model = ToolbarActionsModel::Get(profile());

  // Verify that the extensions that were visible are now the pinned extensions
  // in the extension prefs.
  extensions::ExtensionPrefs* const extension_prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_THAT(extension_prefs->GetPinnedExtensions(),
              testing::ElementsAre(kExtensionAId, kExtensionBId));
  // Verify that the extensions that were visible are now the pinned extensions
  // in the toolbar model.
  EXPECT_TRUE(toolbar_model->IsActionPinned(kExtensionAId));
  EXPECT_TRUE(toolbar_model->IsActionPinned(kExtensionBId));
  EXPECT_FALSE(toolbar_model->IsActionPinned(kExtensionCId));
  // Verify that the extensions that were visible are now visible in the toolbar
  // container.
  ExtensionsToolbarContainer* extensions_container =
      GetExtensionsToolbarContainer();
  EXPECT_TRUE(extensions_container->GetViewForId(kExtensionAId)->GetVisible());
  EXPECT_TRUE(extensions_container->GetViewForId(kExtensionBId)->GetVisible());
  EXPECT_FALSE(extensions_container->GetViewForId(kExtensionCId)->GetVisible());

  // Verify that pinning/unpinning action is reflected in preferences.
  toolbar_model->SetActionVisibility(kExtensionAId, false);
  EXPECT_THAT(extension_prefs->GetPinnedExtensions(),
              testing::ElementsAre(kExtensionBId));
  toolbar_model->SetActionVisibility(kExtensionCId, true);
  EXPECT_THAT(extension_prefs->GetPinnedExtensions(),
              testing::ElementsAre(kExtensionBId, kExtensionCId));

  // Verify that moving an action is reflected in preferences.
  toolbar_model->MovePinnedAction(kExtensionCId, 0);
  EXPECT_THAT(extension_prefs->GetPinnedExtensions(),
              testing::ElementsAre(kExtensionCId, kExtensionBId));
}

// Test that any post-migration extension changes are persisent after Chrome
// restarts.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarMigrationBrowserTest,
                       PostExtensionMigrationChangesPersistAfterRestart) {
  // Wait for any animations to finish.
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());

  extensions::ExtensionPrefs* const extension_prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_THAT(extension_prefs->GetPinnedExtensions(),
              testing::ElementsAre(kExtensionCId, kExtensionBId));
  // Verify that these extensions are also pinned extensions in the toolbar
  // model.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  EXPECT_FALSE(toolbar_model->IsActionPinned(kExtensionAId));
  EXPECT_TRUE(toolbar_model->IsActionPinned(kExtensionBId));
  EXPECT_TRUE(toolbar_model->IsActionPinned(kExtensionCId));
  // Verify that these extensions are visible in the toolbar container.
  ExtensionsToolbarContainer* extensions_container =
      GetExtensionsToolbarContainer();
  EXPECT_FALSE(extensions_container->GetViewForId(kExtensionAId)->GetVisible());
  EXPECT_TRUE(extensions_container->GetViewForId(kExtensionBId)->GetVisible());
  EXPECT_TRUE(extensions_container->GetViewForId(kExtensionCId)->GetVisible());
}

class ActivateWithReloadExtensionsMenuBrowserTest
    : public ExtensionsMenuViewBrowserTest,
      public ::testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(ActivateWithReloadExtensionsMenuBrowserTest,
                       ActivateWithReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadTestExtension("extensions/blocked_actions/content_scripts");
  auto extension = extensions().back();
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html"));

  ShowUi("");
  VerifyUi();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);

  EXPECT_TRUE(action_runner->WantsToRun(extension.get()));

  TriggerSingleExtensionButton();

  auto* const action_bubble =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container()
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

INSTANTIATE_TEST_SUITE_P(AcceptDialog,
                         ActivateWithReloadExtensionsMenuBrowserTest,
                         testing::Values(true));

INSTANTIATE_TEST_SUITE_P(CancelDialog,
                         ActivateWithReloadExtensionsMenuBrowserTest,
                         testing::Values(false));
