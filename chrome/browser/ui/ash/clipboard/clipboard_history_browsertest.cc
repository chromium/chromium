// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/clipboard/clipboard_history.h"

#include <iterator>
#include <list>
#include <memory>
#include <string_view>
#include <tuple>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/containers/adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/ash/clipboard/clipboard_history_test_util.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
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

  raw_ptr<const ui::ClipboardData> clipboard_data_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::Widget> CreateTestWidget(
    views::Widget::InitParams::Ownership ownership) {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params(
      ownership, views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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

// Returns the clipboard history item at the specified `index`, which is assumed
// to exist in the clipboard history list.
const ash::ClipboardHistoryItem& GetClipboardItemAt(size_t index) {
  const auto& items = GetClipboardItems();
  CHECK_LT(index, items.size());
  auto items_iter = items.begin();
  std::advance(items_iter, index);
  return *items_iter;
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

  // Returns the logged-in user's profile.
  Profile* GetProfile() {
    return Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id1_));
  }

  // Click at the delete button of the clipboard history item at the specified
  // `index`.
  void ClickAtDeleteButton(size_t index) {
    const auto* const item_view =
        GetMenuItemViewForClipboardHistoryItemAtIndex(index);
    const auto* const delete_button =
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
    EXPECT_EQ(
        delete_button->GetViewAccessibility().GetCachedName(),
        l10n_util::GetStringFUTF16(IDS_CLIPBOARD_HISTORY_DELETE_ITEM_TEXT,
                                   GetClipboardItemAt(index).display_text()));
    GetEventGenerator()->ClickLeftButton();
  }

  void Press(ui::KeyboardCode key, int modifiers = ui::EF_NONE) {
    event_generator_->PressKeyAndModifierKeys(key, modifiers);
  }

  void Release(ui::KeyboardCode key, int modifiers = ui::EF_NONE) {
    event_generator_->ReleaseKeyAndModifierKeys(key, modifiers);
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

  // Returns the menu item view corresponding to the item at the given `index`
  // in the clipboard history list.
  const views::MenuItemView* GetMenuItemViewForClipboardHistoryItemAtIndex(
      size_t index) const {
    if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      // Skip the header.
      ++index;
    }
    return GetContextMenu()->GetMenuItemViewAtForTest(index);
  }

  views::MenuItemView* GetMenuItemViewForClipboardHistoryItemAtIndex(
      size_t index) {
    return const_cast<views::MenuItemView*>(
        const_cast<const ClipboardHistoryBrowserTest*>(this)
            ->GetMenuItemViewForClipboardHistoryItemAtIndex(index));
  }

  // Get the view for the clipboard history item at the specified `index`.
  const ash::ClipboardHistoryItemView* GetHistoryItemViewForIndex(
      size_t index) const {
    const views::MenuItemView* hosting_menu_item =
        GetMenuItemViewForClipboardHistoryItemAtIndex(index);
    EXPECT_EQ(hosting_menu_item->children().size(), 1u);
    return static_cast<const ash::ClipboardHistoryItemView*>(
        hosting_menu_item->children()[0]);
  }

  ash::ClipboardHistoryItemView* GetHistoryItemViewForIndex(size_t index) {
    return const_cast<ash::ClipboardHistoryItemView*>(
        const_cast<const ClipboardHistoryBrowserTest*>(this)
            ->GetHistoryItemViewForIndex(index));
  }

  // Show the delete button by hovering the mouse over the clipboard history
  // item at the specified `index`.
  void ShowDeleteButtonByMouseHover(size_t index) {
    auto* item_view = GetMenuItemViewForClipboardHistoryItemAtIndex(index);
    views::View* delete_button =
        item_view->GetViewByID(MenuViewID::kDeleteButtonViewID);
    ASSERT_FALSE(delete_button->GetVisible());

    // Hover the mouse on `item_view` to show the delete button.
    GetEventGenerator()->MoveMouseTo(
        item_view->GetBoundsInScreen().CenterPoint(), /*count=*/5);

    // Wait until `delete_button` has meaningful bounds. Note that the bounds
    // are set by the layout manager asynchronously.
    ui_test_utils::ViewBoundsWaiter delete_button_waiter(delete_button);
    delete_button_waiter.WaitForNonEmptyBounds();

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
      scw.WriteHTML(base::UTF8ToUTF16(html), /*source_url=*/"");
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
};

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
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u);
  const views::MenuItemView* const second_menu_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/1u);
  const ash::ClipboardHistoryItemView* const first_history_item_view =
      GetHistoryItemViewForIndex(/*index=*/0u);
  const ash::ClipboardHistoryItemView* const second_history_item_view =
      GetHistoryItemViewForIndex(/*index=*/1u);

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
  const auto* const first_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u);
  GetEventGenerator()->MoveMouseTo(
      first_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();

  // Move the mouse to the second menu item then release.
  const auto* const second_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/1u);
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
    ClickAtDeleteButton(/*index=*/1u);
  }
  EXPECT_EQ(1u, GetContextMenu()->GetMenuItemsCount());
  EXPECT_TRUE(VerifyClipboardTextData({"B"}));
  EXPECT_TRUE(VerifyClipboardBufferAndHistoryInSync());

  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatDeleted", 1);

  // Delete the last menu item. Verify that the menu is closed.
  {
    ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    ClickAtDeleteButton(/*index=*/0u);
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

IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest, ReorderOnCopy) {
  // Confirm initial state.
  const auto& clipboard_history_items = GetClipboardItems();
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(clipboard_history_items.empty());
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ReorderType",
                                    /*expected_count=*/0);

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
                                    /*expected_count=*/0);

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
                                    /*expected_count=*/0);

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

IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest, AccessibleProperties) {
  SetClipboardText("A");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ui::AXNodeData data;

  GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kMenuItem);
}

IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest,
                       ItemViewAccessibleSelectionState) {
  SetClipboardText("A");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  const ash::ClipboardHistoryItemView* const history_item_view =
      GetHistoryItemViewForIndex(/*index=*/0u);

  // Verify initial selection state
  ui::AXNodeData node_data;
  history_item_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Move Pseudo focus away Main Button.
  PressAndRelease(ui::VKEY_TAB);
  node_data = ui::AXNodeData();
  history_item_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Move Pseudo focus back to Main Button.
  PressAndRelease(ui::VKEY_TAB);
  node_data = ui::AXNodeData();
  history_item_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

class ClipboardHistoryPasteTypeBrowserTest
    : public ClipboardHistoryBrowserTest {
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
    auto* browser = CreateBrowser(GetProfile());
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
  void WaitForWebContentsPaste(std::string_view text, bool paste_plain_text) {
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

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
  int paste_num_ = 1;
};

IN_PROC_BROWSER_TEST_F(ClipboardHistoryPasteTypeBrowserTest,
                       PlainAndRichTextPastes) {
  using ClipboardHistoryPasteType =
      ash::ClipboardHistoryControllerImpl::ClipboardHistoryPasteType;

  // Confirm initial state.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                    /*expected_count=*/0);

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
  {
    SCOPED_TRACE("Paste by pressing Enter.");
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
                                      /*expected_count=*/1);

    // Wait for the clipboard buffer to be restored before performing another
    // paste. In production, this should happen faster than a user is able to
    // relaunch clipboard history UI (knock on wood).
    ClipboardDataWaiter().WaitFor(&clipboard_data);
  }

  // Open clipboard history and paste the last history item while holding down
  // a non-shift key (arbitrarily, the alt key). The item should not paste as
  // plain text.
  {
    SCOPED_TRACE("Paste by pressing Enter with a non-shift key.");
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
                                      /*expected_count=*/2);

    // Wait for the clipboard buffer to be restored before performing another
    // paste.
    ClipboardDataWaiter().WaitFor(&clipboard_data);
  }

  // Open clipboard history and paste the last history item while holding down
  // the shift key. The item should paste as plain text.
  {
    SCOPED_TRACE("Paste by pressing Shift+Enter.");
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
                                      /*expected_count=*/3);

    // Wait for the clipboard buffer to be restored before performing another
    // paste.
    ClipboardDataWaiter().WaitFor(&clipboard_data);
  }

  // Open clipboard history and paste the last history item by toggling the
  // clipboard history menu. The item should not paste as plain text.
  {
    SCOPED_TRACE("Paste by pressing Search/Launcher+V.");
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
                                      /*expected_count=*/4);

    // Wait for the clipboard buffer to be restored before performing another
    // paste.
    ClipboardDataWaiter().WaitFor(&clipboard_data);
  }

  const views::MenuItemView* menu_item_view = nullptr;

  // Open clipboard history and paste the last history item via mouse click. The
  // item should not paste as plain text.
  {
    SCOPED_TRACE("Paste by clicking with the mouse.");
    ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
    EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
    menu_item_view =
        GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/2u);
    GetEventGenerator()->MoveMouseTo(
        menu_item_view->GetBoundsInScreen().CenterPoint());
    ASSERT_TRUE(menu_item_view->IsSelected());
    GetEventGenerator()->ClickLeftButton();
    EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

    WaitForWebContentsPaste("A", /*paste_plain_text=*/false);
    histogram_tester.ExpectBucketCount(
        "Ash.ClipboardHistory.PasteType",
        ClipboardHistoryPasteType::kRichTextMouse,
        /*expected_count=*/1);
    histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                      /*expected_count=*/5);

    // Wait for the clipboard buffer to be restored before performing another
    // paste.
    ClipboardDataWaiter().WaitFor(&clipboard_data);
  }

  // Open clipboard history and paste the last history item via mouse click
  // while holding down the shift key. The item should paste as plain text.
  {
    SCOPED_TRACE("Paste by clicking with the mouse with Shift held.");
    ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
    EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
    menu_item_view =
        GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/2u);
    GetEventGenerator()->MoveMouseTo(
        menu_item_view->GetBoundsInScreen().CenterPoint());
    ASSERT_TRUE(menu_item_view->IsSelected());
    GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
    GetEventGenerator()->ClickLeftButton();
    GetEventGenerator()->set_flags(ui::EF_NONE);
    EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

    WaitForWebContentsPaste("A", /*paste_plain_text=*/true);
    histogram_tester.ExpectBucketCount(
        "Ash.ClipboardHistory.PasteType",
        ClipboardHistoryPasteType::kPlainTextMouse,
        /*expected_count=*/1);
    histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                      /*expected_count=*/6);

    // Wait for the clipboard buffer to be restored before performing another
    // paste.
    ClipboardDataWaiter().WaitFor(&clipboard_data);
  }

  // Open clipboard history and paste the first history item by toggling the
  // clipboard history menu while holding down the shift key. The item should
  // paste as plain text.
  {
    SCOPED_TRACE("Paste by pressing Shift+Search/Launcher+V.");
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
                                      /*expected_count=*/7);

    // Verify the clipboard buffer is restored to initial state.
    ClipboardDataWaiter().WaitFor(&clipboard_data);
  }

  // Open clipboard history and paste the first history item by pressing Ctrl+V.
  // The item should not paste as plain text.
  {
    SCOPED_TRACE("Paste by pressing Ctrl+V.");
    ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
    EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
    PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_CONTROL_DOWN);
    EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

    WaitForWebContentsPaste("C", /*paste_plain_text=*/false);
    histogram_tester.ExpectBucketCount(
        "Ash.ClipboardHistory.PasteType",
        ClipboardHistoryPasteType::kRichTextCtrlV,
        /*expected_count=*/1);
    histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                      /*expected_count=*/8);

    // Note: No buffer restoration needs to happen after the above paste.
  }

  // Open clipboard history and paste the first history item by pressing Ctrl+V
  // while holding down the shift key. The item should paste as plain text.
  {
    SCOPED_TRACE("Paste by pressing Shift+Ctrl+V.");
    ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
    EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

    // Remove the menu's first item to verify that pasting via Ctrl+V works even
    // when the first item has changed since the menu was shown.
    {
      ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
      PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
    }
    ui::ClipboardData new_clipboard_data(
        *clipboard->GetClipboardData(&data_dst));

    PressAndRelease(ui::KeyboardCode::VKEY_V,
                    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
    EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

    WaitForWebContentsPaste("B", /*paste_plain_text=*/true);
    histogram_tester.ExpectBucketCount(
        "Ash.ClipboardHistory.PasteType",
        ClipboardHistoryPasteType::kPlainTextCtrlV,
        /*expected_count=*/1);
    histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteType",
                                      /*expected_count=*/9);

    // Verify the clipboard buffer is restored to initial state.
    ClipboardDataWaiter().WaitFor(&new_clipboard_data);
  }
}

