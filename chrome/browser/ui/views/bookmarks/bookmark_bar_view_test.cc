// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/view_event_test_base.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/test_browser_thread.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/aura/window_tree_host.h"
#endif

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;
using content::OpenURLParams;
using content::PageNavigator;
using content::WebContents;

namespace {

#if !defined(OS_MACOSX)

// Waits for a views::Widget dialog to show up.
class DialogWaiter : public aura::EnvObserver,
                     public views::WidgetObserver {
 public:
  explicit DialogWaiter(aura::Env* env) : env_(env) { env_->AddObserver(this); }

  ~DialogWaiter() override { env_->RemoveObserver(this); }

  views::Widget* WaitForDialog() {
    if (dialog_created_)
      return dialog_;
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    return dialog_;
  }

 private:
  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {
    if (dialog_)
      return;
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
    if (!widget || !widget->IsDialogBox())
      return;
    dialog_ = widget;
    dialog_->AddObserver(this);
  }

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    CHECK_EQ(dialog_, widget);
    if (visible) {
      dialog_created_ = true;
      dialog_->RemoveObserver(this);
      if (!quit_closure_.is_null())
        quit_closure_.Run();
    }
  }

  aura::Env* env_;
  bool dialog_created_ = false;
  views::Widget* dialog_ = nullptr;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(DialogWaiter);
};

// Waits for a dialog to terminate.
class DialogCloseWaiter : public views::WidgetObserver {
 public:
  explicit DialogCloseWaiter(views::Widget* dialog)
      : dialog_closed_(false) {
    dialog->AddObserver(this);
  }

  ~DialogCloseWaiter() override {
    // It is not necessary to remove |this| from the dialog's observer, since
    // the dialog is destroyed before this waiter.
  }

  void WaitForDialogClose() {
    if (dialog_closed_)
      return;
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    dialog_closed_ = true;
    if (!quit_closure_.is_null())
      quit_closure_.Run();
  }

  bool dialog_closed_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(DialogCloseWaiter);
};

// Waits for a views::Widget to receive a Tab key.
class TabKeyWaiter : public ui::EventHandler {
 public:
  explicit TabKeyWaiter(views::Widget* widget)
      : widget_(widget),
        received_tab_(false) {
    widget_->GetNativeView()->AddPreTargetHandler(this);
  }

  ~TabKeyWaiter() override {
    widget_->GetNativeView()->RemovePreTargetHandler(this);
  }

  void WaitForTab() {
    if (received_tab_)
      return;
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() == ui::ET_KEY_RELEASED &&
        event->key_code() == ui::VKEY_TAB) {
      received_tab_ = true;
      if (!quit_closure_.is_null())
        quit_closure_.Run();
    }
  }

  views::Widget* widget_;
  bool received_tab_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TabKeyWaiter);
};

void MoveMouseAndPress(const gfx::Point& screen_pos,
                       ui_controls::MouseButton button,
                       int button_state,
                       base::OnceClosure closure) {
  ASSERT_TRUE(ui_controls::SendMouseMove(screen_pos.x(), screen_pos.y()));
  ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(button, button_state,
                                                         std::move(closure)));
}

#endif  // !defined(OS_MACOSX)

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  TestingPageNavigator() {}
  ~TestingPageNavigator() override {}

  WebContents* OpenURL(const OpenURLParams& params) override {
    urls_.push_back(params.url);
    return NULL;
  }

  const std::vector<GURL>& urls() const { return urls_; }
  GURL last_url() const { return urls_.empty() ? GURL() : urls_.back(); }

 private:
  std::vector<GURL> urls_;

  DISALLOW_COPY_AND_ASSIGN(TestingPageNavigator);
};

// TODO(erg): Fix bookmark DND tests on linux_aura. crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

}  // namespace

// Base class for event generating bookmark view tests. These test are intended
// to exercise View's menus, but that's easier done with BookmarkBarView rather
// than View's menu itself.
//
// SetUp creates a bookmark model with the following structure.
// All folders are in upper case, all URLs in lower case.
// F1
//   f1a
//   F11
//     f11a
//   f1b
//   *
// a
// b
// c
// d
// F2
// e
// OTHER
//   oa
//   OF
//     ofa
//     ofb
//   OF2
//     of2a
//     of2b
//
// * if CreateBigMenu returns return true, 100 menu items are created here with
//   the names f1-f100.
//
// Subclasses should be sure and invoke super's implementation of SetUp and
// TearDown.
class BookmarkBarViewEventTestBase : public ViewEventTestBase {
 public:
  BookmarkBarViewEventTestBase()
      : ViewEventTestBase(),
        model_(NULL) {}

  void SetUp() override {
    content_client_.reset(new ChromeContentClient);
    content::SetContentClient(content_client_.get());
    browser_content_client_.reset(new ChromeContentBrowserClient());
    content::SetBrowserClientForTesting(browser_content_client_.get());

    views::MenuController::TurnOffMenuSelectionHoldForTest();
    BookmarkBarView::DisableAnimationsForTesting(true);
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    profile_->GetPrefs()->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);

    Browser::CreateParams native_params(profile_.get(), true);
    browser_ = CreateBrowserWithTestWindowForParams(&native_params);

    local_state_.reset(new ScopedTestingLocalState(
        TestingBrowserProcess::GetGlobal()));
    model_->ClearStore();

    bb_view_.reset(new BookmarkBarView(browser_.get(), NULL));
    bb_view_->set_owned_by_client();
    // Real bookmark bars get a BookmarkBarViewBackground. Set an opaque
    // background here just to avoid triggering subpixel rendering issues.
    bb_view_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
    bb_view_->SetPageNavigator(&navigator_);

