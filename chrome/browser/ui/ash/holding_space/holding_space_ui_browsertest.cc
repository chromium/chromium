// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/capture_mode_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "base/scoped_observation.h"
#include "base/scoped_observer.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Flushes the message loop by posting a task and waiting for it to run.
void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

// Performs a click on `view`.
void Click(const views::View* view) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

// Performs a double click on `view`.
void DoubleClick(const views::View* view) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.DoubleClickLeftButton();
}

// Performs a gesture drag between `from` and `to`.
void GestureDrag(const views::View* from, const views::View* to) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressTouch(from->GetBoundsInScreen().CenterPoint());

  // Gesture drag is initiated only after an `ui::ET_GESTURE_LONG_PRESS` event.
  ui::GestureEvent long_press(
      event_generator.current_screen_location().x(),
      event_generator.current_screen_location().y(), ui::EF_NONE,
      ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  event_generator.Dispatch(&long_press);

  event_generator.MoveTouch(to->GetBoundsInScreen().CenterPoint());
  event_generator.ReleaseTouch();
}

// Performs a gesture tap on `view`.
void GestureTap(const views::View* view) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

// Performs a mouse drag between `from` and `to`.
void MouseDrag(const views::View* from, const views::View* to) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(from->GetBoundsInScreen().CenterPoint());
  event_generator.PressLeftButton();
  event_generator.MoveMouseTo(to->GetBoundsInScreen().CenterPoint());
  event_generator.ReleaseLeftButton();
}

// Moves mouse to `view` over `count` number of events.
void MoveMouseTo(const views::View* view, size_t count = 1u) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint(), count);
}

// Performs a press and release of the specified `key_code` with `flags`.
void PressAndReleaseKey(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressKey(key_code, flags);
  event_generator.ReleaseKey(key_code, flags);
}

// Performs a right click on `view`.
void RightClick(const views::View* view) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickRightButton();
}

// Mocks -----------------------------------------------------------------------

class MockActivationChangeObserver : public wm::ActivationChangeObserver {
 public:
  MOCK_METHOD(void,
              OnWindowActivated,
              (wm::ActivationChangeObserver::ActivationReason reason,
               aura::Window* gained_active,
               aura::Window* lost_active),
              (override));
};

class MockHoldingSpaceModelObserver : public HoldingSpaceModelObserver {
 public:
  MOCK_METHOD(void,
              OnHoldingSpaceItemsAdded,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemsRemoved,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemFinalized,
              (const HoldingSpaceItem* item),
              (override));
};

// DropTargetView --------------------------------------------------------------

class DropTargetView : public views::WidgetDelegateView {
 public:
  DropTargetView(const DropTargetView&) = delete;
  DropTargetView& operator=(const DropTargetView&) = delete;
  ~DropTargetView() override = default;

  static DropTargetView* Create(aura::Window* context) {
    return new DropTargetView(context);
  }

  const base::FilePath& copied_file_path() const { return copied_file_path_; }

 private:
  explicit DropTargetView(aura::Window* context) { InitWidget(context); }

  // views::WidgetDelegateView:
  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    *formats = ui::OSExchangeData::FILE_NAME;
    return true;
  }

  bool CanDrop(const ui::OSExchangeData& data) override { return true; }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_COPY;
  }

  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    EXPECT_TRUE(event.data().GetFilename(&copied_file_path_));
    return ui::DragDropTypes::DRAG_COPY;
  }

  void InitWidget(aura::Window* context) {
    views::Widget::InitParams params;
    params.accept_events = true;
    params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
    params.context = context;
    params.delegate = this;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.wants_mouse_events_when_inactive = true;

    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));
  }

  base::FilePath copied_file_path_;
};

// ViewDrawnWaiter -------------------------------------------------------------

class ViewDrawnWaiter : public views::ViewObserver {
 public:
  ViewDrawnWaiter() = default;
  ViewDrawnWaiter(const ViewDrawnWaiter&) = delete;
  ViewDrawnWaiter& operator=(const ViewDrawnWaiter&) = delete;
  ~ViewDrawnWaiter() override = default;

