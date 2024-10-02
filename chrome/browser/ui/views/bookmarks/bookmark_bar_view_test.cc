// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/bookmarks/test_bookmark_navigation_wrapper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_switches.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_WIN)
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

#if !BUILDFLAG(IS_MAC)

// Waits for a views::Widget dialog to show up.
class DialogWaiter : public aura::EnvObserver, public views::WidgetObserver {
 public:
  DialogWaiter() { aura::Env::GetInstance()->AddObserver(this); }

  DialogWaiter(const DialogWaiter&) = delete;
  DialogWaiter& operator=(const DialogWaiter&) = delete;

  ~DialogWaiter() override { aura::Env::GetInstance()->RemoveObserver(this); }

  views::Widget* WaitForDialog() {
    if (dialog_created_) {
      return dialog_;
    }
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    return dialog_;
  }

 private:
  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {
    if (dialog_) {
      return;
    }
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
    if (!widget || !widget->IsDialogBox()) {
      return;
    }
    dialog_ = widget;
    dialog_->AddObserver(this);
  }

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    CHECK_EQ(dialog_, widget);
    if (active) {
      dialog_created_ = true;
      dialog_->RemoveObserver(this);
      if (!quit_closure_.is_null()) {
        quit_closure_.Run();
      }
    }
  }

  bool dialog_created_ = false;
  raw_ptr<views::Widget> dialog_ = nullptr;
  base::RepeatingClosure quit_closure_;
};

// Waits for a dialog to terminate.
class DialogCloseWaiter : public views::WidgetObserver {
 public:
  explicit DialogCloseWaiter(views::Widget* dialog) : dialog_closed_(false) {
    dialog->AddObserver(this);
  }

  DialogCloseWaiter(const DialogCloseWaiter&) = delete;
  DialogCloseWaiter& operator=(const DialogCloseWaiter&) = delete;

  ~DialogCloseWaiter() override {
    // It is not necessary to remove |this| from the dialog's observer, since
    // the dialog is destroyed before this waiter.
  }

  void WaitForDialogClose() {
    if (dialog_closed_) {
      return;
    }
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    dialog_closed_ = true;
    if (!quit_closure_.is_null()) {
      quit_closure_.Run();
    }
  }

  bool dialog_closed_;
  base::RepeatingClosure quit_closure_;
};

// Waits for a views::Widget to receive a Tab key.
class TabKeyWaiter : public ui::EventHandler {
 public:
  explicit TabKeyWaiter(views::Widget* widget)
      : widget_(widget), received_tab_(false) {
    widget_->GetNativeWindow()->AddPreTargetHandler(this);
  }

  TabKeyWaiter(const TabKeyWaiter&) = delete;
  TabKeyWaiter& operator=(const TabKeyWaiter&) = delete;

  ~TabKeyWaiter() override {
    widget_->GetNativeWindow()->RemovePreTargetHandler(this);
  }

  void WaitForTab() {
    if (received_tab_) {
      return;
    }
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() == ui::EventType::kKeyReleased &&
        event->key_code() == ui::VKEY_TAB) {
      received_tab_ = true;
      if (!quit_closure_.is_null()) {
        quit_closure_.Run();
      }
    }
  }

  raw_ptr<views::Widget> widget_;
  bool received_tab_;
  base::RepeatingClosure quit_closure_;
};

void MoveMouseAndPress(const gfx::Point& screen_pos,
                       ui_controls::MouseButton button,
                       int button_state,
                       base::OnceClosure closure) {
  ASSERT_TRUE(ui_controls::SendMouseMove(screen_pos.x(), screen_pos.y()));
  ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(button, button_state,
                                                         std::move(closure)));
}