    AddTestData(CreateBigMenu());

    // Create the Widget. Note the initial size is given by
    // GetPreferredSizeForContents() during initialization. This occurs after
    // the WidgetDelegate provides |bb_view_| as the contents view and adds it
    // to the hierarchy.
    ViewEventTestBase::SetUp();

    // Verify the layout triggered by the initial size preserves the overflow
    // state calculated in GetPreferredSizeForContents().
    EXPECT_TRUE(GetBookmarkButton(5)->visible());
    EXPECT_FALSE(GetBookmarkButton(6)->visible());
  }

  void TearDown() override {
    // Destroy everything, then run the message loop to ensure we delete all
    // Tasks and fully shut down.
    browser_->tab_strip_model()->CloseAllTabs();
    bb_view_.reset();
    browser_.reset();
    profile_.reset();

    // Run the message loop to ensure we delete allTasks and fully shut down.
    base::RunLoop().RunUntilIdle();

    ViewEventTestBase::TearDown();
    BookmarkBarView::DisableAnimationsForTesting(false);
    constrained_window::SetConstrainedWindowViewsClient(nullptr);

    browser_content_client_.reset();
    content_client_.reset();
    content::SetContentClient(NULL);
  }

 protected:
  views::View* CreateContentsView() override { return bb_view_.get(); }

  gfx::Size GetPreferredSizeForContents() const override {
    // Calculate the preferred size so that one button doesn't fit, which
    // triggers the overflow button to appear. We have to do this incrementally
    // as there isn't a good way to determine the point at which the overflow
    // button is shown.
    //
    // This code looks a bit hacky, but it is written so that it shouldn't
    // depend on any of the layout code in BookmarkBarView, or extra buttons
    // added to the right of the bookmarks. Instead, brute force search for a
    // size that triggers the overflow button.
    gfx::Size size = bb_view_->GetPreferredSize();
    size.set_width(1000);
    do {
      size.set_width(size.width() - 25);
      bb_view_->SetBounds(0, 0, size.width(), size.height());
      bb_view_->Layout();
    } while (bb_view_->GetBookmarkButton(6)->visible());
    return size;
  }

  views::LabelButton* GetBookmarkButton(int view_index) {
    return bb_view_->GetBookmarkButton(view_index);
  }

  // See comment above class description for what this does.
  virtual bool CreateBigMenu() { return false; }

  BookmarkModel* model_;
  std::unique_ptr<BookmarkBarView> bb_view_;
  TestingPageNavigator navigator_;

 private:
  void AddTestData(bool big_menu) {
    const BookmarkNode* bb_node = model_->bookmark_bar_node();
    std::string test_base = "file:///c:/tmp/";
    const BookmarkNode* f1 = model_->AddFolder(bb_node, 0, ASCIIToUTF16("F1"));
    model_->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddFolder(f1, 1, ASCIIToUTF16("F11"));
    model_->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    model_->AddURL(f1, 2, ASCIIToUTF16("f1b"), GURL(test_base + "f1b"));
    if (big_menu) {
      for (int i = 1; i <= 100; ++i) {
        model_->AddURL(f1, i + 1, ASCIIToUTF16("f") + base::IntToString16(i),
                       GURL(test_base + "f" + base::IntToString(i)));
      }
    }
    model_->AddURL(bb_node, 1, ASCIIToUTF16("a"), GURL(test_base + "a"));
    model_->AddURL(bb_node, 2, ASCIIToUTF16("b"), GURL(test_base + "b"));
    model_->AddURL(bb_node, 3, ASCIIToUTF16("c"), GURL(test_base + "c"));
    model_->AddURL(bb_node, 4, ASCIIToUTF16("d"), GURL(test_base + "d"));
    model_->AddFolder(bb_node, 5, ASCIIToUTF16("F2"));
    model_->AddURL(bb_node, 6, ASCIIToUTF16("d"), GURL(test_base + "d"));

    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("oa"),
                   GURL(test_base + "oa"));
    const BookmarkNode* of = model_->AddFolder(model_->other_node(), 1,
                                               ASCIIToUTF16("OF"));
    model_->AddURL(of, 0, ASCIIToUTF16("ofa"), GURL(test_base + "ofa"));
    model_->AddURL(of, 1, ASCIIToUTF16("ofb"), GURL(test_base + "ofb"));
    const BookmarkNode* of2 = model_->AddFolder(model_->other_node(), 2,
                                                ASCIIToUTF16("OF2"));
    model_->AddURL(of2, 0, ASCIIToUTF16("of2a"), GURL(test_base + "of2a"));
    model_->AddURL(of2, 1, ASCIIToUTF16("of2b"), GURL(test_base + "of2b"));
  }

  std::unique_ptr<ChromeContentClient> content_client_;
  std::unique_ptr<ChromeContentBrowserClient> browser_content_client_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

#if !defined(OS_MACOSX)
// The following tests were not enabled on Mac before. Consider enabling those
// that are able to run on Mac (https://crbug.com/845342).

// Clicks on first menu, makes sure button is depressed. Moves mouse to first
// child, clicks it and makes sure a navigation occurs.
class BookmarkBarViewTest1 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Button should be depressed.
    views::LabelButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::Button::STATE_PRESSED);

    // Click on the 2nd menu item (A URL).
    ASSERT_TRUE(menu->GetSubmenu());

    views::MenuItemView* menu_to_select =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ui_test_utils::MoveMouseToCenterAndPress(menu_to_select, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step3));
  }

  void Step3() {
    // We should have navigated to URL f1a.
    ASSERT_EQ(navigator_.last_url(),
              model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url());

    // Make sure button is no longer pushed.
    views::LabelButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::Button::STATE_NORMAL);

    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest1, Basic)