// Regression test for crbug.com/1363828 --- verifies that
// `WebContents::Paste()` works, since that's necessary for the html preview.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryPasteTypeBrowserTest, PasteCommand) {
  SetClipboardTextAndHtml("A", "<span>A</span>");
  web_contents()->Paste();
  WaitForWebContentsPaste("A", /*paste_plain_text=*/false);
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
 protected:
  // ClipboardHistoryBrowserTest:
  void SetUpOnMainThread() override {
    ClipboardHistoryBrowserTest::SetUpOnMainThread();

    CloseAllBrowsers();

    // Create a widget containing a single, focusable textfield.
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    textfield_ = widget_->SetContentsView(std::make_unique<views::Textfield>());
    textfield_->GetViewAccessibility().SetName(u"Textfield");
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
  raw_ptr<views::Textfield> textfield_ = nullptr;
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
                                    /*expected_count=*/0);
  const views::MenuItemView* second_menu_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/1u);
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
                                    /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample("Ash.ClipboardHistory.ConsecutivePastes",
                                      /*sample=*/2,
                                      /*expected_bucket_count=*/1);
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
  bool IsClipboardReadAllowed(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      const std::optional<size_t> size) override {
    // The multipaste menu should have access to any clipboard data.
    if (data_dst.has_value() &&
        data_dst->type() == ui::EndpointType::kClipboardHistory) {
      return true;
    }

    // For other data destinations, only the data from `allowed_url_`
    // should be accessible.
    return data_src.has_value() && data_src->IsUrlType() &&
           (*data_src->GetURL() == allowed_url_);
  }

  void PasteIfAllowed(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst,
      absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
      content::RenderFrameHost* rfh,
      base::OnceCallback<void(bool)> callback) override {}

  void DropIfAllowed(std::optional<ui::DataTransferEndpoint> data_src,
                     std::optional<ui::DataTransferEndpoint> data_dst,
                     std::optional<std::vector<ui::FileInfo>> filenames,
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
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/1u);
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
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u);
  GetEventGenerator()->MoveMouseTo(
      inaccessible_menu_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  base::RunLoop().RunUntilIdle();

  // Verify that the text is not pasted and menu is closed after click.
  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
}

