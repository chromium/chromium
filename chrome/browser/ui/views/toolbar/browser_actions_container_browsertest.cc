// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"

#include <stddef.h>

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

// TODO(devlin): Continue moving any tests that should be platform independent
// from this file to the crossplatform tests in
// chrome/browser/ui/toolbar/browser_actions_bar_browsertest.cc.

// Test that dragging browser actions works, and that dragging a browser action
// from the overflow menu results in it "popping" out (growing the container
// size by 1), rather than just reordering the extensions.

IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest, DragBrowserActions) {
  LoadExtensions();

  // Sanity check: All extensions showing; order is A B C.
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(2));

  BrowserActionsContainer* container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()->browser_actions();

  // The order of the child views should be the same.
  const auto& children = container->children();
  EXPECT_EQ(container->GetViewForId(extension_a()->id()), children[0]);
  EXPECT_EQ(container->GetViewForId(extension_b()->id()), children[1]);
  EXPECT_EQ(container->GetViewForId(extension_c()->id()), children[2]);

  // Simulate a drag and drop to the right.
  ui::OSExchangeData drop_data;
  // Drag extension A from index 0...
  BrowserActionDragData browser_action_drag_data(extension_a()->id(), 0u);
  browser_action_drag_data.Write(profile(), &drop_data);
  ToolbarActionView* view = container->GetViewForId(extension_b()->id());
  // ...to the right of extension B.
  gfx::PointF location(view->x() + view->width(), view->y());
  ui::DropTargetEvent target_event(
      drop_data, location, location, ui::DragDropTypes::DRAG_MOVE);

  // Drag and drop.
  container->OnDragUpdated(target_event);
  container->OnPerformDrop(target_event);

  // The order should now be B A C, since A was dragged to the right of B.
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(2));
  EXPECT_EQ(container->GetViewForId(extension_b()->id()), children[0]);
  EXPECT_EQ(container->GetViewForId(extension_a()->id()), children[1]);
  EXPECT_EQ(container->GetViewForId(extension_c()->id()), children[2]);

  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(profile())->enabled_extensions();
  const std::vector<ToolbarActionsModel::ActionId>& toolbar_action_ids =
      toolbar_model()->action_ids();

  // This order should be reflected in the underlying model.
  EXPECT_EQ(extension_b(), extension_set.GetByID(toolbar_action_ids[0]));
  EXPECT_EQ(extension_a(), extension_set.GetByID(toolbar_action_ids[1]));
  EXPECT_EQ(extension_c(), extension_set.GetByID(toolbar_action_ids[2]));

  // Simulate a drag and drop to the left.
  ui::OSExchangeData drop_data2;
  // Drag extension A from index 1...
  BrowserActionDragData browser_action_drag_data2(extension_a()->id(), 1u);
  browser_action_drag_data2.Write(profile(), &drop_data2);
  // ...to the left of extension B (which is now at index 0).
  location = gfx::PointF(view->x(), view->y());
  ui::DropTargetEvent target_event2(
      drop_data2, location, location, ui::DragDropTypes::DRAG_MOVE);

  // Drag and drop.
  container->OnDragUpdated(target_event2);
  container->OnPerformDrop(target_event2);

  // Order should be restored to A B C.
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(2));
  EXPECT_EQ(container->GetViewForId(extension_a()->id()), children[0]);
  EXPECT_EQ(container->GetViewForId(extension_b()->id()), children[1]);
  EXPECT_EQ(container->GetViewForId(extension_c()->id()), children[2]);

  // Shrink the size of the container so we have an overflow menu.
  toolbar_model()->SetVisibleIconCount(2u);
  EXPECT_EQ(2u, container->VisibleBrowserActions());

  // Simulate a drag and drop from the overflow menu.
  ui::OSExchangeData drop_data3;
  // Drag extension C from index 2 (in the overflow menu)...
  BrowserActionDragData browser_action_drag_data3(extension_c()->id(), 2u);
  browser_action_drag_data3.Write(profile(), &drop_data3);
  // ...to the left of extension B (which is back in index 1 on the main bar).
  location = gfx::PointF(view->x(), view->y());
  ui::DropTargetEvent target_event3(
      drop_data3, location, location, ui::DragDropTypes::DRAG_MOVE);

  // Drag and drop.
  container->OnDragUpdated(target_event3);
  container->OnPerformDrop(target_event3);

  // The order should have changed *and* the container should have grown to
  // accommodate extension C. The new order should be A C B, and all three
  // extensions should be visible, with no overflow menu.
  EXPECT_EQ(extension_a()->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_c()->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_b()->id(), browser_actions_bar()->GetExtensionId(2));
  EXPECT_EQ(3u, container->VisibleBrowserActions());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
}