#endif  // !BUILDFLAG(IS_MAC)

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  TestingPageNavigator() {}

  TestingPageNavigator(const TestingPageNavigator&) = delete;
  TestingPageNavigator& operator=(const TestingPageNavigator&) = delete;

  ~TestingPageNavigator() override {}

  WebContents* OpenURL(const OpenURLParams& params,
                       base::OnceCallback<void(content::NavigationHandle&)>
                           navigation_handle_callback) override {
    urls_.push_back(params.url);
    transitions_.push_back(params.transition);
    return nullptr;
  }

  const std::vector<GURL>& urls() const { return urls_; }
  GURL last_url() const { return urls_.empty() ? GURL() : urls_.back(); }

  ui::PageTransition last_transition() const {
    return transitions_.empty() ? ui::PAGE_TRANSITION_LINK
                                : transitions_.back();
  }

 private:
  std::vector<GURL> urls_;
  std::vector<ui::PageTransition> transitions_;
};

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
      : scoped_testing_factory_installer_(
            base::BindRepeating(&gcm::FakeGCMProfileService::Build)) {}
  ~BookmarkBarViewEventTestBase() override = default;

  void SetUp() override {
    InitializeActionIdStringMapping();
    content_client_ = std::make_unique<ChromeContentClient>();
    content::SetContentClient(content_client_.get());
    browser_content_client_ = std::make_unique<ChromeContentBrowserClient>();
    content::SetBrowserClientForTesting(browser_content_client_.get());

    views::MenuController::TurnOffMenuSelectionHoldForTest();
    BookmarkBarView::DisableAnimationsForTesting(true);
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        BookmarkMergedSurfaceServiceFactory::GetInstance(),
        BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    profile_->GetPrefs()->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);

    Browser::CreateParams native_params(profile_.get(), true);
    browser_ = CreateBrowserWithTestWindowForParams(native_params);

    model_->DisableWritesToDiskForTest();
    PinnedToolbarActionsModel::Get(browser_->profile())
        ->UpdatePinnedState(kActionShowChromeLabs, false);

    AddTestData(CreateBigMenu());

    // Create the Widget. Note the initial size is given by
    // GetPreferredSizeForContents() during initialization. This occurs after
    // the WidgetDelegate provides |bb_view_| as the contents view and adds it
    // to the hierarchy.
    ViewEventTestBase::SetUp();
    ASSERT_TRUE(bb_view_);

    static_cast<TestBrowserWindow*>(browser_->window())
        ->SetNativeWindow(window()->GetNativeWindow());

    chrome::BookmarkNavigationWrapper::SetInstanceForTesting(&wrapper_);

    // Verify the layout triggered by the initial size preserves the overflow
    // state calculated in GetPreferredSizeForContents().
    EXPECT_TRUE(GetBookmarkButton(5)->GetVisible());
    EXPECT_FALSE(GetBookmarkButton(6)->GetVisible());
  }

  void TearDown() override {
    if (window()) {
      // Closing the window ensures |bb_view_| is deleted, which must happen
      // before |model_| is deleted (which happens when |profile_| is reset).
      window()->CloseNow();
    }
    actions::ActionIdMap::ResetMapsForTesting();

    browser_->tab_strip_model()->CloseAllTabs();
    browser_.reset();
    profile_.reset();

    // Run the message loop to ensure we delete all tasks and fully shut down.
    base::RunLoop().RunUntilIdle();

    ViewEventTestBase::TearDown();

    BookmarkBarView::DisableAnimationsForTesting(false);
    constrained_window::SetConstrainedWindowViewsClient(nullptr);

    browser_content_client_.reset();
    content_client_.reset();
    content::SetContentClient(nullptr);
  }

 protected:
  std::unique_ptr<views::View> CreateContentsView() override {
    auto bb_view = std::make_unique<BookmarkBarView>(browser_.get(), nullptr);
    // Real bookmark bars get a BookmarkBarViewBackground. Set an opaque
    // background here just to avoid triggering subpixel rendering issues.
    bb_view->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
    bb_view->SetPageNavigator(&navigator_);
    bb_view_ = bb_view.get();
    return bb_view;
  }

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
      bb_view_->DeprecatedLayoutImmediately();
    } while (bb_view_->bookmark_buttons_[6]->GetVisible());
    return size;
  }

  const views::LabelButton* GetBookmarkButton(size_t view_index) const {
    return bb_view_->bookmark_buttons_[view_index];
  }
  views::LabelButton* GetBookmarkButton(size_t view_index) {
    return const_cast<views::LabelButton*>(
        std::as_const(*this).GetBookmarkButton(view_index));
  }

  // See comment above class description for what this does.
  virtual bool CreateBigMenu() { return false; }

  bool MenuIsShowing(const views::MenuItemView* menu) const {
    if (!menu) {
      return false;
    }
    const views::SubmenuView* const submenu = menu->GetSubmenu();
    return submenu && submenu->IsShowing();
  }
  // Sugar for "The main menu is showing". Can't use a default arg to the above
  // since `bb_view_` is non-static.
  bool MenuIsShowing() const { return MenuIsShowing(bb_view_->GetMenu()); }

  // Clicks `view`, which is expected to open a top-level menu from the bookmark
  // bar, then calls `callback`. This is a helper not only because it is common,
  // but because there are subtleties around message loop management (see
  // comments in implementation details) that not everyone should need to worry
  // about.
  void OpenMenuByClick(views::View* view, base::OnceClosure callback) {
    ui_test_utils::MoveMouseToCenterAndPress(
        view, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        // On Windows, opening a new top-level menu can produce a new mouse move
        // event (to the same screen coordinates). Leaving this event in the
        // queue can cause test failures. So once we're called back and the menu
        // is showing, post another task to the "real" `callback` to give the
        // message loop a chance to process this if necessary. This is harmless
        // on other platforms, so we don't #if it.
        base::BindOnce(&BookmarkBarViewEventTestBase::RunTestMethod,
                       base::Unretained(this),
                       base::BindLambdaForTesting([&, callback = std::move(
                                                          callback)]() mutable {
                         ASSERT_TRUE(MenuIsShowing());
                         // `callback` must be delayed by at least one tick of
                         // the system timer to avoid the chance of posting it
                         // before checking the OS event queue. In principle
                         // this could vary but in practice it's always 15.625
                         // ms (1/64 sec). Round to 20 Just Because.
                         base::SingleThreadTaskRunner::GetCurrentDefault()
                             ->PostDelayedTask(FROM_HERE, std::move(callback),
                                               base::Milliseconds(20));
                       })));
  }

  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;

  raw_ptr<BookmarkModel, AcrossTasksDanglingUntriaged> model_ = nullptr;
  raw_ptr<BookmarkBarView, AcrossTasksDanglingUntriaged> bb_view_ = nullptr;
  TestingPageNavigator navigator_;
  TestingBookmarkNavigationWrapper wrapper_;

 private:
  void AddTestData(bool big_menu) {
    const BookmarkNode* bb_node = model_->bookmark_bar_node();
    std::string test_base = "file:///c:/tmp/";
    const BookmarkNode* f1 = model_->AddFolder(bb_node, 0, u"F1");
    model_->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddFolder(f1, 1, u"F11");
    model_->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model_->AddURL(f1, 2, u"f1b", GURL(test_base + "f1b"));
    if (big_menu) {
      for (size_t i = 1; i <= 100; ++i) {
        model_->AddURL(f1, i + 1, u"f" + base::NumberToString16(i),
                       GURL(test_base + "f" + base::NumberToString(i)));
      }
    }
    model_->AddURL(bb_node, 1, u"a", GURL(test_base + "a"));
    model_->AddURL(bb_node, 2, u"b", GURL(test_base + "b"));
    model_->AddURL(bb_node, 3, u"c", GURL(test_base + "c"));
    model_->AddURL(bb_node, 4, u"d", GURL(test_base + "d"));
    model_->AddFolder(bb_node, 5, u"F2");
    model_->AddURL(bb_node, 6, u"d", GURL(test_base + "d"));

    model_->AddURL(model_->other_node(), 0, u"oa", GURL(test_base + "oa"));
    const BookmarkNode* of = model_->AddFolder(model_->other_node(), 1, u"OF");
    model_->AddURL(of, 0, u"ofa", GURL(test_base + "ofa"));
    model_->AddURL(of, 1, u"ofb", GURL(test_base + "ofb"));
    const BookmarkNode* of2 =
        model_->AddFolder(model_->other_node(), 2, u"OF2");
    model_->AddURL(of2, 0, u"of2a", GURL(test_base + "of2a"));
    model_->AddURL(of2, 1, u"of2b", GURL(test_base + "of2b"));
  }

  std::unique_ptr<ChromeContentClient> content_client_;
  std::unique_ptr<ChromeContentBrowserClient> browser_content_client_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

class BookmarkBarViewDragTestBase : public BookmarkBarViewEventTestBase,
                                    public BookmarkBarViewObserver,
                                    public views::WidgetObserver {
 public:
  BookmarkBarViewDragTestBase() = default;
  ~BookmarkBarViewDragTestBase() override = default;

  // views::WidgetObserver:
  void OnWidgetDragWillStart(views::Widget* widget) override {
    const gfx::Point target = GetDragTargetInScreen();
    GetDragTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove),
                       target.x(), target.y(), ui_controls::kNoWindowHint));
  }

  void OnWidgetDragComplete(views::Widget* widget) override {
    absl::Cleanup done = [&] { Done(); };

    // All drag tests drag node f1a, so at the end of the test, if the node was
    // dropped where it was expected, the dropped node should have f1a's URL.
    const BookmarkNode* dropped_node = GetDroppedNode();
    ASSERT_TRUE(dropped_node);
    EXPECT_EQ(f1a_url_, dropped_node->url());
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    if (widget == window()) {
      DCHECK(bookmark_bar_observation_.IsObserving());
      bookmark_bar_observation_.Reset();
    }
  }

  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_observations_.RemoveObservation(widget);
  }

 protected:
  // BookmarkBarViewEventTestBase:
  void DoTestOnMessageLoop() override {
    widget_observations_.AddObservation(window());
    bookmark_bar_observation_.Observe(bb_view_.get());

    // Record the URL for node f1a.
    const auto& f1 = model_->bookmark_bar_node()->children().front();
    f1a_url_ = f1->children().front()->url();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    OpenMenuByClick(
        GetBookmarkButton(0),
        CreateEventTask(this, &BookmarkBarViewDragTestBase::OnMenuOpened));
  }

  virtual void OnMenuOpened() {
    // The menu is showing, so it has a widget we can observe now.
    views::SubmenuView* submenu = bb_view_->GetMenu()->GetSubmenu();
    widget_observations_.AddObservation(submenu->GetWidget());

    // Move mouse to center of node f1a and press button.
    views::View* f1a = submenu->GetMenuItemAt(0);
    ASSERT_NE(nullptr, f1a);
    ui_test_utils::MoveMouseToCenterAndPress(
        f1a, ui_controls::LEFT, ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewDragTestBase::StartDrag));
  }

  virtual void OnDragEntered() {
    // Drop the element, which should result in calling OnWidgetDragComplete().
    GetDragTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseEvents),
                       ui_controls::LEFT, ui_controls::UP,
                       ui_controls::kNoAccelerator,
                       ui_controls::kNoWindowHint));
  }

  // Called after the drag ends; returns the node the test thinks should be the
  // dropped node.  This is used to verify that the dragged node was dropped in
  // the expected position.
  virtual const BookmarkNode* GetDroppedNode() const = 0;

  // Returns the point the node should be dragged to, in screen coordinates.
  virtual gfx::Point GetDragTargetInScreen() const = 0;

  void SetStopDraggingView(const views::View* view) {
    views::DropHelper::SetDragEnteredCallbackForTesting(
        view, base::BindRepeating(&BookmarkBarViewDragTestBase::OnDragEntered,
                                  base::Unretained(this)));
  }

 private:
  void StartDrag() {
    const views::View* drag_view =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(0);
    const gfx::Point current_position =
        ui_test_utils::GetCenterInScreenCoordinates(drag_view);
    EXPECT_TRUE(ui_controls::SendMouseMove(current_position.x() + 10,
                                           current_position.y()));
  }

  GURL f1a_url_;
  base::ScopedObservation<BookmarkBarView, BookmarkBarViewObserver>
      bookmark_bar_observation_{this};
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
};