  void Wait(views::View* view) {
    if (IsDrawn(view))
      return;

    DCHECK(!wait_loop_);
    DCHECK(!view_observer_.IsObserving());

    view_observer_.Observe(view);

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();

    view_observer_.Reset();
  }

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view) override {
    if (IsDrawn(view))
      wait_loop_->Quit();
  }

  void OnViewBoundsChanged(views::View* view) override {
    if (IsDrawn(view))
      wait_loop_->Quit();
  }

  bool IsDrawn(views::View* view) {
    return view->IsDrawn() && !view->size().IsEmpty();
  }

  std::unique_ptr<base::RunLoop> wait_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
};

// HoldingSpaceUiBrowserTest ---------------------------------------------------

// Base class for holding space UI browser tests.
class HoldingSpaceUiBrowserTest : public HoldingSpaceBrowserTestBase {
 public:
  // HoldingSpaceBrowserTestBase:
  void SetUpOnMainThread() override {
    HoldingSpaceBrowserTestBase::SetUpOnMainThread();

    ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    // The holding space tray will not show until the user has added a file to
    // holding space. Holding space UI browser tests don't need to assert that
    // behavior since it is already asserted in ash_unittests. As a convenience,
    // add and remove a holding space item so that the holding space tray will
    // already be showing during test execution.
    ASSERT_FALSE(IsShowingInShelf());
    RemoveItem(AddDownloadFile());
    ASSERT_TRUE(IsShowingInShelf());

    // Confirm that holding space model has been emptied for test execution.
    ASSERT_TRUE(HoldingSpaceController::Get()->model()->items().empty());
  }
};

}  // namespace

// Tests -----------------------------------------------------------------------

// Base class for holding space UI browser tests that test drag-and-drop.
// Parameterized by a callback to invoke to perform a drag-and-drop.
class HoldingSpaceUiDragAndDropBrowserTest
    : public HoldingSpaceUiBrowserTest,
      public testing::WithParamInterface<base::RepeatingCallback<
          void(const views::View* from, const views::View* to)>> {
 public:
  // Performs a drag-and-drop between `from` and `to`.
  void PerformDragAndDrop(const views::View* from, const views::View* to) {
    GetParam().Run(from, to);
  }

  // Returns the view serving as the drop target for tests.
  const DropTargetView* target() const { return drop_target_view_; }

 private:
  // HoldingSpaceUiBrowserTest:
  void SetUpOnMainThread() override {
    HoldingSpaceUiBrowserTest::SetUpOnMainThread();

    // Initialize `drop_target_view_`.
    drop_target_view_ = DropTargetView::Create(GetRootWindowForNewWindows());
    drop_target_view_->GetWidget()->SetBounds(gfx::Rect(0, 0, 100, 100));
    drop_target_view_->GetWidget()->ShowInactive();
  }

  void TearDownOnMainThread() override {
    drop_target_view_->GetWidget()->Close();
    HoldingSpaceUiBrowserTest::TearDownOnMainThread();
  }

  DropTargetView* drop_target_view_ = nullptr;
};