// The test base used to check the clipboard history refresh feature on an Ash
// browser.
class ClipboardHistoryRefreshAshBrowserTest
    : public ClipboardHistoryBrowserTest,
      public testing::WithParamInterface</*is_refresh_enabled=*/bool> {
 public:
  ClipboardHistoryRefreshAshBrowserTest() {
    // Enable/disable the clipboard history refresh feature based on the param.
    std::vector<base::test::FeatureRef> refresh_features = {
        chromeos::features::kClipboardHistoryRefresh,
        chromeos::features::kJelly};
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (GetParam() ? enabled_features : disabled_features).swap(refresh_features);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryRefreshAshBrowserTest,
                         /*is_refresh_enabled=*/testing::Bool());

// Checks that the clipboard history submenu model of the render view context
// menu works as expected.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshAshBrowserTest,
                       RenderViewContextMenu) {
  // Ensure the render view context menu has the clipboard history menu option.
  content::ContextMenuParams context_menu_params;
  context_menu_params.is_editable = true;

  // Create a browser.
  auto* browser = CreateBrowser(GetProfile());

  const bool is_refresh_enabled =
      chromeos::features::IsClipboardHistoryRefreshEnabled();

  {
    TestRenderViewContextMenu menu(*browser->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame(),
                                   context_menu_params);
    menu.Init();
    std::optional<size_t> found_index = menu.menu_model().GetIndexOfCommandId(
        is_refresh_enabled ? IDC_CONTENT_PASTE_FROM_CLIPBOARD
                           : IDC_CONTENT_CLIPBOARD_HISTORY_MENU);
    ASSERT_TRUE(found_index);

    // The clipboard history menu option should be disabled if clipboard history
    // is empty.
    EXPECT_FALSE(menu.menu_model().IsEnabledAt(*found_index));
  }

  // Write some clipboard data.
  SetClipboardText("A");
  SetClipboardText("B");

  {
    TestRenderViewContextMenu menu(*browser->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame(),
                                   context_menu_params);
    menu.Init();
    const ui::SimpleMenuModel& menu_model = menu.menu_model();
    std::optional<size_t> found_index = menu_model.GetIndexOfCommandId(
        is_refresh_enabled ? IDC_CONTENT_PASTE_FROM_CLIPBOARD
                           : IDC_CONTENT_CLIPBOARD_HISTORY_MENU);
    ASSERT_TRUE(found_index);

    // The clipboard history menu option should be enabled since clipboard
    // history is non-empty.
    EXPECT_TRUE(menu_model.IsEnabledAt(*found_index));

    if (is_refresh_enabled) {
      // The clipboard history menu option is a submenu if the clipboard history
      // refresh feature is enabled.
      EXPECT_EQ(menu_model.GetTypeAt(*found_index),
                ui::MenuModel::TYPE_SUBMENU);

      ui::MenuModel* submenu_model = menu_model.GetSubmenuModelAt(*found_index);
      ASSERT_TRUE(submenu_model);

      // Check the submenu model contents.
      ASSERT_EQ(submenu_model->GetItemCount(), 3u);
      EXPECT_EQ(submenu_model->GetLabelAt(0), u"B");
      EXPECT_EQ(submenu_model->GetLabelAt(1), u"A");
      EXPECT_EQ(submenu_model->GetLabelAt(2),
                l10n_util::GetStringUTF16(
                    IDS_CONTEXT_MENU_SHOW_CLIPBOARD_HISTORY_MENU));
    } else {
      // The clipboard history menu option is a command item if the feature is
      // not enabled.
      EXPECT_EQ(menu_model.GetTypeAt(*found_index),
                ui::MenuModel::TYPE_COMMAND);
    }
  }
}