// Test that changes performed in one container affect containers in other
// windows so that it is consistent.
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest, MultipleWindows) {
  LoadExtensions();
  BrowserActionsContainer* first =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar()->
          browser_actions();

  // Create a second browser.
  Browser* second_browser = new Browser(Browser::CreateParams(profile(), true));
  BrowserActionsContainer* second =
      BrowserView::GetBrowserViewForBrowser(second_browser)->toolbar()->
          browser_actions();

  // Both containers should have the same order and visible actions, which
  // is right now A B C.
  EXPECT_EQ(3u, first->VisibleBrowserActions());
  EXPECT_EQ(3u, second->VisibleBrowserActions());
  EXPECT_EQ(extension_a()->id(), first->GetIdAt(0u));
  EXPECT_EQ(extension_a()->id(), second->GetIdAt(0u));
  EXPECT_EQ(extension_b()->id(), first->GetIdAt(1u));
  EXPECT_EQ(extension_b()->id(), second->GetIdAt(1u));
  EXPECT_EQ(extension_c()->id(), first->GetIdAt(2u));
  EXPECT_EQ(extension_c()->id(), second->GetIdAt(2u));

  // Simulate a drag and drop to the right.
  ui::OSExchangeData drop_data;
  // Drag extension A from index 0...
  BrowserActionDragData browser_action_drag_data(extension_a()->id(), 0u);
  browser_action_drag_data.Write(profile(), &drop_data);
  ToolbarActionView* view = first->GetViewForId(extension_b()->id());
  // ...to the right of extension B.
  gfx::PointF location(view->x() + view->width(), view->y());
  ui::DropTargetEvent target_event(
      drop_data, location, location, ui::DragDropTypes::DRAG_MOVE);

  // Drag and drop.
  first->toolbar_actions_bar()->OnDragStarted(0u);
  first->OnDragUpdated(target_event);

  // Semi-random placement for a regression test for crbug.com/539744.
  first->Layout();
  first->OnPerformDrop(target_event);

  // The new order, B A C, should be reflected in *both* containers, even
  // though the drag only happened in the first one.
  EXPECT_EQ(extension_b()->id(), first->GetIdAt(0u));
  EXPECT_EQ(extension_b()->id(), second->GetIdAt(0u));
  EXPECT_EQ(extension_a()->id(), first->GetIdAt(1u));
  EXPECT_EQ(extension_a()->id(), second->GetIdAt(1u));
  EXPECT_EQ(extension_c()->id(), first->GetIdAt(2u));
  EXPECT_EQ(extension_c()->id(), second->GetIdAt(2u));

  // Next, simulate a resize by shrinking the container.
  first->OnResize(1, true);
  // The first and second container should each have resized.
  EXPECT_EQ(2u, first->VisibleBrowserActions());
  EXPECT_EQ(2u, second->VisibleBrowserActions());
}

// Test that the BrowserActionsContainer responds correctly when the underlying
// model enters highlight mode, and that browser actions are undraggable in
// highlight mode. (Highlight mode itself it tested more thoroughly in the
// ToolbarActionsModel browsertests).
IN_PROC_BROWSER_TEST_F(BrowserActionsBarBrowserTest, HighlightMode) {
  LoadExtensions();

  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->CanBeResized());

  std::vector<std::string> action_ids;
  action_ids.push_back(extension_a()->id());
  action_ids.push_back(extension_b()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);

  // Only two browser actions should be visible.
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());

  // We shouldn't be able to drag in highlight mode.
  EXPECT_FALSE(browser_actions_bar()->CanBeResized());

  // We should go back to normal after leaving highlight mode.
  toolbar_model()->StopHighlighting();
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->CanBeResized());
}