// Verifies that drag-and-drop of holding space items works.
IN_PROC_BROWSER_TEST_P(HoldingSpaceUiDragAndDropBrowserTest, DragAndDrop) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Verify drag-and-drop of download items.
  HoldingSpaceItem* const download_file = AddDownloadFile();

  Show();
  ASSERT_TRUE(IsShowing());

  std::vector<views::View*> download_chips = GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());

  PerformDragAndDrop(/*from=*/download_chips[0], /*to=*/target());
  EXPECT_EQ(download_file->file_path(), target()->copied_file_path());

  // Drag-and-drop should close holding space UI.
  FlushMessageLoop();
  ASSERT_FALSE(IsShowing());

  // Verify drag-and-drop of pinned file items.
  // NOTE: Dragging a pinned file from a non-top row of the pinned files
  // container grid previously resulted in a crash (crbug.com/1143426). To
  // explicitly test against this case we will add and drag a second row item.
  HoldingSpaceItem* const pinned_file = AddPinnedFile();
  AddPinnedFile();
  AddPinnedFile();

  Show();
  ASSERT_TRUE(IsShowing());

  std::vector<views::View*> pinned_file_chips = GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_file_chips.size());

  PerformDragAndDrop(/*from=*/pinned_file_chips.back(), /*to=*/target());
  EXPECT_EQ(pinned_file->file_path(), target()->copied_file_path());

  // Drag-and-drop should close holding space UI.
  FlushMessageLoop();
  ASSERT_FALSE(IsShowing());

  // Verify drag-and-drop of screenshot items.
  HoldingSpaceItem* const screenshot_file = AddScreenshotFile();

  Show();
  ASSERT_TRUE(IsShowing());

  std::vector<views::View*> screen_capture_views = GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_capture_views.size());

  PerformDragAndDrop(/*from=*/screen_capture_views[0], /*to=*/target());
  EXPECT_EQ(screenshot_file->file_path(), target()->copied_file_path());

  // Drag-and-drop should close holding space UI.
  FlushMessageLoop();
  ASSERT_FALSE(IsShowing());
}

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceUiDragAndDropBrowserTest,
                         testing::ValuesIn({
                             base::BindRepeating(&MouseDrag),
                             base::BindRepeating(&GestureDrag),
                         }));

// Verifies that the holding space tray does not appear on the lock screen.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, LockScreen) {
  ASSERT_TRUE(IsShowingInShelf());
  RequestAndAwaitLockScreen();
  ASSERT_FALSE(IsShowingInShelf());
}

// Verifies that opening holding space items works.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, OpenItem) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto* const activation_client = wm::GetActivationClient(
      HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows());

  // Observe the `activation_client` so we can detect windows becoming active as
  // a result of opening holding space items.
  testing::NiceMock<MockActivationChangeObserver> mock;
  ScopedObserver<wm::ActivationClient, wm::ActivationChangeObserver> obs{&mock};
  obs.Add(activation_client);

  // Create a holding space item.
  AddScreenshotFile();

  // We're going to verify we can open holding space items by interacting with
  // the view in a few ways as we expect a user to.
  std::vector<base::OnceCallback<void(const views::View*)>> user_interactions;
  user_interactions.push_back(base::BindOnce(&DoubleClick));
  user_interactions.push_back(base::BindOnce(&GestureTap));
  user_interactions.push_back(base::BindOnce([](const views::View* view) {
    while (!view->HasFocus())
      PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  }));

  for (auto& user_interaction : user_interactions) {
    // Show holding space UI and verify a holding space item view exists.
    Show();
    ASSERT_TRUE(IsShowing());
    std::vector<views::View*> screen_capture_views = GetScreenCaptureViews();
    ASSERT_EQ(1u, screen_capture_views.size());

    // Attempt to open the holding space item via user interaction on its view.
    std::move(user_interaction).Run(screen_capture_views[0]);

    // Expect and wait for a `Gallery` window to be activated since the holding
    // space item that we attempted to open was a screenshot.
    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnWindowActivated)
        .WillOnce([&](wm::ActivationChangeObserver::ActivationReason reason,
                      aura::Window* gained_active, aura::Window* lost_active) {
          EXPECT_EQ("Gallery", base::UTF16ToUTF8(gained_active->GetTitle()));
          run_loop.Quit();
        });
    run_loop.Run();

    // Reset.
    testing::Mock::VerifyAndClearExpectations(&mock);
    activation_client->DeactivateWindow(activation_client->GetActiveWindow());
  }
}