#if !BUILDFLAG(IS_MAC)
// The following tests were not enabled on Mac before. Consider enabling those
// that are able to run on Mac (https://crbug.com/845342).

// Clicks on first menu, makes sure button is depressed. Moves mouse to first
// child, clicks it and makes sure a navigation occurs.
class BookmarkBarViewTest1 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest1::Step2));
  }

 private:
  void Step2() {
    // Button should be depressed.
    views::LabelButton* button = GetBookmarkButton(0);
    ASSERT_EQ(views::Button::STATE_PRESSED, button->GetState());

    // Click on the 2nd menu item (A URL).
    views::MenuItemView* menu_to_select =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        menu_to_select, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step3));
  }

  void Step3() {
    // We should have navigated to URL f1a.
    const auto& f1 = model_->bookmark_bar_node()->children().front();
    ASSERT_EQ(wrapper_.last_url(), f1->children().front()->url());
    ASSERT_FALSE(PageTransitionIsWebTriggerable(wrapper_.last_transition()));

    // Make sure button is no longer pushed.
    views::LabelButton* button = GetBookmarkButton(0);
    ASSERT_EQ(views::Button::STATE_NORMAL, button->GetState());

    ASSERT_FALSE(MenuIsShowing());
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
VIEW_TEST(BookmarkBarViewTest1, MAYBE_Basic)

// Brings up menu, clicks on empty space and make sure menu hides.
class BookmarkBarViewTest2 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest2::Step2));
  }

 private:
  void Step2() {
    // Click on 0x0, which should trigger closing menu.
    // NOTE: this code assume there is a left margin, which is currently
    // true. If that changes, this code will need to find another empty space
    // to press the mouse on.
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
    ASSERT_FALSE(MenuIsShowing());

    // Make sure button is no longer pushed.
    views::LabelButton* button = GetBookmarkButton(0);
    ASSERT_EQ(views::Button::STATE_NORMAL, button->GetState());

    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
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
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest3::Step2));
  }

 private:
  void Step2() {
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_NE(nullptr, child_menu);

    // Click on second child, which has a submenu.
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step3));
  }

  void Step3() {
    views::SubmenuView* submenu = bb_view_->GetMenu()->GetSubmenu();
    ASSERT_TRUE(MenuIsShowing(submenu->GetMenuItemAt(2)));

    // Click on third child, which has a submenu too.
    views::MenuItemView* child_menu = submenu->GetMenuItemAt(3);
    ASSERT_NE(nullptr, child_menu);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step4));
  }

  void Step4() {
    views::MenuItemView* menu = bb_view_->GetMenu();
    views::SubmenuView* submenu = menu->GetSubmenu();
    ASSERT_FALSE(MenuIsShowing(submenu->GetMenuItemAt(2)));
    ASSERT_TRUE(MenuIsShowing(submenu->GetMenuItemAt(3)));

    // Nothing should have been selected.
    EXPECT_EQ(GURL(), wrapper_.last_url());

    menu->GetMenuController()->Cancel(views::MenuController::ExitType::kAll);
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Submenus DISABLED_Submenus
#else
#define MAYBE_Submenus Submenus
#endif
VIEW_TEST(BookmarkBarViewTest3, MAYBE_Submenus)

// Observer that posts a task upon the context menu creation.
// This is necessary for Linux as the context menu has to check the clipboard,
// which invokes the event loop.
// Because |task| is a OnceClosure, callers should use a separate observer
// instance for each successive context menu creation they wish to observe.
class BookmarkContextMenuNotificationObserver {
 public:
  explicit BookmarkContextMenuNotificationObserver(base::OnceClosure task)
      : task_(std::move(task)) {
    BookmarkContextMenu::InstallPreRunCallback(base::BindOnce(
        &BookmarkContextMenuNotificationObserver::ScheduleCallback,
        base::Unretained(this)));
  }

  BookmarkContextMenuNotificationObserver(
      const BookmarkContextMenuNotificationObserver&) = delete;
  BookmarkContextMenuNotificationObserver& operator=(
      const BookmarkContextMenuNotificationObserver&) = delete;

  void ScheduleCallback() {
    DCHECK(!task_.is_null());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(task_));
  }

 private:
  base::OnceClosure task_;
};

// Opens a bookmark folder, right clicks on the first bookmark to get a context
// menu, and selects the first menu item (open).
class BookmarkBarViewTest4 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest4()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest4::Step3)) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest4::Step2));
  }

 private:
  void Step2() {
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_NE(nullptr, child_menu);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        base::OnceClosure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    // Select the first menu item (open).
    ui_test_utils::MoveMouseToCenterAndPress(
        context_menu->GetSubmenu()->GetMenuItemAt(1), ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step4));
  }

  void Step4() {
    EXPECT_EQ(wrapper_.last_url(),
              model_->other_node()->children().front()->url());
    ASSERT_FALSE(PageTransitionIsWebTriggerable(wrapper_.last_transition()));
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ContextMenus DISABLED_ContextMenus
#else
#define MAYBE_ContextMenus ContextMenus
#endif
VIEW_TEST(BookmarkBarViewTest4, MAYBE_ContextMenus)

// Tests drag and drop within the same menu.
class BookmarkBarViewTest5 : public BookmarkBarViewDragTestBase {
 protected:
  // BookmarkBarViewDragTestBase:
  void OnMenuOpened() override {
    BookmarkBarViewDragTestBase::OnMenuOpened();

    // Cause the second menu item to trigger a mouse up when dragged over.
    SetStopDraggingView(bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1));
  }

  const BookmarkNode* GetDroppedNode() const override {
    const auto& bar_items = model_->bookmark_bar_node()->children();
    EXPECT_FALSE(bar_items.empty());
    if (bar_items.empty()) {
      return nullptr;
    }
    const auto& f1_items = bar_items.front()->children();
    EXPECT_GE(f1_items.size(), 2u);
    return (f1_items.size() >= 2) ? f1_items[1].get() : nullptr;
  }

  gfx::Point GetDragTargetInScreen() const override {
    const views::View* target_view =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    gfx::Point target(target_view->width() / 2, target_view->height() - 1);
    views::View::ConvertPointToScreen(target_view, &target);
    return target;
  }
};

VIEW_TEST(BookmarkBarViewTest5, DND)

// Tests holding mouse down on overflow button, dragging such that menu pops up
// then selecting an item.
class BookmarkBarViewTest6 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Press the mouse button on the overflow button. Don't release it though.
    views::LabelButton* button = bb_view_->overflow_button();
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest6::Step2));
  }

 private:
  void Step2() {
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_NE(nullptr, child_menu);

    // Move mouse to center of menu and release mouse.
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest6::Step3));
  }

  void Step3() {
    ASSERT_EQ(wrapper_.last_url(),
              model_->bookmark_bar_node()->children()[6]->url());
    ASSERT_FALSE(PageTransitionIsWebTriggerable(wrapper_.last_transition()));
    Done();
  }
};

#if BUILDFLAG(IS_OZONE_WAYLAND) || BUILDFLAG(IS_WIN)
// TODO (crbug/1523247): This test is failing under wayland and Windows. This
// skips it until it can be fixed.
#define MAYBE_OpenMenuOnClickAndHold DISABLED_OpenMenuOnClickAndHold
#else
#define MAYBE_OpenMenuOnClickAndHold OpenMenuOnClickAndHold
#endif  // BUILDFLAG(IS_OZONE_WAYLAND) || BUILDFLAG(IS_WIN)
// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(BookmarkBarViewTest6, MAYBE_OpenMenuOnClickAndHold)

