// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>
#include <tuple>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/clipboard_history_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace {

using ImageModelRequestTestParams = ClipboardImageModelRequest::TestParams;
using ScopedClipboardHistoryListUpdateWaiter =
    clipboard_history::ScopedClipboardHistoryListUpdateWaiter;
using ClipboardImageModelRequestWaiter =
    clipboard_history::ClipboardImageModelRequestWaiter;
using MenuViewID = ash::clipboard_history_util::MenuViewID;

constexpr char kUrlString[] = "https://www.example.com";

// A class which can wait until a matching `ui::ClipboardData` is in the buffer.
class ClipboardDataWaiter : public ui::ClipboardObserver {
 public:
  ClipboardDataWaiter() = default;
  ClipboardDataWaiter(const ClipboardDataWaiter&) = delete;
  ClipboardDataWaiter& operator=(const ClipboardDataWaiter&) = delete;
  ~ClipboardDataWaiter() override = default;

  void WaitFor(const ui::ClipboardData* clipboard_data) {
    base::AutoReset scoped_data(&clipboard_data_, clipboard_data);
    if (BufferMatchesClipboardData())
      return;

    base::ScopedObservation<ui::ClipboardMonitor, ui::ClipboardObserver>
        clipboard_observer_{this};
    clipboard_observer_.Observe(ui::ClipboardMonitor::GetInstance());

    base::AutoReset scoped_loop(&run_loop_, std::make_unique<base::RunLoop>());
    run_loop_->Run();
  }

 private:
  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override {
    if (BufferMatchesClipboardData())
      run_loop_->Quit();
  }

  bool BufferMatchesClipboardData() const {
    auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
    ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
    const auto* clipboard_data = clipboard->GetClipboardData(&data_dst);

    if ((clipboard_data == nullptr) != (clipboard_data_ == nullptr))
      return false;

    return clipboard_data == nullptr || *clipboard_data == *clipboard_data_;
  }

  const ui::ClipboardData* clipboard_data_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// The helper class to wait for the observed view's bounds update.
class ViewBoundsWaiter : public views::ViewObserver {
 public:
  explicit ViewBoundsWaiter(views::View* observed_view)
      : observed_view_(observed_view) {
    observed_view_->AddObserver(this);
  }

  ViewBoundsWaiter(const ViewBoundsWaiter&) = delete;
  ViewBoundsWaiter& operator=(const ViewBoundsWaiter&) = delete;
  ~ViewBoundsWaiter() override { observed_view_->RemoveObserver(this); }

  void WaitForMeaningfulBounds() {
    // No-op if `observed_view_` already has meaningful bounds.
    if (!observed_view_->bounds().IsEmpty())
      return;

    run_loop_.Run();
  }

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    EXPECT_FALSE(observed_view->bounds().IsEmpty());
    run_loop_.Quit();
  }

  const raw_ptr<views::View, ExperimentalAsh> observed_view_;
  base::RunLoop run_loop_;
};

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::Widget> CreateTestWidget() {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget->Init(std::move(params));

  return widget;
}

ash::ClipboardHistoryControllerImpl* GetClipboardHistoryController() {
  return ash::Shell::Get()->clipboard_history_controller();
}

ash::ClipboardHistoryMenuModelAdapter* GetContextMenu() {
  return GetClipboardHistoryController()->context_menu_for_test();
}

const std::list<ash::ClipboardHistoryItem>& GetClipboardItems() {
  return GetClipboardHistoryController()->history()->GetItems();
}

gfx::Rect GetClipboardHistoryMenuBoundsInScreen() {
  return GetClipboardHistoryController()->GetMenuBoundsInScreenForTest();
}

bool VerifyClipboardTextData(const std::initializer_list<std::string>& texts) {
  const std::list<ash::ClipboardHistoryItem>& items = GetClipboardItems();
  if (items.size() != texts.size())
    return false;

  auto items_iter = items.cbegin();
  const auto* texts_iter = texts.begin();
  while (items_iter != items.cend() && texts_iter != texts.end()) {
    if (items_iter->data().text() != *texts_iter)
      return false;
    ++items_iter;
    ++texts_iter;
  }

  return true;
}

// Returns whether the clipboard buffer matches clipboard history's first item.
// If clipboard history is empty, returns whether the clipboard buffer is empty.
bool VerifyClipboardBufferAndHistoryInSync() {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  if (!clipboard)
    return false;

  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* const clipboard_data = clipboard->GetClipboardData(&data_dst);
  const auto& items = GetClipboardItems();
  return items.empty() ? clipboard_data == nullptr
                       : items.front().data() == *clipboard_data;
}

}  // namespace

class ClipboardHistoryBrowserTest : public ash::LoginManagerTest {
 public:
  ClipboardHistoryBrowserTest() {
    login_mixin_.AppendRegularUsers(1);
    account_id1_ = login_mixin_.users()[0].account_id;
    std::vector<base::test::FeatureRef> disabled_features = {
        ash::features::kClipboardHistoryReorder};
    feature_list_.InitWithFeatures(/*enabled_features=*/{}, disabled_features);
  }

  ~ClipboardHistoryBrowserTest() override = default;

  ui::test::EventGenerator* GetEventGenerator() {
    return event_generator_.get();
  }