// Brings up menu, clicks on empty space and make sure menu hides.
class BookmarkBarViewTest2 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest2::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL && menu->GetSubmenu()->IsShowing());

    // Click on 0x0, which should trigger closing menu.
    // NOTE: this code assume there is a left margin, which is currently
    // true. If that changes, this code will need to find another empty space
    // to press the mouse on.
    gfx::Point mouse_loc;
    views::View::ConvertPointToScreen(bb_view_.get(), &mouse_loc);
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        0, 0, CreateEventTask(this, &BookmarkBarViewTest2::Step3)));
  }

  void Step3() {
    // As the click is on the desktop the hook never sees the up, so we only
    // wait on the down. We still send the up though else the system thinks
    // the mouse is still down.
    ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest2::Step4)));
    ASSERT_TRUE(
        ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP));
  }

  void Step4() {
    // The menu shouldn't be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    // Make sure button is no longer pushed.
    views::LabelButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::Button::STATE_NORMAL);

    Done();
  }
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) || defined(OS_WIN)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
// Disable this test on Win10:    http://crbug.com/828063
#define MAYBE_HideOnDesktopClick DISABLED_HideOnDesktopClick
#else
#define MAYBE_HideOnDesktopClick HideOnDesktopClick
#endif

VIEW_TEST(BookmarkBarViewTest2, MAYBE_HideOnDesktopClick)

// Brings up menu. Moves over child to make sure submenu appears, moves over
// another child and make sure next menu appears.
class BookmarkBarViewTest3 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::MenuButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // Click on second child, which has a submenu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step3));
  }

  void Step3() {
    // Make sure sub menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu->GetSubmenu() != NULL);
    ASSERT_TRUE(child_menu->GetSubmenu()->IsShowing());

    // Click on third child, which has a submenu too.
    child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step4));
  }

  void Step4() {
    // Make sure sub menu we first clicked isn't showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu->GetSubmenu() != NULL);
    ASSERT_FALSE(child_menu->GetSubmenu()->IsShowing());

    // And submenu we last clicked is showing.
    child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ASSERT_TRUE(child_menu->GetSubmenu()->IsShowing());

    // Nothing should have been selected.
    EXPECT_EQ(GURL(), navigator_.last_url());

    // Hide menu.
    menu->GetMenuController()->CancelAll();

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest3, Submenus)

// Observer that posts task upon the context menu creation.
// This is necessary for Linux as the context menu has to check
// the clipboard, which invokes the event loop.
class BookmarkContextMenuNotificationObserver
    : public content::NotificationObserver {
 public:
  explicit BookmarkContextMenuNotificationObserver(const base::Closure& task)
      : task_(task) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BOOKMARK_CONTEXT_MENU_SHOWN,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task_);
  }

  // Sets the task that is posted when the context menu is shown.
  void set_task(const base::Closure& task) { task_ = task; }

 private:
  content::NotificationRegistrar registrar_;
  base::Closure task_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuNotificationObserver);
};

// Tests context menus by way of opening a context menu for a bookmark,
// then right clicking to get context menu and selecting the first menu item
// (open).
class BookmarkBarViewTest4 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest4()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest4::Step3)) {
  }

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Select the first menu item (open).
    ui_test_utils::MoveMouseToCenterAndPress(
        menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step4));
  }

  void Step4() {
    EXPECT_EQ(navigator_.last_url(), model_->other_node()->GetChild(0)->url());
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest4, ContextMenus)

// Tests drag and drop within the same menu.
class BookmarkBarViewTest5 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    url_dragging_ =
        model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest5::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest5::Step3));
  }

  void Step3() {
    views::MenuItemView* target_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    gfx::Point loc(1, target_menu->height() - 1);
    views::View::ConvertPointToScreen(target_menu, &loc);

    // Start a drag.
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x() + 10, loc.y(),
        CreateEventTask(this, &BookmarkBarViewTest5::Step4)));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    // Drop the item so that it's now the second item.
    views::MenuItemView* target_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    gfx::Point loc(1, target_menu->height() - 2);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ASSERT_TRUE(ui_controls::SendMouseMove(loc.x(), loc.y()));

    ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest5::Step5)));
  }

  void Step5() {
    GURL url = model_->bookmark_bar_node()->GetChild(0)->GetChild(1)->url();
    EXPECT_EQ(url_dragging_, url);
    Done();
  }

  GURL url_dragging_;
};
// flaky on Windows: https://crbug.com/400578
// flaky on ChromeOS: https://crbug.com/758210
#if !defined(OS_WIN) && !defined(OS_CHROMEOS)
VIEW_TEST(BookmarkBarViewTest5, DND)
#endif

// Tests holding mouse down on overflow button, dragging such that menu pops up
// then selecting an item.
class BookmarkBarViewTest6 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Press the mouse button on the overflow button. Don't release it though.
    views::LabelButton* button = bb_view_->overflow_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN, CreateEventTask(this, &BookmarkBarViewTest6::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and release mouse.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::UP, CreateEventTask(this, &BookmarkBarViewTest6::Step3));
  }

  void Step3() {
    ASSERT_EQ(navigator_.last_url(),
              model_->bookmark_bar_node()->GetChild(6)->url());
    Done();
  }

  GURL url_dragging_;
};

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(BookmarkBarViewTest6, OpenMenuOnClickAndHold)

// Tests drag and drop to different menu.
class BookmarkBarViewTest7 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    url_dragging_ =
        model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest7::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest7::Step3));
  }

  void Step3() {
    // Drag over other button.
    views::LabelButton* other_button = bb_view_->other_bookmarks_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

#if defined(USE_AURA)
    // TODO: fix this. Aura requires an additional mouse event to trigger drag
    // and drop checking state.
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x() + 10, loc.y(),
        base::BindOnce(&BookmarkBarViewTest7::Step3A, base::Unretained(this))));
