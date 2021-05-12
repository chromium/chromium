// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// Manages a Browser instance created by BrowserWithTestWindowTest beyond the
// default instance it creates in SetUp.
class AdditionalBrowser {
 public:
  explicit AdditionalBrowser(std::unique_ptr<Browser> browser)
      : browser_(std::move(browser)),
        browser_view_(BrowserView::GetBrowserViewForBrowser(browser_.get())) {}

  ~AdditionalBrowser() {
    // Tear down |browser_|, similar to TestWithBrowserView::TearDown.
    browser_.release();
    browser_view_->GetWidget()->CloseNow();
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view_->toolbar()->extensions_container();
  }

 private:
  std::unique_ptr<Browser> browser_;
  BrowserView* browser_view_;
};

}  // namespace

class ExtensionsMenuViewUnitTest : public TestWithBrowserView {
 public:
  ExtensionsMenuViewUnitTest()
      : allow_extension_menu_instances_(
            ExtensionsMenuView::AllowInstancesForTesting()) {}
  ~ExtensionsMenuViewUnitTest() override = default;

  // TestWithBrowserView:
  void SetUp() override;

  // Adds a simple extension to the profile.
  scoped_refptr<const extensions::Extension> AddSimpleExtension(
      const std::string& name);

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view()->toolbar()->extensions_container();
  }

  ExtensionsMenuView* extensions_menu() {
    return ExtensionsMenuView::GetExtensionsMenuViewForTesting();
  }

  // Asserts there is exactly 1 menu item and then returns it.
  ExtensionsMenuItemView* GetOnlyMenuItem();

  void ClickPinButton(ExtensionsMenuItemView* menu_item) const;
  void ClickContextMenuButton(ExtensionsMenuItemView* menu_item) const;
  void ClickButton(views::Button* button) const;

  std::vector<ToolbarActionView*> GetPinnedExtensionViews();

  ToolbarActionView* GetPinnedExtensionView(const std::string& name);

  // Returns a list of the names of the currently pinned extensions, in order
  // from left to right.
  std::vector<std::string> GetPinnedExtensionNames();

  // Since this is a unittest (and doesn't have as much "real" rendering),
  // the ExtensionsMenuView sometimes needs a nudge to re-layout the views.
  void LayoutMenuIfNecessary();

  // Waits for the extensions container to animate (on pin, unpin, pop-out,
  // etc.)
  void WaitForAnimation();

 private:
  base::AutoReset<bool> allow_extension_menu_instances_;

  extensions::ExtensionService* extension_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuViewUnitTest);
};

void ExtensionsMenuViewUnitTest::SetUp() {
  TestWithBrowserView::SetUp();

  // Set up some extension-y bits.
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  extension_service_ =
      extensions::ExtensionSystem::Get(profile())->extension_service();

  // Shorten delay on animations so tests run faster.
  views::test::ReduceAnimationDuration(extensions_container());

  ExtensionsMenuView::ShowBubble(extensions_container()->extensions_button(),
                                 browser(), extensions_container(), true);
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewUnitTest::AddSimpleExtension(const std::string& name) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name).Build();
  extension_service()->AddExtension(extension.get());
  // Force the menu to re-layout, since a new item was added.
  LayoutMenuIfNecessary();

  return extension;
}

ExtensionsMenuItemView* ExtensionsMenuViewUnitTest::GetOnlyMenuItem() {
  std::vector<ExtensionsMenuItemView*> menu_items =
      extensions_menu()->extensions_menu_items_for_testing();
  if (menu_items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << menu_items.size();
    return nullptr;
  }
  return menu_items[0];
}

void ExtensionsMenuViewUnitTest::ClickPinButton(
    ExtensionsMenuItemView* menu_item) const {
  ClickButton(menu_item->pin_button_for_testing());
}

void ExtensionsMenuViewUnitTest::ClickContextMenuButton(
    ExtensionsMenuItemView* menu_item) const {
  ClickButton(menu_item->context_menu_button_for_testing());
}

void ExtensionsMenuViewUnitTest::ClickButton(views::Button* button) const {
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(1, 1),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(1, 1),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
}

std::vector<ToolbarActionView*>
ExtensionsMenuViewUnitTest::GetPinnedExtensionViews() {
  std::vector<ToolbarActionView*> result;
  for (views::View* child : extensions_container()->children()) {
    // Ensure we don't downcast the ExtensionsToolbarButton.
    if (views::IsViewClass<ToolbarActionView>(child)) {
      ToolbarActionView* const action = static_cast<ToolbarActionView*>(child);
#if defined(OS_MAC)
      // TODO(crbug.com/1045212): Use IsActionVisibleOnToolbar() because it
      // queries the underlying model and not GetVisible(), as that relies on an
      // animation running, which is not reliable in unit tests on Mac.
      const bool is_visible = extensions_container()->IsActionVisibleOnToolbar(
          action->view_controller());
#else
      const bool is_visible = action->GetVisible();
#endif
      if (is_visible)
        result.push_back(action);
    }
  }
  return result;
}