 protected:
  // ash::LoginManagerTest:
  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
    LoginUser(account_id1_);
    GetClipboardHistoryController()->set_confirmed_operation_callback_for_test(
        operation_confirmed_future_.GetCallback());
  }

  // Click at the delete button of the menu entry specified by `index`.
  void ClickAtDeleteButton(int index) {
    auto* item_view = GetContextMenu()->GetMenuItemViewAtForTest(index);
    views::View* delete_button =
        item_view->GetViewByID(MenuViewID::kDeleteButtonViewID);

    if (delete_button->GetVisible()) {
      // Assume that `delete_button` already has meaningful bounds.
      ASSERT_FALSE(delete_button->GetBoundsInScreen().IsEmpty());
    } else {
      ShowDeleteButtonByMouseHover(index);
    }

    GetEventGenerator()->MoveMouseTo(
        delete_button->GetBoundsInScreen().CenterPoint());
    EXPECT_EQ(
        delete_button->GetTooltipText(delete_button->bounds().CenterPoint()),
        l10n_util::GetStringUTF16(
            IDS_CLIPBOARD_HISTORY_DELETE_BUTTON_HOVER_TEXT));
    GetEventGenerator()->ClickLeftButton();
  }

  void Press(ui::KeyboardCode key, int modifiers = ui::EF_NONE) {
    event_generator_->PressKey(key, modifiers);
  }

  void Release(ui::KeyboardCode key, int modifiers = ui::EF_NONE) {
    event_generator_->ReleaseKey(key, modifiers);
  }

  void PressAndRelease(ui::KeyboardCode key, int modifiers = ui::EF_NONE) {
    Press(key, modifiers);
    Release(key, modifiers);
  }

  void ShowContextMenuViaAccelerator(bool wait_for_selection) {
    PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
    if (!wait_for_selection)
      return;

    base::RunLoop run_loop;
    GetClipboardHistoryController()
        ->set_initial_item_selected_callback_for_test(run_loop.QuitClosure());
    run_loop.Run();
  }

  const views::MenuItemView* GetMenuItemViewForIndex(int index) const {
    return GetContextMenu()->GetMenuItemViewAtForTest(index);
  }

  const ash::ClipboardHistoryItemView* GetHistoryItemViewForIndex(
      int index) const {
    const views::MenuItemView* hosting_menu_item =
        GetMenuItemViewForIndex(index);
    EXPECT_EQ(1u, hosting_menu_item->children().size());
    return static_cast<const ash::ClipboardHistoryItemView*>(
        hosting_menu_item->children()[0]);
  }

  ash::ClipboardHistoryItemView* GetHistoryItemViewForIndex(int index) {
    return const_cast<ash::ClipboardHistoryItemView*>(
        const_cast<const ClipboardHistoryBrowserTest*>(this)
            ->GetHistoryItemViewForIndex(index));
  }

  // Show the delete button by hovering the mouse on the menu entry specified
  // by the index.
  void ShowDeleteButtonByMouseHover(int index) {
    auto* item_view = GetContextMenu()->GetMenuItemViewAtForTest(index);
    views::View* delete_button =
        item_view->GetViewByID(MenuViewID::kDeleteButtonViewID);
    ASSERT_FALSE(delete_button->GetVisible());

    // Hover the mouse on `item_view` to show the delete button.
    GetEventGenerator()->MoveMouseTo(
        item_view->GetBoundsInScreen().CenterPoint(), /*count=*/5);

    // Wait until `delete_button` has meaningful bounds. Note that the bounds
    // are set by the layout manager asynchronously.
    ViewBoundsWaiter waiter(delete_button);
    waiter.WaitForMeaningfulBounds();

    EXPECT_TRUE(delete_button->GetVisible());
    EXPECT_TRUE(item_view->IsSelected());
  }

  void WaitForOperationConfirmed(bool success_expected) {
    EXPECT_EQ(operation_confirmed_future_.Take(), success_expected);
  }

  void SetClipboardText(const std::string& text) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(text));

    // ClipboardHistory will post a task to process clipboard data in order to
    // debounce multiple clipboard writes occurring in sequence. Here we give
    // ClipboardHistory the chance to run its posted tasks before proceeding.
    WaitForOperationConfirmed(/*success_expected=*/true);
  }

  void SetClipboardTextAndHtml(const std::string& text,
                               const std::string& html) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(base::UTF8ToUTF16(text));
      scw.WriteHTML(base::UTF8ToUTF16(html), /*source_url=*/"",
                    ui::ClipboardContentType::kSanitized);
    }

    // ClipboardHistory will post a task to process clipboard data in order to
    // debounce multiple clipboard writes occurring in sequence. Here we give
    // ClipboardHistory the chance to run its posted tasks before proceeding.
    WaitForOperationConfirmed(/*success_expected=*/true);
  }

  AccountId account_id1_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::test::RepeatingTestFuture<bool> operation_confirmed_future_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies the clipboard history menu response to mouse and arrow key inputs.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest,
                       VerifyMouseAndArrowKeyTraversal) {
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  base::HistogramTester histogram_tester;

  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(3u, GetContextMenu()->GetMenuItemsCount());
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.ContextMenu.ShowMenu",
      crosapi::mojom::ClipboardHistoryControllerShowSource::kAccelerator, 1);

  // The history menu's first item should be selected as default after the menu
  // shows. Meanwhile, its delete button should not show.
  const views::MenuItemView* const first_menu_item_view =
      GetMenuItemViewForIndex(/*index=*/0);
  EXPECT_TRUE(first_menu_item_view->IsSelected());
  EXPECT_FALSE(GetHistoryItemViewForIndex(/*index=*/0)
                   ->GetViewByID(MenuViewID::kDeleteButtonViewID)
                   ->GetVisible());
  EXPECT_EQ(gfx::Size(256, 36), first_menu_item_view->size());

  // Move the mouse to the second menu item.
  const views::MenuItemView* const second_menu_item_view =
      GetMenuItemViewForIndex(/*index=*/1);
  EXPECT_FALSE(second_menu_item_view->IsSelected());
  GetEventGenerator()->MoveMouseTo(
      second_menu_item_view->GetBoundsInScreen().CenterPoint());

  // The first menu item should not be selected while the second one should be.
  EXPECT_FALSE(first_menu_item_view->IsSelected());
  EXPECT_TRUE(second_menu_item_view->IsSelected());

  // Under mouse hovering, the second item's delete button should show.
  EXPECT_TRUE(GetHistoryItemViewForIndex(/*index=*/1)
                  ->GetViewByID(MenuViewID::kDeleteButtonViewID)
                  ->GetVisible());

  // Move the selection to the third item by pressing the arrow key.
  const views::MenuItemView* const third_menu_item_view =
      GetMenuItemViewForIndex(/*index=*/2);
  EXPECT_FALSE(third_menu_item_view->IsSelected());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);

  // The third item should be selected and its delete button should not show.
  EXPECT_FALSE(second_menu_item_view->IsSelected());
  EXPECT_TRUE(third_menu_item_view->IsSelected());
  EXPECT_FALSE(GetHistoryItemViewForIndex(/*index=*/2)
                   ->GetViewByID(MenuViewID::kDeleteButtonViewID)
                   ->GetVisible());
}