#else
    // Start a drag.
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x() + 10, loc.y(),
        base::BindOnce(&BookmarkBarViewTest7::Step4, base::Unretained(this))));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
#endif
  }

  void Step3A() {
    // Drag over other button.
    views::LabelButton* other_button = bb_view_->other_bookmarks_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x(), loc.y(),
        base::BindOnce(&BookmarkBarViewTest7::Step4, base::Unretained(this))));
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());
    // The button should be highlighted now.
    views::LabelButton* other_button = bb_view_->other_bookmarks_button();
    ASSERT_EQ(views::Button::STATE_PRESSED, other_button->state());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(0);
    gfx::Point loc(1, 1);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x(), loc.y(), CreateEventTask(this, &BookmarkBarViewTest7::Step5)));
  }

  void Step5() {
    ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest7::Step6)));
  }

  void Step6() {
    ASSERT_TRUE(model_->other_node()->GetChild(0)->url() == url_dragging_);
    // The button should be in normal state now.
    views::LabelButton* other_button = bb_view_->other_bookmarks_button();
    ASSERT_EQ(views::Button::STATE_NORMAL, other_button->state());
    Done();
  }

  GURL url_dragging_;
};

#if !defined(OS_WIN)
// This test passes locally (on aero and non-aero) but fails on the trybots and
// buildbot.
// http://crbug.com/154081
VIEW_TEST(BookmarkBarViewTest7, MAYBE(DNDToDifferentMenu))
#endif

// Drags from one menu to next so that original menu closes, then back to
// original menu.
class BookmarkBarViewTest8 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    url_dragging_ =
        model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest8::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest8::Step3));
  }

  void Step3() {
    // Drag over other button.
    views::LabelButton* other_button = bb_view_->other_bookmarks_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    // Start a drag.
#if defined(USE_AURA)
    // TODO: fix this. Aura requires an additional mouse event to trigger drag
    // and drop checking state.
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x() + 10, loc.y(),
        base::BindOnce(&BookmarkBarViewTest8::Step3A, base::Unretained(this))));
#else
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x() + 10, loc.y(),
        base::BindOnce(&BookmarkBarViewTest8::Step4, base::Unretained(this))));
    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
#endif
  }

  void Step3A() {
    // Drag over other button.
    views::LabelButton* other_button = bb_view_->other_bookmarks_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x() + 10, loc.y(),
        base::BindOnce(&BookmarkBarViewTest8::Step4, base::Unretained(this))));
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    // Now drag back over first menu.
    views::LabelButton* button = GetBookmarkButton(0);
    gfx::Point loc(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &loc);
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x(), loc.y(),
        base::BindOnce(&BookmarkBarViewTest8::Step5, base::Unretained(this))));
  }

  void Step5() {
    // Drop on folder F11.
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(1);
    ui_test_utils::MoveMouseToCenterAndPress(
        target_menu, ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest8::Step6));
  }

  void Step6() {
    // Make sure drop was processed.
    GURL final_url = model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->
        GetChild(1)->url();
    ASSERT_TRUE(final_url == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

#if !defined(OS_WIN)
// This test passes locally (on aero and non-aero) but fails on the trybots and
// buildbot.
// http://crbug.com/154081
VIEW_TEST(BookmarkBarViewTest8, MAYBE(DNDBackToOriginatingMenu))
#endif

// Moves the mouse over the scroll button and makes sure we get scrolling.
class BookmarkBarViewTest9 : public BookmarkBarViewEventTestBase {
 protected:
  bool CreateBigMenu() override { return true; }

  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest9::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    first_menu_ = menu->GetSubmenu()->GetMenuItemAt(0);
    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(first_menu_, &menu_loc);
    start_y_ = menu_loc.y();

    // Move the mouse over the scroll button.
    views::View* scroll_container = menu->GetSubmenu()->parent();
    ASSERT_TRUE(scroll_container != NULL);
    scroll_container = scroll_container->parent();
    ASSERT_TRUE(scroll_container != NULL);
    views::View* scroll_down_button = scroll_container->child_at(1);
    ASSERT_TRUE(scroll_down_button);
    gfx::Point loc(scroll_down_button->width() / 2,
                   scroll_down_button->height() / 2);
    views::View::ConvertPointToScreen(scroll_down_button, &loc);

    // On linux, the sending one location isn't enough.
    ASSERT_TRUE(ui_controls::SendMouseMove(loc.x() - 1, loc.y() - 1));
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x(), loc.y(), CreateEventTask(this, &BookmarkBarViewTest9::Step3)));
  }

  void Step3() {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BookmarkBarViewTest9::Step4, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(200));
  }

  void Step4() {
    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(first_menu_, &menu_loc);
    ASSERT_NE(start_y_, menu_loc.y());

    // Hide menu.
    bb_view_->GetMenu()->GetMenuController()->CancelAll();

    // On linux, Cancelling menu will call Quit on the message loop,
    // which can interfere with Done. We need to run Done in the
    // next execution loop.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ViewEventTestBase::Done, base::Unretained(this)));
  }

  int start_y_;
  views::MenuItemView* first_menu_;
};

// Fails on official cros bot. crbug.com/431427.
#if defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD)
#define MAYBE_ScrollButtonScrolls DISABLED_ScrollButtonScrolls
#else
#define MAYBE_ScrollButtonScrolls ScrollButtonScrolls
#endif