// Tests drag and drop to different menu.
class BookmarkBarViewTest7 : public BookmarkBarViewDragTestBase {
 public:
  // BookmarkBarViewDragTestBase:
  void OnDropMenuShown() override {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(MenuIsShowing(drop_menu));

    // The button should be highlighted now.
    EXPECT_EQ(views::Button::STATE_PRESSED,
              bb_view_->all_bookmarks_button()->GetState());

    // Cause the target view to trigger a mouse up when dragged over.
    const views::View* target_view = drop_menu->GetSubmenu()->GetMenuItemAt(1);
    SetStopDraggingView(target_view);

    // Drag to the top of the target view. Use 2 instead of 0 for target.y
    // so that the mouse event will be in the target view for device scale
    // factors other than 1.0.
    gfx::Point target(target_view->width() / 2, 2);
    views::View::ConvertPointToScreen(target_view, &target);
    GetDragTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove),
                       target.x(), target.y(), ui_controls::kNoWindowHint));
  }

  void OnWidgetDragComplete(views::Widget* widget) override {
    // The button should be in normal state now.
    EXPECT_EQ(views::Button::STATE_NORMAL,
              bb_view_->all_bookmarks_button()->GetState());

    BookmarkBarViewDragTestBase::OnWidgetDragComplete(widget);
  }

 protected:
  // BookmarkBarViewDragTestBase:
  const BookmarkNode* GetDroppedNode() const override {
    const auto& other_bookmarks = model_->other_node()->children();
    EXPECT_FALSE(other_bookmarks.empty());
    return other_bookmarks.empty() ? nullptr : other_bookmarks.front().get();
  }

  gfx::Point GetDragTargetInScreen() const override {
    return ui_test_utils::GetCenterInScreenCoordinates(
        bb_view_->all_bookmarks_button());
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DNDToDifferentMenu DISABLED_DNDToDifferentMenu
#else
#define MAYBE_DNDToDifferentMenu DNDToDifferentMenu
#endif
VIEW_TEST(BookmarkBarViewTest7, MAYBE_DNDToDifferentMenu)

// Drags from one menu to next so that original menu closes, then back to
// original menu.
class BookmarkBarViewTest8 : public BookmarkBarViewDragTestBase {
 public:
  // BookmarkBarViewDragTestBase:
  void OnDropMenuShown() override {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(MenuIsShowing(drop_menu));

    const views::View* target_view;
    const auto* controller =
        static_cast<const BookmarkMenuController*>(drop_menu->GetDelegate());
    if (controller->node() == model_->other_node()) {
      // Now drag back over first menu.
      target_view = GetBookmarkButton(0);
    } else {
      // Drag to folder F11.
      target_view = drop_menu->GetSubmenu()->GetMenuItemAt(1);

      // Cause folder F11 to trigger a mouse up when dragged over.
      SetStopDraggingView(target_view);
    }
    const gfx::Point target =
        ui_test_utils::GetCenterInScreenCoordinates(target_view);
    GetDragTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove),
                       target.x(), target.y(), ui_controls::kNoWindowHint));
  }

 protected:
  // BookmarkBarViewDragTestBase:
  const BookmarkNode* GetDroppedNode() const override {
    const auto& bar_items = model_->bookmark_bar_node()->children();
    EXPECT_FALSE(bar_items.empty());
    if (bar_items.empty()) {
      return nullptr;
    }
    const auto& f1_items = bar_items.front()->children();
    EXPECT_FALSE(f1_items.empty());
    if (f1_items.empty()) {
      return nullptr;
    }
    const auto& f11_items = f1_items.front()->children();
    EXPECT_GE(f11_items.size(), 2u);
    return (f11_items.size() >= 2) ? f11_items[1].get() : nullptr;
  }

  gfx::Point GetDragTargetInScreen() const override {
    return ui_test_utils::GetCenterInScreenCoordinates(
        bb_view_->all_bookmarks_button());
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DNDBackToOriginatingMenu DISABLED_DNDBackToOriginatingMenu
#else
#define MAYBE_DNDBackToOriginatingMenu DNDBackToOriginatingMenu
#endif
VIEW_TEST(BookmarkBarViewTest8, MAYBE_DNDBackToOriginatingMenu)

// Moves the mouse over the scroll button and makes sure we get scrolling.
class BookmarkBarViewTest9 : public BookmarkBarViewEventTestBase {
 protected:
  bool CreateBigMenu() override { return true; }

  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest9::Step2));
  }

 private:
  void Step2() {
    views::SubmenuView* submenu = bb_view_->GetMenu()->GetSubmenu();
    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(submenu->GetMenuItemAt(0), &menu_loc);
    start_y_ = menu_loc.y();

    // Get notified when the scroll up button becomes visible.
    views::View* parent = submenu->parent();
    while (parent &&
           !views::IsViewClass<views::MenuScrollViewContainer>(parent)) {
      parent = parent->parent();
    }
    auto* scroll_container =
        views::AsViewClass<views::MenuScrollViewContainer>(parent);
    ASSERT_NE(nullptr, scroll_container);
    views::View* scroll_up_button = scroll_container->scroll_up_button();
    ASSERT_NE(nullptr, scroll_up_button);
    ASSERT_FALSE(scroll_up_button->GetVisible());
    subscription_ = scroll_up_button->AddVisibleChangedCallback(
        // We'd like to pass CreateEventTask(this, &Step3) directly, but that
        // produces a OnceClosure and AddVisibleChangedCallback() requires a
        // RepeatingClosure.
        base::BindLambdaForTesting([&] {
          CreateEventTask(this, &BookmarkBarViewTest9::Step3).Run();
        }));

    // Move the mouse over the scroll down button.
    views::View* scroll_down_button = scroll_container->scroll_down_button();
    ASSERT_NE(nullptr, scroll_down_button);
    ASSERT_TRUE(scroll_down_button->GetVisible());
    const gfx::Point loc =
        ui_test_utils::GetCenterInScreenCoordinates(scroll_down_button);
    const gfx::NativeWindow window =
        scroll_down_button->GetWidget()->GetNativeWindow();
    // TODO(pkasting): As of November 2023, LaCrOS fails without this first
    // mouse move, which seems wrong.
    ASSERT_TRUE(ui_controls::SendMouseMove(loc.x() - 1, loc.y() - 1, window));
    ASSERT_TRUE(ui_controls::SendMouseMove(loc.x(), loc.y(), window));
  }

  void Step3() {
    subscription_.reset();

    ASSERT_TRUE(MenuIsShowing());

    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(0), &menu_loc);
    ASSERT_NE(start_y_, menu_loc.y());

    // The Cancel() call in Step4() will synchronously delete the view holding
    // the callback list that is currently calling us, so it must be done after
    // the current call stack unwinds.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, CreateEventTask(this, &BookmarkBarViewTest9::Step4));
  }

  void Step4() {
    bb_view_->GetMenu()->GetMenuController()->Cancel(
        views::MenuController::ExitType::kAll);
    Done();
  }

  int start_y_ = 0;
  std::optional<base::CallbackListSubscription> subscription_;
};