// Checks that launching the standalone clipboard history menu from a render
// view's context menu works as expected.
// TODO(crbug.com/333463820): Flaky test. Re-enable once the root cause is
// identified.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshAshBrowserTest,
                       DISABLED_LaunchStandaloneMenuFromRenderViewContextMenu) {
  // Write some clipboard data.
  SetClipboardText("A");
  SetClipboardText("B");

  // Create a browser and cache its active web contents.
  auto* browser = CreateBrowser(GetProfile());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Navigate to a web page with textfield.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(R"(data:text/html,
    <!DOCTYPE html>
    <html>
      <body>
        <script type='text/javascript'>
          function getTextfieldCenterOnPage() {
            const rect = document.getElementById('text_input').
                getBoundingClientRect();
            return [(rect.left + rect.right)/2, (rect.top + rect.bottom)/2];
          }
        </script>
        <input type='text' id='text_input'/>
      </body>
    </html>
  )")));

  // Get the textfield center in the the web contents coordinates.
  auto result = content::EvalJs(web_contents, "getTextfieldCenterOnPage();");
  ASSERT_TRUE(result.error.empty());
  auto value = result.ExtractList();
  ASSERT_TRUE(value.is_list());
  const base::Value::List center_as_list = std::move(value).TakeList();
  ASSERT_EQ(center_as_list.size(), 2u);

  // Calculate the textfield center in the screen coordinates. Then right click
  // at the textfield center.
  gfx::Point textfield_center_in_screen =
      web_contents->GetContainerBounds().origin();
  textfield_center_in_screen.Offset(center_as_list.begin()->GetDouble(),
                                    center_as_list.back().GetDouble());
  GetEventGenerator()->MoveMouseTo(textfield_center_in_screen);
  GetEventGenerator()->ClickRightButton();

  // If the clipboard history refresh feature is enabled, show the submenu.
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    // Expect the menu item that hosts the clipboard history submenu exists.
    const views::MenuItemView* const submenu_item =
        ash::WaitForMenuItemWithLabel(
            l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_PASTE_FROM_CLIPBOARD));
    ASSERT_TRUE(submenu_item);

    // Mouse hover on `submenu_item`. Wait until the submenu shows.
    base::HistogramTester submenu_histogram_tester;
    GetEventGenerator()->MoveMouseTo(
        submenu_item->GetBoundsInScreen().CenterPoint());
    views::View* const submenu_view = submenu_item->GetSubmenu();
    ash::ViewDrawnWaiter().Wait(submenu_view);

    // Verify that the submenu source is recorded as expected when
    // `submenu_view` shows.
    submenu_histogram_tester.ExpectUniqueSample(
        "Ash.ClipboardHistory.ContextMenu.ShowMenu",
        crosapi::mojom::ClipboardHistoryControllerShowSource::
            kRenderViewContextSubmenu,
        1);
  }

  // Expect that the menu option to launch the clipboard history menu exists.
  const views::View* const menu_item = ash::WaitForMenuItemWithLabel(
      l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SHOW_CLIPBOARD_HISTORY_MENU));
  ASSERT_TRUE(menu_item);

  // Left mouse click at `menu_item`. The standalone clipboard history menu
  // should show.
  base::HistogramTester histogram_tester;
  GetEventGenerator()->MoveMouseTo(
      menu_item->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // The source of the standalone clipboard history menu should be recorded.
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.ContextMenu.ShowMenu",
      crosapi::mojom::ClipboardHistoryControllerShowSource::
          kRenderViewContextMenu,
      1);
}