// Verifies tab traversal behavior when there is only one item in clipboard
// history.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest,
                       VerifySingleItemTabTraversal) {
  SetClipboardText("A");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);

  // Verify the default state right after the menu shows.
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(1u, GetContextMenu()->GetMenuItemsCount());

  const views::MenuItemView* const menu_item_view =
      GetMenuItemViewForIndex(/*index=*/0);
  const ash::ClipboardHistoryItemView* const history_item_view =
      GetHistoryItemViewForIndex(/*index=*/0);

  EXPECT_TRUE(menu_item_view->IsSelected());
  EXPECT_TRUE(history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(history_item_view->IsDeleteButtonPseudoFocused());

  // Press the Tab key. Verify that the history item's pseudo focus moves from
  // the main button to the delete button.
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(menu_item_view->IsSelected());
  EXPECT_FALSE(history_item_view->IsMainButtonPseudoFocused());
  EXPECT_TRUE(history_item_view->IsDeleteButtonPseudoFocused());

  // Verify that the history item's delete button shows. In addition, the
  // delete button's inkdrop highlight should fade in or be visible because the
  // button is focused.
  const views::View* const delete_button =
      history_item_view->GetViewByID(MenuViewID::kDeleteButtonViewID);
  EXPECT_TRUE(delete_button->GetVisible());
  EXPECT_TRUE(views::InkDrop::Get(const_cast<views::View*>(delete_button))
                  ->GetInkDrop()
                  ->IsHighlightFadingInOrVisible());

  // Press the Tab key. Verify that the history item's pseudo focus moves from
  // the delete button back to the main button and the delete button stops being
  // visible.
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(menu_item_view->IsSelected());
  EXPECT_TRUE(history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(history_item_view->IsDeleteButtonPseudoFocused());
  EXPECT_FALSE(delete_button->GetVisible());
}

// Verifies tab traversal behavior when there are multiple items in clipboard
// history.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest,
                       VerifyMultiItemTabTraversal) {
  SetClipboardText("A");
  SetClipboardText("B");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);

  // Verify the default state right after the menu shows.
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(2u, GetContextMenu()->GetMenuItemsCount());

  const views::MenuItemView* const first_menu_item_view =
      GetMenuItemViewForIndex(/*index=*/0);
  const views::MenuItemView* const second_menu_item_view =
      GetMenuItemViewForIndex(/*index=*/1);
  const ash::ClipboardHistoryItemView* const first_history_item_view =
      GetHistoryItemViewForIndex(/*index=*/0);
  const ash::ClipboardHistoryItemView* const second_history_item_view =
      GetHistoryItemViewForIndex(/*index=*/1);

  EXPECT_TRUE(first_menu_item_view->IsSelected());
  EXPECT_TRUE(first_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(first_history_item_view->IsDeleteButtonPseudoFocused());

  EXPECT_FALSE(second_menu_item_view->IsSelected());
  EXPECT_FALSE(second_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(second_history_item_view->IsDeleteButtonPseudoFocused());

  // Press the Tab key. Verify that the first menu item is still selected while
  // the history item's pseudo focus moves from the main button to the delete
  // button.
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(first_menu_item_view->IsSelected());
  EXPECT_FALSE(first_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_TRUE(first_history_item_view->IsDeleteButtonPseudoFocused());

  // Press the Tab key. Verify that the second menu item is selected and its
  // main button has pseudo focus.
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(second_menu_item_view->IsSelected());
  EXPECT_TRUE(second_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(second_history_item_view->IsDeleteButtonPseudoFocused());

  // Press the Tab key. Verify that the second history item's pseudo focus moves
  // from its main button to its delete button.
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(second_menu_item_view->IsSelected());
  EXPECT_FALSE(second_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_TRUE(second_history_item_view->IsDeleteButtonPseudoFocused());

  // Press the Tab key with the Shift key pressed. Verify that the second
  // history item's pseudo focus goes back to its main button.
  PressAndRelease(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(second_menu_item_view->IsSelected());
  EXPECT_TRUE(second_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(second_history_item_view->IsDeleteButtonPseudoFocused());

  // Press the Tab key with the Shift key pressed. Verify that the first menu
  // item is selected and its delete button has pseudo focus.
  PressAndRelease(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(first_menu_item_view->IsSelected());
  EXPECT_FALSE(first_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_TRUE(first_history_item_view->IsDeleteButtonPseudoFocused());

  EXPECT_FALSE(second_menu_item_view->IsSelected());
  EXPECT_FALSE(second_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(second_history_item_view->IsDeleteButtonPseudoFocused());

  // Press the Enter key. Verify that the first item is deleted. The second item
  // should now be selected and its main button should have pseudo focus.
  PressAndRelease(ui::VKEY_RETURN);
  EXPECT_EQ(1u, GetContextMenu()->GetMenuItemsCount());
  EXPECT_TRUE(second_menu_item_view->IsSelected());
  EXPECT_TRUE(second_history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(second_history_item_view->IsDeleteButtonPseudoFocused());
}

// Verifies that the history menu is anchored at the cursor's location when
// not having any textfield.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest,
                       ShowHistoryMenuWhenNoTextfieldExists) {
  // Close the browser window to ensure that textfield does not exist.
  CloseAllBrowsers();

  // No clipboard data. So the clipboard history menu should not show.
  ASSERT_TRUE(GetClipboardItems().empty());
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/false);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  SetClipboardText("test");

  const gfx::Point mouse_location =
      ash::Shell::Get()->GetPrimaryRootWindow()->bounds().CenterPoint();
  GetEventGenerator()->MoveMouseTo(mouse_location);
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);

  // Verifies that the menu is anchored at the cursor's location.
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  const gfx::Point menu_origin =
      GetClipboardHistoryMenuBoundsInScreen().origin();
  EXPECT_EQ(mouse_location.x(), menu_origin.x());
  EXPECT_EQ(mouse_location.y() +
                views::MenuConfig::instance().touchable_anchor_offset,
            menu_origin.y());
}

// Verify the handling of the click cancel event.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest, HandleClickCancelEvent) {
  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");

  // Show the menu.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(2u, GetContextMenu()->GetMenuItemsCount());

  // Press on the first menu item.
  ash::ClipboardHistoryItemView* first_item_view =
      GetHistoryItemViewForIndex(/*index=*/0);
  GetEventGenerator()->MoveMouseTo(
      first_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();

  // Move the mouse to the second menu item then release.
  auto* second_item_view =
      GetContextMenu()->GetMenuItemViewAtForTest(/*index=*/1);
  ASSERT_FALSE(second_item_view->IsSelected());
  GetEventGenerator()->MoveMouseTo(
      second_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ReleaseLeftButton();

  // Verify that the second menu item is selected now.
  EXPECT_TRUE(second_item_view->IsSelected());
}

// Verifies item deletion through the mouse click at the delete button.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest,
                       DeleteItemByClickAtDeleteButton) {
  base::HistogramTester histogram_tester;

  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");

  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(2u, GetContextMenu()->GetMenuItemsCount());

  // Delete the second menu item.
  {
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    ClickAtDeleteButton(/*index=*/1);
  }
  EXPECT_EQ(1u, GetContextMenu()->GetMenuItemsCount());
  EXPECT_TRUE(VerifyClipboardTextData({"B"}));
  EXPECT_TRUE(VerifyClipboardBufferAndHistoryInSync());

  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatDeleted", 1);

  // Delete the last menu item. Verify that the menu is closed.
  {
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    ClickAtDeleteButton(/*index=*/0);
  }
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_TRUE(VerifyClipboardBufferAndHistoryInSync());

  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatDeleted", 2);

  // No menu shows because of the empty clipboard history.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/false);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
}

// Verifies that the selected item should be deleted by the backspace key.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest, DeleteItemViaBackspaceKey) {
  base::HistogramTester histogram_tester;

  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  // Show the menu.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(3u, GetContextMenu()->GetMenuItemsCount());

  // Select the first menu item via key then delete it. Verify the menu and the
  // clipboard history.
  {
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    PressAndRelease(ui::KeyboardCode::VKEY_BACK);
  }
  EXPECT_EQ(2u, GetContextMenu()->GetMenuItemsCount());
  EXPECT_TRUE(VerifyClipboardTextData({"B", "A"}));
  EXPECT_TRUE(VerifyClipboardBufferAndHistoryInSync());

  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatDeleted", 1);

  // Select the second menu item via key then delete it. Verify the menu and the
  // clipboard history.
  {
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
    PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  }
  EXPECT_EQ(1u, GetContextMenu()->GetMenuItemsCount());
  EXPECT_TRUE(VerifyClipboardTextData({"B"}));
  EXPECT_TRUE(VerifyClipboardBufferAndHistoryInSync());

  // Delete the last item. Verify that the menu is closed.
  {
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  }
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_TRUE(VerifyClipboardBufferAndHistoryInSync());

  // Trigger the accelerator of opening the clipboard history menu. No menu
  // shows because of the empty history data.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/false);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
}

IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest,
                       OpenClipboardHistoryViaAccelerator) {
  // Verify Command+V shortcut does not open empty clipboard history menu.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify Shift+Command+V shortcut does not open clipboard history menu.
  PressAndRelease(ui::KeyboardCode::VKEY_V,
                  ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Write some things to the clipboard to allow test to potentially show menu.
  SetClipboardText("A");
  SetClipboardText("B");

  // Verify Shift+Command+V shortcut does not open clipboard history menu.
  PressAndRelease(ui::KeyboardCode::VKEY_V,
                  ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify Command+V shortcut opens non-empty clipboard history menu.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
}

class ClipboardHistoryPasteTypeBrowserTest
    : public ClipboardHistoryBrowserTest {
 public:
  ClipboardHistoryPasteTypeBrowserTest() = default;
  ~ClipboardHistoryPasteTypeBrowserTest() override = default;

 protected:
  // ClipboardHistoryBrowserTest:
  void SetUpOnMainThread() override {
    ClipboardHistoryBrowserTest::SetUpOnMainThread();
    // Increase delay interval before restoring the clipboard buffer following
    // a paste event as this test has exhibited flakiness due to the amount of
    // time it takes a paste event to reach the web contents under test. Remove
    // this code when possible (https://crbug.com/1303131).
    GetClipboardHistoryController()->set_buffer_restoration_delay_for_test(
        base::Milliseconds(500));

    // Create a browser and cache its active web contents.
    auto* browser = CreateBrowser(
        ash::ProfileHelper::Get()->GetProfileByAccountId(account_id1_));
    web_contents_ = browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents_);

    // Load the web contents synchronously.
    // The contained script:
    //  - Listens for paste events and caches the last pasted data.
    //  - Notifies observers of paste events by changing document title.
    //  - Provides an API to expose the last pasted data.
    ASSERT_TRUE(content::NavigateToURL(web_contents_, GURL(R"(data:text/html,
      <!DOCTYPE html>
      <html>
        <body>
          <script>

            let lastPaste = undefined;
            let lastPasteId = 1;

            window.addEventListener('paste', e => {
              e.stopPropagation();
              e.preventDefault();

              const clipboardData = e.clipboardData || window.clipboardData;
              lastPaste = clipboardData.types.map((type) => {
                return `${type}: ${clipboardData.getData(type)}`;
              });

              document.title = `Paste ${lastPasteId++}`;
            });

            window.getLastPaste = () => {
              return lastPaste || [];
            };

          </script>
        </body>
      </html>
    )")));

    ASSERT_TRUE(GetLastPaste().empty());
  }

  // Waits for a paste event to propagate to the web contents and confirms that
  // the expected `text` is pasted, formatted according to `paste_plain_text`.
  void WaitForWebContentsPaste(base::StringPiece text, bool paste_plain_text) {
    // The web contents will update its page title once it receives a paste
    // event.
    std::ignore =
        content::TitleWatcher(
            web_contents_,
            base::StrCat({u"Paste ", base::NumberToString16(paste_num_++)}))
            .WaitAndGetTitle();

    auto last_paste = GetLastPaste();
    ASSERT_EQ(last_paste.size(), paste_plain_text ? 1u : 2u);
    EXPECT_EQ(last_paste[0].GetString(), base::StrCat({"text/plain: ", text}));
    if (!paste_plain_text) {
      EXPECT_EQ(last_paste[1].GetString(),
                base::StrCat({"text/html: <span>", text, "</span>"}));
    }
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  // Returns all valid data formats for the last paste.
  base::Value::List GetLastPaste() {
    auto result =
        content::EvalJs(web_contents_.get(),
                        "(function() { return window.getLastPaste(); })();");
    EXPECT_TRUE(result.error.empty());
    auto paste_list_value = result.ExtractList();
    EXPECT_TRUE(paste_list_value.is_list());
    return std::move(paste_list_value).TakeList();
  }

  raw_ptr<content::WebContents, ExperimentalAsh> web_contents_ = nullptr;
  int paste_num_ = 1;
};

IN_PROC_BROWSER_TEST_F(ClipboardHistoryPasteTypeBrowserTest,
                       PlainAndRichTextPastes) {
  using ClipboardHistoryPasteType =
      ash::ClipboardHistoryControllerImpl::ClipboardHistoryPasteType;

  // Confirm initial state.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/0);

  // Write some things to the clipboard.
  SetClipboardTextAndHtml("A", "<span>A</span>");
  SetClipboardTextAndHtml("B", "<span>B</span>");
  SetClipboardTextAndHtml("C", "<span>C</span>");

  // Pasting can result in temporary modification of the clipboard buffer. Cache
  // the buffer's current `clipboard_data` so state can be verified later.
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  ui::ClipboardData clipboard_data(*clipboard->GetClipboardData(&data_dst));

  // Open clipboard history and paste the last history item.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("A", /*paste_plain_text=*/false);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.PasteType",
      ClipboardHistoryPasteType::kRichTextKeystroke,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/1);

  // Wait for the clipboard buffer to be restored before performing another
  // paste. In production, this should happen faster than a user is able to
  // relaunch clipboard history UI (knock on wood).
  ClipboardDataWaiter().WaitFor(&clipboard_data);

  // Open clipboard history and paste the last history item while holding down
  // a non-shift key (arbitrarily, the alt key). The item should not paste as
  // plain text.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN, ui::EF_ALT_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("A", /*paste_plain_text=*/false);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.PasteType",
      ClipboardHistoryPasteType::kRichTextKeystroke,
      /*expected_count=*/2);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/2);

  // Wait for the clipboard buffer to be restored before performing another
  // paste.
  ClipboardDataWaiter().WaitFor(&clipboard_data);

  // Open clipboard history and paste the last history item while holding down
  // the shift key. The item should paste as plain text.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("A", /*paste_plain_text=*/true);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.PasteType",
      ClipboardHistoryPasteType::kPlainTextKeystroke,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/3);

  // Wait for the clipboard buffer to be restored before performing another
  // paste.
  ClipboardDataWaiter().WaitFor(&clipboard_data);

  // Open clipboard history and paste the last history item by toggling the
  // clipboard history menu. The item should not paste as plain text.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/false);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("A", /*paste_plain_text=*/false);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.PasteType",
      ClipboardHistoryPasteType::kRichTextAccelerator,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/4);

  // Wait for the clipboard buffer to be restored before performing another
  // paste.
  ClipboardDataWaiter().WaitFor(&clipboard_data);

  // Open clipboard history and paste the last history item via mouse click. The
  // item should not paste as plain text.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  const auto* menu_item_view =
      GetContextMenu()->GetMenuItemViewAtForTest(/*index=*/2);
  GetEventGenerator()->MoveMouseTo(
      menu_item_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(menu_item_view->IsSelected());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("A", /*paste_plain_text=*/false);
  histogram_tester.ExpectBucketCount("Ash.ClipboardHistory.PasteType",
                                     ClipboardHistoryPasteType::kRichTextMouse,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/5);

  // Wait for the clipboard buffer to be restored before performing another
  // paste.
  ClipboardDataWaiter().WaitFor(&clipboard_data);

  // Open clipboard history and paste the last history item via mouse click
  // while holding down the shift key. The item should paste as plain text.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  menu_item_view = GetContextMenu()->GetMenuItemViewAtForTest(/*index=*/2);
  GetEventGenerator()->MoveMouseTo(
      menu_item_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(menu_item_view->IsSelected());
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(ui::EF_NONE);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("A", /*paste_plain_text=*/true);
  histogram_tester.ExpectBucketCount("Ash.ClipboardHistory.PasteType",
                                     ClipboardHistoryPasteType::kPlainTextMouse,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/6);

  // Wait for the clipboard buffer to be restored before performing another
  // paste.
  ClipboardDataWaiter().WaitFor(&clipboard_data);

  // Open clipboard history and paste the first history item by toggling the
  // clipboard history menu while holding down the shift key. The item should
  // paste as plain text.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_V,
                  ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("C", /*paste_plain_text=*/true);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.PasteType",
      ClipboardHistoryPasteType::kPlainTextAccelerator,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/7);

  // Verify the clipboard buffer is restored to initial state.
  ClipboardDataWaiter().WaitFor(&clipboard_data);
}

// Regression test for crbug.com/1363828 --- verifies that
// `WebContents::Paste()` works, since that's necessary for the html preview.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryPasteTypeBrowserTest, PasteCommand) {
  SetClipboardTextAndHtml("A", "<span>A</span>");
  web_contents()->Paste();
  WaitForWebContentsPaste("A", /*paste_plain_text=*/false);
}

class ClipboardHistoryReorderBrowserTest
    : public ClipboardHistoryPasteTypeBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool /* clipboard_history_reorder_enabled */,
                     bool /* paste_plain_text */>> {
 public:
  ClipboardHistoryReorderBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    (ClipboardHistoryReorderEnabled() ? enabled_features : disabled_features)
        .push_back(ash::features::kClipboardHistoryReorder);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~ClipboardHistoryReorderBrowserTest() override = default;

 protected:
  bool ClipboardHistoryReorderEnabled() { return std::get<0>(GetParam()); }
  bool PastePlainText() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ClipboardHistoryReorderBrowserTest,
    ::testing::Combine(
        /*clipboard_history_reorder_enabled=*/::testing::Bool(),
        /*paste_plain_text=*/::testing::Bool()));

IN_PROC_BROWSER_TEST_P(ClipboardHistoryReorderBrowserTest, OnCopy) {
  // Confirm initial state.
  const auto& clipboard_history_items = GetClipboardItems();
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(clipboard_history_items.empty());
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ReorderType",
                                    /*count=*/0);

  const auto* const clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);

  // Write some data to the clipboard.
  {
    // Start listening for changes to the item list. We must wait for the item
    // list to update before checking verifying the clipboard history state.
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    SetClipboardTextAndHtml("A", "<span>A</span>");
  }
  ui::ClipboardData clipboard_data_a(*clipboard->GetClipboardData(&data_dst));
  ASSERT_EQ(clipboard_history_items.size(), 1u);
  EXPECT_EQ(clipboard_history_items.front().data(), clipboard_data_a);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ReorderType",
                                    /*count=*/0);

  // Write different data to the clipboard.
  {
    // Start listening for changes to the item list. We must wait for the item
    // list to update before checking verifying the clipboard history state.
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    SetClipboardTextAndHtml("B", "<span>B</span>");
  }
  ui::ClipboardData clipboard_data_b(*clipboard->GetClipboardData(&data_dst));
  ASSERT_EQ(clipboard_history_items.size(), 2u);
  EXPECT_EQ(clipboard_history_items.front().data(), clipboard_data_b);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ReorderType",
                                    /*count=*/0);

  // Write the original data to the clipboard again. Instead of creating a new
  // clipboard history item, this should bump the original item to the top slot.
  {
    // Start listening for changes to the item list. We must wait for the item
    // list to update before checking verifying the clipboard history state.
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    SetClipboardTextAndHtml("A", "<span>A</span>");
  }
  ASSERT_EQ(clipboard_history_items.size(), 2u);
  EXPECT_EQ(clipboard_history_items.front().data(), clipboard_data_a);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ReorderType",
      /*sample=*/ash::clipboard_history_util::ReorderType::kOnCopy,
      /*expected_count=*/1);

  // Verify that after the original data is written to the clipboard again, the
  // corresponding clipboard history item's data is updated to have the same
  // sequence number as the new clipboard.
  EXPECT_EQ(clipboard_history_items.front().data().sequence_number_token(),
            clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_NE(clipboard_history_items.front().data().sequence_number_token(),
            clipboard_data_a.sequence_number_token());
}