// Something about coordinate transforms is wrong on Wayland -- attempting to
// hover the scroll buttons sends the mouse to the wrong location, so it never
// winds up over the button, so the test times out.
// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_OZONE_WAYLAND) || BUILDFLAG(IS_WIN)
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
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest10::Step2));
  }

 private:
  void Step2() {
    // Send a down event, which should select the first item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step3)));
  }

  void Step3() {
    ASSERT_TRUE(MenuIsShowing());
    ASSERT_TRUE(
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send a key down event, which should select the next item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step4)));
  }

  void Step4() {
    ASSERT_TRUE(MenuIsShowing());
    views::SubmenuView* submenu = bb_view_->GetMenu()->GetSubmenu();
    ASSERT_FALSE(submenu->GetMenuItemAt(0)->IsSelected());
    ASSERT_TRUE(submenu->GetMenuItemAt(1)->IsSelected());

    // Send a right arrow to force the menu to open.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_RIGHT, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step5)));
  }

  void Step5() {
    ASSERT_TRUE(MenuIsShowing());
    views::MenuItemView* menu_item =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(menu_item->IsSelected());
    ASSERT_TRUE(MenuIsShowing(menu_item));

    // Send a left arrow to close the submenu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_LEFT, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step6)));
  }

  void Step6() {
    ASSERT_TRUE(MenuIsShowing());
    views::MenuItemView* menu_item =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(menu_item->IsSelected());
    ASSERT_FALSE(MenuIsShowing(menu_item));

    // Send a down arrow to go down to f1b.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step7)));
  }

  void Step7() {
    ASSERT_TRUE(MenuIsShowing());
    ASSERT_TRUE(
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(2)->IsSelected());

    // Send a down arrow to wrap back to f1a.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step8)));
  }

  void Step8() {
    ASSERT_TRUE(MenuIsShowing());
    ASSERT_TRUE(
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send enter, which should select the item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_RETURN, false, false, false,
        false, CreateEventTask(this, &BookmarkBarViewTest10::Step9)));
  }

  void Step9() {
    const auto& f1 = model_->bookmark_bar_node()->children().front();
    ASSERT_EQ(wrapper_.last_url(), f1->children().front()->url());
    ASSERT_FALSE(PageTransitionIsWebTriggerable(wrapper_.last_transition()));
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_KeyEvents DISABLED_KeyEvents
#else
#define MAYBE_KeyEvents KeyEvents
#endif
VIEW_TEST(BookmarkBarViewTest10, MAYBE_KeyEvents)

// Make sure the menu closes with the following sequence: show menu, show
// context menu, close context menu (via escape), then click else where. This
// effectively verifies we maintain mouse capture after the context menu is
// hidden.
class BookmarkBarViewTest11 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest11()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest11::Step3)) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest11::Step2));
  }

 private:
  void Step2() {
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_NE(nullptr, child_menu);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        base::OnceClosure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    // Send escape so that the context menu hides.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false,
        false, CreateEventTask(this, &BookmarkBarViewTest11::Step4)));
  }

  void Step4() {
    ASSERT_FALSE(MenuIsShowing(bb_view_->GetContextMenu()));
    ASSERT_TRUE(MenuIsShowing());

    // Now click on empty space.
    gfx::Point mouse_loc;
    views::View::ConvertPointToScreen(bb_view_, &mouse_loc);
    ASSERT_TRUE(ui_controls::SendMouseMove(mouse_loc.x(), mouse_loc.y()));
    ASSERT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP | ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest11::Step5)));
  }

  void Step5() {
    ASSERT_FALSE(MenuIsShowing());
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// TODO(crbug.com/40282036): Fails on latest versions of ChromeOS.
// TODO(crbug.com/337055374): Flaky on Windows.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_CloseMenuAfterClosingContextMenu \
  DISABLED_CloseMenuAfterClosingContextMenu
#else
#define MAYBE_CloseMenuAfterClosingContextMenu CloseMenuAfterClosingContextMenu
#endif
VIEW_TEST(BookmarkBarViewTest11, MAYBE_CloseMenuAfterClosingContextMenu)

// Tests showing a modal dialog from a context menu.
class BookmarkBarViewTest12 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Open up the "All Bookmarks" folder.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest12::Step2));
  }

 private:
  void Step2() {
    // Right click on the second child (a folder) to get its context menu.
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_NE(nullptr, child_menu);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step3));
  }

  void Step3() {
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    // Select the first item in the context menu (open all).
    views::MenuItemView* child_menu =
        context_menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_NE(nullptr, child_menu);

    // Click and wait until the dialog box appears.
    auto dialog_waiter = std::make_unique<DialogWaiter>();
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        base::BindOnce(&BookmarkBarViewTest12::Step4, base::Unretained(this),
                       std::move(dialog_waiter)));
  }

  void Step4(std::unique_ptr<DialogWaiter> waiter) {
    views::Widget* dialog = waiter->WaitForDialog();
    waiter.reset();

    // Press tab to give focus to the cancel button. Wait until the widget
    // receives the tab key.
    TabKeyWaiter tab_waiter(dialog);
    ASSERT_TRUE(ui_controls::SendKeyPress(
        dialog->GetNativeWindow(), ui::VKEY_TAB, false, false, false, false));
    tab_waiter.WaitForTab();

    // For some reason return isn't processed correctly unless we delay.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BookmarkBarViewTest12::Step5, base::Unretained(this),
                       base::Unretained(dialog)),
        base::Seconds(1));
  }

  void Step5(views::Widget* dialog) {
    DialogCloseWaiter waiter(dialog);
    // And press enter so that the cancel button is selected.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        dialog->GetNativeWindow(), ui::VKEY_RETURN, false, false, false, false,
        base::OnceClosure()));
    waiter.WaitForDialogClose();
    Done();
  }

  base::AutoReset<size_t> prompt_immediately_resetter_{
      &chrome::kNumBookmarkUrlsBeforePrompting, 1};
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
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
      : observer_(CreateEventTask(this, &BookmarkBarViewTest13::Step3)) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest13::Step2));
  }

 private:
  void Step2() {
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_NE(nullptr, child_menu);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        base::OnceClosure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    // Find the first separator.
    views::SubmenuView* submenu = context_menu->GetSubmenu();
    const auto i = base::ranges::find_if_not(
        submenu->children(), views::IsViewClass<views::MenuItemView>);
    ASSERT_FALSE(i == submenu->children().end());

    // Click on the separator. Clicking on the separator shouldn't visually
    // change anything.
    ui_test_utils::MoveMouseToCenterAndPress(
        *i, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step4));
  }

  void Step4() {
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    // Select the first context menu item.
    ui_test_utils::MoveMouseToCenterAndPress(
        context_menu->GetSubmenu()->GetMenuItemAt(1), ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        base::BindOnce(&ViewEventTestBase::Done, base::Unretained(this)));
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ClickOnContextMenuSeparator DISABLED_ClickOnContextMenuSeparator
#else
#define MAYBE_ClickOnContextMenuSeparator ClickOnContextMenuSeparator
#endif
VIEW_TEST(BookmarkBarViewTest13, MAYBE_ClickOnContextMenuSeparator)

// Makes sure right clicking on a folder on the bookmark bar doesn't result in
// both a context menu and showing the menu.
class BookmarkBarViewTest14 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest14()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest14::Step2)) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // right mouse button.
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        base::OnceClosure());
    // Step2 will be invoked by BookmarkContextMenuNotificationObserver.
  }

 private:
  void Step2() {
    ASSERT_FALSE(MenuIsShowing());

    // Send escape so that the context menu hides.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false,
        false,
        base::BindOnce(&ViewEventTestBase::Done, base::Unretained(this))));
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ContextMenus2 DISABLED_ContextMenus2
#else
#define MAYBE_ContextMenus2 ContextMenus2
#endif
VIEW_TEST(BookmarkBarViewTest14, MAYBE_ContextMenus2)

