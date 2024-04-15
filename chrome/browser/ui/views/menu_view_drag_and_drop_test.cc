// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/menu_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/drop_helper.h"

namespace {

using ::ui::mojom::DragOperation;

const char16_t kTestNestedDragData[] = u"test_nested_drag_data";
const char16_t kTestTopLevelDragData[] = u"test_top_level_drag_data";

// A simple view which can be dragged.
class TestDragView : public views::View {
  METADATA_HEADER(TestDragView, views::View)

 public:
  TestDragView();

  TestDragView(const TestDragView&) = delete;
  TestDragView& operator=(const TestDragView&) = delete;

  ~TestDragView() override;

 private:
  // views::View:
  int GetDragOperations(const gfx::Point& point) override;
  void WriteDragData(const gfx::Point& point,
                     ui::OSExchangeData* data) override;
};

TestDragView::TestDragView() {
}

TestDragView::~TestDragView() {
}

int TestDragView::GetDragOperations(const gfx::Point& point) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void TestDragView::WriteDragData(const gfx::Point& point,
                                 ui::OSExchangeData* data) {
  data->SetString(kTestNestedDragData);
}

BEGIN_METADATA(TestDragView)
END_METADATA

// A simple view to serve as a drop target.
class TestTargetView : public views::View {
  METADATA_HEADER(TestTargetView, views::View)

 public:
  TestTargetView() = default;

  TestTargetView(const TestTargetView&) = delete;
  TestTargetView& operator=(const TestTargetView&) = delete;

  ~TestTargetView() override = default;

  // Initializes this view to have the same bounds as its parent, and to have
  // two draggable child views.
  void Init();

  bool dragging() const { return dragging_; }
  bool dropped() const { return dropped_; }

 private:
  // views::View:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;

  // Performs the drop operation and updates |output_drag_op| accordingly.
  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Whether or not we are currently dragging.
  bool dragging_ = false;

  // Whether or not a drop has been performed on the view.
  bool dropped_ = false;
};

void TestTargetView::Init() {
  // First, fill the parent completely.
  SetBoundsRect(parent()->GetLocalBounds());

  // Then add two draggable views, each 10x5.
  views::View* first = new TestDragView();
  AddChildView(first);
  first->SetBounds(2, 2, 10, 5);

  views::View* second = new TestDragView();
  AddChildView(second);
  second->SetBounds(15, 2, 10, 5);
}

bool TestTargetView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::STRING;
  return true;
}

bool TestTargetView::AreDropTypesRequired() {
  return true;
}

bool TestTargetView::CanDrop(const OSExchangeData& data) {
  std::u16string contents = data.GetString().value_or(std::u16string());
  return contents == kTestNestedDragData;
}

void TestTargetView::OnDragEntered(const ui::DropTargetEvent& event) {
  dragging_ = true;
}

int TestTargetView::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

views::View::DropCallback TestTargetView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  dragging_ = false;
  return base::BindOnce(&TestTargetView::PerformDrop, base::Unretained(this));
}

void TestTargetView::OnDragExited() {
  dragging_ = false;
}

void TestTargetView::PerformDrop(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  dropped_ = true;
  output_drag_op = DragOperation::kMove;
}

BEGIN_METADATA(TestTargetView)
END_METADATA

}  // namespace