// Verifies the clipboard history menu response to mouse and arrow key inputs.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshAshBrowserTest,
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
  // shows. Its delete button should not show, so the contents should not be
  // clipped.
  const views::MenuItemView* const first_menu_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u);
  EXPECT_TRUE(first_menu_item_view->IsSelected());
  const auto* const first_history_item_view =
      GetHistoryItemViewForIndex(/*index=*/0u);
  EXPECT_FALSE(
      first_history_item_view->GetViewByID(MenuViewID::kDeleteButtonViewID)
          ->GetVisible());
  EXPECT_TRUE(first_history_item_view->GetViewByID(MenuViewID::kContentsViewID)
                  ->clip_path()
                  .isEmpty());

  // Move the mouse to the second menu item.
  const views::MenuItemView* const second_menu_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/1u);
  EXPECT_FALSE(second_menu_item_view->IsSelected());
  GetEventGenerator()->MoveMouseTo(
      second_menu_item_view->GetBoundsInScreen().CenterPoint());

  // The first menu item should not be selected while the second one should be.
  EXPECT_FALSE(first_menu_item_view->IsSelected());
  EXPECT_TRUE(second_menu_item_view->IsSelected());

  // Under mouse hovering, the second item's delete button should show. If the
  // clipboard history refresh is enabled, the contents should be clipped.
  const auto* const second_history_item_view =
      GetHistoryItemViewForIndex(/*index=*/1u);
  EXPECT_TRUE(
      second_history_item_view->GetViewByID(MenuViewID::kDeleteButtonViewID)
          ->GetVisible());
  EXPECT_NE(second_history_item_view->GetViewByID(MenuViewID::kContentsViewID)
                ->clip_path()
                .isEmpty(),
            chromeos::features::IsClipboardHistoryRefreshEnabled());

  // Move the selection to the third item by pressing the arrow key.
  const views::MenuItemView* const third_menu_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/2u);
  EXPECT_FALSE(third_menu_item_view->IsSelected());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);

  // The third item should be selected. Its delete button should not show, so
  // the contents should not be clipped.
  EXPECT_FALSE(second_menu_item_view->IsSelected());
  EXPECT_TRUE(third_menu_item_view->IsSelected());
  const auto* const third_history_item_view =
      GetHistoryItemViewForIndex(/*index=*/2u);
  EXPECT_FALSE(
      third_history_item_view->GetViewByID(MenuViewID::kDeleteButtonViewID)
          ->GetVisible());
  EXPECT_TRUE(third_history_item_view->GetViewByID(MenuViewID::kContentsViewID)
                  ->clip_path()
                  .isEmpty());
}

// Verifies tab traversal behavior when there is only one item in clipboard
// history.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshAshBrowserTest,
                       VerifySingleItemTabTraversal) {
  SetClipboardText("A");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);

  // Verify the default state right after the menu shows.
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(1u, GetContextMenu()->GetMenuItemsCount());

  const views::MenuItemView* const menu_item_view =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u);
  const ash::ClipboardHistoryItemView* const history_item_view =
      GetHistoryItemViewForIndex(/*index=*/0u);

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
  // button is focused. If the clipboard history refresh is enabled, the delete
  // button's visibility should cause the contents to be clipped.
  const views::View* const delete_button =
      history_item_view->GetViewByID(MenuViewID::kDeleteButtonViewID);
  const views::View* const contents_view =
      history_item_view->GetViewByID(MenuViewID::kContentsViewID);
  EXPECT_TRUE(delete_button->GetVisible());
  EXPECT_TRUE(views::InkDrop::Get(const_cast<views::View*>(delete_button))
                  ->GetInkDrop()
                  ->IsHighlightFadingInOrVisible());
  EXPECT_NE(contents_view->clip_path().isEmpty(),
            chromeos::features::IsClipboardHistoryRefreshEnabled());

  // Press the Tab key. Verify that the history item's pseudo focus moves from
  // the delete button back to the main button and the delete button stops being
  // visible. The contents view should not be clipped.
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(menu_item_view->IsSelected());
  EXPECT_TRUE(history_item_view->IsMainButtonPseudoFocused());
  EXPECT_FALSE(history_item_view->IsDeleteButtonPseudoFocused());
  EXPECT_FALSE(delete_button->GetVisible());
  EXPECT_TRUE(contents_view->clip_path().isEmpty());
}

// Verifies that the delete button should show after its host item view is under
// gesture press for enough long time (https://crbug.com/1147584).
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshAshBrowserTest,
                       DeleteButtonShowAfterLongPress) {
  SetClipboardText("A");
  SetClipboardText("B");
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/true);
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  ash::ClipboardHistoryItemView* second_item_view =
      GetHistoryItemViewForIndex(/*index=*/1u);
  views::View* second_item_delete_button =
      second_item_view->GetViewByID(MenuViewID::kDeleteButtonViewID);
  const views::View* const second_item_contents_view =
      second_item_view->GetViewByID(MenuViewID::kContentsViewID);
  EXPECT_FALSE(second_item_delete_button->GetVisible());
  EXPECT_TRUE(second_item_contents_view->clip_path().isEmpty());

  // Long press on the second item until its delete button shows.
  GetEventGenerator()->PressTouch(
      second_item_view->GetBoundsInScreen().CenterPoint());
  base::RunLoop run_loop;
  auto subscription = second_item_delete_button->AddVisibleChangedCallback(
      run_loop.QuitClosure());
  run_loop.Run();
  GetEventGenerator()->ReleaseTouch();
  EXPECT_TRUE(second_item_delete_button->GetVisible());
  EXPECT_NE(second_item_contents_view->clip_path().isEmpty(),
            chromeos::features::IsClipboardHistoryRefreshEnabled());
}