// Disabled due to flakiness: crbug.com/1385806
IN_PROC_BROWSER_TEST_P(ClipboardHistoryReorderBrowserTest, DISABLED_OnPaste) {
  // Confirm initial state.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ConsecutivePastes",
                                    /*count=*/0);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ReorderType",
                                    /*count=*/0);

  // Write some things to the clipboard. Pasting may result in temporary
  // modification of the clipboard buffer. Cache the clipboard data for each
  // item so state can be verified later.
  const auto* const clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  SetClipboardTextAndHtml("A", "<span>A</span>");
  ui::ClipboardData clipboard_data_a(*clipboard->GetClipboardData(&data_dst));
  SetClipboardTextAndHtml("B", "<span>B</span>");
  ui::ClipboardData clipboard_data_b(*clipboard->GetClipboardData(&data_dst));

  // Open clipboard history and paste the first history item in rich text. This
  // predictable paste helps us verify that nothing is emitted to the
  // `Ash.ClipboardHistory.ConsecutivePastes` histogram if a reordering paste
  // happens next.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  WaitForWebContentsPaste("B", /*paste_plain_text=*/false);

  // Wait for clipboard history metrics to update with the paste's success.
  WaitForOperationConfirmed(/*success_expected=*/true);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ConsecutivePastes",
                                    /*count=*/0);

  // Wait for the clipboard buffer to be restored before performing another
  // paste.
  ClipboardDataWaiter().WaitFor(&clipboard_data_b);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ReorderType",
                                    /*count=*/0);

  // Open clipboard history and paste the last history item.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN,
                  PastePlainText() ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  const auto* const expected_clipboard_data =
      ClipboardHistoryReorderEnabled() ? &clipboard_data_a : &clipboard_data_b;
  {
    // If the paste will reorder clipboard history, start listening for changes
    // to the item list. Ultimately, we must wait for the item list to update
    // before checking whether the reorder was successful. This is only strictly
    // necessary if the paste is also plain text, because in that scenario,
    // clipboard history is reordered in a task posted after the buffer is
    // restored.
    std::unique_ptr<ScopedClipboardHistoryListUpdateWaiter> scoped_waiter;
    if (ClipboardHistoryReorderEnabled()) {
      scoped_waiter =
          std::make_unique<ScopedClipboardHistoryListUpdateWaiter>();
    }

    WaitForWebContentsPaste("A", PastePlainText());

    // Wait for clipboard history metrics to update with the paste's success.
    WaitForOperationConfirmed(/*success_expected=*/true);
    histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ConsecutivePastes",
                                      /*count=*/0);

    // Wait for the clipboard buffer to be restored before verifying results. We
    // expect the buffer to contain the last pasted item's data if clipboard
    // history reordering is enabled, or the buffer's original data if not.
    // Because the waiter times out if it expects the wrong clipboard data, a
    // successful wait verifies that the clipboard correctly reflects the top
    // item of the clipboard history item list.
    ClipboardDataWaiter().WaitFor(expected_clipboard_data);
  }
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ReorderType",
      /*sample=*/ash::clipboard_history_util::ReorderType::kOnPaste,
      /*expected_count=*/ClipboardHistoryReorderEnabled() ? 1 : 0);

  const auto& clipboard_history_items = GetClipboardItems();
  ASSERT_EQ(clipboard_history_items.size(), 2u);
  EXPECT_EQ(clipboard_history_items.front().data(), *expected_clipboard_data);

  SetClipboardTextAndHtml("C", "<span>C</span>");
  histogram_tester.ExpectBucketCount("Ash.ClipboardHistory.ConsecutivePastes",
                                     /*sample=*/2, /*expected_count=*/1);
}

