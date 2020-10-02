// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observer.h"
#include "base/test/bind_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Performs a double click on `view`.
void DoubleClick(const views::View* view) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.DoubleClickLeftButton();
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

// Performs a press and release of the specified `key_code` with `flags`.
void PressAndReleaseKey(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressKey(key_code, flags);
  event_generator.ReleaseKey(key_code, flags);
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
              OnHoldingSpaceItemAdded,
              (const HoldingSpaceItem* item),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemRemoved,
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

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceUiBrowserTest = HoldingSpaceBrowserTestBase;

// Verifies that drag-and-drop of holding space items works.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, DragAndDrop) {
  HoldingSpaceItem* const download_file = AddDownloadFile();
  HoldingSpaceItem* const pinned_file = AddPinnedFile();
  HoldingSpaceItem* const screenshot_file = AddScreenshotFile();

  Show();
  ASSERT_TRUE(IsShowing());

  std::vector<views::View*> download_chips = GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());

  std::vector<views::View*> pinned_file_chips = GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_file_chips.size());

  std::vector<views::View*> screenshot_views = GetScreenshotViews();
  ASSERT_EQ(1u, screenshot_views.size());

  auto* drop_target_view = DropTargetView::Create(GetRootWindowForNewWindows());
  drop_target_view->GetWidget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  drop_target_view->GetWidget()->ShowInactive();

  MouseDrag(/*from=*/download_chips[0], /*to=*/drop_target_view);
  EXPECT_EQ(download_file->file_path(), drop_target_view->copied_file_path());

  MouseDrag(/*from=*/pinned_file_chips[0], /*to=*/drop_target_view);
  EXPECT_EQ(pinned_file->file_path(), drop_target_view->copied_file_path());

  MouseDrag(/*from=*/screenshot_views[0], /*to=*/drop_target_view);
  EXPECT_EQ(screenshot_file->file_path(), drop_target_view->copied_file_path());

  drop_target_view->GetWidget()->Close();
}

// Verifies that the holding space tray does not appear on the lock screen.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, LockScreen) {
  ASSERT_TRUE(IsShowingInShelf());
  RequestAndAwaitLockScreen();
  ASSERT_FALSE(IsShowingInShelf());
}

// Verifies that opening holding space items works.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, OpenItem) {
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
    std::vector<views::View*> screenshot_views = GetScreenshotViews();
    ASSERT_EQ(1u, screenshot_views.size());

    // Attempt to open the holding space item via user interaction on its view.
    std::move(user_interaction).Run(screenshot_views[0]);

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
  EXPECT_TRUE(GetScreenshotViews().empty());

  Close();
  ASSERT_FALSE(IsShowing());

  // Take a screenshot using the keyboard. If `features::kCaptureMode` is
  // enabled, the screenshot will be taken using the `CaptureModeController`.
  // Otherwise the screenshot will be taken using the `ChromeScreenshotGrabber`.
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_RETURN);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> observer{&mock};
  observer.Add(HoldingSpaceController::Get()->model());

  // Expect and wait for a screenshot item to be added to holding space.
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemAdded)
      .WillOnce([&](const HoldingSpaceItem* item) {
        if (item->type() == HoldingSpaceItem::Type::kScreenshot)
          run_loop.Quit();
      });
  run_loop.Run();

  // Verify that the screenshot appears in holding space UI.
  Show();
  ASSERT_TRUE(IsShowing());
  EXPECT_EQ(1u, GetScreenshotViews().size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceUiScreenshotBrowserTest,
                         testing::Bool());

}  // namespace ash