// Base class for tests exercising the `ClipboardHistoryUrlTitleFetcher`'s
// end-to-end functionality, parameterized by whether the clipboard history URL
// titles feature is enabled.
class ClipboardHistoryUrlTitleFetcherBrowserTest
    : public ClipboardHistoryBrowserTest,
      public testing::WithParamInterface</*enable_url_titles=*/bool> {
 public:
  ClipboardHistoryUrlTitleFetcherBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{chromeos::features::kClipboardHistoryRefresh,
          IsClipboardHistoryUrlTitlesEnabled()},
         {ash::features::kClipboardHistoryUrlTitles,
          IsClipboardHistoryUrlTitlesEnabled()},
         {chromeos::features::kJelly, IsClipboardHistoryUrlTitlesEnabled()}});
  }

 protected:
  GURL GetTestUrl(std::string_view base_name) {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(base_name));
  }

  std::vector<GURL> GetHistoryContents() {
    ui_test_utils::HistoryEnumerator enumerator(GetProfile());
    return enumerator.urls();
  }

  bool IsClipboardHistoryUrlTitlesEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryUrlTitleFetcherBrowserTest,
                         /*enable_url_titles=*/testing::Bool());

// Verifies that if the clipboard history URL titles feature is enabled and the
// user copies a URL they have visited before, then the clipboard history item
// will show that page's title.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryUrlTitleFetcherBrowserTest, UrlTitles) {
  const auto unvisited_url = GetTestUrl("title1.html");
  const auto visited_url = GetTestUrl("title2.html");
  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());

  // Populate the primary user's browsing history with a URL.
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS));
  EXPECT_TRUE(GetHistoryContents().empty());

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(CreateBrowser(GetProfile()), visited_url));
  WaitForHistoryBackendToRun(GetProfile());

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(visited_url.spec(), urls[0].spec());

  // Verify that copying the unvisited URL produces a clipboard history item
  // with no URL title.
  SetClipboardText(unvisited_url.spec());
  ASSERT_EQ(GetClipboardItems().size(), 1u);
  EXPECT_FALSE(GetClipboardItems().front().secondary_display_text());

  // Show the clipboard history menu and verify that the unvisited URL's item
  // has no title label.
  event_generator.PressAndReleaseKeyAndModifierKeys(ui::VKEY_V,
                                                    ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetMenuItemViewForClipboardHistoryItemAtIndex(0u)->GetViewByID(
      ash::clipboard_history_util::kSecondaryDisplayTextLabelID));
  event_generator.PressAndReleaseKey(ui::VKEY_ESCAPE);

  // Verify that copying the visited URL produces a clipboard history item with
  // a URL title iff the clipboard history URL titles feature is enabled.
  SetClipboardText(visited_url.spec());
  ASSERT_EQ(GetClipboardItems().size(), 2u);
  EXPECT_EQ(!!GetClipboardItems().front().secondary_display_text(),
            IsClipboardHistoryUrlTitlesEnabled());

  // Show the clipboard history menu and verify that the visited URL's item has
  // a title label iff the clipboard history URL titles feature is enabled.
  event_generator.PressAndReleaseKeyAndModifierKeys(ui::VKEY_V,
                                                    ui::EF_COMMAND_DOWN);
  EXPECT_EQ(!!GetMenuItemViewForClipboardHistoryItemAtIndex(0u)->GetViewByID(
                ash::clipboard_history_util::kSecondaryDisplayTextLabelID),
            IsClipboardHistoryUrlTitlesEnabled());
  event_generator.PressAndReleaseKey(ui::VKEY_ESCAPE);
}

// Base class used to test features that only exist when the Ctrl+V longpress
// feature is enabled.
class ClipboardHistoryLongpressEnabledBrowserTest
    : public ClipboardHistoryTextfieldBrowserTest {
 public:
  ClipboardHistoryLongpressEnabledBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kClipboardHistoryLongpress);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that clicking the clipboard history menu's footer does nothing and