// Makes sure deleting from the context menu keeps the bookmark menu showing.
class BookmarkBarViewTest15 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest15()
      : deleted_menu_id_(0),
        observer_(CreateEventTask(this, &BookmarkBarViewTest15::Step3)) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Show the "All Bookmarks" folder.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest15::Step2));
  }

 private:
  void Step2() {
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_NE(nullptr, child_menu);

    deleted_menu_id_ = child_menu->GetCommand();

    // Right click on the second child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        base::OnceClosure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    views::MenuItemView* delete_menu =
        context_menu->GetMenuItemByID(IDC_BOOKMARK_BAR_REMOVE);
    ASSERT_TRUE(delete_menu);

    // Click on the delete button.
    ui_test_utils::MoveMouseToCenterAndPress(
        delete_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest15::Step4));
  }

  void Step4() {
    ASSERT_FALSE(MenuIsShowing(bb_view_->GetContextMenu()));
    ASSERT_TRUE(MenuIsShowing());

    // The deleted_menu_id_ should have been removed.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_EQ(nullptr, menu->GetMenuItemByID(deleted_menu_id_));

    menu->GetMenuController()->Cancel(views::MenuController::ExitType::kAll);
    Done();
  }

  int deleted_menu_id_;
  BookmarkContextMenuNotificationObserver observer_;
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MenuStaysVisibleAfterDelete \
    DISABLED_MenuStaysVisibleAfterDelete
#else
#define MAYBE_MenuStaysVisibleAfterDelete MenuStaysVisibleAfterDelete
#endif
VIEW_TEST(BookmarkBarViewTest15, MAYBE_MenuStaysVisibleAfterDelete)

// Tests that we don't crash or get stuck if the parent of a menu is closed.
class BookmarkBarViewTest16 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest16::Step2));
  }

 private:
  void Step2() {
    // Button should be depressed.
    ASSERT_EQ(views::Button::STATE_PRESSED, GetBookmarkButton(0)->GetState());

    // Close the window.
    window()->Close();

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, CreateEventTask(this, &BookmarkBarViewTest16::Done));
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DeleteMenu DISABLED_DeleteMenu
#else
#define MAYBE_DeleteMenu DeleteMenu
#endif
VIEW_TEST(BookmarkBarViewTest16, MAYBE_DeleteMenu)

// Makes sure right clicking on an item while a context menu is already showing
// doesn't crash and works.
class BookmarkBarViewTest17 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest17()
      : observer_(std::make_unique<BookmarkContextMenuNotificationObserver>(
            CreateEventTask(this, &BookmarkBarViewTest17::Step3))) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the "All Bookmarks" button and press the left mouse
    // button.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest17::Step2));
  }

 private:
  void Step2() {
    // Right click on the second item to show its context menu.
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(3);
    ASSERT_NE(nullptr, child_menu);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        base::OnceClosure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step3() {
    ASSERT_TRUE(MenuIsShowing());
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    // Right click on the first menu item to trigger its context menu.
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_NE(nullptr, child_menu);

    // The context menu and child_menu can be overlapped, calculate the
    // non-intersected Rect of the child menu and click on its center to make
    // sure the click is always on the child menu.
    gfx::Rect context_rect = context_menu->GetSubmenu()->GetBoundsInScreen();
    gfx::Rect child_menu_rect = child_menu->GetBoundsInScreen();
    gfx::Rect clickable_rect =
        gfx::SubtractRects(child_menu_rect, context_rect);
    ASSERT_FALSE(clickable_rect.IsEmpty());
    observer_ = std::make_unique<BookmarkContextMenuNotificationObserver>(
        CreateEventTask(this, &BookmarkBarViewTest17::Step4));
    MoveMouseAndPress(clickable_rect.CenterPoint(), ui_controls::RIGHT,
                      ui_controls::DOWN | ui_controls::UP, base::OnceClosure());
    // Step4 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  void Step4() {
    ASSERT_TRUE(MenuIsShowing());
    ASSERT_TRUE(MenuIsShowing(bb_view_->GetContextMenu()));

    bb_view_->GetMenu()->GetMenuController()->Cancel(
        views::MenuController::ExitType::kAll);
    Done();
  }

  std::unique_ptr<BookmarkContextMenuNotificationObserver> observer_;
};

// TODO(crbug.com/40282036): Fails on latest versions of ChromeOS.
// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_ContextMenus3 DISABLED_ContextMenus3
#else
#define MAYBE_ContextMenus3 ContextMenus3
#endif
VIEW_TEST(BookmarkBarViewTest17, MAYBE_ContextMenus3)

// Verifies sibling menus works. Clicks on the 'all bookmarks' folder, then
// moves the mouse over the first item on the bookmark bar and makes sure the
// menu appears.
class BookmarkBarViewTest18 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest18::Step2));
  }

 private:
  void Step2() {
    // The button should be pressed.
    EXPECT_EQ(views::Button::STATE_PRESSED,
              bb_view_->all_bookmarks_button()->GetState());

    // Move the mouse to the first folder on the bookmark bar.
    views::LabelButton* button = GetBookmarkButton(0);
    gfx::Point button_center(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &button_center);
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        button_center.x(), button_center.y(),
        CreateEventTask(this, &BookmarkBarViewTest18::Step3)));
  }

  void Step3() {
    ASSERT_TRUE(MenuIsShowing());

    // The menu for the first folder should be in the pressed state (since the
    // menu is showing for it)...
    EXPECT_EQ(views::Button::STATE_PRESSED, GetBookmarkButton(0)->GetState());
    // ... And the "all bookmarks" button should no longer be pressed.
    EXPECT_EQ(views::Button::STATE_NORMAL,
              bb_view_->all_bookmarks_button()->GetState());

    bb_view_->GetMenu()->GetMenuController()->Cancel(
        views::MenuController::ExitType::kAll);
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BookmarkBarViewTest18_SiblingMenu \
    DISABLED_BookmarkBarViewTest18_SiblingMenu
#else
#define MAYBE_BookmarkBarViewTest18_SiblingMenu \
    BookmarkBarViewTest18_SiblingMenu
#endif
VIEW_TEST(BookmarkBarViewTest18, MAYBE_BookmarkBarViewTest18_SiblingMenu)

// Verifies mousing over an already open sibling menu doesn't prematurely cancel
// the menu.
class BookmarkBarViewTest19 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest19::Step2));
  }

 private:
  void Step2() {
    // Click on the first folder.
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_NE(nullptr, child_menu);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step3));
  }

  void Step3() {
    ASSERT_TRUE(MenuIsShowing());

    // Move the mouse back to the "All Bookmarks" button.
    views::LabelButton* button = bb_view_->all_bookmarks_button();
    gfx::Point button_center(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &button_center);
    ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        button_center.x() + 1, button_center.y() + 1,
        CreateEventTask(this, &BookmarkBarViewTest19::Step4)));
  }

  void Step4() {
    ASSERT_TRUE(MenuIsShowing());

    // Click on the first folder.
    views::MenuItemView* child_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_NE(nullptr, child_menu);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step5));
  }

  void Step5() {
    ASSERT_TRUE(MenuIsShowing());

    bb_view_->GetMenu()->GetMenuController()->Cancel(
        views::MenuController::ExitType::kAll);
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BookmarkBarViewTest19_SiblingMenu \
    DISABLED_BookmarkBarViewTest19_SiblingMenu
#else
#define MAYBE_BookmarkBarViewTest19_SiblingMenu \
    BookmarkBarViewTest19_SiblingMenu
#endif
VIEW_TEST(BookmarkBarViewTest19, MAYBE_BookmarkBarViewTest19_SiblingMenu)

// Verify that when clicking a mouse button outside a context menu,
// the context menu is dismissed *and* the underlying view receives
// the the mouse event (due to event reposting).
class BookmarkBarViewTest20 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest20() = default;

 protected:
  void DoTestOnMessageLoop() override {
    // Add |test_view_| next to |bb_view_|.
    views::View* parent = bb_view_->parent();
    parent->RemoveChildView(bb_view_);
    auto* const container_view =
        parent->AddChildView(std::make_unique<views::View>());
    auto* const layout =
        container_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, 2));
    container_view->AddChildView(bb_view_.get());
    bb_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    test_view_ =
        container_view->AddChildView(std::make_unique<TestViewForMenuExit>());
    test_view_->SetPreferredSize(gfx::Size(20, 0));
    parent->DeprecatedLayoutImmediately();

    EXPECT_EQ(0, test_view_->press_count());

    // Move the mouse to the Test View and press the left mouse button.
    ui_test_utils::MoveMouseToCenterAndPress(
        test_view_, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step1));
  }

 private:
  void Step1() {
    EXPECT_EQ(1, test_view_->press_count());
    EXPECT_EQ(nullptr, bb_view_->GetMenu());

    // Move the mouse to the first folder on the bookmark bar and press the
    // left mouse button.
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest20::Step2));
  }

  void Step2() {
    EXPECT_EQ(1, test_view_->press_count());

    // Move the mouse to the Test View and press the left mouse button.
    // The context menu will consume the event and exit. Thereafter,
    // the event is reposted and delivered to the Test View which
    // increases its press-count.
    ui_test_utils::MoveMouseToCenterAndPress(
        test_view_, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step3));
  }

  void Step3() {
    EXPECT_EQ(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ? 1 : 2,
              test_view_->press_count());
    EXPECT_EQ(nullptr, bb_view_->GetMenu());
    Done();
  }

  class TestViewForMenuExit : public views::View {
    METADATA_HEADER(TestViewForMenuExit, views::View)

   public:
    TestViewForMenuExit() = default;

    bool OnMousePressed(const ui::MouseEvent& event) override {
      ++press_count_;
      return true;
    }
    int press_count() const { return press_count_; }

   private:
    int press_count_ = 0;
  };

  raw_ptr<TestViewForMenuExit, DanglingUntriaged> test_view_ = nullptr;
};