// Verifies that unpinning a pinned holding space item works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, UnpinItem) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Add enough pinned items for there to be multiple rows in the section.
  constexpr size_t kNumPinnedItems = 3u;
  for (size_t i = 0; i < kNumPinnedItems; ++i)
    AddPinnedFile();

  Show();
  ASSERT_TRUE(IsShowing());

  std::vector<views::View*> pinned_file_chips = GetPinnedFileChips();
  ASSERT_EQ(kNumPinnedItems, pinned_file_chips.size());

  // Operate on the last `pinned_file_chip` as there was an easy to reproduce
  // bug in which unpinning a chip *not* in the top row resulted in a crash on
  // destruction due to its ink drop layer attempting to be reordered.
  views::View* pinned_file_chip = pinned_file_chips.back();

  // The pin button is only visible after mousing over the `pinned_file_chip`,
  // so move the mouse and wait for the pin button to be drawn. Note that the
  // mouse is moved over multiple events to ensure that the appropriate mouse
  // enter event is also generated.
  MoveMouseTo(pinned_file_chip, /*count=*/10);
  auto* pin_btn = pinned_file_chip->GetViewByID(kHoldingSpaceItemPinButtonId);
  ViewDrawnWaiter().Wait(pin_btn);

  Click(pin_btn);

  pinned_file_chips = GetPinnedFileChips();
  ASSERT_EQ(kNumPinnedItems - 1, pinned_file_chips.size());
}

// Base class for holding space UI browser tests that test previews.
class HoldingSpaceUiPreviewsBrowserTest : public HoldingSpaceUiBrowserTest {
 public:
  HoldingSpaceUiPreviewsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kTemporaryHoldingSpace,
                              features::kTemporaryHoldingSpacePreviews},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that previews can be toggled via context menu.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiPreviewsBrowserTest, TogglePreviews) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ASSERT_TRUE(IsShowingInShelf());

  // Initially, the default icon should be shown.
  auto* default_tray_icon = GetDefaultTrayIcon();
  ASSERT_TRUE(default_tray_icon);
  EXPECT_TRUE(default_tray_icon->GetVisible());

  auto* previews_tray_icon = GetPreviewsTrayIcon();
  ASSERT_TRUE(previews_tray_icon);
  ASSERT_TRUE(previews_tray_icon->layer());
  ASSERT_EQ(1u, previews_tray_icon->layer()->children().size());
  auto* previews_container_layer = previews_tray_icon->layer()->children()[0];
  EXPECT_FALSE(previews_tray_icon->GetVisible());

  // After pinning a file, we should have a single preview in the tray icon.
  AddPinnedFile();
  FlushMessageLoop();

  EXPECT_FALSE(default_tray_icon->GetVisible());
  EXPECT_TRUE(previews_tray_icon->GetVisible());

  EXPECT_EQ(1u, previews_container_layer->children().size());
  EXPECT_EQ(gfx::Size(32, 32), previews_tray_icon->size());

  // After downloading a file, we should have two previews in the tray icon.
  AddDownloadFile();
  FlushMessageLoop();

  EXPECT_FALSE(default_tray_icon->GetVisible());
  EXPECT_TRUE(previews_tray_icon->GetVisible());
  EXPECT_EQ(2u, previews_container_layer->children().size());
  EXPECT_EQ(gfx::Size(48, 32), previews_tray_icon->size());

  // After taking a screenshot, we should have three previews in the tray icon.
  AddScreenshotFile();
  FlushMessageLoop();

  EXPECT_FALSE(default_tray_icon->GetVisible());
  EXPECT_TRUE(previews_tray_icon->GetVisible());
  EXPECT_EQ(3u, previews_container_layer->children().size());
  EXPECT_EQ(gfx::Size(64, 32), previews_tray_icon->size());

  // Right click the tray icon, and expect a context menu to be shown which will
  // allow the user to hide previews.
  RightClick(previews_tray_icon);
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // Use the keyboard to select the context menu item to hide previews. Doing so
  // should dismiss the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(views::MenuController::GetActiveInstance());
  FlushMessageLoop();

  // The tray icon should now contain no previews, but have a single child which
  // contains the static image to show when previews are disabled.
  EXPECT_TRUE(default_tray_icon->GetVisible());
  EXPECT_FALSE(previews_tray_icon->GetVisible());

  EXPECT_EQ(gfx::Size(32, 32), default_tray_icon->size());

  // Right click the tray icon, and expect a context menu to be shown which will
  // allow the user to show previews.
  RightClick(default_tray_icon);
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // Use the keyboard to select the context menu item to show previews. Doing so
  // should dismiss the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(views::MenuController::GetActiveInstance());
  FlushMessageLoop();

  // The tray icon should once again show three previews.
  EXPECT_FALSE(default_tray_icon->GetVisible());
  EXPECT_TRUE(previews_tray_icon->GetVisible());

  EXPECT_EQ(3u, previews_container_layer->children().size());
  EXPECT_EQ(gfx::Size(64, 32), previews_tray_icon->size());
}

