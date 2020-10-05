// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/shell.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/clipboard/clipboard_dlp_controller.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"

namespace {

constexpr char kUrlString[] = "https://www.example.com";

std::unique_ptr<views::Widget> CreateTestWidget() {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget->Init(std::move(params));

  return widget;
}

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

void SetClipboardText(const std::string& text) {
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(text));

  // ClipboardHistory will post a task to process clipboard data in order to
  // debounce multiple clipboard writes occurring in sequence. Here we give
  // ClipboardHistory the chance to run its posted tasks before proceeding.
  FlushMessageLoop();
}

void SetClipboardTextAndHtml(const std::string& text, const std::string& html) {
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(base::UTF8ToUTF16(text));
    scw.WriteHTML(base::UTF8ToUTF16(html), /*source_url=*/"");
  }

  // ClipboardHistory will post a task to process clipboard data in order to
  // debounce multiple clipboard writes occurring in sequence. Here we give
  // ClipboardHistory the chance to run its posted tasks before proceeding.
  FlushMessageLoop();
}

ash::ClipboardHistoryControllerImpl* GetClipboardHistoryController() {
  return ash::Shell::Get()->clipboard_history_controller();
}

const ash::ClipboardHistoryMenuModelAdapter* GetContextMenu() {
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

}  // namespace

// Verify clipboard history's features in the multiprofile environment.
class ClipboardHistoryWithMultiProfileBrowserTest
    : public chromeos::LoginManagerTest {
 public:
  ClipboardHistoryWithMultiProfileBrowserTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;

    feature_list_.InitAndEnableFeature(chromeos::features::kClipboardHistory);
  }

  ~ClipboardHistoryWithMultiProfileBrowserTest() override = default;

  ui::test::EventGenerator* GetEventGenerator() {
    return event_generator_.get();
  }

 protected:
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

  void ShowContextMenuViaAccelerator() {
    PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  }

  // chromeos::LoginManagerTest:
  void SetUpOnMainThread() override {
    chromeos::LoginManagerTest::SetUpOnMainThread();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  AccountId account_id1_;
  AccountId account_id2_;
  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};

  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  base::test::ScopedFeatureList feature_list_;
};

// Verify that the clipboard data history is recorded as expected in the
// Multiuser environment.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMultiProfileBrowserTest,
                       VerifyClipboardHistoryAcrossMultiUser) {
  LoginUser(account_id1_);
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
  chromeos::UserAddingScreen::Get()->Start();
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

// Verifies that the history menu is anchored at the cursor's location when
// not having any textfield.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMultiProfileBrowserTest,
                       ShowHistoryMenuWhenNoTextfieldExists) {
  LoginUser(account_id1_);

  // Close the browser window to ensure that textfield does not exist.
  CloseAllBrowsers();

  // No clipboard data. So the clipboard history menu should not show.
  ASSERT_TRUE(GetClipboardItems().empty());
  ShowContextMenuViaAccelerator();
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  SetClipboardText("test");

  const gfx::Point mouse_location =
      ash::Shell::Get()->GetPrimaryRootWindow()->bounds().CenterPoint();
  GetEventGenerator()->MoveMouseTo(mouse_location);
  ShowContextMenuViaAccelerator();

  // Verifies that the menu is anchored at the cursor's location.
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  const gfx::Point menu_origin =
      GetClipboardHistoryMenuBoundsInScreen().origin();
  EXPECT_EQ(mouse_location.x() +
                views::MenuConfig::instance().touchable_anchor_offset,
            menu_origin.x());
  EXPECT_EQ(mouse_location.y(), menu_origin.y());
}

// Verifies that the selected item should be deleted by the backspace key.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMultiProfileBrowserTest,
                       DeleteItemViaBackspaceKey) {
  base::HistogramTester histogram_tester;
  LoginUser(account_id1_);

  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  // Show the menu.
  ShowContextMenuViaAccelerator();
  ASSERT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(3, GetContextMenu()->GetMenuItemsCount());

  // Select the first menu item via key then delete it. Verify the menu and the
  // clipboard history.
  PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_EQ(2, GetContextMenu()->GetMenuItemsCount());
  EXPECT_TRUE(VerifyClipboardTextData({"B", "A"}));

  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatDeleted", 1);

  // Select the second menu item via key then delete it. Verify the menu and the
  // clipboard history.
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_EQ(1, GetContextMenu()->GetMenuItemsCount());
  EXPECT_TRUE(VerifyClipboardTextData({"B"}));

  // Delete the last item. Verify that the menu is closed.
  PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Trigger the accelerator of opening the clipboard history menu. No menu
  // shows because of the empty history data.
  ShowContextMenuViaAccelerator();
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
}