class MenuViewDragAndDropTest : public MenuTestBase,
                                public views::WidgetObserver {
 public:
  MenuViewDragAndDropTest() = default;

  MenuViewDragAndDropTest(const MenuViewDragAndDropTest&) = delete;
  MenuViewDragAndDropTest& operator=(const MenuViewDragAndDropTest&) = delete;

  ~MenuViewDragAndDropTest() override = default;

 protected:
  // MenuTestBase:
  void BuildMenu(views::MenuItemView* menu) override;
  void DoTestWithMenuOpen() override;
  void TearDown() override;

  virtual void OnDragEntered();

  void SetStopDraggingView(const views::View* view) {
    views::DropHelper::SetDragEnteredCallbackForTesting(
        view, base::BindRepeating(&MenuViewDragAndDropTest::OnDragEntered,
                                  base::Unretained(this)));
  }

  TestTargetView* target_view() { return target_view_; }
  bool asked_to_close() const { return asked_to_close_; }
  bool performed_in_menu_drop() const { return performed_in_menu_drop_; }

 private:
  // views::MenuDelegate:
  bool GetDropFormats(views::MenuItemView* menu,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired(views::MenuItemView* menu) override;
  bool CanDrop(views::MenuItemView* menu,
               const ui::OSExchangeData& data) override;
  DragOperation GetDropOperation(views::MenuItemView* item,
                                 const ui::DropTargetEvent& event,
                                 DropPosition* position) override;
  views::View::DropCallback GetDropCallback(
      views::MenuItemView* menu,
      DropPosition position,
      const ui::DropTargetEvent& event) override;
  bool CanDrag(views::MenuItemView* menu) override;
  void WriteDragData(views::MenuItemView* sender,
                     ui::OSExchangeData* data) override;
  int GetDragOperations(views::MenuItemView* sender) override;
  bool ShouldCloseOnDragComplete() override;

  // Performs the drop operation and updates |output_drag_op| accordingly.
  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // The special view in the menu, which supports its own drag and drop.
  raw_ptr<TestTargetView, DanglingUntriaged> target_view_ = nullptr;

  // Whether or not we have been asked to close on drag complete.
  bool asked_to_close_ = false;

  // Whether or not a drop was performed in-menu (i.e., not including drops
  // in separate child views).
  bool performed_in_menu_drop_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

void MenuViewDragAndDropTest::BuildMenu(views::MenuItemView* menu) {
  // Build a menu item that has a nested view that supports its own drag and
  // drop...
  views::MenuItemView* menu_item_view = menu->AppendMenuItem(1, u"item 1");
  target_view_ = new TestTargetView();
  menu_item_view->AddChildView(target_view_.get());
  // ... as well as two other, normal items.
  menu->AppendMenuItem(2, u"item 2");
  menu->AppendMenuItem(3, u"item 3");
}

void MenuViewDragAndDropTest::DoTestWithMenuOpen() {
  // Sanity checks: We should be showing the menu, it should have three
  // children, and the first of those children should have a nested view of the
  // TestTargetView.
  views::SubmenuView* submenu = menu()->GetSubmenu();
  ASSERT_TRUE(submenu);
  ASSERT_TRUE(submenu->IsShowing());
  ASSERT_EQ(3u, submenu->GetMenuItems().size());
  const views::View* first_view = submenu->GetMenuItemAt(0);
  ASSERT_EQ(1u, first_view->children().size());
  const views::View* child_view = first_view->children().front();
  EXPECT_EQ(child_view, target_view_);

  // The menu is showing, so it has a widget we can observe now.
  widget_observation_.Observe(submenu->GetWidget());

  // We do this here (instead of in BuildMenu()) so that the menu is already
  // built and the bounds are correct.
  target_view_->Init();
}

void MenuViewDragAndDropTest::TearDown() {
  widget_observation_.Reset();
  MenuTestBase::TearDown();
}

void MenuViewDragAndDropTest::OnDragEntered() {
  // Drop the element, which should result in calling OnWidgetDragComplete().
  GetDragTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseEvents),
                     ui_controls::LEFT, ui_controls::UP,
                     ui_controls::kNoAccelerator, ui_controls::kNoWindowHint));
}

bool MenuViewDragAndDropTest::GetDropFormats(
    views::MenuItemView* menu,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::STRING;
  return true;
}

bool MenuViewDragAndDropTest::AreDropTypesRequired(views::MenuItemView* menu) {
  return true;
}

bool MenuViewDragAndDropTest::CanDrop(views::MenuItemView* menu,
                                      const ui::OSExchangeData& data) {
  std::u16string contents = data.GetString().value_or(std::u16string());
  return contents == kTestTopLevelDragData;
}

DragOperation MenuViewDragAndDropTest::GetDropOperation(
    views::MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  return DragOperation::kMove;
}

views::View::DropCallback MenuViewDragAndDropTest::GetDropCallback(
    views::MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&MenuViewDragAndDropTest::PerformDrop,
                        base::Unretained(this));
}

bool MenuViewDragAndDropTest::CanDrag(views::MenuItemView* menu) {
  return true;
}

void MenuViewDragAndDropTest::WriteDragData(
    views::MenuItemView* sender, ui::OSExchangeData* data) {
  data->SetString(kTestTopLevelDragData);
}

int MenuViewDragAndDropTest::GetDragOperations(views::MenuItemView* sender) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool MenuViewDragAndDropTest::ShouldCloseOnDragComplete() {
  asked_to_close_ = true;
  return false;
}

void MenuViewDragAndDropTest::PerformDrop(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  performed_in_menu_drop_ = true;
  output_drag_op = DragOperation::kMove;
}

class MenuViewDragAndDropTestTestInMenuDrag : public MenuViewDragAndDropTest {
 public:
  MenuViewDragAndDropTestTestInMenuDrag() = default;
  ~MenuViewDragAndDropTestTestInMenuDrag() override = default;

  // views::WidgetObserver:
  void OnWidgetDragWillStart(views::Widget* widget) override;
  void OnWidgetDragComplete(views::Widget* widget) override;

 protected:
  // MenuViewDragAndDropTest:
  void DoTestWithMenuOpen() override;

 private:
  void StartDrag();
};