BEGIN_METADATA(BookmarkBarViewTest20, TestViewForMenuExit)
END_METADATA

// TODO(crbug.com/40947483): Flaky on Windows.
// TODO (crbug/1523247): This test is failing under wayland and Windows. This
// skips it until it can be fixed.
#if BUILDFLAG(IS_OZONE_WAYLAND) || BUILDFLAG(IS_WIN)
#define MAYBE_ContextMenuExitTest DISABLED_ContextMenuExitTest
#else
#define MAYBE_ContextMenuExitTest ContextMenuExitTest
#endif  // BUILDFLAG(IS_OZONE_WAYLAND) || BUILDFLAG(IS_WIN)
VIEW_TEST(BookmarkBarViewTest20, MAYBE_ContextMenuExitTest)

// Tests context menu by way of opening a context menu for a empty folder menu.
// The opened context menu should behave as it is from the folder button.
class BookmarkBarViewTest21 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest21()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest21::Step3)) {}

 protected:
  // Move the mouse to the empty folder on the bookmark bar and press the
  // left mouse button.
  void DoTestOnMessageLoop() override {
    OpenMenuByClick(GetBookmarkButton(5),
                    CreateEventTask(this, &BookmarkBarViewTest21::Step2));
  }

 private:
  // Confirm that a menu for empty folder shows and right click the menu.
  void Step2() {
    views::SubmenuView* submenu = bb_view_->GetMenu()->GetSubmenu();
    ASSERT_EQ(1u, submenu->children().size());

    auto* empty_item =
        AsViewClass<views::EmptyMenuMenuItem>(submenu->children().front());
    ASSERT_NE(nullptr, empty_item);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(
        empty_item, ui_controls::RIGHT, ui_controls::DOWN | ui_controls::UP,
        base::OnceClosure());
    // Step3 will be invoked by BookmarkContextMenuNotificationObserver.
  }

  // Confirm that context menu shows and click REMOVE menu.
  void Step3() {
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    views::MenuItemView* delete_menu =
        context_menu->GetMenuItemByID(IDC_BOOKMARK_BAR_REMOVE);
    ASSERT_TRUE(delete_menu);

    // Click on the delete menu item.
    ui_test_utils::MoveMouseToCenterAndPress(
        delete_menu, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest21::Step4));
  }

  // Confirm that the empty folder gets removed and menu doesn't show.
  void Step4() {
    views::LabelButton* button = GetBookmarkButton(5);
    ASSERT_TRUE(button);
    EXPECT_EQ(u"d", button->GetText());
    EXPECT_FALSE(MenuIsShowing());
    EXPECT_FALSE(MenuIsShowing(bb_view_->GetContextMenu()));
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// If this flakes, disable and log details in http://crbug.com/523255.
// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ContextMenusForEmptyFolder DISABLED_ContextMenusForEmptyFolder
#else
#define MAYBE_ContextMenusForEmptyFolder ContextMenusForEmptyFolder
#endif
VIEW_TEST(BookmarkBarViewTest21, MAYBE_ContextMenusForEmptyFolder)

// Test that closing the source browser window while dragging a bookmark does
// not cause a crash.
class BookmarkBarViewTest22 : public BookmarkBarViewDragTestBase {
 public:
  // BookmarkBarViewDragTestBase:
  void OnWidgetDragComplete(views::Widget* widget) override {}

  void OnWidgetDestroyed(views::Widget* widget) override {
    BookmarkBarViewDragTestBase::OnWidgetDestroyed(widget);
    Done();
  }

 protected:
  // BookmarkBarViewDragTestBase:
  void OnMenuOpened() override {
    BookmarkBarViewDragTestBase::OnMenuOpened();

    // Cause the second menu item to close the window when dragged over.
    SetStopDraggingView(bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1));
  }

  void OnDragEntered() override {
    // Stop the drag, so any nested message loop will terminate; closing the
    // window alone may not exit this message loop.
    BookmarkBarViewDragTestBase::OnDragEntered();

    window()->Close();
  }

  const BookmarkNode* GetDroppedNode() const override {
    // This test doesn't check what happens on drop.
    return nullptr;
  }

  gfx::Point GetDragTargetInScreen() const override {
    return ui_test_utils::GetCenterInScreenCoordinates(
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1));
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CloseSourceBrowserDuringDrag DISABLED_CloseSourceBrowserDuringDrag
#else
#define MAYBE_CloseSourceBrowserDuringDrag CloseSourceBrowserDuringDrag
#endif
VIEW_TEST(BookmarkBarViewTest22, MAYBE_CloseSourceBrowserDuringDrag)

// Tests opening a context menu for a bookmark node from the keyboard.
class BookmarkBarViewTest23 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest23()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest23::Step5)) {}

 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest23::Step2));
  }

 private:
  void Step2() {
    // Navigate down to highlight the first menu item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest23::Step3)));
  }

  void Step3() {
    ASSERT_TRUE(MenuIsShowing());

    // Navigate down to highlight the second menu item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest23::Step4)));
  }

  void Step4() {
    ASSERT_TRUE(MenuIsShowing());

    // Open the context menu via the keyboard.
    ASSERT_TRUE(ui_controls::SendKeyPress(window()->GetNativeWindow(),
                                          ui::VKEY_APPS, false, false, false,
                                          false));
    // The BookmarkContextMenuNotificationObserver triggers Step5.
  }

  void Step5() {
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(MenuIsShowing(context_menu));

    // Select the first menu item (open).
    ui_test_utils::MoveMouseToCenterAndPress(
        context_menu->GetSubmenu()->GetMenuItemAt(1), ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest23::Step6));
  }

  void Step6() {
    EXPECT_EQ(wrapper_.last_url(),
              model_->other_node()->children().front()->url());
    ASSERT_FALSE(PageTransitionIsWebTriggerable(wrapper_.last_transition()));
    Done();
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
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
    // Move the mouse to the "All Bookmarks" button and press the mouse.
    OpenMenuByClick(bb_view_->all_bookmarks_button(),
                    CreateEventTask(this, &BookmarkBarViewTest24::Step2));
  }

 private:
  void Step2() {
    // Navigate down to highlight the first menu item.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest24::Step3)));
  }

  void Step3() {
    ASSERT_TRUE(MenuIsShowing());

    // Open the context menu via the keyboard.
    ASSERT_TRUE(ui_controls::SendKeyPress(window()->GetNativeWindow(),
                                          ui::VKEY_APPS, false, false, false,
                                          false));
    // The BookmarkContextMenuNotificationObserver triggers Step4.
  }

  void Step4() {
    ASSERT_TRUE(MenuIsShowing(bb_view_->GetContextMenu()));

    // Send escape to close the context menu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false,
        false, CreateEventTask(this, &BookmarkBarViewTest24::Step5)));
  }

  void Step5() {
    ASSERT_FALSE(MenuIsShowing(bb_view_->GetContextMenu()));
    ASSERT_TRUE(MenuIsShowing());

    // Send escape to close the main menu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false,
        false, CreateEventTask(this, &BookmarkBarViewTest24::Done)));
  }

  BookmarkContextMenuNotificationObserver observer_;
};