// Flaky: crbug.com/1123542
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMultiProfileBrowserTest,
                       DISABLED_ShouldPasteHistoryAsPlainText) {
  LoginUser(account_id1_);

  // Create a browser and cache its active web contents.
  auto* browser = CreateBrowser(
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id1_));
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Load the web contents synchronously.
  // The contained script:
  //  - Listens for paste events and caches the last pasted data.
  //  - Notifies observers of paste events by changing document title.
  //  - Provides an API to expose the last pasted data.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(R"(data:text/html,
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

  // Cache a function to return the last paste.
  auto GetLastPaste = [&]() {
    auto result = content::EvalJs(
        web_contents, "(function() { return window.getLastPaste(); })();");
    EXPECT_EQ(result.error, "");
    return result.ExtractList();
  };

  // Confirm initial state.
  ASSERT_TRUE(GetLastPaste().GetList().empty());

  // Write some things to the clipboard.
  SetClipboardTextAndHtml("A", "<span>A</span>");
  SetClipboardTextAndHtml("B", "<span>B</span>");
  SetClipboardTextAndHtml("C", "<span>C</span>");

  // Open clipboard history and paste the last history item.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Wait for the paste event to propagate to the web contents.
  // The web contents will notify us a paste occurred by updating page title.
  ignore_result(
      content::TitleWatcher(web_contents, base ::UTF8ToUTF16("Paste 1"))
          .WaitAndGetTitle());

  // Confirm the expected paste data.
  base::ListValue last_paste = GetLastPaste();
  ASSERT_EQ(last_paste.GetList().size(), 2u);
  EXPECT_EQ(last_paste.GetList()[0].GetString(), "text/plain: A");
  EXPECT_EQ(last_paste.GetList()[1].GetString(), "text/html: <span>A</span>");

  // Open clipboard history and paste the middle history item as plain text.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Wait for the paste event to propagate to the web contents.
  // The web contents will notify us a paste occurred by updating page title.
  ignore_result(
      content::TitleWatcher(web_contents, base ::UTF8ToUTF16("Paste 2"))
          .WaitAndGetTitle());

  // Confirm the expected paste data.
  last_paste = GetLastPaste();
  ASSERT_EQ(last_paste.GetList().size(), 1u);
  EXPECT_EQ(last_paste.GetList()[0].GetString(), "text/plain: A");
}

// The browser test which creates a widget with a textfield during setting-up
// to help verify the multipaste menu item's response to the gesture tap and
// the mouse click.
class ClipboardHistoryTextfieldBrowserTest
    : public ClipboardHistoryWithMultiProfileBrowserTest {
 public:
  ClipboardHistoryTextfieldBrowserTest() = default;
  ~ClipboardHistoryTextfieldBrowserTest() override = default;

 protected:
  // ClipboardHistoryWithMultiProfileBrowserTest:
  void SetUpOnMainThread() override {
    ClipboardHistoryWithMultiProfileBrowserTest::SetUpOnMainThread();

    LoginUser(account_id1_);
    CloseAllBrowsers();

    // Create a widget containing a single, focusable textfield.
    widget_ = CreateTestWidget();
    textfield_ = widget_->SetContentsView(std::make_unique<views::Textfield>());
    textfield_->SetAccessibleName(base::UTF8ToUTF16("Textfield"));
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

  std::unique_ptr<views::Widget> widget_;
  views::Textfield* textfield_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       ShouldPasteHistoryViaKeyboard) {
  base::HistogramTester histogram_tester;
  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  // Verify we can paste the first history item via the ENTER key.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 3);

  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield_->GetText()));
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatPasted", 1);

  textfield_->SetText(base::string16());
  EXPECT_TRUE(textfield_->GetText().empty());

  // Verify we can paste the first history item via the COMMAND+V shortcut.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield_->GetText()));

  textfield_->SetText(base::string16());
  EXPECT_TRUE(textfield_->GetText().empty());

  // Verify we can paste the last history item via the ENTER key.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_RETURN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));

  textfield_->SetText(base::string16());

  EXPECT_TRUE(textfield_->GetText().empty());

  // Verify we can paste the last history item via the COMMAND+V shortcut.
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));
}