void MenuViewDragAndDropTestTestInMenuDrag::OnWidgetDragWillStart(
    views::Widget* widget) {
  // Enqueue an event to drag the second menu element to the third element,
  // which should result in calling OnDragEntered().
  const views::View* drop_target_view = menu()->GetSubmenu()->GetMenuItemAt(2);
  const gfx::Point target =
      ui_test_utils::GetCenterInScreenCoordinates(drop_target_view);
  GetDragTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove),
                     target.x(), target.y(), ui_controls::kNoWindowHint));
}

void MenuViewDragAndDropTestTestInMenuDrag::OnWidgetDragComplete(
    views::Widget* widget) {
  // We should have performed an in-menu drop, and the nested view should not
  // have had a drag and drop. Since the drag happened in menu code, the
  // delegate should not have been asked whether or not to close, and the menu
  // should simply be closed.
  EXPECT_TRUE(performed_in_menu_drop());
  EXPECT_FALSE(target_view()->dropped());
  EXPECT_FALSE(asked_to_close());
  EXPECT_FALSE(menu()->GetSubmenu()->IsShowing());

  Done();
}

void MenuViewDragAndDropTestTestInMenuDrag::DoTestWithMenuOpen() {
  MenuViewDragAndDropTest::DoTestWithMenuOpen();

  // Cause the third menu item to trigger a mouse up when dragged over.
  views::SubmenuView* submenu = menu()->GetSubmenu();
  SetStopDraggingView(submenu->GetMenuItemAt(2));

  // We're going to drag the second menu element.
  views::MenuItemView* drag_view = submenu->GetMenuItemAt(1);
  ASSERT_NE(nullptr, drag_view);
  ui_test_utils::MoveMouseToCenterAndPress(
      drag_view, ui_controls::LEFT, ui_controls::DOWN,
      CreateEventTask(this, &MenuViewDragAndDropTestTestInMenuDrag::StartDrag));
}

void MenuViewDragAndDropTestTestInMenuDrag::StartDrag() {
  // Begin dragging the second menu element, which should result in calling
  // OnWidgetDragWillStart().
  const views::View* drag_view = menu()->GetSubmenu()->GetMenuItemAt(1);
  const gfx::Point current_position =
      ui_test_utils::GetCenterInScreenCoordinates(drag_view);
  EXPECT_TRUE(ui_controls::SendMouseMove(current_position.x() + 10,
                                         current_position.y()));
}

// Test that an in-menu (i.e., entirely implemented in the menu code) closes the
// menu automatically once the drag is complete, and does not ask the delegate
// to stay open.
// TODO(pkasting): https://crbug.com/939621 Fails on Mac.
// TODO(crbug.com/40911016): Re-enable this test for linux.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_TestInMenuDrag DISABLED_TestInMenuDrag
#else
#define MAYBE_TestInMenuDrag TestInMenuDrag
#endif
VIEW_TEST(MenuViewDragAndDropTestTestInMenuDrag, MAYBE_TestInMenuDrag)

class MenuViewDragAndDropTestNestedDrag : public MenuViewDragAndDropTest {
 public:
  MenuViewDragAndDropTestNestedDrag() = default;
  ~MenuViewDragAndDropTestNestedDrag() override = default;

  // views::WidgetObserver:
  void OnWidgetDragWillStart(views::Widget* widget) override;
  void OnWidgetDragComplete(views::Widget* widget) override;

 protected:
  // MenuViewDragAndDropTest:
  void DoTestWithMenuOpen() override;
  void OnDragEntered() override;

 private:
  void StartDrag();
};

void MenuViewDragAndDropTestNestedDrag::OnWidgetDragWillStart(
    views::Widget* widget) {
  // Enqueue an event to drag the target's first child to its second, which
  // should result in calling OnDragEntered().
  const views::View* drop_target_view = target_view()->children()[1];
  const gfx::Point target =
      ui_test_utils::GetCenterInScreenCoordinates(drop_target_view);
  GetDragTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove),
                     target.x(), target.y(), ui_controls::kNoWindowHint));
}

void MenuViewDragAndDropTestNestedDrag::OnWidgetDragComplete(
    views::Widget* widget) {
  // The target view should have finished its drag, and should have dropped the
  // view. The main menu should not have done any drag, and the delegate should
  // have been asked if it wanted to close. Since the delegate did not want to
  // close, the menu should still be open.
  EXPECT_FALSE(target_view()->dragging());
  EXPECT_TRUE(target_view()->dropped());
  EXPECT_FALSE(performed_in_menu_drop());
  EXPECT_TRUE(asked_to_close());
  views::SubmenuView* submenu = menu()->GetSubmenu();
  EXPECT_TRUE(submenu->IsShowing());

  // Clean up.
  submenu->Close();

  Done();
}

