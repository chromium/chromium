// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/any_widget_observer.h"

using ::testing::ElementsAre;

class ExtensionsMenuViewInteractiveUITest : public ExtensionsToolbarUITest {
 public:
  static base::flat_set<raw_ptr<ExtensionMenuItemView, CtnExperimental>>
  GetExtensionMenuItemViews() {
    return ExtensionsMenuView::GetExtensionsMenuViewForTesting()
        ->extensions_menu_items_for_testing();
  }

  void ShowUi(const std::string& name) override {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
      EXPECT_FALSE(container->IsActionVisibleOnToolbar(extensions()[0]->id()));
      EXPECT_FALSE(
          container->GetViewForId(extensions()[0]->id())->GetVisible());

      // Trigger uninstall dialog.
      views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                           "ExtensionUninstallDialog");
      extensions::ExtensionContextMenuModel menu_model(
          extensions()[0].get(), browser(),
          /*is_pinned=*/true, nullptr,
          /*can_show_icon_in_toolbar=*/false,
          extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem);
      menu_model.ExecuteCommand(
          extensions::ExtensionContextMenuModel::UNINSTALL, 0);
      ASSERT_TRUE(waiter.WaitIfNeededAndGet());
    } else if (ui_test_name_ == "InstallDialog") {
      LoadTestExtension("extensions/uitest/long_name");
      LoadTestExtension("extensions/uitest/window_open");

      // Trigger post-install dialog.
      ExtensionInstallUI::ShowBubble(extensions()[0], browser(), SkBitmap());
    } else {
      ClickExtensionsMenuButton();
      ASSERT_TRUE(ExtensionsMenuView::GetExtensionsMenuViewForTesting());
    }

    // Wait for any pending animations to finish so that correct pinned
    // extensions and dialogs are actually showing.
    WaitForAnimation();
  }

  bool VerifyUi() override {
    EXPECT_TRUE(ExtensionsToolbarUITest::VerifyUi());

    if (ui_test_name_ == "ReloadPageBubble") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      // Clicking the extension should close the extensions menu, pop out the
      // extension, and display the "reload this page" bubble.
      EXPECT_TRUE(container->GetAnchoredWidgetForExtensionForTesting(
          extensions()[0]->id()));
      EXPECT_EQ(std::nullopt, container->GetPoppedOutActionId());
      EXPECT_FALSE(ExtensionsMenuView::IsShowing());
    } else if (ui_test_name_ == "UninstallDialog_Accept" ||
               ui_test_name_ == "UninstallDialog_Cancel" ||
               ui_test_name_ == "InstallDialog") {
      ExtensionsToolbarContainer* const container =
          GetExtensionsToolbarContainer();
      EXPECT_TRUE(container->IsActionVisibleOnToolbar(extensions()[0]->id()));
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
      views::DialogDelegate* const install_bubble =
          container->GetViewForId(extensions()[0]->id())
              ->GetProperty(views::kAnchoredDialogKey);
      ASSERT_TRUE(install_bubble);
      install_bubble->GetWidget()->Close();
      return;
    }

    // Use default implementation for other tests.
    ExtensionsToolbarUITest::DismissUi();
  }

  void DismissUninstallDialog() {
    ExtensionsToolbarContainer* const container =
        GetExtensionsToolbarContainer();
    // Accept or cancel the dialog.
    views::DialogDelegate* const uninstall_bubble =
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
      EXPECT_FALSE(container->IsActionVisibleOnToolbar(extensions()[0]->id()));
      EXPECT_FALSE(
          container->GetViewForId(extensions()[0]->id())->GetVisible());
    }
  }

  void TriggerSingleExtensionButton() {
    auto menu_items = GetExtensionMenuItemViews();
    ASSERT_EQ(1u, menu_items.size());
    TriggerExtensionButton((*menu_items.begin())->view_controller()->GetId());
  }

  void TriggerExtensionButton(const std::string& id) {
    auto menu_items = GetExtensionMenuItemViews();
    auto iter =
        base::ranges::find(menu_items, id, [](ExtensionMenuItemView* view) {
          return view->view_controller()->GetId();
        });
    ASSERT_TRUE(iter != menu_items.end());

    ClickButton((*iter)->primary_action_button_for_testing());

    WaitForAnimation();
  }

  void RightClickExtensionInToolbar(ToolbarActionView* extension) {
    ui::MouseEvent click_down_event(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_RIGHT_MOUSE_BUTTON, 0);
    ui::MouseEvent click_up_event(ui::EventType::kMouseReleased, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_RIGHT_MOUSE_BUTTON, 0);
    extension->OnMouseEvent(&click_down_event);
    extension->OnMouseEvent(&click_up_event);
  }

  void ClickExtensionsMenuButton(Browser* browser) {
    ClickButton(BrowserView::GetBrowserViewForBrowser(browser)
                    ->toolbar()
                    ->GetExtensionsButton());
  }

  void ClickExtensionsMenuButton() { ClickExtensionsMenuButton(browser()); }

  std::string ui_test_name_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest, InvokeUi_default) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");

  ShowAndVerifyUi();
}