ToolbarActionView* ExtensionsMenuViewUnitTest::GetPinnedExtensionView(
    const std::string& name) {
  std::vector<ToolbarActionView*> actions = GetPinnedExtensionViews();
  auto it = std::find_if(
      actions.begin(), actions.end(), [name](ToolbarActionView* action) {
        return base::UTF16ToUTF8(action->view_controller()->GetActionName()) ==
               name;
      });
  if (it == actions.end())
    return nullptr;
  return *it;
}

std::vector<std::string> ExtensionsMenuViewUnitTest::GetPinnedExtensionNames() {
  std::vector<ToolbarActionView*> views = GetPinnedExtensionViews();
  std::vector<std::string> result;
  result.resize(views.size());
  std::transform(
      views.begin(), views.end(), result.begin(), [](ToolbarActionView* view) {
        return base::UTF16ToUTF8(view->view_controller()->GetActionName());
      });
  return result;
}

void ExtensionsMenuViewUnitTest::LayoutMenuIfNecessary() {
  extensions_menu()->GetWidget()->LayoutRootViewIfNecessary();
}

void ExtensionsMenuViewUnitTest::WaitForAnimation() {
#if defined(OS_MAC)
  // TODO(crbug.com/1045212): we avoid using animations on Mac due to the lack
  // of support in unit tests. Therefore this is a no-op.
#else
  views::test::WaitForAnimatingLayoutManager(extensions_container());
#endif
}

TEST_F(ExtensionsMenuViewUnitTest, ExtensionsAreShownInTheMenu) {
  // To start, there should be no extensions in the menu.
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());

  // Add an extension, and verify that it's added to the menu.
  constexpr char kExtensionName[] = "Test 1";
  AddSimpleExtension(kExtensionName);

  {
    std::vector<ExtensionsMenuItemView*> menu_items =
        extensions_menu()->extensions_menu_items_for_testing();
    ASSERT_EQ(1u, menu_items.size());
    EXPECT_EQ(kExtensionName,
              base::UTF16ToUTF8(menu_items[0]
                                    ->primary_action_button_for_testing()
                                    ->label_text_for_testing()));
  }
}

TEST_F(ExtensionsMenuViewUnitTest, ExtensionsAreSortedInTheMenu) {
  constexpr char kExtensionZName[] = "Z Extension";
  AddSimpleExtension(kExtensionZName);
  constexpr char kExtensionAName[] = "A Extension";
  AddSimpleExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "b Extension";
  AddSimpleExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  AddSimpleExtension(kExtensionCName);

  std::vector<ExtensionsMenuItemView*> menu_items =
      ExtensionsMenuView::GetSortedItemsForSectionForTesting(
          ToolbarActionViewController::PageInteractionStatus::kNone);
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

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionAppearsInToolbar) {
  constexpr char kName[] = "Test Name";
  AddSimpleExtension(kName);

  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ToolbarActionViewController* controller = menu_item->view_controller();
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  ClickPinButton(menu_item);
  WaitForAnimation();

  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName));

  ClickPinButton(menu_item);  // Unpin.
  WaitForAnimation();

  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(
      menu_item->view_controller()));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionAppearsInAnotherWindow) {
  AddSimpleExtension("Test Name");

  AdditionalBrowser browser2(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ClickPinButton(menu_item);

  // Window that was already open gets the pinned extension.
  browser2.extensions_container()->IsActionVisibleOnToolbar(
      menu_item->view_controller());

  AdditionalBrowser browser3(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  // Brand-new window also gets the pinned extension.
  browser3.extensions_container()->IsActionVisibleOnToolbar(
      menu_item->view_controller());
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionRemovedWhenDisabled) {
  constexpr char kName[] = "Test Name";
  const extensions::ExtensionId id = AddSimpleExtension(kName)->id();

  {
    ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
    ASSERT_TRUE(menu_item);
    ClickPinButton(menu_item);
  }

  extension_service()->DisableExtension(
      id, extensions::disable_reason::DISABLE_USER_ACTION);
  WaitForAnimation();

  ASSERT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  extension_service()->EnableExtension(id);
  WaitForAnimation();

  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre(kName));
}

TEST_F(ExtensionsMenuViewUnitTest, ReorderPinnedExtensions) {
  constexpr char kName1[] = "Test 1";
  AddSimpleExtension(kName1);
  constexpr char kName2[] = "Test 2";
  AddSimpleExtension(kName2);
  constexpr char kName3[] = "Test 3";
  AddSimpleExtension(kName3);

  std::vector<ExtensionsMenuItemView*> menu_items =
      extensions_menu()->extensions_menu_items_for_testing();
  ASSERT_EQ(3u, menu_items.size());
  for (auto* menu_item : menu_items) {
    ClickPinButton(menu_item);
    EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
        menu_item->view_controller()));
  }
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));

  // Simulate dragging "Test 3" to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(kName3);
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(kName1)->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  extensions_container()->OnPerformDrop(drop_event);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName3, kName1, kName2));
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionsReorderOnPrefChange) {
  constexpr char kName1[] = "Test 1";
  const extensions::ExtensionId id1 = AddSimpleExtension(kName1)->id();
  constexpr char kName2[] = "Test 2";
  const extensions::ExtensionId id2 = AddSimpleExtension(kName2)->id();
  constexpr char kName3[] = "Test 3";
  const extensions::ExtensionId id3 = AddSimpleExtension(kName3)->id();
  for (auto* menu_item :
       extensions_menu()->extensions_menu_items_for_testing()) {
    ClickPinButton(menu_item);
  }
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName1, kName2, kName3));

  extensions::ExtensionPrefs::Get(profile())->SetPinnedExtensions(
      {id2, id3, id1});
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kName2, kName3, kName1));
}