void MenuViewDragAndDropTestNestedDrag::DoTestWithMenuOpen() {
  MenuViewDragAndDropTest::DoTestWithMenuOpen();

  // Cause the target's second child to trigger a mouse up when dragged over.
  ASSERT_EQ(2u, target_view()->children().size());
  SetStopDraggingView(target_view()->children()[1]);

  // We're going to drag the target's first child.
  views::View* drag_view = target_view()->children()[0];
  ASSERT_NE(nullptr, drag_view);
  ui_test_utils::MoveMouseToCenterAndPress(
      drag_view, ui_controls::LEFT, ui_controls::DOWN,
      CreateEventTask(this, &MenuViewDragAndDropTestNestedDrag::StartDrag));
}

void MenuViewDragAndDropTestNestedDrag::OnDragEntered() {
  // The view should be dragging now.
  EXPECT_TRUE(target_view()->dragging());

  MenuViewDragAndDropTest::OnDragEntered();
}

void MenuViewDragAndDropTestNestedDrag::StartDrag() {
  // Begin dragging the target's first child, which should result in calling
  // OnWidgetDragWillStart().
  const views::View* drag_view = target_view()->children().front();
  const gfx::Point current_position =
      ui_test_utils::GetCenterInScreenCoordinates(drag_view);
  EXPECT_TRUE(ui_controls::SendMouseMove(current_position.x() + 10,
                                         current_position.y()));
}

// Test that a nested drag (i.e. one via a child view, and not entirely
// implemented in menu code) will consult the delegate before closing the view
// after the drag.
// TODO(pkasting): https://crbug.com/939621 Fails on Mac.
// TODO(crbug.com/41496561): Test is failing under ChromeRefresh2023 on wayland.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE_WAYLAND)
#define MAYBE_MenuViewDragAndDropNestedDrag \
  DISABLED_MenuViewDragAndDropNestedDrag
#else
#define MAYBE_MenuViewDragAndDropNestedDrag MenuViewDragAndDropNestedDrag
#endif
VIEW_TEST(MenuViewDragAndDropTestNestedDrag,
          MAYBE_MenuViewDragAndDropNestedDrag)

class MenuViewDragAndDropForDropStayOpen : public MenuViewDragAndDropTest {
 public:
  MenuViewDragAndDropForDropStayOpen() {}
  ~MenuViewDragAndDropForDropStayOpen() override {}

 private:
  // MenuViewDragAndDropTest:
  int GetMenuRunnerFlags() override;
  void DoTestWithMenuOpen() override;
};

int MenuViewDragAndDropForDropStayOpen::GetMenuRunnerFlags() {
  return views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::NESTED_DRAG |
         views::MenuRunner::FOR_DROP;
}

void MenuViewDragAndDropForDropStayOpen::DoTestWithMenuOpen() {
  MenuViewDragAndDropTest::DoTestWithMenuOpen();

  views::MenuController* controller = menu()->GetMenuController();
  ASSERT_TRUE(controller);
  EXPECT_FALSE(controller->IsCancelAllTimerRunningForTest());

  Done();
}

// Test that if a menu is opened for a drop which is handled by a child view
// that the menu does not immediately try to close.
// If this flakes, disable and log details in http://crbug.com/523255.
// Flaky on Lacros. https://crbug.com/1281104
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_MenuViewStaysOpenForNestedDrag \
  DISABLED_MenuViewStaysOpenForNestedDrag
#else
#define MAYBE_MenuViewStaysOpenForNestedDrag MenuViewStaysOpenForNestedDrag
#endif
VIEW_TEST(MenuViewDragAndDropForDropStayOpen,
          MAYBE_MenuViewStaysOpenForNestedDrag)

class MenuViewDragAndDropForDropCancel : public MenuViewDragAndDropTest {
 public:
  MenuViewDragAndDropForDropCancel() {}
  ~MenuViewDragAndDropForDropCancel() override {}

 private:
  // MenuViewDragAndDropTest:
  int GetMenuRunnerFlags() override;
  void DoTestWithMenuOpen() override;
};

int MenuViewDragAndDropForDropCancel::GetMenuRunnerFlags() {
  return views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::FOR_DROP;
}

void MenuViewDragAndDropForDropCancel::DoTestWithMenuOpen() {
  MenuViewDragAndDropTest::DoTestWithMenuOpen();

  views::MenuController* controller = menu()->GetMenuController();
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsCancelAllTimerRunningForTest());

  Done();
}

// Test that if a menu is opened for a drop handled entirely by menu code, the
// menu will try to close if it does not receive any drag updates.
// Flaky on Lacros. https://crbug.com/1281103
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_MenuViewCancelsForOwnDrag \
  DISABLED_MenuViewCancelsForOwnDrag
#else
#define MAYBE_MenuViewCancelsForOwnDrag MenuViewCancelsForOwnDrag
#endif
VIEW_TEST(MenuViewDragAndDropForDropCancel, MAYBE_MenuViewCancelsForOwnDrag)