// Invokes the UI shown when a user has to reload a page in order to run an
// extension.
// TODO(crbug.com/40171640): Very flaky on Linux and Windows.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_ReloadPageBubble DISABLED_InvokeUi_ReloadPageBubble
#else
#define MAYBE_InvokeUi_ReloadPageBubble InvokeUi_ReloadPageBubble
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       MAYBE_InvokeUi_ReloadPageBubble) {
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       ExtensionsMenuButtonHighlight) {
  LoadTestExtension("extensions/uitest/window_open");
  ClickExtensionsMenuButton();
  EXPECT_EQ(views::InkDrop::Get(BrowserView::GetBrowserViewForBrowser(browser())
                                    ->toolbar()
                                    ->GetExtensionsButton())
                ->GetInkDrop()
                ->GetTargetInkDropState(),
            views::InkDropState::ACTIVATED);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest, TriggerPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  TriggerSingleExtensionButton();

  // After triggering an extension with a popup, there should a popped-out
  // action and show the view.
  auto visible_icons = GetVisibleToolbarActionViews();
  EXPECT_NE(std::nullopt, extensions_container->GetPoppedOutActionId());
  EXPECT_EQ(extensions_container->GetPoppedOutActionId(),
            visible_icons[0]->view_controller()->GetId());
  EXPECT_EQ(1u, visible_icons.size());
  extensions_container->HideActivePopup();

  // Wait for animations to finish.
  views::test::WaitForAnimatingLayoutManager(extensions_container);

  // After dismissing the popup there should no longer be a popped-out action
  // and the icon should no longer be visible in the extensions container.
  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       ContextMenuKeepsExtensionPoppedOut) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();

  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());

  TriggerSingleExtensionButton();

  // After triggering an extension with a popup, there should a popped-out
  // action and show the view.
  auto visible_icons = GetVisibleToolbarActionViews();
  EXPECT_NE(std::nullopt, extensions_container->GetPoppedOutActionId());
  EXPECT_EQ(extensions_container->GetPoppedOutActionId(),
            visible_icons[0]->view_controller()->GetId());
  EXPECT_EQ(std::nullopt,
            extensions_container->GetExtensionWithOpenContextMenuForTesting());
  ASSERT_EQ(1u, visible_icons.size());

  RightClickExtensionInToolbar(extensions_container->GetViewForId(
      extensions_container->GetPoppedOutActionId().value()));
  extensions_container->HideActivePopup();

  // Wait for animations to finish.
  views::test::WaitForAnimatingLayoutManager(extensions_container);

  visible_icons = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());
  EXPECT_NE(std::nullopt,
            extensions_container->GetExtensionWithOpenContextMenuForTesting());
  EXPECT_EQ(extensions_container->GetExtensionWithOpenContextMenuForTesting(),
            visible_icons[0]->view_controller()->GetId());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       RemoveExtensionShowingPopup) {
  LoadTestExtension("extensions/simple_with_popup");
  ShowUi("");
  VerifyUi();
  TriggerSingleExtensionButton();

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  std::optional<extensions::ExtensionId> action_id =
      extensions_container->GetPoppedOutActionId();
  ASSERT_NE(std::nullopt, action_id);
  ASSERT_EQ(1u, GetVisibleToolbarActionViews().size());

  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->DisableExtension(action_id.value(),
                         extensions::disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());
  EXPECT_TRUE(GetVisibleToolbarActionViews().empty());
}

