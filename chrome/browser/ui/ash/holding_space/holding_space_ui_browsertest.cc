// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"

#include <unordered_map>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/capture_mode_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "base/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/scoped_observer.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns all holding space item types.
std::vector<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::vector<HoldingSpaceItem::Type> types;
  for (int i = 0; i <= static_cast<int>(HoldingSpaceItem::Type::kMaxValue); ++i)
    types.push_back(static_cast<HoldingSpaceItem::Type>(i));
  return types;
}

// Flushes the message loop by posting a task and waiting for it to run.
void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

// Performs a click on `view` with optional `flags`.
void Click(const views::View* view, int flags = ui::EF_NONE) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.set_flags(flags);
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

using DragUpdateCallback =
    base::RepeatingCallback<void(const gfx::Point& screen_location)>;

// Performs a gesture drag between `from` and `to`.
void GestureDrag(const views::View* from,
                 const views::View* to,
                 DragUpdateCallback drag_update_callback = base::DoNothing(),
                 base::OnceClosure before_release_callback = base::DoNothing(),
                 base::OnceClosure after_release_callback = base::DoNothing()) {
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

  // Generate multiple interpolated touch move events.
  // NOTE: The `ash::DragDropController` applies a vertical offset when
  // determining the target view for touch initiated dragging so that needs to
  // be compensated for here.
  constexpr int kNumberOfTouchMoveEvents = 25;
  constexpr gfx::Vector2d offset(0, 25);
  const gfx::Point endpoint = to->GetBoundsInScreen().CenterPoint() + offset;
  const gfx::Point origin = event_generator.current_screen_location();
  const gfx::Vector2dF diff(endpoint - origin);
  for (int i = 1; i <= kNumberOfTouchMoveEvents; ++i) {
    gfx::Vector2dF step(diff);
    step.Scale(i / static_cast<float>(kNumberOfTouchMoveEvents));
    event_generator.MoveTouch(origin + gfx::ToRoundedVector2d(step));
    drag_update_callback.Run(event_generator.current_screen_location());
  }

  std::move(before_release_callback).Run();
  event_generator.ReleaseTouch();
  std::move(after_release_callback).Run();
}

// Performs a gesture tap on `view`.
void GestureTap(const views::View* view) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