VIEW_TEST(BookmarkBarViewTest9, MAYBE_ScrollButtonScrolls)

// Tests up/down/left/enter key messages.
class BookmarkBarViewTest10 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest10::Step2));
    base::RunLoop().RunUntilIdle();
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send a down event, which should select the first item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step3)));
  }

  void Step3() {
    // Make sure menu is showing and item is selected.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send a key down event, which should select the next item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step4)));
  }

  void Step4() {
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_FALSE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(1)->IsSelected());

    // Send a right arrow to force the menu to open.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_RIGHT, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step5)));
  }

  void Step5() {
    // Make sure the submenu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* submenu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(submenu->IsSelected());
    ASSERT_TRUE(submenu->GetSubmenu());
    ASSERT_TRUE(submenu->GetSubmenu()->IsShowing());

    // Send a left arrow to close the submenu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_LEFT, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step6)));
  }

  void Step6() {
    // Make sure the submenu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* submenu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(submenu->IsSelected());
    ASSERT_TRUE(!submenu->GetSubmenu() || !submenu->GetSubmenu()->IsShowing());

    // Send a down arrow to go down to f1b.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step7)));
  }

  void Step7() {
    // Make sure menu is showing and item is selected.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(2)->IsSelected());

    // Send a down arrow to wrap back to f1a.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step8)));
  }

  void Step8() {
    // Make sure menu is showing and item is selected.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send enter, which should select the item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_RETURN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step9)));
  }

  void Step9() {
    ASSERT_EQ(navigator_.last_url(),
              model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url());
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest10, KeyEvents)

// Make sure the menu closes with the following sequence: show menu, show
// context menu, close context menu (via escape), then click else where. This
// effectively verifies we maintain mouse capture after the context menu is
// hidden.
class BookmarkBarViewTest11 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest11()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest11::Step3)) {
  }

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest11::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    // Send escape so that the context menu hides.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest11::Step4)));
  }

  void Step4() {
    // Make sure the context menu is no longer showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(!menu || !menu->GetSubmenu() ||
                !menu->GetSubmenu()->IsShowing());

    // But the menu should be showing.
    menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu && menu->GetSubmenu() && menu->GetSubmenu()->IsShowing());

    // Now click on empty space.
    gfx::Point mouse_loc;
    views::View::ConvertPointToScreen(bb_view_.get(), &mouse_loc);
    ASSERT_TRUE(ui_controls::SendMouseMove(mouse_loc.x(), mouse_loc.y()));
    ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP | ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest11::Step5)));
  }

  void Step5() {
    // Make sure the menu is not showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(!menu || !menu->GetSubmenu() ||
                !menu->GetSubmenu()->IsShowing());
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest11, CloseMenuAfterClosingContextMenu)

// Tests showing a modal dialog from a context menu.
class BookmarkBarViewTest12 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Open up the other folder.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step2));
    chrome::kNumBookmarkUrlsBeforePrompting = 1;
  }

  ~BookmarkBarViewTest12() override {
    chrome::kNumBookmarkUrlsBeforePrompting = 15;
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the second child (a folder) to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step3));
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu && menu->GetSubmenu() && menu->GetSubmenu()->IsShowing());

    // Select the first item in the context menu (open all).
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Click and wait until the dialog box appears.
    auto dialog_waiter = std::make_unique<DialogWaiter>(
        bb_view_->GetWidget()->GetNativeWindow()->env());
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        base::Bind(&BookmarkBarViewTest12::Step4, base::Unretained(this),
                   base::Passed(&dialog_waiter)));
  }

  void Step4(std::unique_ptr<DialogWaiter> waiter) {
    views::Widget* dialog = waiter->WaitForDialog();
    waiter.reset();

    // Press tab to give focus to the cancel button. Wait until the widget
    // receives the tab key.
    TabKeyWaiter tab_waiter(dialog);
    ASSERT_TRUE(ui_controls::SendKeyPress(
        window_->GetNativeWindow(), ui::VKEY_TAB, false, false, false, false));
    tab_waiter.WaitForTab();

    // For some reason return isn't processed correctly unless we delay.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BookmarkBarViewTest12::Step5, base::Unretained(this),
                       base::Unretained(dialog)),
        base::TimeDelta::FromSeconds(1));
  }

  void Step5(views::Widget* dialog) {
    DialogCloseWaiter waiter(dialog);
    // And press enter so that the cancel button is selected.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_RETURN, false, false, false, false,
        base::OnceClosure()));
    waiter.WaitForDialogClose();
    Done();
  }
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_CloseWithModalDialog DISABLED_CloseWithModalDialog
#else
#define MAYBE_CloseWithModalDialog CloseWithModalDialog
#endif

VIEW_TEST(BookmarkBarViewTest12, MAYBE_CloseWithModalDialog)

// Tests clicking on the separator of a context menu (this is for coverage of
// bug 17862).
class BookmarkBarViewTest13 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest13()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest13::Step3)) {
  }

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Find the first separator.
    views::SubmenuView* submenu = menu->GetSubmenu();
    views::View* separator_view = NULL;
    for (int i = 0; i < submenu->child_count(); ++i) {
      if (submenu->child_at(i)->id() != views::MenuItemView::kMenuItemViewID) {
        separator_view = submenu->child_at(i);
        break;
      }
    }
    ASSERT_TRUE(separator_view);

    // Click on the separator. Clicking on the separator shouldn't visually
    // change anything.
    ui_test_utils::MoveMouseToCenterAndPress(separator_view,
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step4));
  }

  void Step4() {
    // The context menu should still be showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Select the first context menu item.
    ui_test_utils::MoveMouseToCenterAndPress(
        menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step5));
  }

  void Step5() {
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest13, ClickOnContextMenuSeparator)