// Test for crbug.com/1099456.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       RemoveMultipleExtensionsWhileShowingPopup) {
  auto& id1 = LoadTestExtension("extensions/simple_with_popup")->id();
  auto& id2 = LoadTestExtension("extensions/uitest/window_open")->id();
  ShowUi("");
  VerifyUi();
  TriggerExtensionButton(id1);

  ExtensionsContainer* const extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  ASSERT_NE(std::nullopt, extensions_container->GetPoppedOutActionId());

  auto* extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();

  extension_service->DisableExtension(
      id1, extensions::disable_reason::DISABLE_USER_ACTION);
  extension_service->DisableExtension(
      id2, extensions::disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       TriggeringExtensionClosesMenu) {
  LoadTestExtension("extensions/api_test/trigger_actions/browser_action");
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
  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());

  EXPECT_FALSE(ExtensionsMenuView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       CreatesOneMenuItemPerExtension) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");
  ShowUi("");
  VerifyUi();
  EXPECT_EQ(2u, extensions().size());
  EXPECT_EQ(extensions().size(), GetExtensionMenuItemViews().size());
  DismissUi();
}

// Failing on Mac. https://crbug.com/1176703
// Flaky on Linux. https://crbug.com/1202112
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_PinningDisabledInIncognito DISABLED_PinningDisabledInIncognito
#else
#define MAYBE_PinningDisabledInIncognito PinningDisabledInIncognito
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       MAYBE_PinningDisabledInIncognito) {
  LoadTestExtension("extensions/uitest/window_open", true);
  SetUpIncognitoBrowser();

  // Make sure the pinning item is disabled for context menus in the Incognito
  // browser.
  extensions::ExtensionContextMenuModel menu(
      extensions()[0].get(), incognito_browser(),
      /*is_pinned=*/true, nullptr,
      /* can_show_icon_in_toolbar=*/true,
      extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem);
  EXPECT_FALSE(menu.IsCommandIdEnabled(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY));

  // Show menu and verify that the in-menu pin button is disabled too.
  ClickExtensionsMenuButton(incognito_browser());

  ASSERT_TRUE(VerifyUi());
  ASSERT_EQ(1u, GetExtensionMenuItemViews().size());
  EXPECT_EQ(views::Button::STATE_DISABLED,
            (*GetExtensionMenuItemViews().begin())
                ->pin_button_for_testing()
                ->GetState());

  DismissUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       PinnedExtensionShowsCorrectContextMenuPinOption) {
  LoadTestExtension("extensions/simple_with_popup");

  ClickExtensionsMenuButton();
  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  // Pin extension from menu.
  ASSERT_TRUE(VerifyUi());
  ASSERT_EQ(1u, GetExtensionMenuItemViews().size());
  ui::MouseEvent click_pressed_event(ui::EventType::kMousePressed, gfx::Point(),
                                     gfx::Point(), base::TimeTicks(),
                                     ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent click_released_event(
      ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  ExtensionMenuItemView* const menu_item_view =
      *GetExtensionMenuItemViews().begin();
  menu_item_view->pin_button_for_testing()->OnMousePressed(click_pressed_event);
  menu_item_view->pin_button_for_testing()->OnMouseReleased(
      click_released_event);

  // Wait for any pending animations to finish so that correct pinned
  // extensions and dialogs are actually showing.
  WaitForAnimation();

  // Verify extension is pinned but not stored as the popped out action.
  auto visible_icons = GetVisibleToolbarActionViews();
  visible_icons = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_EQ(std::nullopt, extensions_container->GetPoppedOutActionId());

  // Trigger the pinned extension.
  ToolbarActionView* pinned_extension =
      extensions_container->GetViewForId(extensions()[0]->id());
  pinned_extension->OnMouseEvent(&click_pressed_event);
  pinned_extension->OnMouseEvent(&click_released_event);

  // Wait for any pending animations to finish so that correct pinned
  // extensions and dialogs are actually showing.
  WaitForAnimation();

  EXPECT_NE(std::nullopt, extensions_container->GetPoppedOutActionId());

  // Verify the context menu option, when opened from the toolbar action, is to
  // unpin the extension.
  ui::SimpleMenuModel* context_menu = static_cast<ui::SimpleMenuModel*>(
      extensions_container->GetActionForId(extensions()[0]->id())
          ->GetContextMenu(extensions::ExtensionContextMenuModel::
                               ContextMenuSource::kToolbarAction));
  std::optional<size_t> visibility_index = context_menu->GetIndexOfCommandId(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
  ASSERT_TRUE(visibility_index.has_value());
  std::u16string visibility_label =
      context_menu->GetLabelAt(visibility_index.value());
  EXPECT_EQ(visibility_label, u"Unpin");
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       UnpinnedExtensionShowsCorrectContextMenuPinOption) {
  LoadTestExtension("extensions/simple_with_popup");

  ClickExtensionsMenuButton();
  ExtensionsToolbarContainer* const extensions_container =
      GetExtensionsToolbarContainer();

  TriggerSingleExtensionButton();

  // Wait for any pending animations to finish so that correct pinned
  // extensions and dialogs are actually showing.
  WaitForAnimation();

  // Verify extension is visible and tbere is a popped out action.
  auto visible_icons = GetVisibleToolbarActionViews();
  ASSERT_EQ(1u, visible_icons.size());
  EXPECT_NE(std::nullopt, extensions_container->GetPoppedOutActionId());

  // Verify the context menu option, when opened from the toolbar action, is to
  // unpin the extension.
  ui::SimpleMenuModel* context_menu = static_cast<ui::SimpleMenuModel*>(
      extensions_container->GetActionForId(extensions()[0]->id())
          ->GetContextMenu(extensions::ExtensionContextMenuModel::
                               ContextMenuSource::kToolbarAction));
  std::optional<size_t> visibility_index = context_menu->GetIndexOfCommandId(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
  ASSERT_TRUE(visibility_index.has_value());
  std::u16string visibility_label =
      context_menu->GetLabelAt(visibility_index.value());
  EXPECT_EQ(visibility_label, u"Pin");
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       ManageExtensionsOpensExtensionsPage) {
  // Ensure the menu is visible by adding an extension.
  LoadTestExtension("extensions/api_test/trigger_actions/browser_action");
  ShowUi("");
  VerifyUi();

  EXPECT_TRUE(ExtensionsMenuView::IsShowing());

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  ExtensionsMenuView::GetExtensionsMenuViewForTesting()
      ->manage_extensions_button_for_testing()
      ->button_controller()
      ->OnMouseReleased(click_event);

  // Clicking the Manage Extensions button should open chrome://extensions.
  EXPECT_EQ(
      chrome::kChromeUIExtensionsURL,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

#if BUILDFLAG(IS_LINUX)
// TODO(crbug.com/40792869): Flaky on Linux (CFI)
#define MAYBE_ClickingContextMenuButton DISABLED_ClickingContextMenuButton
#else
#define MAYBE_ClickingContextMenuButton ClickingContextMenuButton
#endif

// Tests that clicking on the context menu button of an extension item opens the
// context menu.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       MAYBE_ClickingContextMenuButton) {
  LoadTestExtension("extensions/uitest/window_open");
  ClickExtensionsMenuButton();

  auto menu_items = GetExtensionMenuItemViews();
  ASSERT_EQ(1u, menu_items.size());
  ExtensionMenuItemView* const item_view = *menu_items.begin();
  EXPECT_FALSE(item_view->IsContextMenuRunningForTesting());

  HoverButton* context_menu_button =
      item_view->context_menu_button_for_testing();
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  context_menu_button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                               gfx::Point(), base::TimeTicks(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  context_menu_button->OnMouseReleased(release_event);

  EXPECT_TRUE(item_view->IsContextMenuRunningForTesting());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       InvokeUi_InstallDialog) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40740852): Flaky on Linux and Lacros.
#define MAYBE_InvokeUi_UninstallDialog_Accept \
  DISABLED_InvokeUi_UninstallDialog_Accept
#else
#define MAYBE_InvokeUi_UninstallDialog_Accept InvokeUi_UninstallDialog_Accept
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       MAYBE_InvokeUi_UninstallDialog_Accept) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40746111): Flaky on Linux.
#define MAYBE_InvokeUi_UninstallDialog_Cancel \
  DISABLED_InvokeUi_UninstallDialog_Cancel
#else
#define MAYBE_InvokeUi_UninstallDialog_Cancel InvokeUi_UninstallDialog_Cancel
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       MAYBE_InvokeUi_UninstallDialog_Cancel) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       InvocationSourceMetrics) {
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

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewInteractiveUITest,
                       MenuGetsUpdatedAfterPermissionsChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallExtensionWithHostPermissions("All Urls Extension", "<all_urls>");
  ASSERT_EQ(1u, extensions().size());

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open the extension menu so we can test the UI when permissions
  // change.
  ClickExtensionsMenuButton();
  auto menu_items = GetExtensionMenuItemViews();
  ASSERT_EQ(1u, menu_items.size());
  auto* item_button =
      (*menu_items.begin())->primary_action_button_for_testing();
  ASSERT_TRUE(item_button);

  // The extension should have access to the site by default.
  EXPECT_EQ(u"All Urls Extension\n" +
                l10n_util::GetStringUTF16(IDS_EXTENSIONS_HAS_ACCESS_TO_SITE),
            item_button->GetTooltipText());

  EXPECT_EQ(base::JoinString(
                {u"All Urls Extension",
                 l10n_util::GetStringUTF16(IDS_EXTENSIONS_HAS_ACCESS_TO_SITE)},
                u"\n"),
            item_button->GetTooltipText());

  std::vector<ExtensionMenuItemView*> active_menu_items =
      ExtensionsMenuView::GetSortedItemsForSectionForTesting(
          extensions::SitePermissionsHelper::SiteInteraction::kGranted);
  ASSERT_EQ(1u, active_menu_items.size());
  EXPECT_EQ(u"All Urls Extension", active_menu_items[0]
                                       ->primary_action_button_for_testing()
                                       ->label_text_for_testing());

  // Change the extension permissions to run on click using the context menu.
  auto* context_menu = static_cast<extensions::ExtensionContextMenuModel*>(
      GetExtensionsToolbarContainer()
          ->GetActionForId(extensions()[0]->id())
          ->GetContextMenu(extensions::ExtensionContextMenuModel::
                               ContextMenuSource::kMenuItem));
  ASSERT_TRUE(context_menu);
  {
    // Since we are revoking permissions, automatically accept the reload page
    // bubble to update the permissions.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    extensions::ExtensionActionRunner::GetForWebContents(web_contents)
        ->accept_bubble_for_testing(true);
    extensions::PermissionsManagerWaiter waiter(
        extensions::PermissionsManager::Get(profile()));
    context_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK,
        /*event_flags=*/0);
    waiter.WaitForExtensionPermissionsUpdate();
  }

  // The extension should not have access to the website.
  EXPECT_EQ(
      base::JoinString(
          {u"All Urls Extension",
           l10n_util::GetStringUTF16(IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE)},
          u"\n"),
      item_button->GetTooltipText());
  std::vector<ExtensionMenuItemView*> pending_menu_items =
      ExtensionsMenuView::GetSortedItemsForSectionForTesting(
          extensions::SitePermissionsHelper::SiteInteraction::kWithheld);
  ASSERT_EQ(1u, pending_menu_items.size());
  EXPECT_EQ(u"All Urls Extension", pending_menu_items[0]
                                       ->primary_action_button_for_testing()
                                       ->label_text_for_testing());

  // Change the extension permissions to run on site using the context menu.
  {
    extensions::PermissionsManagerWaiter waiter(
        extensions::PermissionsManager::Get(profile()));
    context_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
        /*event_flags=*/0);
    waiter.WaitForExtensionPermissionsUpdate();
  }

  // The extension should have access to the site by default.
  EXPECT_EQ(base::JoinString(
                {u"All Urls Extension",
                 l10n_util::GetStringUTF16(IDS_EXTENSIONS_HAS_ACCESS_TO_SITE)},
                u"\n"),
            item_button->GetTooltipText());
  active_menu_items = ExtensionsMenuView::GetSortedItemsForSectionForTesting(
      extensions::SitePermissionsHelper::SiteInteraction::kGranted);
  ASSERT_EQ(1u, active_menu_items.size());
  EXPECT_EQ(u"All Urls Extension", active_menu_items[0]
                                       ->primary_action_button_for_testing()
                                       ->label_text_for_testing());
}