// Verify clipboard history's features in the multiprofile environment.
class ClipboardHistoryMultiProfileBrowserTest
    : public ClipboardHistoryBrowserTest {
 public:
  ClipboardHistoryMultiProfileBrowserTest() {
    login_mixin_.AppendRegularUsers(1);
    // Previous user was added in base class.
    EXPECT_EQ(2u, login_mixin_.users().size());
    account_id2_ = login_mixin_.users()[1].account_id;
  }

  ~ClipboardHistoryMultiProfileBrowserTest() override = default;

 protected:
  AccountId account_id2_;
};

// Verify that the clipboard data history is recorded as expected in the
// Multiuser environment.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryMultiProfileBrowserTest,
                       VerifyClipboardHistoryAcrossMultiUser) {
  EXPECT_TRUE(GetClipboardItems().empty());

  // Store text when the user1 is active.
  const std::string copypaste_data1("user1_text1");
  SetClipboardText(copypaste_data1);

  {
    const std::list<ash::ClipboardHistoryItem>& items = GetClipboardItems();
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(copypaste_data1, items.front().data().text());
  }

  // Log in as the user2. The clipboard history should be non-empty.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  EXPECT_FALSE(GetClipboardItems().empty());

  // Store text when the user2 is active.
  const std::string copypaste_data2("user2_text1");
  SetClipboardText(copypaste_data2);

  {
    const std::list<ash::ClipboardHistoryItem>& items = GetClipboardItems();
    EXPECT_EQ(2u, items.size());
    EXPECT_EQ(copypaste_data2, items.front().data().text());
  }

  // Switch to the user1.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);

  // Store text when the user1 is active.
  const std::string copypaste_data3("user1_text2");
  SetClipboardText(copypaste_data3);

  {
    const std::list<ash::ClipboardHistoryItem>& items = GetClipboardItems();
    EXPECT_EQ(3u, items.size());

    // Note that items in |data| follow the time ordering. The most recent item
    // is always the first one.
    auto it = items.begin();
    EXPECT_EQ(copypaste_data3, it->data().text());

    std::advance(it, 1u);
    EXPECT_EQ(copypaste_data2, it->data().text());

    std::advance(it, 1u);
    EXPECT_EQ(copypaste_data1, it->data().text());
  }
}