// Fails on latest versions of Windows. (https://crbug.com/1108551).
// Flaky on Linux (https://crbug.com/1193137).
VIEW_TEST(BookmarkBarViewTest24, DISABLED_ContextMenusKeyboardEscape)

#if BUILDFLAG(IS_WIN)
// Tests that pressing the key KEYCODE closes the menu.
template <ui::KeyboardCode KEYCODE>
class BookmarkBarViewTest25 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest25::Step2));
  }

 private:
  void Step2() {
    // Send KEYCODE key event, which should close the menu.
    ASSERT_TRUE(ui_controls::SendKeyPressNotifyWhenDone(
        window()->GetNativeWindow(), KEYCODE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest25::Step3)));
  }

  void Step3() {
    ASSERT_FALSE(MenuIsShowing());
    Done();
  }
};

// Tests that pressing F10 system key closes the menu.
using BookmarkBarViewTest25F10 = BookmarkBarViewTest25<ui::VKEY_F10>;
// TODO(crbug.com/41493431) flaky on windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_F10ClosesMenu DISABLED_F10ClosesMenu
#else
#define MAYBE_F10ClosesMenu F10ClosesMenu
#endif
VIEW_TEST(BookmarkBarViewTest25F10, MAYBE_F10ClosesMenu)

// Tests that pressing Alt system key closes the menu.
using BookmarkBarViewTest25Alt = BookmarkBarViewTest25<ui::VKEY_MENU>;
// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AltClosesMenu DISABLED_AltClosesMenu
#else
#define MAYBE_AltClosesMenu AltClosesMenu
#endif
VIEW_TEST(BookmarkBarViewTest25Alt, MAYBE_AltClosesMenu)

// Tests that WM_CANCELMODE closes the menu.
class BookmarkBarViewTest26 : public BookmarkBarViewEventTestBase {
 protected:
  void DoTestOnMessageLoop() override {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    OpenMenuByClick(GetBookmarkButton(0),
                    CreateEventTask(this, &BookmarkBarViewTest26::Step2));
  }

 private:
  void Step2() {
    // Send WM_CANCELMODE, which should close the menu. The message is sent
    // synchronously, however, we post a task to make sure that the message is
    // processed completely before finishing the test.
    ::SendMessage(window()->GetNativeView()->GetHost()->GetAcceleratedWidget(),
                  WM_CANCELMODE, 0, 0);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&BookmarkBarViewTest26::Step3, base::Unretained(this)));
  }

  void Step3() {
    ASSERT_FALSE(MenuIsShowing());
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CancelModeClosesMenu DISABLED_CancelModeClosesMenu
#else
#define MAYBE_CancelModeClosesMenu CancelModeClosesMenu
#endif
VIEW_TEST(BookmarkBarViewTest26, MAYBE_CancelModeClosesMenu)
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
    ASSERT_EQ(2u, wrapper_.urls().size());
    EXPECT_EQ(wrapper_.urls()[0],
              model_->bookmark_bar_node()->children()[0]->children()[0]->url());
    EXPECT_EQ(wrapper_.urls()[1],
              model_->bookmark_bar_node()->children()[0]->children()[2]->url());
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MiddleClickOnFolderOpensAllBookmarks \
    DISABLED_MiddleClickOnFolderOpensAllBookmarks
#else
#define MAYBE_MiddleClickOnFolderOpensAllBookmarks \
    MiddleClickOnFolderOpensAllBookmarks
#endif
VIEW_TEST(BookmarkBarViewTest27, MAYBE_MiddleClickOnFolderOpensAllBookmarks)

#endif  // BUILDFLAG(IS_MAC)

class BookmarkBarViewTest28 : public BookmarkBarViewEventTestBase {
 protected:
  static constexpr ui_controls::AcceleratorState kAccelatorState =
      BUILDFLAG(IS_MAC) ? ui_controls::kCommand : ui_controls::kControl;

  void DoTestOnMessageLoop() override {
    views::LabelButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::UP | ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest28::Step2), kAccelatorState);
  }

 private:
  void Step2() {
    ASSERT_EQ(2u, wrapper_.urls().size());
    EXPECT_EQ(wrapper_.urls()[0],
              model_->bookmark_bar_node()->children()[0]->children()[0]->url());
    EXPECT_EQ(wrapper_.urls()[1],
              model_->bookmark_bar_node()->children()[0]->children()[2]->url());
    Done();
  }
};

// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ClickWithModifierOnFolderOpensAllBookmarks \
    DISABLED_ClickWithModifierOnFolderOpensAllBookmarks
#else
#define MAYBE_ClickWithModifierOnFolderOpensAllBookmarks \
    ClickWithModifierOnFolderOpensAllBookmarks
#endif

VIEW_TEST(BookmarkBarViewTest28,
          MAYBE_ClickWithModifierOnFolderOpensAllBookmarks)

// Tests drag and drop to an empty menu.
class BookmarkBarViewTest29 : public BookmarkBarViewDragTestBase {
 public:
  // BookmarkBarViewDragTestBase:
  void OnDropMenuShown() override {
    // The folder's menu should be open, showing an "(empty)" placeholder.
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(MenuIsShowing(drop_menu));
    views::SubmenuView* drop_submenu = drop_menu->GetSubmenu();
    ASSERT_TRUE(drop_submenu->GetEnabled());
    ASSERT_FALSE(drop_submenu->children().empty());
    const views::View* target_view = drop_submenu->children().front();
    EXPECT_TRUE(views::IsViewClass<views::EmptyMenuMenuItem>(target_view));

    // Drag to the "(empty)" placeholder item, then release.
    //
    // Since the EmptyMenuMenuItem is disabled, we need to stop on the
    // containing submenu, not on the item itself.
    SetStopDraggingView(drop_submenu);
    const gfx::Point target =
        ui_test_utils::GetCenterInScreenCoordinates(target_view);
    GetDragTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove),
                       target.x(), target.y(), ui_controls::kNoWindowHint));
  }

 protected:
  // BookmarkBarViewDragTestBase:
  const BookmarkNode* GetDroppedNode() const override {
    // Should be the first (and only) child of folder F2.
    const auto& bar_items = model_->bookmark_bar_node()->children();
    EXPECT_GE(bar_items.size(), 6u);
    if (bar_items.size() < 6) {
      return nullptr;
    }
    const auto& f2_items = bar_items[5]->children();
    EXPECT_FALSE(f2_items.empty());
    return f2_items.empty() ? nullptr : f2_items.front().get();
  }

  gfx::Point GetDragTargetInScreen() const override {
    // Drag over folder F2.
    return ui_test_utils::GetCenterInScreenCoordinates(GetBookmarkButton(5));
  }
};

// TODO(crbug.com/40943907): Flaky on Mac.
// TODO(crbug.com/40947483): Flaky on Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_DNDToEmptyMenu DISABLED_DNDToEmptyMenu
#else
#define MAYBE_DNDToEmptyMenu DNDToEmptyMenu
#endif
VIEW_TEST(BookmarkBarViewTest29, MAYBE_DNDToEmptyMenu)