// Makes sure right clicking on a folder on the bookmark bar doesn't result in
// both a context menu and showing the menu.
class BookmarkBarViewTest14 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest14()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest14::Step2)) {
  }

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // right mouse button.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step2 will be invoked by BookmarkContextMenuNotificationObserver.
  }

 private:

  void Step2() {
    // Menu should NOT be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL);

    // Send escape so that the context menu hides.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest14::Step3)));
  }

  void Step3() {
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest14, ContextMenus2)

// Makes sure deleting from the context menu keeps the bookmark menu showing.
class BookmarkBarViewTest15 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest15()
      : deleted_menu_id_(0),
        observer_(CreateEventTask(this, &BookmarkBarViewTest15::Step3)) {
  }

 protected:
  void DoTestOnMessageLoop() override {
    // Show the other bookmarks.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest15::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    deleted_menu_id_ = child_menu->GetCommand();

    // Right click on the second child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* delete_menu =
        menu->GetMenuItemByID(IDC_BOOKMARK_BAR_REMOVE);
    ASSERT_TRUE(delete_menu);

    // Click on the delete button.
    ui_test_utils::MoveMouseToCenterAndPress(delete_menu,
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest15::Step4));
  }

  void Step4() {
    // The context menu should not be showing.
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(context_menu == NULL);

    // But the menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // And the deleted_menu_id_ should have been removed.
    ASSERT_TRUE(menu->GetMenuItemByID(deleted_menu_id_) == NULL);

    bb_view_->GetMenu()->GetMenuController()->CancelAll();

    Done();
  }

  int deleted_menu_id_;
  BookmarkContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest15, MenuStaysVisibleAfterDelete)

// Tests that we don't crash or get stuck if the parent of a menu is closed.
class BookmarkBarViewTest16 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest16::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Button should be depressed.
    views::LabelButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::Button::STATE_PRESSED);

    // Close the window.
    window_->Close();
    window_ = NULL;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, CreateEventTask(this, &BookmarkBarViewTest16::Done));
  }
};

VIEW_TEST(BookmarkBarViewTest16, DeleteMenu)

// Makes sure right clicking on an item while a context menu is already showing
// doesn't crash and works.
class BookmarkBarViewTest17 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest17()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest17::Step3)) {
  }

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the other folder on the bookmark bar and press the
    // left mouse button.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest17::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Right click on the second item to show its context menu.
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(context_menu != NULL);
    ASSERT_TRUE(context_menu->GetSubmenu());
    ASSERT_TRUE(context_menu->GetSubmenu()->IsShowing());

    // Right click on the first menu item to trigger its context menu.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // The context menu and child_menu can be overlapped, calculate the
    // non-intersected Rect of the child menu and click on its center to make
    // sure the click is always on the child menu.
    gfx::Rect context_rect = context_menu->GetSubmenu()->GetBoundsInScreen();
    gfx::Rect child_menu_rect = child_menu->GetBoundsInScreen();
    gfx::Rect clickable_rect =
        gfx::SubtractRects(child_menu_rect, context_rect);
    ASSERT_FALSE(clickable_rect.IsEmpty());
    observer_.set_task(CreateEventTask(this, &BookmarkBarViewTest17::Step4));
    MoveMouseAndPress(clickable_rect.CenterPoint(), ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step4 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step4() {
    // The context menu should still be showing.
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(context_menu != NULL);

    // And the menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    bb_view_->GetMenu()->GetMenuController()->CancelAll();

    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// Flaky. See http://crbug.com/820435.
VIEW_TEST(BookmarkBarViewTest17, DISABLED_ContextMenus3)

// Verifies sibling menus works. Clicks on the 'other bookmarks' folder, then
// moves the mouse over the first item on the bookmark bar and makes sure the
// menu appears.
class BookmarkBarViewTest18 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the other folder on the bookmark bar and press the
    // left mouse button.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest18::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    // The button should be pressed.
    EXPECT_EQ(views::Button::STATE_PRESSED,
              bb_view_->other_bookmarks_button()->state());

    // Move the mouse to the first folder on the bookmark bar.
    views::LabelButton* button = GetBookmarkButton(0);
    gfx::Point button_center(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &button_center);
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        button_center.x(), button_center.y(),
        CreateEventTask(this, &BookmarkBarViewTest18::Step3)));
  }

  void Step3() {
    // Make sure the menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // The menu for the first folder should be in the pressed state (since the
    // menu is showing for it)...
    EXPECT_EQ(views::Button::STATE_PRESSED, GetBookmarkButton(0)->state());
    // ... And the "other bookmarks" button should no longer be pressed.
    EXPECT_EQ(views::Button::STATE_NORMAL,
              bb_view_->other_bookmarks_button()->state());

    menu->GetMenuController()->CancelAll();

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest18, BookmarkBarViewTest18_SiblingMenu)

// Verifies mousing over an already open sibling menu doesn't prematurely cancel
// the menu.
class BookmarkBarViewTest19 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the other folder on the bookmark bar and press the
    // left mouse button.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Click on the first folder.
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step3));
  }

  void Step3() {
    // Make sure the menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Move the mouse back to the "Other Bookmarks" button.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    gfx::Point button_center(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &button_center);
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        button_center.x() + 1, button_center.y() + 1,
        CreateEventTask(this, &BookmarkBarViewTest19::Step4)));
  }

  void Step4() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Click on the first folder.
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu,
        ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step5));
  }

  void Step5() {
    // Make sure the menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    menu->GetMenuController()->CancelAll();

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest19, BookmarkBarViewTest19_SiblingMenu)