// Performs a mouse drag between `from` and `to`.
void MouseDrag(const views::View* from,
               const views::View* to,
               DragUpdateCallback drag_update_callback = base::DoNothing(),
               base::OnceClosure before_release_callback = base::DoNothing(),
               base::OnceClosure after_release_callback = base::DoNothing()) {
  auto* root_window = HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(from->GetBoundsInScreen().CenterPoint());
  event_generator.PressLeftButton();

  // Generate multiple interpolated mouse move events so that views are notified
  // of mouse enter/exit as they would be in production.
  constexpr int kNumberOfMouseMoveEvents = 25;
  const gfx::Point origin = event_generator.current_screen_location();
  const gfx::Vector2dF diff(to->GetBoundsInScreen().CenterPoint() - origin);
  for (int i = 1; i <= kNumberOfMouseMoveEvents; ++i) {
    gfx::Vector2dF step(diff);
    step.Scale(i / static_cast<float>(kNumberOfMouseMoveEvents));
    event_generator.MoveMouseTo(origin + gfx::ToRoundedVector2d(step));
    drag_update_callback.Run(event_generator.current_screen_location());
  }

  std::move(before_release_callback).Run();
  event_generator.ReleaseLeftButton();
  std::move(after_release_callback).Run();
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

// Selects the menu item with the specified command ID. Returns the selected
// menu item if successful, `nullptr` otherwise.
views::MenuItemView* SelectMenuItemWithCommandId(
    HoldingSpaceCommandId command_id) {
  auto* menu_controller = views::MenuController::GetActiveInstance();
  if (!menu_controller)
    return nullptr;

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  auto* const first_selected_menu_item = menu_controller->GetSelectedMenuItem();
  if (!first_selected_menu_item)
    return nullptr;

  auto* selected_menu_item = first_selected_menu_item;
  do {
    if (selected_menu_item->GetCommand() == static_cast<int>(command_id))
      return selected_menu_item;

    PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
    selected_menu_item = menu_controller->GetSelectedMenuItem();

    // It is expected that context menus loop selection traversal. If the
    // currently `selected_menu_item` is the `first_selected_menu_item` then the
    // context menu has been completely traversed.
  } while (selected_menu_item != first_selected_menu_item);

  // If this LOC is reached the menu has been completely traversed without
  // finding a menu item for the desired `command_id`.
  return nullptr;
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

class MockHoldingSpaceClient : public HoldingSpaceClient {
 public:
  MOCK_METHOD(void,
              AddScreenshot,
              (const base::FilePath& file_path),
              (override));
  MOCK_METHOD(void,
              AddScreenRecording,
              (const base::FilePath& file_path),
              (override));
  MOCK_METHOD(void,
              CopyImageToClipboard,
              (const HoldingSpaceItem& item, SuccessCallback callback),
              (override));
  MOCK_METHOD(base::FilePath,
              CrackFileSystemUrl,
              (const GURL& file_system_url),
              (const, override));
  MOCK_METHOD(void, OpenDownloads, (SuccessCallback callback), (override));
  MOCK_METHOD(void, OpenMyFiles, (SuccessCallback callback), (override));
  MOCK_METHOD(void,
              OpenItems,
              (const std::vector<const HoldingSpaceItem*>& items,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowItemInFolder,
              (const HoldingSpaceItem& item, SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              PinFiles,
              (const std::vector<base::FilePath>& file_paths),
              (override));
  MOCK_METHOD(void,
              PinItems,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              UnpinItems,
              (const std::vector<const HoldingSpaceItem*>& items),
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

// DropSenderView --------------------------------------------------------------

class DropSenderView : public views::WidgetDelegateView,
                       public views::DragController {
 public:
  DropSenderView(const DropSenderView&) = delete;
  DropSenderView& operator=(const DropSenderView&) = delete;
  ~DropSenderView() override = default;

  static DropSenderView* Create(aura::Window* context) {
    return new DropSenderView(context);
  }

  void ClearFilenamesData() { filenames_data_.reset(); }

  void SetFilenamesData(const std::vector<base::FilePath> file_paths) {
    std::vector<ui::FileInfo> filenames;
    for (const base::FilePath& file_path : file_paths)
      filenames.emplace_back(file_path, /*display_name=*/base::FilePath());
    filenames_data_.emplace(std::move(filenames));
  }

  void ClearFileSystemSourcesData() { file_system_sources_data_.reset(); }

  void SetFileSystemSourcesData(const std::vector<GURL>& file_system_urls) {
    constexpr char kFileSystemSourcesType[] = "fs/sources";

    std::stringstream file_system_sources;
    for (const GURL& file_system_url : file_system_urls)
      file_system_sources << file_system_url.spec() << "\n";

    base::Pickle pickle;
    ui::WriteCustomDataToPickle(
        std::unordered_map<std::u16string, std::u16string>(
            {{base::UTF8ToUTF16(kFileSystemSourcesType),
              base::UTF8ToUTF16(file_system_sources.str())}}),
        &pickle);

    file_system_sources_data_.emplace(std::move(pickle));
  }

 private:
  explicit DropSenderView(aura::Window* context) {
    InitWidget(context);
    set_drag_controller(this);
  }

  // views::DragController:
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& current_pt) override {
    DCHECK_EQ(sender, this);
    return true;
  }

  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& press_pt) override {
    DCHECK_EQ(sender, this);
    return ui::DragDropTypes::DRAG_COPY;
  }

  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override {
    // Drag image.
    // NOTE: Gesture drag is only enabled if a drag image is specified.
    data->provider().SetDragImage(
        /*image=*/gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10),
        /*cursor_offset=*/gfx::Vector2d());

    // Payload.
    if (filenames_data_)
      data->provider().SetFilenames(filenames_data_.value());
    if (file_system_sources_data_) {
      data->provider().SetPickledData(
          ui::ClipboardFormatType::GetWebCustomDataType(),
          file_system_sources_data_.value());
    }
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

  base::Optional<std::vector<ui::FileInfo>> filenames_data_;
  base::Optional<base::Pickle> file_system_sources_data_;
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

  ui::mojom::DragOperation OnPerformDrop(
      const ui::DropTargetEvent& event) override {
    EXPECT_TRUE(event.data().GetFilename(&copied_file_path_));
    return ui::mojom::DragOperation::kCopy;
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

using PerformDragAndDropCallback =
    base::RepeatingCallback<void(const views::View* from,
                                 const views::View* to,
                                 DragUpdateCallback drag_update_callback,
                                 base::OnceClosure before_release_callback,
                                 base::OnceClosure after_release_callback)>;

enum StorageLocationFlag : uint32_t {
  kFilenames = 1 << 0,
  kFileSystemSources = 1 << 1,
};

using StorageLocationFlags = uint32_t;

// Base class for holding space UI browser tests that test drag-and-drop.
// Parameterized by:
//   [0] - callback to invoke to perform a drag-and-drop.
//   [1] - storage location(s) on `ui::OSExchangeData` at which to store files.
class HoldingSpaceUiDragAndDropBrowserTest
    : public HoldingSpaceUiBrowserTest,
      public testing::WithParamInterface<
          std::tuple<PerformDragAndDropCallback, StorageLocationFlags>> {
 public:
  // Asserts expectations that the holding space tray is or isn't a drop target.
  void ExpectTrayIsDropTarget(bool is_drop_target) {
    EXPECT_EQ(GetTrayDropTargetOverlay()->layer()->GetTargetOpacity(),
              is_drop_target ? 1.f : 0.f);
    EXPECT_EQ(GetDefaultTrayIcon()->layer()->GetTargetOpacity(),
              is_drop_target ? 0.f : 1.f);
    EXPECT_EQ(GetPreviewsTrayIcon()->layer()->GetTargetOpacity(),
              is_drop_target ? 0.f : 1.f);

    // Cache a reference to preview layers.
    const ui::Layer* previews_container_layer =
        GetPreviewsTrayIcon()->layer()->children()[0];
    const std::vector<ui::Layer*>& preview_layers =
        previews_container_layer->children();

    // Iterate over the layers for each preview.
    for (size_t i = 0; i < preview_layers.size(); ++i) {
      const ui::Layer* preview_layer = preview_layers[i];
      const float preview_width = preview_layer->size().width();

      // Previews layers are expected to be translated w/ incremental offset.
      gfx::Vector2dF expected_translation(i * preview_width / 2.f, 0.f);

      // When the holding space tray is a drop target, preview layers are
      // expected to be translated by a fixed amount in addition to the standard
      // incremental offset.
      if (is_drop_target) {
        constexpr int kPreviewIndexOffsetForDropTarget = 3;
        expected_translation += gfx::Vector2dF(
            kPreviewIndexOffsetForDropTarget * preview_width / 2.f, 0.f);
      }

      EXPECT_EQ(preview_layer->transform().To2dTranslation(),
                expected_translation);
    }
  }

  // Returns true if `screen_location` is within sufficient range of the holding
  // space tray so as to make it present itself as a drop target.
  bool IsWithinTrayDropTargetRange(const gfx::Point& screen_location) {
    constexpr int kProximityThreshold = 20;
    gfx::Rect screen_bounds(GetTray()->GetBoundsInScreen());
    screen_bounds.Inset(gfx::Insets(-kProximityThreshold));
    return screen_bounds.Contains(screen_location);
  }

  // Performs a drag-and-drop between `from` and `to`.
  void PerformDragAndDrop(
      const views::View* from,
      const views::View* to,
      DragUpdateCallback drag_update_callback = base::DoNothing(),
      base::OnceClosure before_release_callback = base::DoNothing(),
      base::OnceClosure after_release_callback = base::DoNothing()) {
    GetPerformDragAndDropCallback().Run(
        from, to, std::move(drag_update_callback),
        std::move(before_release_callback), std::move(after_release_callback));
  }

  // Sets data on `sender()` at the storage location specified by test params.
  void SetSenderData(const std::vector<base::FilePath>& file_paths) {
    if (ShouldStoreDataIn(StorageLocationFlag::kFilenames))
      sender()->SetFilenamesData(file_paths);
    else
      sender()->ClearFilenamesData();

    if (!ShouldStoreDataIn(StorageLocationFlag::kFileSystemSources)) {
      sender()->ClearFileSystemSourcesData();
      return;
    }

    std::vector<GURL> file_system_urls;
    for (const base::FilePath& file_path : file_paths) {
      file_system_urls.push_back(
          holding_space_util::ResolveFileSystemUrl(GetProfile(), file_path));
    }

    sender()->SetFileSystemSourcesData(file_system_urls);
  }

  // Returns the view serving as the drop sender for tests.
  DropSenderView* sender() { return drop_sender_view_; }

  // Returns the view serving as the drop target for tests.
  const DropTargetView* target() const { return drop_target_view_; }

 private:
  // HoldingSpaceUiBrowserTest:
  void SetUpOnMainThread() override {
    HoldingSpaceUiBrowserTest::SetUpOnMainThread();

    // Initialize `drop_sender_view_`.
    drop_sender_view_ = DropSenderView::Create(GetRootWindowForNewWindows());
    drop_sender_view_->GetWidget()->SetBounds(gfx::Rect(0, 0, 100, 100));
    drop_sender_view_->GetWidget()->ShowInactive();

    // Initialize `drop_target_view_`.
    drop_target_view_ = DropTargetView::Create(GetRootWindowForNewWindows());
    drop_target_view_->GetWidget()->SetBounds(gfx::Rect(100, 100, 100, 100));
    drop_target_view_->GetWidget()->ShowInactive();
  }

  void TearDownOnMainThread() override {
    drop_sender_view_->GetWidget()->Close();
    drop_target_view_->GetWidget()->Close();
    HoldingSpaceUiBrowserTest::TearDownOnMainThread();
  }

  PerformDragAndDropCallback GetPerformDragAndDropCallback() {
    return std::get<0>(GetParam());
  }

  StorageLocationFlags GetStorageLocationFlags() const {
    return std::get<1>(GetParam());
  }

  bool ShouldStoreDataIn(StorageLocationFlag flag) const {
    return GetStorageLocationFlags() & flag;
  }

  DropSenderView* drop_sender_view_ = nullptr;
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

// Verifies that drag-and-drop to pin holding space items works.
IN_PROC_BROWSER_TEST_P(HoldingSpaceUiDragAndDropBrowserTest, DragAndDropToPin) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Add an item to holding space to cause the holding space tray to appear.
  AddDownloadFile();
  ASSERT_TRUE(IsShowingInShelf());

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Create a file to be dragged into the holding space.
  std::vector<base::FilePath> file_paths;
  file_paths.push_back(CreateFile());
  SetSenderData(file_paths);

  // Expect no events have been recorded to histograms.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.Pod.Action.All",
      holding_space_metrics::PodAction::kDragAndDropToPin, 0);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnHoldingSpaceItemsAdded)
        .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
          ASSERT_EQ(items.size(), 1u);
          ASSERT_EQ(items[0]->type(), HoldingSpaceItem::Type::kPinnedFile);
          ASSERT_EQ(items[0]->file_path(), file_paths[0]);
          run_loop.Quit();
        });

    // Perform and verify the ability to pin a file via drag-and-drop.
    ExpectTrayIsDropTarget(false);
    PerformDragAndDrop(
        /*from=*/sender(), /*to=*/GetTray(),
        /*drag_update_callback=*/
        base::BindRepeating(
            &HoldingSpaceUiDragAndDropBrowserTest::IsWithinTrayDropTargetRange,
            base::Unretained(this))
            .Then(base::BindRepeating(
                &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
                base::Unretained(this))),
        /*before_release_callback=*/
        base::BindOnce(
            &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
            base::Unretained(this), true),
        /*after_release_callback=*/
        base::BindOnce(
            &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
            base::Unretained(this), false));
    run_loop.Run();

    // Expect the event has been recorded to histograms.
    histogram_tester.ExpectBucketCount(
        "HoldingSpace.Pod.Action.All",
        holding_space_metrics::PodAction::kDragAndDropToPin, 1);
  }

  // Create a few more files to be dragged into the holding space.
  file_paths.push_back(CreateFile());
  file_paths.push_back(CreateFile());
  SetSenderData(file_paths);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnHoldingSpaceItemsAdded)
        .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
          ASSERT_EQ(items.size(), 2u);
          ASSERT_EQ(items[0]->type(), HoldingSpaceItem::Type::kPinnedFile);
          ASSERT_EQ(items[0]->file_path(), file_paths[1]);
          ASSERT_EQ(items[1]->type(), HoldingSpaceItem::Type::kPinnedFile);
          ASSERT_EQ(items[1]->file_path(), file_paths[2]);
          run_loop.Quit();
        });

    // Perform and verify the ability to pin multiple files via drag-and-drop.
    // Note that any already pinned files in the drop payload are ignored.
    ExpectTrayIsDropTarget(false);
    PerformDragAndDrop(
        /*from=*/sender(), /*to=*/GetTray(),
        /*drag_update_callback=*/
        base::BindRepeating(
            &HoldingSpaceUiDragAndDropBrowserTest::IsWithinTrayDropTargetRange,
            base::Unretained(this))
            .Then(base::BindRepeating(
                &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
                base::Unretained(this))),
        /*before_release_callback=*/
        base::BindOnce(
            &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
            base::Unretained(this), true),
        /*after_release_callback=*/
        base::BindOnce(
            &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
            base::Unretained(this), false));
    run_loop.Run();

    // Expect the event has been recorded to histograms.
    histogram_tester.ExpectBucketCount(
        "HoldingSpace.Pod.Action.All",
        holding_space_metrics::PodAction::kDragAndDropToPin, 2);
  }

  // Swap out the registered holding space client with a mock.
  testing::NiceMock<MockHoldingSpaceClient> client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      ProfileHelper::Get()->GetUserByProfile(GetProfile())->GetAccountId(),
      &client, HoldingSpaceController::Get()->model());
  ASSERT_EQ(&client, HoldingSpaceController::Get()->client());

  {
    // Verify that attempting to drag-and-drop a payload which contains only
    // files that are already pinned will not result in a client interaction.
    EXPECT_CALL(client, PinFiles).Times(0);
    ExpectTrayIsDropTarget(false);
    PerformDragAndDrop(
        /*from=*/sender(), /*to=*/GetTray(),
        /*drag_update_callback=*/
        base::BindRepeating(
            [](HoldingSpaceUiDragAndDropBrowserTest* test,
               const gfx::Point& screen_location) {
              // The drag payload cannot be handled by holding space so the tray
              // should never indicate it is a drop target regardless of drag
              // update `screen_location`.
              test->ExpectTrayIsDropTarget(false);
            },
            base::Unretained(this)),
        /*before_release_callback=*/
        base::BindOnce(
            &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
            base::Unretained(this), false),
        /*after_release_callback=*/
        base::BindOnce(
            &HoldingSpaceUiDragAndDropBrowserTest::ExpectTrayIsDropTarget,
            base::Unretained(this), false));
    testing::Mock::VerifyAndClearExpectations(&client);

    // Expect no event has been recorded to histograms.
    histogram_tester.ExpectBucketCount(
        "HoldingSpace.Pod.Action.All",
        holding_space_metrics::PodAction::kDragAndDropToPin, 2);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceUiDragAndDropBrowserTest,
    testing::Combine(testing::ValuesIn({
                         base::BindRepeating(&MouseDrag),
                         base::BindRepeating(&GestureDrag),
                     }),
                     testing::ValuesIn(std::vector<StorageLocationFlags>({
                         StorageLocationFlag::kFilenames,
                         StorageLocationFlag::kFileSystemSources,
                         StorageLocationFlag::kFilenames |
                             StorageLocationFlag::kFileSystemSources,
                     }))));