namespace {

// Wraps an existing browser actions container (BAC) delegate and forwards all
// calls to it, except for reporting the max width available to the BAC, which
// it intercepts. Injected into a live BAC for testing.
class ForwardingDelegate : public BrowserActionsContainer::Delegate {
 public:
  explicit ForwardingDelegate(BrowserActionsContainer::Delegate* forward_to);
  ~ForwardingDelegate() override = default;

  BrowserActionsContainer::Delegate* forward_to() { return forward_to_; }
  void set_max_browser_actions_width(
      const base::Optional<int>& max_browser_actions_width) {
    max_browser_actions_width_ = max_browser_actions_width;
  }

 protected:
  // BrowserActionsContainer::Delegate:
  views::LabelButton* GetOverflowReferenceView() override;
  std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
      ToolbarActionsBarDelegate* delegate,
      Browser* browser,
      ToolbarActionsBar* main_bar) const override;
  base::Optional<int> GetMaxBrowserActionsWidth() const override;

 private:
  BrowserActionsContainer::Delegate* const forward_to_;
  base::Optional<int> max_browser_actions_width_;
};

ForwardingDelegate::ForwardingDelegate(
    BrowserActionsContainer::Delegate* forward_to)
    : forward_to_(forward_to),
      max_browser_actions_width_(forward_to->GetMaxBrowserActionsWidth()) {}

views::LabelButton* ForwardingDelegate::GetOverflowReferenceView() {
  return forward_to_->GetOverflowReferenceView();
}

std::unique_ptr<ToolbarActionsBar> ForwardingDelegate::CreateToolbarActionsBar(
    ToolbarActionsBarDelegate* delegate,
    Browser* browser,
    ToolbarActionsBar* main_bar) const {
  return forward_to_->CreateToolbarActionsBar(delegate, browser, main_bar);
}

base::Optional<int> ForwardingDelegate::GetMaxBrowserActionsWidth() const {
  return max_browser_actions_width_;
}

}  // namespace

// Contains (mostly regression) tests that rely on direct access to the browser
// action container's internal members. This test fixture is a friend of
// BrowserActionContainer.
class BrowserActionsContainerBrowserTest : public BrowserActionsBarBrowserTest {
 public:
  BrowserActionsContainerBrowserTest() = default;
  BrowserActionsContainerBrowserTest(
      const BrowserActionsContainerBrowserTest&) = delete;
  BrowserActionsContainerBrowserTest& operator=(
      const BrowserActionsContainerBrowserTest&) = delete;
  ~BrowserActionsContainerBrowserTest() override = default;

  ForwardingDelegate* test_delegate() { return test_delegate_.get(); }

  views::ResizeArea* GetResizeArea();
  void UpdateResizeArea();
  int GetMinimumSize();
  int GetMaximumSize();

 protected:
  // BrowserActionsBarBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  BrowserActionsContainer* GetContainer();

  std::unique_ptr<ForwardingDelegate> test_delegate_;
};

views::ResizeArea* BrowserActionsContainerBrowserTest::GetResizeArea() {
  return GetContainer()->resize_area_;
}

void BrowserActionsContainerBrowserTest::UpdateResizeArea() {
  return GetContainer()->UpdateResizeArea();
}

int BrowserActionsContainerBrowserTest::GetMinimumSize() {
  return GetContainer()->GetWidthForIconCount(1);
}

int BrowserActionsContainerBrowserTest::GetMaximumSize() {
  return GetContainer()->GetWidthWithAllActionsVisible();
}

void BrowserActionsContainerBrowserTest::SetUpOnMainThread() {
  BrowserActionsBarBrowserTest::SetUpOnMainThread();
  BrowserActionsContainer* const container = GetContainer();
  // Create and inject a test delegate. We need to do const-fu because in the
  // production code the delegate is (rightly) a const pointer.
  test_delegate_ = std::make_unique<ForwardingDelegate>(container->delegate_);
  *const_cast<BrowserActionsContainer::Delegate**>(&container->delegate_) =
      test_delegate_.get();
  LoadExtensions();
}

