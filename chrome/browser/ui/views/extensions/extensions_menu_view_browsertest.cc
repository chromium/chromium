// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

class ExtensionsMenuViewBrowserTest : public InProcessBrowserTest {
 public:
  ExtensionsMenuViewBrowserTest()
      : allow_extension_menu_instances_(
            ExtensionsMenuView::AllowInstancesForTesting()) {
    scoped_feature_list_.InitWithFeatures(
        {}, {extensions_features::kExtensionsMenuAccessControl,
             extensions_features::kExtensionDisableUnsupportedDeveloper,
             features::kExtensionsPinnedByDefault});
  }

  ExtensionsMenuViewBrowserTest(const ExtensionsMenuViewBrowserTest&) = delete;
  ExtensionsMenuViewBrowserTest& operator=(
      const ExtensionsMenuViewBrowserTest&) = delete;

  ~ExtensionsMenuViewBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Shorten delay on animations so tests run faster.
    views::test::ReduceAnimationDuration(extensions_container());

    ExtensionsMenuView::ShowBubble(
        views::BubbleAnchor(extensions_container()->GetExtensionsButton()),
        browser(), extensions_container()->GetToolbarViewModel(),
        extensions_container());
    if (extensions_menu()) {
      extensions_menu()->set_close_on_deactivate(false);
    }
  }

  void TearDownOnMainThread() override {
    if (ExtensionsMenuView::IsShowing()) {
      ExtensionsMenuView::GetExtensionsMenuViewForTesting()
          ->GetWidget()
          ->CloseNow();
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->GetProfile(); }

  extensions::ExtensionRegistrar* extension_registrar() {
    return extensions::ExtensionRegistrar::Get(profile());
  }

  ExtensionsToolbarDesktop* extensions_container() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->extensions_container();
  }

  scoped_refptr<const extensions::Extension> InstallExtension(
      const std::string& name) {
    auto extension_dir = std::make_unique<extensions::TestExtensionDir>();
    constexpr char kManifestTemplate[] = R"({
      "name": "%s",
      "version": "1",
      "manifest_version": 3
    })";
    extension_dir->WriteManifest(
        base::StringPrintf(kManifestTemplate, name.c_str()));
    extensions::ChromeTestExtensionLoader loader(profile());
    scoped_refptr<const extensions::Extension> extension =
        loader.LoadExtension(extension_dir->UnpackedPath());
    test_extension_dirs_.push_back(std::move(extension_dir));
    LayoutContainerIfNecessary();
    LayoutMenuIfNecessary();
    return extension;
  }

  void DisableExtension(const extensions::ExtensionId& extension_id) {
    extension_registrar()->DisableExtension(
        extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
  }

  void EnableExtension(const extensions::ExtensionId& extension_id) {
    extension_registrar()->EnableExtension(extension_id);
  }

  void ReloadExtension(const extensions::ExtensionId& extension_id) {
    extension_registrar()->ReloadExtension(extension_id);
  }

  void ClickButton(views::Button* button) const {
    ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMousePressed(press_event);
    ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                                 gfx::Point(), ui::EventTimeForNow(),
                                 ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMouseReleased(release_event);
  }

  void ClickPinButton(ExtensionMenuItemView* menu_item) const {
    ClickButton(menu_item->pin_button_for_testing());
  }

  void WaitForAnimation() {
#if BUILDFLAG(IS_MAC)
    // Animation is not reliable in tests on Mac or we avoid using animations.
#else
    views::test::WaitForAnimatingLayoutManager(extensions_container());
#endif
    LayoutContainerIfNecessary();
    LayoutMenuIfNecessary();
  }

  void LayoutContainerIfNecessary() {
    extensions_container()->GetWidget()->LayoutRootViewIfNecessary();
  }

  void LayoutMenuIfNecessary() {
    if (extensions_menu() && extensions_menu()->GetWidget()) {
      extensions_menu()->GetWidget()->LayoutRootViewIfNecessary();
    }
  }

  ExtensionsMenuView* extensions_menu() {
    return ExtensionsMenuView::GetExtensionsMenuViewForTesting();
  }

  // Asserts there is exactly 1 menu item and then returns it.
  ExtensionMenuItemView* GetOnlyMenuItem() {
    base::flat_set<raw_ptr<ExtensionMenuItemView, CtnExperimental>> menu_items =
        extensions_menu()->extensions_menu_items_for_testing();
    if (menu_items.size() != 1u) {
      ADD_FAILURE() << "Not exactly one item; size is: " << menu_items.size();
      return nullptr;
    }
    return *menu_items.begin();
  }

  std::vector<ToolbarActionView*> GetPinnedExtensionViews() {
    std::vector<ToolbarActionView*> result;
    for (views::View* child : extensions_container()->children()) {
      // Ensure we don't downcast the ExtensionsToolbarButton.
      if (views::IsViewClass<ToolbarActionView>(child)) {
        ToolbarActionView* const action =
            static_cast<ToolbarActionView*>(child);
#if BUILDFLAG(IS_MAC)
        const bool is_visible =
            extensions_container()->IsActionVisibleOnToolbar(
                action->view_model()->GetId());
#else
        const bool is_visible = action->GetVisible();
#endif
        if (is_visible) {
          result.push_back(action);
        }
      }
    }
    return result;
  }

  std::vector<std::string> GetPinnedExtensionNames() {
    return base::ToVector(
        GetPinnedExtensionViews(), [](ToolbarActionView* view) {
          return base::UTF16ToUTF8(view->view_model()->GetActionName());
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::AutoReset<bool> allow_extension_menu_instances_;
  std::vector<std::unique_ptr<extensions::TestExtensionDir>>
      test_extension_dirs_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ExtensionsAreShownInTheMenu) {
  // To start, there should be no extensions in the menu.
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());

  // Add an extension, and verify that it's added to the menu.
  constexpr char kExtensionName[] = "Test 1";
  InstallExtension(kExtensionName);

  {
    base::flat_set<raw_ptr<ExtensionMenuItemView, CtnExperimental>> menu_items =
        extensions_menu()->extensions_menu_items_for_testing();
    ASSERT_EQ(1u, menu_items.size());
    EXPECT_EQ(kExtensionName,
              base::UTF16ToUTF8((*menu_items.begin())
                                    ->primary_action_button_for_testing()
                                    ->label_text_for_testing()));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       ExtensionsAreSortedInTheMenu) {
  constexpr char kExtensionZName[] = "Z Extension";
  InstallExtension(kExtensionZName);
  constexpr char kExtensionAName[] = "A Extension";
  InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "b Extension";
  InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  InstallExtension(kExtensionCName);

  std::vector<ExtensionMenuItemView*> menu_items =
      ExtensionsMenuView::GetSortedItemsForSectionForTesting(
          extensions::SitePermissionsHelper::SiteInteraction::kNone);
  ASSERT_EQ(4u, menu_items.size());

  std::vector<std::string> item_names;
  for (auto* menu_item : menu_items) {
    item_names.push_back(
        base::UTF16ToUTF8(menu_item->primary_action_button_for_testing()
                              ->label_text_for_testing()));
  }

  // Basic std::sort would do A,C,Z,b however we want A,b,C,Z
  EXPECT_THAT(item_names,
              testing::ElementsAre(kExtensionAName, kExtensionBName,
                                   kExtensionCName, kExtensionZName));
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       PinnedExtensionAppearsInToolbar) {
  constexpr char kName[] = "Test Name";
  const extensions::ExtensionId extension_id =
      InstallExtension(kName)->id();

  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  ClickPinButton(menu_item);
  WaitForAnimation();

  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName));

  ClickPinButton(menu_item);  // Unpin.
  WaitForAnimation();

  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       PinnedExtensionAppearsInAnotherWindow) {
  const extensions::ExtensionId extension_id =
      InstallExtension("Test Name")->id();
  const auto is_action_visible_on_toolbar = [&extension_id](Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->extensions_container()
        ->IsActionVisibleOnToolbar(extension_id);
  };

  Browser* browser2 = CreateBrowser(profile());
  views::test::ReduceAnimationDuration(
      BrowserView::GetBrowserViewForBrowser(browser2)
          ->toolbar()
          ->extensions_container());

  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ClickPinButton(menu_item);
  WaitForAnimation();
#if !BUILDFLAG(IS_MAC)
  views::test::WaitForAnimatingLayoutManager(
      BrowserView::GetBrowserViewForBrowser(browser2)
          ->toolbar()
          ->extensions_container());
#endif

  // Window that was already open gets the pinned extension.
  EXPECT_TRUE(is_action_visible_on_toolbar(browser2));

  Browser* browser3 = CreateBrowser(profile());
  views::test::ReduceAnimationDuration(
      BrowserView::GetBrowserViewForBrowser(browser3)
          ->toolbar()
          ->extensions_container());
#if !BUILDFLAG(IS_MAC)
  views::test::WaitForAnimatingLayoutManager(
      BrowserView::GetBrowserViewForBrowser(browser3)
          ->toolbar()
          ->extensions_container());
#endif

  // Brand-new window also gets the pinned extension.
  EXPECT_TRUE(is_action_visible_on_toolbar(browser3));
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       PinnedExtensionRemovedWhenDisabled) {
  constexpr char kName[] = "Test Name";
  const extensions::ExtensionId id = InstallExtension(kName)->id();

  {
    ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
    ASSERT_TRUE(menu_item);
    ClickPinButton(menu_item);
  }

  DisableExtension(id);
  WaitForAnimation();

  ASSERT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  EnableExtension(id);
  WaitForAnimation();

  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_PinnedExtensionLayout DISABLED_PinnedExtensionLayout
#else
#define MAYBE_PinnedExtensionLayout PinnedExtensionLayout
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       MAYBE_PinnedExtensionLayout) {
  for (int i = 0; i < 3; i++) {
    InstallExtension(base::StringPrintf("Test %d", i));
  }
  for (ExtensionMenuItemView* menu_item :
       extensions_menu()->extensions_menu_items_for_testing()) {
    ClickPinButton(menu_item);
  }
  WaitForAnimation();

  std::vector<ToolbarActionView*> action_views = GetPinnedExtensionViews();
  ASSERT_EQ(3u, action_views.size());
  ExtensionsToolbarButton* menu_button =
      extensions_container()->GetExtensionsButton();

  // All views should be lined up horizontally with the menu button.
  EXPECT_EQ(action_views[0]->y(), action_views[1]->y());
  EXPECT_EQ(action_views[1]->y(), action_views[2]->y());
  EXPECT_EQ(action_views[2]->y(), menu_button->y());

  // Views are ordered left-to-right (in LTR mode).
  EXPECT_LE(action_views[0]->x() + action_views[0]->width(),
            action_views[1]->x());
  EXPECT_LE(action_views[1]->x() + action_views[1]->width(),
            action_views[2]->x());
  EXPECT_LE(action_views[2]->x() + action_views[2]->width(), menu_button->x());
}

// Tests that when an extension is reloaded it remains visible in the toolbar
// and extensions menu.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, ReloadExtension) {
  // The extension must have a manifest to be reloaded.
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());
  // Force the menu to re-layout, since a new item was added.
  LayoutMenuIfNecessary();
  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());

  {
    ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
    ClickPinButton(menu_item);
    EXPECT_TRUE(
        extensions_container()->IsActionVisibleOnToolbar(extension->id()));
    // |menu_item| will not be valid after the extension reloads.
  }

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  LayoutMenuIfNecessary();

  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the toolbar and