// that tab and arrow key traversal pass over the footer.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryLongpressEnabledBrowserTest,
                       FooterNotInteractive) {
  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");

  // Show the clipboard history menu via the Ctrl+V long-press shortcut so that
  // the menu's educational footer shows.
  EXPECT_TRUE(GetClipboardHistoryController()->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE,
      crosapi::mojom::ClipboardHistoryControllerShowSource::
          kControlVLongpress));
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify that the menu has two clipboard history items and a third item (the
  // menu footer). If the clipboard history refresh is enabled, a fourth item
  // (the menu header) will also be present.
  const bool is_refresh_enabled =
      chromeos::features::IsClipboardHistoryRefreshEnabled();
  const auto* menu = GetClipboardHistoryController()->context_menu_for_test();
  EXPECT_EQ(menu->GetMenuItemsCount(), 2u);
  ASSERT_EQ(menu->GetModelForTest()->GetItemCount(),
            is_refresh_enabled ? 4u : 3u);

  // Verify that clicking on the footer does nothing.
  EXPECT_TRUE(textfield_->GetText().empty());
  const auto* footer = menu->GetMenuItemViewAtForTest(
      /*index=*/is_refresh_enabled ? 3u : 2u);
  GetEventGenerator()->MoveMouseTo(footer->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(textfield_->GetText().empty());

  // Verify that traversing over the menu with arrow keys skips the footer.
  const auto* item1 =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u);
  const auto* item2 =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/1u);
  PressAndRelease(ui::VKEY_DOWN);
  EXPECT_TRUE(item1->IsSelected());
  PressAndRelease(ui::VKEY_DOWN);
  EXPECT_TRUE(item2->IsSelected());
  PressAndRelease(ui::VKEY_DOWN);
  EXPECT_TRUE(item1->IsSelected());

  // Verify that traversing over the menu with the Tab key (two presses at a
  // time for each item's main button and delete button) skips the footer.
  PressAndRelease(ui::VKEY_TAB);
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(item2->IsSelected());
  PressAndRelease(ui::VKEY_TAB);
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(item1->IsSelected());
}

// Base class used to test features that only exist when the UI refresh is
// enabled.
class ClipboardHistoryRefreshEnabledBrowserTest
    : public ClipboardHistoryTextfieldBrowserTest {
 public:
  ClipboardHistoryRefreshEnabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kClipboardHistoryRefresh,
         chromeos::features::kJelly},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that clicking the clipboard history menu's header/footer does
// nothing, and that tab and arrow key traversal passes over the header/footer.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryRefreshEnabledBrowserTest,
                       HeaderAndFooterNotInteractive) {
  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");

  // Show the clipboard history menu and verify that the menu has a header, a
  // footer, and two clipboard history items.
  ShowContextMenuViaAccelerator(/*wait_for_selection=*/false);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  const auto* const menu =
      GetClipboardHistoryController()->context_menu_for_test();
  ASSERT_TRUE(menu);
  EXPECT_EQ(menu->GetMenuItemsCount(), 2u);
  ASSERT_EQ(menu->GetModelForTest()->GetItemCount(), 4u);

  // Verify that clicking on the header does nothing.
  EXPECT_TRUE(textfield_->GetText().empty());
  const auto* const header = menu->GetMenuItemViewAtForTest(/*index=*/0u);
  ASSERT_TRUE(header);
  GetEventGenerator()->MoveMouseTo(header->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(textfield_->GetText().empty());
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify that clicking on the footer does nothing.
  EXPECT_TRUE(textfield_->GetText().empty());
  const auto* const footer = menu->GetMenuItemViewAtForTest(/*index=*/3u);
  ASSERT_TRUE(footer);
  GetEventGenerator()->MoveMouseTo(footer->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(textfield_->GetText().empty());
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify traversing over the menu with arrow keys skips the header/footer.
  const auto* const item1 =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/0u);
  const auto* const item2 =
      GetMenuItemViewForClipboardHistoryItemAtIndex(/*index=*/1u);
  PressAndRelease(ui::VKEY_DOWN);
  EXPECT_TRUE(item1->IsSelected());
  PressAndRelease(ui::VKEY_DOWN);
  EXPECT_TRUE(item2->IsSelected());
  PressAndRelease(ui::VKEY_DOWN);
  EXPECT_TRUE(item1->IsSelected());

  // Verify traversing over the menu with the Tab key (two presses at a time for
  // each item's main button and delete button) skips the header/footer.
  PressAndRelease(ui::VKEY_TAB);
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(item2->IsSelected());
  PressAndRelease(ui::VKEY_TAB);
  PressAndRelease(ui::VKEY_TAB);
  EXPECT_TRUE(item1->IsSelected());
}