// The browser test which creates a widget with a textfield during setting-up
// to help verify the multipaste menu item's response to the gesture tap and
// the mouse click.
class ClipboardHistoryTextfieldBrowserTest
    : public ClipboardHistoryBrowserTest {
 public:
  ClipboardHistoryTextfieldBrowserTest() = default;
  ~ClipboardHistoryTextfieldBrowserTest() override = default;

 protected:
  // ClipboardHistoryBrowserTest:
  void SetUpOnMainThread() override {
    ClipboardHistoryBrowserTest::SetUpOnMainThread();

    CloseAllBrowsers();

    // Create a widget containing a single, focusable textfield.
    widget_ = CreateTestWidget();
    textfield_ = widget_->SetContentsView(std::make_unique<views::Textfield>());
    textfield_->SetAccessibleName(u"Textfield");
    textfield_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    // Show the widget.
    widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    widget_->Show();
    ASSERT_TRUE(widget_->IsActive());

    // Focus the textfield and confirm initial state.
    textfield_->RequestFocus();
    ASSERT_TRUE(textfield_->HasFocus());
    ASSERT_TRUE(textfield_->GetText().empty());
  }

  void PasteFromClipboardHistoryMenuAndWait() {
    ASSERT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
    ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
    PressAndRelease(ui::VKEY_RETURN);
    WaitForOperationConfirmed(/*success_expected=*/true);
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::Textfield, ExperimentalAsh> textfield_ = nullptr;
};

// Verifies that the clipboard history menu responses to the gesture tap
// correctly (https://crbug.com/1142088).
IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       VerifyResponseToGestures) {
  base::HistogramTester histogram_tester;

  SetClipboardText("A");
  SetClipboardText("B");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // Tap at the second menu item view. Verify that "A" is pasted.
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*count=*/0);
  const views::MenuItemView* second_menu_item_view =
      GetMenuItemViewForIndex(/*index=*/1);
  GetEventGenerator()->GestureTapAt(
      second_menu_item_view->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.PasteType",
      ash::ClipboardHistoryControllerImpl::ClipboardHistoryPasteType::
          kRichTextTouch,
      /*expected_bucket_count=*/1);
}