// Verify that when clicking a mouse button outside a context menu,
// the context menu is dismissed *and* the underlying view receives
// the the mouse event (due to event reposting).
class BookmarkBarViewTest20 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest20() : test_view_(new TestViewForMenuExit) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Add |test_view_| next to |bb_view_|.
    views::View* parent = bb_view_->parent();
    views::View* container_view = new ContainerViewForMenuExit;
    container_view->AddChildView(bb_view_.get());
    container_view->AddChildView(test_view_);
    parent->AddChildView(container_view);
    parent->Layout();

    ASSERT_EQ(test_view_->press_count(), 0);

    // Move the mouse to the Test View and press the left mouse button.
    ui_test_utils::MoveMouseToCenterAndPress(
        test_view_, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step1));
  }

 private:
  void Step1() {
    ASSERT_EQ(test_view_->press_count(), 1);
    ASSERT_TRUE(bb_view_->GetMenu() == NULL);

    // Move the mouse to the first folder on the bookmark bar and press the
    // left mouse button.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step2));
  }

  void Step2() {
    ASSERT_EQ(test_view_->press_count(), 1);
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Move the mouse to the Test View and press the left mouse button.
    // The context menu will consume the event and exit. Thereafter,
    // the event is reposted and delivered to the Test View which
    // increases its press-count.
    ui_test_utils::MoveMouseToCenterAndPress(
        test_view_, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step3));
  }

  void Step3() {
#if defined(OS_CHROMEOS)
    ASSERT_EQ(test_view_->press_count(), 1);
#else
    ASSERT_EQ(test_view_->press_count(), 2);
#endif
    ASSERT_TRUE(bb_view_->GetMenu() == NULL);
    Done();
  }

  class ContainerViewForMenuExit : public views::View {
   public:
    ContainerViewForMenuExit() {
    }

    void Layout() override {
      DCHECK_EQ(2, child_count());
      views::View* bb_view = child_at(0);
      views::View* test_view = child_at(1);
      const int width = bb_view->width();
      const int height = bb_view->height();
      bb_view->SetBounds(0,0, width - 22, height);
      test_view->SetBounds(width - 20, 0, 20, height);
    }

   private:

    DISALLOW_COPY_AND_ASSIGN(ContainerViewForMenuExit);
  };

  class TestViewForMenuExit : public views::View {
   public:
    TestViewForMenuExit() : press_count_(0) {
    }
    bool OnMousePressed(const ui::MouseEvent& event) override {
      ++press_count_;
      return true;
    }
    int press_count() const { return press_count_; }

   private:
    int press_count_;

    DISALLOW_COPY_AND_ASSIGN(TestViewForMenuExit);
  };

  TestViewForMenuExit* test_view_;
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_ContextMenuExitTest DISABLED_ContextMenuExitTest
#else
#define MAYBE_ContextMenuExitTest ContextMenuExitTest
#endif

VIEW_TEST(BookmarkBarViewTest20, MAYBE_ContextMenuExitTest)

// Tests context menu by way of opening a context menu for a empty folder menu.
// The opened context menu should behave as it is from the folder button.
class BookmarkBarViewTest21 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest21()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest21::Step3)) {
  }

 protected:
  // Move the mouse to the empty folder on the bookmark bar and press the
  // left mouse button.
  void DoTestOnMessageLoop() override {
    views::LabelButton* button = GetBookmarkButton(5);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest21::Step2));
  }

 private:
  // Confirm that a menu for empty folder shows and right click the menu.
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);

    views::SubmenuView* submenu = menu->GetSubmenu();
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(1, submenu->child_count());

    views::View* view = submenu->child_at(0);
    ASSERT_TRUE(view != NULL);
    EXPECT_EQ(views::MenuItemView::kEmptyMenuItemViewID, view->id());

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(view, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  // Confirm that context menu shows and click REMOVE menu.
  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* delete_menu =
        menu->GetMenuItemByID(IDC_BOOKMARK_BAR_REMOVE);
    ASSERT_TRUE(delete_menu);

    // Click on the delete menu item.
    ui_test_utils::MoveMouseToCenterAndPress(delete_menu,
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest21::Step4));
  }

  // Confirm that the empty folder gets removed and menu doesn't show.
  void Step4() {
    views::LabelButton* button = GetBookmarkButton(5);
    ASSERT_TRUE(button);
    EXPECT_EQ(ASCIIToUTF16("d"), button->GetText());
    EXPECT_TRUE(bb_view_->GetContextMenu() == NULL);
    EXPECT_TRUE(bb_view_->GetMenu() == NULL);

    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(BookmarkBarViewTest21, ContextMenusForEmptyFolder)

// Test that closing the source browser window while dragging a bookmark does
// not cause a crash.
class BookmarkBarViewTest22 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest22::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest22::Step3));
  }

  void Step3() {
    views::MenuItemView* target_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    gfx::Point loc(1, target_menu->height() - 1);
    views::View::ConvertPointToScreen(target_menu, &loc);

    // Start a drag.
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x() + 10, loc.y(),
        CreateEventTask(this, &BookmarkBarViewTest22::Step4)));
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    window_->Close();
    window_ = NULL;

#if defined(OS_CHROMEOS)
    ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest22::Done)));
#else
    // There are no widgets to send the mouse release to.
    Done();
#endif
  }
};

// This test times out on Windows. TODO(pkotwicz): Find out why.
// It also flakes on CrOS and Linux : http://crbug/754188.
VIEW_TEST(BookmarkBarViewTest22, DISABLED_CloseSourceBrowserDuringDrag)