// Verifies that the holding space tray does not appear on the lock screen.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, LockScreen) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

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

// Verifies that removing holding space items works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, RemoveItem) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Populate holding space with items of all types.
  for (HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes())
    AddItem(GetProfile(), type, CreateFile());

  Show();
  ASSERT_TRUE(IsShowing());

  std::vector<views::View*> pinned_file_chips = GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_file_chips.size());

  // Right clicking a pinned item should cause a context menu to show.
  ASSERT_FALSE(views::MenuController::GetActiveInstance());
  RightClick(pinned_file_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // There should be no `kRemoveItem` command for pinned items.
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Close the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  ASSERT_FALSE(views::MenuController::GetActiveInstance());

  std::vector<views::View*> download_chips = GetDownloadChips();
  ASSERT_GT(download_chips.size(), 1u);

  // Add a download item to the selection and show the context menu.
  Click(download_chips.front(), ui::EF_CONTROL_DOWN);
  RightClick(download_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // There should be no `kRemoveItem` command since a pinned item is selected.
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Close the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  ASSERT_FALSE(views::MenuController::GetActiveInstance());

  // Unselect the pinned item and right click show the context menu.
  Click(pinned_file_chips.front(), ui::EF_CONTROL_DOWN);
  RightClick(download_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // There should be a `kRemoveItem` command in the context menu.
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
        .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
          ASSERT_EQ(items.size(), 1u);
          run_loop.Quit();
        });

    const size_t download_chips_size = download_chips.size();

    // Press `ENTER` to remove the selected download item.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    run_loop.Run();

    // Verify a download chip has been removed.
    download_chips = GetDownloadChips();
    ASSERT_EQ(download_chips.size(), download_chips_size - 1);
  }

  std::vector<views::View*> screen_capture_views = GetScreenCaptureViews();
  ASSERT_GT(screen_capture_views.size(), 1u);

  // Select a screen capture item and show the context menu.
  Click(screen_capture_views.front());
  RightClick(screen_capture_views.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // There should be a `kRemoveItem` command in the context menu.
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
        .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
          ASSERT_EQ(items.size(), 1u);
          run_loop.Quit();
        });

    const size_t screen_capture_views_size = screen_capture_views.size();

    // Press `ENTER` to remove the selected screen capture item.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    run_loop.Run();

    // Verify a screen capture view has been removed.
    screen_capture_views = GetScreenCaptureViews();
    ASSERT_EQ(screen_capture_views.size(), screen_capture_views_size - 1);
  }

  // Select all download items.
  for (views::View* download_chip : download_chips)
    Click(download_chip, ui::EF_SHIFT_DOWN);

  // Select all screen capture items.
  for (views::View* screen_capture_view : screen_capture_views)
    Click(screen_capture_view, ui::EF_SHIFT_DOWN);

  // Show the context menu. There should be a `kRemoveItem` command.
  RightClick(download_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  {
    const size_t recent_files_size =
        download_chips.size() + screen_capture_views.size();

    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
        .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
          ASSERT_EQ(items.size(), recent_files_size);
          run_loop.Quit();
        });

    // Press `ENTER` to remove the selected items.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    run_loop.Run();

    // Verify all download chips and screen capture views have been removed.
    ASSERT_EQ(GetDownloadChips().size(), 0u);
    ASSERT_EQ(GetScreenCaptureViews().size(), 0u);
  }

  // The recent files bubble should be empty and therefore hidden.
  ASSERT_FALSE(RecentFilesBubbleShown());
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