void BrowserActionsContainerBrowserTest::TearDownOnMainThread() {
  if (test_delegate_) {
    BrowserActionsContainer* const container = GetContainer();
    // De-inject the test delegate. We need to do const-fu because in the
    // production code the delegate is (rightly) a const pointer.
    *const_cast<BrowserActionsContainer::Delegate**>(&container->delegate_) =
        test_delegate_->forward_to();
    test_delegate_.reset();
  }
  BrowserActionsBarBrowserTest::TearDownOnMainThread();
}

BrowserActionsContainer* BrowserActionsContainerBrowserTest::GetContainer() {
  return BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar()
      ->browser_actions();
}

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerBrowserTest,
                       CanResize_InHighlightMode) {
  views::ResizeArea* const resize_area = GetResizeArea();
  UpdateResizeArea();

  // Resize area should be enabled by default.
  EXPECT_TRUE(resize_area->GetEnabled());

  std::vector<std::string> action_ids;
  action_ids.push_back(extension_a()->id());
  action_ids.push_back(extension_b()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);

  UpdateResizeArea();

  // Resize area is disabled in highlight mode.
  EXPECT_FALSE(resize_area->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerBrowserTest,
                       CanResize_AtMinimumWidth) {
  views::ResizeArea* const resize_area = GetResizeArea();
  UpdateResizeArea();

  // Resize area should be enabled by default.
  EXPECT_TRUE(resize_area->GetEnabled());

  // Resize area should be enabled when there is enough space for one icon.
  const int required_space = GetMinimumSize();
  test_delegate()->set_max_browser_actions_width(required_space);
  UpdateResizeArea();
  EXPECT_TRUE(resize_area->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerBrowserTest,
                       CanResize_AboveMaximumWidth) {
  views::ResizeArea* const resize_area = GetResizeArea();
  UpdateResizeArea();

  // Resize area should be enabled by default.
  EXPECT_TRUE(resize_area->GetEnabled());

  // Resize area should be enabled when there is more than the maximum space
  // requested.
  const int max_space = GetMaximumSize();
  test_delegate()->set_max_browser_actions_width(max_space + 1);
  UpdateResizeArea();
  EXPECT_TRUE(resize_area->GetEnabled());

  // Resize area should remain enabled when the space shrinks to the minimum
  // required.
  const int required_space = GetMinimumSize();
  test_delegate()->set_max_browser_actions_width(required_space);
  UpdateResizeArea();
  EXPECT_TRUE(resize_area->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerBrowserTest,
                       CannotResize_AtZeroWidth) {
  views::ResizeArea* const resize_area = GetResizeArea();
  UpdateResizeArea();

  // Resize area should be enabled by default.
  EXPECT_TRUE(resize_area->GetEnabled());

  // Resize area should be disabled when there is zero space available.
  test_delegate()->set_max_browser_actions_width(0);
  UpdateResizeArea();
  EXPECT_FALSE(resize_area->GetEnabled());

  // Resize area should be re-enabled when there is enough space.
  const int required_space = GetMinimumSize();
  test_delegate()->set_max_browser_actions_width(required_space);
  UpdateResizeArea();
  EXPECT_TRUE(resize_area->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerBrowserTest,
                       CannotResize_BelowMinimumWidth) {
  views::ResizeArea* const resize_area = GetResizeArea();
  UpdateResizeArea();

  // Resize area should be enabled by default.
  EXPECT_TRUE(resize_area->GetEnabled());

  // Resize area should be disabled when there is less than the minimum space
  // for one icon.
  const int required_space = GetMinimumSize();
  test_delegate()->set_max_browser_actions_width(required_space - 1);
  UpdateResizeArea();
  EXPECT_FALSE(resize_area->GetEnabled());

  // Resize area should be re-enabled when there is enough space.
  test_delegate()->set_max_browser_actions_width(required_space);
  UpdateResizeArea();
  EXPECT_TRUE(resize_area->GetEnabled());
}

// Test the behavior of the overflow container for Extension Actions.
class BrowserActionsContainerOverflowTest
    : public BrowserActionsBarBrowserTest {
 public:
  BrowserActionsContainerOverflowTest() : main_bar_(nullptr),
                                          overflow_bar_(nullptr) {
  }
  BrowserActionsContainerOverflowTest(
      const BrowserActionsContainerOverflowTest&) = delete;
  BrowserActionsContainerOverflowTest& operator=(
      const BrowserActionsContainerOverflowTest&) = delete;
  ~BrowserActionsContainerOverflowTest() override = default;

 protected:
  // Returns true if the order of the ToolbarActionViews in |main_bar_|
  // and |overflow_bar_| match.
  bool ViewOrdersMatch();

  // Returns Success if the visible count matches |expected_visible|. This means
  // that the number of visible browser actions in |main_bar_| is
  // |expected_visible| and shows the first icons, and that the overflow bar
  // shows all (and only) the remainder.
  testing::AssertionResult VerifyVisibleCount(size_t expected_visible)
      WARN_UNUSED_RESULT;

  // Accessors.
  BrowserActionsContainer* main_bar() { return main_bar_; }
  BrowserActionsContainer* overflow_bar() { return overflow_bar_; }

 private:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // The main BrowserActionsContainer (owned by the browser view).
  BrowserActionsContainer* main_bar_;

  // A parent view for the overflow menu.
  std::unique_ptr<views::View> overflow_parent_;

  // The overflow BrowserActionsContainer. We manufacture this so that we don't
  // have to open the app menu.
  // Owned by the |overflow_parent_|.
  BrowserActionsContainer* overflow_bar_;
};

void BrowserActionsContainerOverflowTest::SetUpOnMainThread() {
  BrowserActionsBarBrowserTest::SetUpOnMainThread();
  main_bar_ = BrowserView::GetBrowserViewForBrowser(browser())
                  ->toolbar()->browser_actions();
  overflow_parent_ = std::make_unique<views::ResizeAwareParentView>();
  overflow_parent_->set_owned_by_client();
  overflow_bar_ = new BrowserActionsContainer(
      browser(), main_bar_,
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar(), true);
  overflow_parent_->AddChildView(overflow_bar_);
}

void BrowserActionsContainerOverflowTest::TearDownOnMainThread() {
  overflow_parent_.reset();
  BrowserActionsBarBrowserTest::TearDownOnMainThread();
}

bool BrowserActionsContainerOverflowTest::ViewOrdersMatch() {
  if (main_bar_->num_toolbar_actions() !=
      overflow_bar_->num_toolbar_actions())
    return false;
  for (size_t i = 0; i < main_bar_->num_toolbar_actions(); ++i) {
    if (main_bar_->GetIdAt(i) != overflow_bar_->GetIdAt(i))
      return false;
  }
  return true;
}

testing::AssertionResult
BrowserActionsContainerOverflowTest::VerifyVisibleCount(
    size_t expected_visible) {
  // Views order should always match (as it is based directly off the model).
  if (!ViewOrdersMatch())
    return testing::AssertionFailure() << "View orders don't match";

  // Loop through and check each browser action for proper visibility (which
  // implicitly also guarantees that the proper number are visible).
  for (size_t i = 0; i < overflow_bar_->num_toolbar_actions(); ++i) {
    bool visible = i < expected_visible;
    if (main_bar_->GetToolbarActionViewAt(i)->GetVisible() != visible) {
      return testing::AssertionFailure() << "Index " << i <<
          " has improper visibility in main: " << !visible;
    }
    if (overflow_bar_->GetToolbarActionViewAt(i)->GetVisible() == visible) {
      return testing::AssertionFailure() << "Index " << i <<
          " has improper visibility in overflow: " << visible;
    }
  }
  return testing::AssertionSuccess();
}

// Test the basic functionality of the BrowserActionsContainer in overflow mode.
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerOverflowTest,
                       TestBasicActionOverflow) {
  LoadExtensions();

  // Since the overflow bar isn't attached to a view, we have to kick it in
  // order to retrigger layout each time we change the number of icons in the
  // bar.
  overflow_bar()->Layout();

  // All actions are showing, and are in the installation order.
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(3u, toolbar_model()->visible_icon_count());
  ASSERT_EQ(3u, main_bar()->num_toolbar_actions());
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(3u));

  // Reduce the visible count to 2. Order should be unchanged (A B C), but
  // only A and B should be visible on the main bar.
  toolbar_model()->SetVisibleIconCount(2u);
  overflow_bar()->Layout();  // Kick.
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(2u));

  // Move extension C to the first position. Order should now be C A B, with
  // C and A visible in the main bar.
  toolbar_model()->MoveActionIcon(extension_c()->id(), 0);
  overflow_bar()->Layout();  // Kick.
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(2u));

  // Hide action A via a context menu. This results in it being sent to
  // overflow, and reducing the visible size to 1, so the order should be C A B,
  // with only C visible in the main bar.
  ui::MenuModel* menu_model = main_bar()
                                  ->GetToolbarActionViewAt(1)
                                  ->view_controller()
                                  ->GetContextMenu();
  extensions::ExtensionContextMenuModel* extension_menu =
      static_cast<extensions::ExtensionContextMenuModel*>(menu_model);
  extension_menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY, 0);
  overflow_bar()->Layout();  // Kick.
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(1u));
}