class ActivateWithReloadExtensionsMenuInteractiveUITest
    : public ExtensionsMenuViewInteractiveUITest,
      public ::testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(ActivateWithReloadExtensionsMenuInteractiveUITest,
                       ActivateWithReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadTestExtension("extensions/blocked_actions/content_scripts");
  auto extension = extensions().back();
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ShowUi("");
  VerifyUi();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);

  EXPECT_TRUE(action_runner->WantsToRun(extension.get()));
  extensions::SitePermissionsHelper permissions_helper(browser()->profile());
  // A refresh should be needed in order to run the actions and inject the
  // content script.
  EXPECT_TRUE(permissions_helper.PageNeedsRefreshToRun(
      action_runner->GetBlockedActions(extension->id())));

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
    action_bubble->AcceptDialog();
    EXPECT_TRUE(web_contents->IsLoading());
    // Wait for reload to finish.
    ASSERT_TRUE(content::WaitForLoadStop(web_contents));
    // After reload the extension should run.
    EXPECT_TRUE(DidInjectScript(web_contents));
    EXPECT_FALSE(action_runner->WantsToRun(extension.get()));
  } else {
    action_bubble->CancelDialog();
    EXPECT_FALSE(web_contents->IsLoading());
    // The extension permission should have been applied at this point, but the
    // extension's script and blocked actions should not inject/run since a
    // reload is needed.
    EXPECT_EQ(permissions_helper.GetSiteInteraction(*extension, web_contents),
              extensions::SitePermissionsHelper::SiteInteraction::kGranted);
    EXPECT_FALSE(DidInjectScript(web_contents));
    EXPECT_TRUE(action_runner->WantsToRun(extension.get()));
    // Manual reload should then allow for script inject and blocked actions to
    // run.
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    ASSERT_TRUE(content::WaitForLoadStop(web_contents));
    EXPECT_TRUE(DidInjectScript(web_contents));
    EXPECT_FALSE(action_runner->WantsToRun(extension.get()));
  }
}

INSTANTIATE_TEST_SUITE_P(AcceptDialog,
                         ActivateWithReloadExtensionsMenuInteractiveUITest,
                         testing::Values(true));

INSTANTIATE_TEST_SUITE_P(CancelDialog,
                         ActivateWithReloadExtensionsMenuInteractiveUITest,
                         testing::Values(false));