// Base class for holding space UI browser tests that take screenshots.
// Parameterized by whether or not `features::CaptureMode` is enabled.
class HoldingSpaceUiScreenshotBrowserTest
    : public HoldingSpaceUiBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  HoldingSpaceUiScreenshotBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(features::kCaptureMode,
                                              GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that taking a screenshot adds a screenshot holding space item.
IN_PROC_BROWSER_TEST_P(HoldingSpaceUiScreenshotBrowserTest, AddScreenshot) {
  // Verify that no screenshots exist in holding space UI.
  Show();
  ASSERT_TRUE(IsShowing());
  EXPECT_TRUE(GetScreenCaptureViews().empty());

  Close();
  ASSERT_FALSE(IsShowing());

  // Take a screenshot using the keyboard. If `features::kCaptureMode` is
  // enabled, the screenshot will be taken using the `CaptureModeController`.
  // Otherwise the screenshot will be taken using the `ChromeScreenshotGrabber`.
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  // Move the mouse over to the browser window. The reason for that is with
  // `features::kCaptureMode` enabled, the new capture mode implementation will
  // not automatically capture the topmost window unless the mouse is hovered
  // above it.
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow());
  event_generator.MoveMouseTo(
      browser_window->GetBoundsInScreen().CenterPoint());
  PressAndReleaseKey(ui::VKEY_RETURN);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> observer{&mock};
  observer.Add(HoldingSpaceController::Get()->model());

  // Expect and wait for a screenshot item to be added to holding space.
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsAdded)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        ASSERT_EQ(items[0]->type(), HoldingSpaceItem::Type::kScreenshot);
        run_loop.Quit();
      });
  run_loop.Run();

  // Verify that the screenshot appears in holding space UI.
  Show();
  ASSERT_TRUE(IsShowing());
  EXPECT_EQ(1u, GetScreenCaptureViews().size());
}

// Base class for holding space UI browser tests that take screen recordings.
class HoldingSpaceUiScreenCaptureBrowserTest
    : public HoldingSpaceUiBrowserTest {
 public:
  HoldingSpaceUiScreenCaptureBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureMode);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that taking a screen recording adds a screen recording holding space
// item.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiScreenCaptureBrowserTest,
                       AddScreenRecording) {
  // Verify that no screen recordings exist in holding space UI.
  Show();
  ASSERT_TRUE(IsShowing());
  EXPECT_TRUE(GetScreenCaptureViews().empty());

  Close();
  ASSERT_FALSE(IsShowing());
  ash::CaptureModeTestApi capture_mode_test_api;
  capture_mode_test_api.StartForFullscreen(/*for_video=*/true);
  capture_mode_test_api.PerformCapture();
  // Record a 100 ms long video.
  base::RunLoop video_recording_time;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, video_recording_time.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  video_recording_time.Run();
  capture_mode_test_api.StopVideoRecording();

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> observer{&mock};
  observer.Add(HoldingSpaceController::Get()->model());

  base::RunLoop wait_for_item;
  // Expect and wait for a screen recording item to be added to holding space.
  EXPECT_CALL(mock, OnHoldingSpaceItemsAdded)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        ASSERT_EQ(items[0]->type(), HoldingSpaceItem::Type::kScreenRecording);
        wait_for_item.Quit();
      });
  wait_for_item.Run();

  // Verify that the screen recording appears in holding space UI.
  Show();
  ASSERT_TRUE(IsShowing());
  EXPECT_EQ(1u, GetScreenCaptureViews().size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceUiScreenshotBrowserTest,
                         testing::Bool());

}  // namespace ash