IN_PROC_BROWSER_TEST_F(ClipboardHistoryTextfieldBrowserTest,
                       ShouldPasteHistoryWhileHoldingDownCommandKey) {
  // Write some things to the clipboard.
  SetClipboardText("A");
  SetClipboardText("B");
  SetClipboardText("C");

  // Verify we can traverse clipboard history and paste the first history item
  // while holding down the COMMAND key.
  Press(ui::KeyboardCode::VKEY_COMMAND);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("C", base::UTF16ToUTF8(textfield_->GetText()));
  Release(ui::KeyboardCode::VKEY_COMMAND);

  textfield_->SetText(base::string16());
  EXPECT_TRUE(textfield_->GetText().empty());

  // Verify we can traverse clipboard history and paste the last history item
  // while holding down the COMMAND key.
  Press(ui::KeyboardCode::VKEY_COMMAND);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_COMMAND_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_COMMAND_DOWN);
  PressAndRelease(ui::KeyboardCode::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));
  Release(ui::KeyboardCode::VKEY_COMMAND);
}

class FakeDlpController : public ui::ClipboardDlpController {
 public:
  FakeDlpController()
      : allowed_origin_(url::Origin::Create(GURL(kUrlString))) {}
  ~FakeDlpController() override = default;

  // ui::ClipboardDlpController:
  bool IsDataReadAllowed(
      const ui::ClipboardDataEndpoint* const data_src,
      const ui::ClipboardDataEndpoint* const data_dst) const override {
    // The multipaste menu should have access to any clipboard data.
    if (data_dst && data_dst->type() == ui::EndpointType::kClipboardHistory)
      return true;

    // For other data destinations, only the data from `allowed_origin_`
    // should be accessible.
    return data_src && data_src->IsUrlType() &&
           (*data_src->origin() == allowed_origin_);
  }

 private:
  const url::Origin allowed_origin_;
};

// The browser test equipped with the custom DLP controller.
class ClipboardHistoryWithMockDLPBrowserTest
    : public ClipboardHistoryTextfieldBrowserTest {
 public:
  ClipboardHistoryWithMockDLPBrowserTest()
      : dlp_controller_(std::make_unique<FakeDlpController>()) {}
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
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste,
                              std::make_unique<ui::ClipboardDataEndpoint>(
                                  url::Origin::Create(GURL(kUrlString))))
        .WriteText(base::UTF8ToUTF16(text));

    // ClipboardHistory will post a task to process clipboard data in order to
    // debounce multiple clipboard writes occurring in sequence. Here we give
    // ClipboardHistory the chance to run its posted tasks before proceeding.
    FlushMessageLoop();
  }

 private:
  std::unique_ptr<FakeDlpController> dlp_controller_;
};

// Verifies the basic features related to the inaccessible menu item, the one
// whose clipboard data should not be leaked through the multipaste menu.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWithMockDLPBrowserTest, Basics) {
  SetClipboardTextWithAccessibleSrc("A");
  SetClipboardTextWithInaccessibleSrc("B");
  EXPECT_TRUE(VerifyClipboardTextData({"B", "A"}));

  ShowContextMenuViaAccelerator();
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // Verify that the text is pasted into `textfield_` after the mouse click at
  // `accessible_menu_item_view`.
  const views::MenuItemView* accessible_menu_item_view =
      GetContextMenu()->GetMenuItemViewAtForTest(/*index=*/1);
  GetEventGenerator()->MoveMouseTo(
      accessible_menu_item_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(accessible_menu_item_view->IsSelected());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ("A", base::UTF16ToUTF8(textfield_->GetText()));

  // Clear `textfield_`'s contents.
  textfield_->SetText(base::string16());
  ASSERT_TRUE(textfield_->GetText().empty());

  // Re-show the multipaste menu since the menu is closed after the previous
  // mouse click.
  ASSERT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  ShowContextMenuViaAccelerator();

  // Move mouse to `inaccessible_menu_item_view` then click the left button.
  const views::MenuItemView* inaccessible_menu_item_view =
      GetContextMenu()->GetMenuItemViewAtForTest(/*index=*/0);
  GetEventGenerator()->MoveMouseTo(
      inaccessible_menu_item_view->GetBoundsInScreen().CenterPoint());

  // Verify that `inaccessible_menu_item_view` cannot be selected by mouse
  // hovering. It does not respond to mouse click either.
  EXPECT_FALSE(inaccessible_menu_item_view->IsSelected());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));

  // Move the selection through the arrow key. Then delete the item by the
  // backspace key. After deletion, `inaccessible_menu_item_view` is left.
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_TRUE(VerifyClipboardTextData({"B"}));
  EXPECT_EQ(1, GetContextMenu()->GetMenuItemsCount());

  // Move the selection through the arrow key again. Verify that
  // `inaccessible_menu_item_view` cannot be selected. Pressing the backspace
  // key does not delete the item.
  PressAndRelease(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndRelease(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_FALSE(inaccessible_menu_item_view->IsSelected());
  EXPECT_TRUE(VerifyClipboardTextData({"B"}));
  EXPECT_EQ(1, GetContextMenu()->GetMenuItemsCount());
}