TEST_F(ExtensionsMenuViewUnitTest, PinnedExtensionLayout) {
  for (int i = 0; i < 3; i++)
    AddSimpleExtension(base::StringPrintf("Test %d", i));
  for (auto* menu_item :
       extensions_menu()->extensions_menu_items_for_testing()) {
    ClickPinButton(menu_item);
  }
  WaitForAnimation();

  std::vector<ToolbarActionView*> action_views = GetPinnedExtensionViews();
  ASSERT_EQ(3u, action_views.size());
  ExtensionsToolbarButton* menu_button =
      extensions_container()->extensions_button();

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
TEST_F(ExtensionsMenuViewUnitTest, ReloadExtension) {
  // The extension must have a manifest to be reloaded.
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 2
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());
  // Force the menu to re-layout, since a new item was added.
  LayoutMenuIfNecessary();
  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());

  {
    ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
    ClickPinButton(menu_item);
    EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
        menu_item->view_controller()));
    // |menu_item| will not be valid after the extension reloads.
  }

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  extension_service()->ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  LayoutMenuIfNecessary();

  ASSERT_EQ(1u, extensions_menu()->extensions_menu_items_for_testing().size());
  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
      GetOnlyMenuItem()->view_controller()));
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the toolbar and
// extensions menu.
TEST_F(ExtensionsMenuViewUnitTest, ReloadExtensionFailed) {
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 2
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());
  LayoutMenuIfNecessary();
  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ClickPinButton(menu_item);

  // Replace the extension's valid manifest with one containing errors. In this
  // case, the error is that both the 'browser_action' and 'page_action' keys
  // are specified instead of only one.
  constexpr char kManifestWithErrors[] = R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 2,
        "page_action" : {},
        "browser_action" : {}
      })";
  extension_directory.WriteManifest(kManifestWithErrors);

  // Reload the extension. It should fail due to the manifest errors.
  extension_service()->ReloadExtensionWithQuietFailure(extension->id());
  base::RunLoop().RunUntilIdle();

  // Since the extension is removed it's no longer visible on the toolbar or in
  // the menu.
  for (views::View* child : extensions_container()->children())
    EXPECT_FALSE(views::IsViewClass<ToolbarActionView>(child));
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());
}

TEST_F(ExtensionsMenuViewUnitTest, PinButtonUserAction) {
  base::UserActionTester user_action_tester;
  AddSimpleExtension("Test Extension");

  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);

  constexpr char kPinButtonUserAction[] = "Extensions.Toolbar.PinButtonPressed";
  EXPECT_EQ(0, user_action_tester.GetActionCount(kPinButtonUserAction));
  ClickPinButton(menu_item);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kPinButtonUserAction));
  ClickPinButton(menu_item);  // Unpin.
  EXPECT_EQ(2, user_action_tester.GetActionCount(kPinButtonUserAction));
}

TEST_F(ExtensionsMenuViewUnitTest, WindowTitle) {
  AddSimpleExtension("Test Extension");

  ExtensionsMenuView* const menu_view = extensions_menu();
  EXPECT_FALSE(menu_view->GetWindowTitle().empty());
  EXPECT_TRUE(menu_view->GetAccessibleWindowTitle().empty());
}

TEST_F(ExtensionsMenuViewUnitTest, ContextMenuButtonUserAction) {
  base::UserActionTester user_action_tester;
  AddSimpleExtension("Test Extension");

  ExtensionsMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);

  constexpr char kContextMenuButtonUserAction[] =
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu";
  EXPECT_EQ(0, user_action_tester.GetActionCount(kContextMenuButtonUserAction));
  ClickContextMenuButton(menu_item);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kContextMenuButtonUserAction));
}

// TODO(crbug.com/984654): When supported, add a test to verify the
// ExtensionsToolbarContainer shrinks when the window is too small to show all
// pinned extensions.
// TODO(crbug.com/984654): When supported, add a test to verify an extension
// is shown when a bubble pops up and needs to draw attention to it.