// Verifies that the metric to record the count of the consecutive pastes from
// the clipboard history menu works as expected.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       VerifyConsecutivePasteMetric) {
  base::HistogramTester histogram_tester;

  SetClipboardText("A");
  PasteFromClipboardHistoryMenuAndWait();
  PasteFromClipboardHistoryMenuAndWait();
  SetClipboardText("B");

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ConsecutivePastes",
                                    /*count=*/1);
  histogram_tester.ExpectUniqueSample("Ash.ClipboardHistory.ConsecutivePastes",
                                      /*sample=*/2, /*count=*/1);
}

// Verifies that the delete button should show after its host item view is under
// gesture press for enough long time (https://crbug.com/1147584).
IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       DeleteButtonShowAfterLongPress) {
  SetClipboardText("A");
  SetClipboardText("B");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  ash::ClipboardHistoryItemView* second_item_view =
      GetHistoryItemViewForIndex(/*index=*/1);
  views::View* second_item_delete_button =
      second_item_view->GetViewByID(MenuViewID::kDeleteButtonViewID);
  EXPECT_FALSE(second_item_delete_button->GetVisible());

  // Long press on the second item until its delete button shows.
  GetEventGenerator()->PressTouch(
      second_item_view->GetBoundsInScreen().CenterPoint());
  base::RunLoop run_loop;
  auto subscription = second_item_delete_button->AddVisibleChangedCallback(
      run_loop.QuitClosure());
  run_loop.Run();
  GetEventGenerator()->ReleaseTouch();
  EXPECT_TRUE(second_item_delete_button->GetVisible());

  // Press the up arrow key then press the ENTER key. Verify that the first
  // item's clipboard data is pasted.
  PressAndRelease(ui::KeyboardCode::VKEY_UP, ui::EF_NONE);
  PressAndRelease(ui::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("B", base::UTF16ToUTF8(textfield_->GetText()));
}

IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       ShouldPasteHistoryViaKeyboard) {
  base::HistogramTester histogram_tester;
  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 3);

  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield_->GetText()));
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatPasted", 1);

  textfield_->SetText(std::u16string());
  EXPECT_TRUE(textfield_->GetText().empty());

  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify we can paste the first history item via the COMMAND+V shortcut.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield_->GetText()));
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatPasted", 2);

  textfield_->SetText(std::u16string());
  EXPECT_TRUE(textfield_->GetText().empty());

  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatPasted", 3);

  textfield_->SetText(std::u16string());

  EXPECT_TRUE(textfield_->GetText().empty());

  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatPasted", 4);
}

IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       ShouldPasteHistoryWhileHoldingDownCommandKey) {
  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  // Verify we can traverse clipboard history and paste the first history item
  // while holding down the COMMAND key.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield_->GetText()));
  Release(ui::KeyboardCode::VKEY_COMMAND);

  textfield_->SetText(std::u16string());
  EXPECT_TRUE(textfield_->GetText().empty());

  // Verify we can traverse clipboard history and paste the last history item
  // while holding down the COMMAND key.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_COMMAND_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_COMMAND_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));
  Release(ui::KeyboardCode::VKEY_COMMAND);
}

IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       PasteWithLockedScreen) {
  // Write an item to the clipboard.
  SetClipboardText("A");

  // Verify that the item can be pasted successfully.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));

  // Start a new paste.
  textfield_->SetText(std::u16string());
  EXPECT_TRUE(textfield_->GetText().empty());
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Lock the screen.
  ash::SessionManagerClient::Get()->RequestLockScreen();
  ash::SessionStateWaiter(session_manager::SessionState::LOCKED).Wait();

  // Verify that the item was not pasted.
  WaitForOperationConfirmed(/*success_expected=*/false);
  EXPECT_TRUE(textfield_->GetText().empty());
}

class FakeDataTransferPolicyController
    : public ui::DataTransferPolicyController {
 public:
  FakeDataTransferPolicyController() : allowed_url_(GURL(kUrlString)) {}
  ~FakeDataTransferPolicyController() override = default;

  // ui::DataTransferPolicyController:
  bool IsClipboardReadAllowed(const ui::DataTransferEndpoint* const data_src,
                              const ui::DataTransferEndpoint* const data_dst,
                              const absl::optional<size_t> size) override {
    // The multipaste menu should have access to any clipboard data.
    if (data_dst && data_dst->type() == ui::EndpointType::kClipboardHistory)
      return true;

    // For other data destinations, only the data from `allowed_url_`
    // should be accessible.
    return data_src && data_src->IsUrlType() &&
           (*data_src->GetURL() == allowed_url_);
  }

  void PasteIfAllowed(const ui::DataTransferEndpoint* const data_src,
                      const ui::DataTransferEndpoint* const data_dst,
                      const absl::optional<size_t> size,
                      content::RenderFrameHost* rfh,
                      base::OnceCallback<void(bool)> callback) override {}

  void DropIfAllowed(const ui::OSExchangeData* drag_data,
                     const ui::DataTransferEndpoint* data_dst,
                     base::OnceClosure drop_cb) override {}

 private:
  const GURL allowed_url_;
};

// The browser test equipped with the custom policy controller.
class ClipboardHistoryWithMockDLPBrowserTest
    : public ClipboardHistoryTextfieldBrowserTest {
 public:
  ClipboardHistoryWithMockDLPBrowserTest()
      : data_transfer_policy_controller_(
            std::make_unique<FakeDataTransferPolicyController>()) {}
  ~ClipboardHistoryWithMockDLPBrowserTest() override = default;

  // Write text into the clipboard buffer and it should be inaccessible from
  // the multipaste menu, meaning that the clipboard data item created from
  // the written text cannot be pasted through the multipaste menu.
  void SetClipboardTextWithInaccessibleSrc(const std::string& text) {
    SetClipboardText(text);
  }

  // Write text into the clipboard buffer and it should be accessible from
  // the multipaste menu.
  void SetClipboardTextWithAccessibleSrc(const std::string& text) {
    ui::ScopedClipboardWriter(
        ui::ClipboardBuffer::kCopyPaste,
        std::make_unique<ui::DataTransferEndpoint>((GURL(kUrlString))))
        .WriteText(base::UTF8ToUTF16(text));

    // ClipboardHistory will post a task to process clipboard data in order to
    // debounce multiple clipboard writes occurring in sequence. Here we give
    // ClipboardHistory the chance to run its posted tasks before proceeding.
    WaitForOperationConfirmed(/*success_expected=*/true);
  }

 private:
  std::unique_ptr<FakeDataTransferPolicyController>
      data_transfer_policy_controller_;
};

// Verifies the basic features related to the inaccessible menu item, the one
// whose clipboard data should not be leaked through the multipaste menu.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMockDLPBrowserTest, Basics) {
  SetClipboardTextWithAccessibleSrc("A");
  SetClipboardTextWithInaccessibleSrc("B");
  EXPECT_TRUE(VerifyClipboardTextData({"B", "A"}));

  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify that the text is pasted into `textfield_` after the mouse click at
  // `accessible_menu_item_view`.
  const views::MenuItemView* accessible_menu_item_view =
      GetContextMenu()->GetMenuItemViewAtForTest(/*index=*/1);
  GetEventGenerator()->MoveMouseTo(
      accessible_menu_item_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(accessible_menu_item_view->IsSelected());
  GetEventGenerator()->ClickLeftButton();
  WaitForOperationConfirmed(/*success_expected=*/true);
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));

  // Clear `textfield_`'s contents.
  textfield_->SetText(std::u16string());
  ASSERT_TRUE(textfield_->GetText().empty());

  // Re-show the multipaste menu since the menu is closed after the previous
  // mouse click.
  ASSERT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);

  // Move mouse to `inaccessible_menu_item_view` then click the left button.
  const views::MenuItemView* inaccessible_menu_item_view =
      GetContextMenu()->GetMenuItemViewAtForTest(/*index=*/0);
  GetEventGenerator()->MoveMouseTo(
      inaccessible_menu_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  base::RunLoop().RunUntilIdle();

  // Verify that the text is not pasted and menu is closed after click.
  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
}