// Test drag and drop between the overflow container and the main container.
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerOverflowTest,
                       TestOverflowDragging) {
  LoadExtensions();

  // Start with one extension in overflow.
  toolbar_model()->SetVisibleIconCount(2u);
  overflow_bar()->Layout();

  // Verify starting state is A B [C].
  ASSERT_EQ(3u, main_bar()->num_toolbar_actions());
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(2u));

  // Drag extension A (on the main bar) to the left of extension C (in
  // overflow).
  ui::OSExchangeData drop_data;
  BrowserActionDragData browser_action_drag_data(extension_a()->id(), 0u);
  browser_action_drag_data.Write(profile(), &drop_data);
  ToolbarActionView* view = overflow_bar()->GetViewForId(extension_c()->id());
  gfx::PointF location(view->x(), view->y());
  ui::DropTargetEvent target_event(
      drop_data, location, location, ui::DragDropTypes::DRAG_MOVE);

  overflow_bar()->OnDragUpdated(target_event);
  overflow_bar()->OnPerformDrop(target_event);
  overflow_bar()->Layout();

  // Order should now be B [A C].
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(1u));

  // Drag extension A back from overflow to the main bar.
  ui::OSExchangeData drop_data2;
  BrowserActionDragData browser_action_drag_data2(extension_a()->id(), 1u);
  browser_action_drag_data2.Write(profile(), &drop_data2);
  view = main_bar()->GetViewForId(extension_b()->id());
  location = gfx::PointF(view->x(), view->y());
  ui::DropTargetEvent target_event2(
      drop_data2, location, location, ui::DragDropTypes::DRAG_MOVE);

  main_bar()->OnDragUpdated(target_event2);
  main_bar()->OnPerformDrop(target_event2);

  // Order should be A B [C] again.
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(2u));

  // Drag extension C from overflow to the main bar (before extension B).
  ui::OSExchangeData drop_data3;
  BrowserActionDragData browser_action_drag_data3(extension_c()->id(), 2u);
  browser_action_drag_data3.Write(profile(), &drop_data3);
  location = gfx::PointF(view->x(), view->y());
  ui::DropTargetEvent target_event3(
      drop_data3, location, location, ui::DragDropTypes::DRAG_MOVE);

  main_bar()->OnDragUpdated(target_event3);
  main_bar()->OnPerformDrop(target_event3);

  // Order should be A C B, and there should be no extensions in overflow.
  EXPECT_EQ(extension_a()->id(), main_bar()->GetIdAt(0u));
  EXPECT_EQ(extension_c()->id(), main_bar()->GetIdAt(1u));
  EXPECT_EQ(extension_b()->id(), main_bar()->GetIdAt(2u));
  EXPECT_TRUE(VerifyVisibleCount(3u));
}