// extensions menu.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, ReloadExtensionFailed) {
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());
  LayoutMenuIfNecessary();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ClickPinButton(menu_item);
  WaitForAnimation();

  // Replace the extension's valid manifest with one containing errors. In this
  // case, the error is that the version key is invalid.
  constexpr char kManifestWithErrors[] = R"({
        "name": "Test",
        "version": 1,
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifestWithErrors);

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()), extension->id());
  // Reload the extension. It should fail due to the manifest errors.
  extension_registrar()->ReloadExtensionWithQuietFailure(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionUnloaded());
  WaitForAnimation();
  LayoutMenuIfNecessary();

  // Since the extension is removed it's no longer visible on the toolbar or in
  // the menu.
  for (views::View* child : extensions_container()->children()) {
    EXPECT_FALSE(views::IsViewClass<ToolbarActionView>(child));
  }
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       PinButtonUserActionWithAccessibility) {
  base::UserActionTester user_action_tester;
  InstallExtension("Test Extension");
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_NE(nullptr, menu_item);
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  constexpr char kPinButtonUserAction[] = "Extensions.Toolbar.PinButtonPressed";

  // Verify behavior before pin, after pin, and after unpin.
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(i, user_action_tester.GetActionCount(kPinButtonUserAction));
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40670141): No Mac animations in unit tests cause errors.
#else
    EXPECT_EQ(i, counter.GetCount(ax::mojom::Event::kAlert));
    EXPECT_EQ(i, counter.GetCount(ax::mojom::Event::kTextChanged));
#endif
    ClickPinButton(menu_item);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, WindowTitle) {
  InstallExtension("Test Extension");

  ExtensionsMenuView* const menu_view = extensions_menu();
  EXPECT_FALSE(menu_view->GetWindowTitle().empty());
  EXPECT_TRUE(menu_view->GetAccessibleWindowTitle().empty());
}