// Tests opening a context menu for a bookmark node from the keyboard.
class BookmarkBarViewTest23 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest23()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest23::Step4)) {
  }

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest23::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Navigate down to highlight the first menu item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        GetWidget()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false,
        false,  // No modifer keys
        CreateEventTask(this, &BookmarkBarViewTest23::Step3)));
  }

  void Step3() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Open the context menu via the keyboard.
    ASSERT_TRUE(ui_controls::SendKeyPress(GetWidget()->GetNativeWindow(),
                                          ui::VKEY_APPS, false, false, false,
                                          false  // No modifer keys
                                          ));
    // The BookmarkContextMenuNotificationObserver triggers Step4.
  }

  void Step4() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Select the first menu item (open).
    ui_test_utils::MoveMouseToCenterAndPress(
        menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest23::Step5));
  }

  void Step5() {
    EXPECT_EQ(navigator_.last_url(), model_->other_node()->GetChild(0)->url());
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

#if defined(USE_OZONE)
// ozone bringup - http://crbug.com/401304
#define MAYBE_ContextMenusKeyboard DISABLED_ContextMenusKeyboard
#else
#define MAYBE_ContextMenusKeyboard ContextMenusKeyboard
#endif
VIEW_TEST(BookmarkBarViewTest23, MAYBE_ContextMenusKeyboard)

// Test that pressing escape on a menu opened via the keyboard dismisses the
// context menu but not the parent menu.
class BookmarkBarViewTest24 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest24()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest24::Step4)) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = bb_view_->other_bookmarks_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest24::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Navigate down to highlight the first menu item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        GetWidget()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false,
        false,  // No modifer keys
        CreateEventTask(this, &BookmarkBarViewTest24::Step3)));
  }

  void Step3() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Open the context menu via the keyboard.
    ASSERT_TRUE(ui_controls::SendKeyPress(GetWidget()->GetNativeWindow(),
                                          ui::VKEY_APPS, false, false, false,
                                          false  // No modifer keys
                                          ));
    // The BookmarkContextMenuNotificationObserver triggers Step4.
  }

  void Step4() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send escape to close the context menu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest24::Step5)));
  }

  void Step5() {
    // The context menu should be closed but the parent menu should still be
    // showing.
    ASSERT_FALSE(bb_view_->GetContextMenu());

    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send escape to close the main menu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest24::Done)));
  }

  BookmarkContextMenuNotificationObserver observer_;
};

#if defined(USE_OZONE)
// ozone bringup - http://crbug.com/401304
#define MAYBE_ContextMenusKeyboardEscape DISABLED_ContextMenusKeyboardEscape
#else
#define MAYBE_ContextMenusKeyboardEscape ContextMenusKeyboardEscape
#endif
VIEW_TEST(BookmarkBarViewTest24, MAYBE_ContextMenusKeyboardEscape)

#if defined(OS_WIN)
// Tests that pressing the key KEYCODE closes the menu.
template <ui::KeyboardCode KEYCODE>
class BookmarkBarViewTest25 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest25::Step2));
    base::RunLoop().RunUntilIdle();
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != nullptr);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send KEYCODE key event, which should close the menu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), KEYCODE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest25::Step3)));
  }

  void Step3() {
    // Make sure menu is not showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == nullptr);

    Done();
  }
};

// Tests that pressing F10 system key closes the menu.
using BookmarkBarViewTest25F10 = BookmarkBarViewTest25<ui::VKEY_F10>;
VIEW_TEST(BookmarkBarViewTest25F10, F10ClosesMenu);

// Tests that pressing Alt system key closes the menu.
using BookmarkBarViewTest25Alt = BookmarkBarViewTest25<ui::VKEY_MENU>;
VIEW_TEST(BookmarkBarViewTest25Alt, AltClosesMenu);

// Tests that WM_CANCELMODE closes the menu.
class BookmarkBarViewTest26 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest26::Step2));
    base::RunLoop().RunUntilIdle();
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != nullptr);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send WM_CANCELMODE, which should close the menu. The message is sent
    // synchronously, however, we post a task to make sure that the message is
    // processed completely before finishing the test.
    ::SendMessage(
        GetWidget()->GetNativeView()->GetHost()->GetAcceleratedWidget(),
        WM_CANCELMODE, 0, 0);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&BookmarkBarViewTest26::Step3, base::Unretained(this)));
  }

  void Step3() {
    // Menu should not be showing anymore.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == nullptr);

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest26, CancelModeClosesMenu);
#endif

class BookmarkBarViewTest27 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::MIDDLE, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest27::Step2));
  }

 private:
  void Step2() {
    ASSERT_EQ(2u, navigator_.urls().size());
    EXPECT_EQ(navigator_.urls()[0],
              model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url());
    EXPECT_EQ(navigator_.urls()[1],
              model_->bookmark_bar_node()->GetChild(0)->GetChild(2)->url());
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest27, MiddleClickOnFolderOpensAllBookmarks);

#endif  // defined(OS_MACOSX)

class BookmarkBarViewTest28 : public BookmarkBarViewEventTestBase {
 protected:
#if defined(OS_MACOSX)
  const ui_controls::AcceleratorState kAccelatorState = ui_controls::kCommand;
#else
  const ui_controls::AcceleratorState kAccelatorState = ui_controls::kControl;
#endif  // defined(OS_MACOSX)

  void DoTestOnMessageLoop() override {
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::UP | ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest28::Step2), kAccelatorState);
  }

 private:
  void Step2() {
    ASSERT_EQ(2u, navigator_.urls().size());
    EXPECT_EQ(navigator_.urls()[0],
              model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url());
    EXPECT_EQ(navigator_.urls()[1],
              model_->bookmark_bar_node()->GetChild(0)->GetChild(2)->url());
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest28, ClickWithModifierOnFolderOpensAllBookmarks);
