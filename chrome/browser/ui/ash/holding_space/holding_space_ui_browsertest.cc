// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_item_updated_fields.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/public/cpp/holding_space/mock_holding_space_model_observer.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/holding_space/holding_space_animation_registry.h"
#include "ash/system/notification_center/message_popup_animation_waiter.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_locale.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/local_file_suggestion_provider.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/user_manager/user.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_download_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/mock_activation_change_observer.h"

namespace ash {
namespace {

// Aliases.
using ::testing::Conditional;
using ::testing::Eq;
using ::testing::Matches;
using ::testing::Optional;
using ::testing::Property;

// Matchers --------------------------------------------------------------------

MATCHER_P(EnabledColorId, matcher, "") {
  return Matches(matcher)(arg->GetEnabledColorId());
}

// Helpers ---------------------------------------------------------------------

// Returns the accessible name of the specified `view`.
std::string GetAccessibleName(const views::View* view) {
  ui::AXNodeData a11y_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&a11y_data);
  return a11y_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
}

// Flushes the message loop by posting a task and waiting for it to run.
void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  // Gesture drag is initiated only after an `ui::EventType::kGestureLongPress`
  // event.
  ui::GestureEvent long_press(
      event_generator.current_screen_location().x(),
      event_generator.current_screen_location().y(), ui::EF_NONE,
      ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator.Dispatch(&long_press);

  // Generate multiple interpolated touch move events.
  // NOTE: The `ash::DragDropController` applies a vertical offset when
  // determining the target view for touch-initiated dragging, so that needs to
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

// Waits for the specified `label` to have the desired `text`.
void WaitForText(views::Label* label, const std::u16string& text) {
  if (label->GetText() == text)
    return;
  base::RunLoop run_loop;
  auto subscription =
      label->AddTextChangedCallback(base::BindLambdaForTesting([&]() {
        if (label->GetText() == text)
          run_loop.Quit();
      }));
  run_loop.Run();
}

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
    constexpr char16_t kFileSystemSourcesType[] = u"fs/sources";

    std::stringstream file_system_sources;
    for (const GURL& file_system_url : file_system_urls)
      file_system_sources << file_system_url.spec() << "\n";

    base::Pickle pickle;
    ui::WriteCustomDataToPickle(
        std::unordered_map<std::u16string, std::u16string>(
            {{kFileSystemSourcesType,
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
          ui::ClipboardFormatType::DataTransferCustomType(),
          file_system_sources_data_.value());
    }
  }

  void InitWidget(aura::Window* context) {
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.accept_events = true;
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.context = context;
    params.delegate = this;
    params.wants_mouse_events_when_inactive = true;

    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));
  }

  std::optional<std::vector<ui::FileInfo>> filenames_data_;
  std::optional<base::Pickle> file_system_sources_data_;
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

  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override {
    return base::BindOnce(&DropTargetView::PerformDrop, base::Unretained(this));
  }

  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
    std::optional<std::vector<ui::FileInfo>> files =
        event.data().GetFilenames();
    ASSERT_TRUE(files.has_value());
    ASSERT_EQ(1u, files.value().size());
    copied_file_path_ = files.value()[0].path;
    output_drag_op = ui::mojom::DragOperation::kCopy;
  }

  void InitWidget(aura::Window* context) {
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.accept_events = true;
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.context = context;
    params.delegate = this;
    params.wants_mouse_events_when_inactive = true;

    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));
  }

  base::FilePath copied_file_path_;
};

// NextMainFrameWaiter ---------------------------------------------------------

// A helper class that waits until the next main frame is processed.
class NextMainFrameWaiter : public ui::CompositorObserver {
 public:
  explicit NextMainFrameWaiter(ui::Compositor* compositor) {
    observation_.Observe(compositor);
  }

  void Wait() {
    CHECK(!run_loop_.running());
    run_loop_.Run();
  }

 private:
  // ui::CompositorObserver:
  void OnDidBeginMainFrame(ui::Compositor* compositor) override {
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  base::ScopedObservation<ui::Compositor, ui::CompositorObserver> observation_{
      this};
};

// HoldingSpaceUiBrowserTest ---------------------------------------------------

using HoldingSpaceUiBrowserTest = HoldingSpaceUiBrowserTestBase;

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
  HoldingSpaceUiDragAndDropBrowserTest() {
    // Drag-and-drop tests will close the browser because browser events
    // sometimes get in the way of drag-and-drop events, causing test flakiness.
    set_exit_when_last_browser_closes(false);
  }

  // Asserts expectations that the holding space tray is or isn't a drop target.
  void ExpectTrayIsDropTarget(bool is_drop_target) {
    EXPECT_EQ(
        test_api().GetTrayDropTargetOverlay()->layer()->GetTargetOpacity(),
        is_drop_target ? 1.f : 0.f);
    EXPECT_EQ(test_api().GetDefaultTrayIcon()->layer()->GetTargetOpacity(),
              is_drop_target ? 0.f : 1.f);
    EXPECT_EQ(test_api().GetPreviewsTrayIcon()->layer()->GetTargetOpacity(),
              is_drop_target ? 0.f : 1.f);

    // Cache a reference to preview layers.
    const ui::Layer* previews_container_layer =
        test_api().GetPreviewsTrayIcon()->layer()->children()[0];
    const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& preview_layers =
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
    gfx::Rect screen_bounds(test_api().GetTray()->GetBoundsInScreen());
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

    // Close the browser because browser events sometimes get in the way of
    // drag-and-drop events, causing test flakiness.
    CloseBrowserSynchronously(browser());
    content::RunAllTasksUntilIdle();

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

  raw_ptr<DropSenderView, DanglingUntriaged> drop_sender_view_ = nullptr;
  raw_ptr<DropTargetView, DanglingUntriaged> drop_target_view_ = nullptr;
};

// Verifies that drag-and-drop of holding space items works.
IN_PROC_BROWSER_TEST_P(HoldingSpaceUiDragAndDropBrowserTest, DragAndDrop) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Verify drag-and-drop of download items.
  HoldingSpaceItem* const download_file = AddDownloadFile();

  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());

  PerformDragAndDrop(/*from=*/download_chips[0], /*to=*/target());
  EXPECT_EQ(download_file->file().file_path, target()->copied_file_path());

  // Drag-and-drop should close holding space UI.
  FlushMessageLoop();
  ASSERT_FALSE(test_api().IsShowing());

  // Verify drag-and-drop of pinned file items.
  // NOTE: Dragging a pinned file from a non-top row of the pinned files
  // container grid previously resulted in a crash (crbug.com/1143426). To
  // explicitly test against this case we will add and drag a second row item.
  HoldingSpaceItem* const pinned_file = AddPinnedFile();
  AddPinnedFile();
  AddPinnedFile();

  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  std::vector<views::View*> pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_file_chips.size());

  PerformDragAndDrop(/*from=*/pinned_file_chips.back(), /*to=*/target());
  EXPECT_EQ(pinned_file->file().file_path, target()->copied_file_path());

  // Drag-and-drop should close holding space UI.
  FlushMessageLoop();
  ASSERT_FALSE(test_api().IsShowing());

  // Verify drag-and-drop of screenshot items.
  HoldingSpaceItem* const screenshot_file = AddScreenshotFile();

  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  std::vector<views::View*> screen_capture_views =
      test_api().GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_capture_views.size());

  PerformDragAndDrop(/*from=*/screen_capture_views[0], /*to=*/target());
  EXPECT_EQ(screenshot_file->file().file_path, target()->copied_file_path());

  // Drag-and-drop should close holding space UI.
  FlushMessageLoop();
  ASSERT_FALSE(test_api().IsShowing());
}

// Verifies that drag-and-drop to pin holding space items works.
IN_PROC_BROWSER_TEST_P(HoldingSpaceUiDragAndDropBrowserTest, DragAndDropToPin) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Add an item to holding space to cause the holding space tray to appear.
  AddDownloadFile();
  ASSERT_TRUE(test_api().IsShowingInShelf());

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
          ASSERT_EQ(items[0]->file().file_path, file_paths[0]);
          run_loop.Quit();
        });

    // Perform and verify the ability to pin a file via drag-and-drop.
    ExpectTrayIsDropTarget(false);
    PerformDragAndDrop(
        /*from=*/sender(), /*to=*/test_api().GetTray(),
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
          ASSERT_EQ(items[0]->file().file_path, file_paths[1]);
          ASSERT_EQ(items[1]->type(), HoldingSpaceItem::Type::kPinnedFile);
          ASSERT_EQ(items[1]->file().file_path, file_paths[2]);
          run_loop.Quit();
        });

    // Perform and verify the ability to pin multiple files via drag-and-drop.
    // Note that any already pinned files in the drop payload are ignored.
    ExpectTrayIsDropTarget(false);
    PerformDragAndDrop(
        /*from=*/sender(), /*to=*/test_api().GetTray(),
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
        /*from=*/sender(), /*to=*/test_api().GetTray(),
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

  ASSERT_TRUE(test_api().IsShowingInShelf());
  RequestAndAwaitLockScreen();
  ASSERT_FALSE(test_api().IsShowingInShelf());
}

// Verifies that pinning and unpinning holding space items works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, PinAndUnpinItems) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Add an item of every type. For downloads, also add an in-progress item.
  for (const auto type : holding_space_util::GetAllItemTypes()) {
    AddItem(GetProfile(), type, CreateFile());
  }
  AddItem(GetProfile(), HoldingSpaceItem::Type::kDownload, CreateFile(),
          HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100));

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Verify existence of views for pinned files, screen captures, and downloads.
  using ViewList = std::vector<views::View*>;
  ViewList pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 1u);
  ViewList screen_capture_views = test_api().GetScreenCaptureViews();
  ASSERT_GE(screen_capture_views.size(), 1u);
  ViewList download_chips = test_api().GetDownloadChips();
  ASSERT_GE(download_chips.size(), 2u);

  // Attempt to pin a screen capture via context menu.
  RightClick(screen_capture_views.front());
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kPinItem));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 2u);
  ASSERT_EQ(
      test_api().GetHoldingSpaceItemFilePath(pinned_file_chips.front()),
      test_api().GetHoldingSpaceItemFilePath(screen_capture_views.front()));

  // Attempt to pin a completed download via context menu. Note that the first
  // download is the in-progress download, so don't select that one.
  RightClick(download_chips.at(1));
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kPinItem));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 3u);
  ASSERT_EQ(test_api().GetHoldingSpaceItemFilePath(pinned_file_chips.front()),
            test_api().GetHoldingSpaceItemFilePath(download_chips.at(1)));

  // Attempt to pin an in-progress download via context menu. Because the
  // download is in-progress, it should neither be pin- or unpin-able.
  RightClick(download_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kPinItem));
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kUnpinItem));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);

  // Attempt to unpin the pinned download via context menu without de-selecting
  // the in-progress download. Because the selection contains items which are
  // not in-progress and all of those items are pinned, the selection should be
  // unpin-able.
  RightClick(download_chips.at(1), ui::EF_CONTROL_DOWN);
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kUnpinItem));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 2u);
  ASSERT_EQ(
      test_api().GetHoldingSpaceItemFilePath(pinned_file_chips.front()),
      test_api().GetHoldingSpaceItemFilePath(screen_capture_views.front()));

  // Select the pinned file and again attempt to pin the completed download via
  // context menu, still without de-selecting the in-progress download. Because
  // the selection contains items which are not in-progress and at least one of
  // those items are unpinned, the selection should be pin-able.
  test::Click(pinned_file_chips.front(), ui::EF_CONTROL_DOWN);
  RightClick(download_chips.front());
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kPinItem));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(pinned_file_chips.size(), 3u);
  ASSERT_EQ(test_api().GetHoldingSpaceItemFilePath(pinned_file_chips.front()),
            test_api().GetHoldingSpaceItemFilePath(download_chips.at(1)));
}

// Verifies that opening holding space items works.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, OpenItem) {
  // Install the Media App, which we expect to open holding space items.
  WaitForTestSystemAppInstall();

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto* const activation_client = wm::GetActivationClient(
      HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows());

  // Observe the `activation_client` so we can detect windows becoming active as
  // a result of opening holding space items.
  testing::NiceMock<MockActivationChangeObserver> mock;
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      obs{&mock};
  obs.Observe(activation_client);

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
    test_api().Show();
    ASSERT_TRUE(test_api().IsShowing());
    std::vector<views::View*> screen_capture_views =
        test_api().GetScreenCaptureViews();
    ASSERT_EQ(1u, screen_capture_views.size());

    // Attempt to open the holding space item via user interaction on its view.
    std::move(user_interaction).Run(screen_capture_views[0]);

    // Expect and wait for a `Gallery` window to be activated since the holding
    // space item that we attempted to open was a screenshot.
    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnWindowActivated)
        .WillRepeatedly(
            [&](wm::ActivationChangeObserver::ActivationReason reason,
                aura::Window* gained_active, aura::Window* lost_active) {
              if (gained_active->GetTitle() == u"Gallery")
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
  for (const auto type : holding_space_util::GetAllItemTypes()) {
    AddItem(GetProfile(), type, CreateFile());
  }

  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  std::vector<views::View*> pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_file_chips.size());

  // Right clicking a pinned item should cause a context menu to show.
  ASSERT_FALSE(views::MenuController::GetActiveInstance());
  ViewDrawnWaiter().Wait(pinned_file_chips.front());
  RightClick(pinned_file_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // There should be no `kRemoveItem` command for pinned items.
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Close the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  ASSERT_FALSE(views::MenuController::GetActiveInstance());

  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_GT(download_chips.size(), 1u);

  // Add a download item to the selection and show the context menu.
  ViewDrawnWaiter().Wait(download_chips.front());
  test::Click(download_chips.front(), ui::EF_CONTROL_DOWN);
  RightClick(download_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // There should be no `kRemoveItem` command since a pinned item is selected.
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Close the context menu.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  ASSERT_FALSE(views::MenuController::GetActiveInstance());

  // Unselect the pinned item and right click show the context menu.
  test::Click(pinned_file_chips.front(), ui::EF_CONTROL_DOWN);
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
    // Cache `item_id` of the download item to be removed.
    const std::string item_id =
        test_api().GetHoldingSpaceItemId(download_chips.front());
    EXPECT_EQ(test_api().GetHoldingSpaceItemView(download_chips, item_id),
              download_chips.front());

    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
        .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
          ASSERT_EQ(items.size(), 1u);
          EXPECT_EQ(items[0]->id(), item_id);
          run_loop.Quit();
        });

    // Press `ENTER` to remove the selected download item.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    run_loop.Run();

    // Verify the download chip has been removed.
    download_chips = test_api().GetDownloadChips();
    EXPECT_FALSE(test_api().GetHoldingSpaceItemView(download_chips, item_id));
  }

  std::vector<views::View*> screen_capture_views =
      test_api().GetScreenCaptureViews();
  ASSERT_GT(screen_capture_views.size(), 1u);

  // Select a screen capture item and show the context menu.
  ViewDrawnWaiter().Wait(screen_capture_views.front());
  test::Click(screen_capture_views.front());
  RightClick(screen_capture_views.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // There should be a `kRemoveItem` command in the context menu.
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  {
    // Cache `item_id` of the screen capture item to be removed.
    const std::string item_id =
        test_api().GetHoldingSpaceItemId(screen_capture_views.front());
    EXPECT_EQ(test_api().GetHoldingSpaceItemView(screen_capture_views, item_id),
              screen_capture_views.front());

    base::RunLoop run_loop;
    EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
        .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
          ASSERT_EQ(items.size(), 1u);
          EXPECT_EQ(items[0]->id(), item_id);
          run_loop.Quit();
        });

    // Press `ENTER` to remove the selected screen capture item.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    run_loop.Run();

    // Verify the screen capture view has been removed.
    screen_capture_views = test_api().GetScreenCaptureViews();
    EXPECT_FALSE(
        test_api().GetHoldingSpaceItemView(screen_capture_views, item_id));
  }

  // Remove all items in the recent files bubble. Note that not all download
  // items or screen capture items that exist may be visible at the same time
  // due to max visibility count restrictions.
  while (!download_chips.empty() || !screen_capture_views.empty()) {
    // Select all visible download items.
    for (views::View* download_chip : download_chips) {
      ViewDrawnWaiter().Wait(download_chip);
      test::Click(download_chip, ui::EF_CONTROL_DOWN);
    }

    // Select all visible screen capture items.
    for (views::View* screen_capture_view : screen_capture_views) {
      ViewDrawnWaiter().Wait(screen_capture_view);
      test::Click(screen_capture_view, ui::EF_CONTROL_DOWN);
    }

    // Show the context menu. There should be a `kRemoveItem` command.
    RightClick(download_chips.size() ? download_chips.front()
                                     : screen_capture_views.front());
    ASSERT_TRUE(views::MenuController::GetActiveInstance());
    ASSERT_TRUE(
        SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

    {
      // Cache `item_ids` of download and screen capture items to be removed.
      std::set<std::string> item_ids;
      for (const views::View* download_chip : download_chips) {
        auto it =
            item_ids.insert(test_api().GetHoldingSpaceItemId(download_chip));
        EXPECT_EQ(test_api().GetHoldingSpaceItemView(download_chips, *it.first),
                  download_chip);
      }
      for (const views::View* screen_capture_view : screen_capture_views) {
        auto it = item_ids.insert(
            test_api().GetHoldingSpaceItemId(screen_capture_view));
        EXPECT_EQ(
            test_api().GetHoldingSpaceItemView(screen_capture_views, *it.first),
            screen_capture_view);
      }

      base::RunLoop run_loop;
      EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
          .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
            ASSERT_EQ(items.size(), item_ids.size());
            for (const HoldingSpaceItem* item : items) {
              ASSERT_TRUE(base::Contains(item_ids, item->id()));
            }
            run_loop.Quit();
          });

      // Press `ENTER` to remove the selected items.
      PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
      run_loop.Run();

      // Verify all previously visible download chips and screen capture views
      // have been removed.
      download_chips = test_api().GetDownloadChips();
      screen_capture_views = test_api().GetScreenCaptureViews();
      for (const std::string& item_id : item_ids) {
        EXPECT_FALSE(
            test_api().GetHoldingSpaceItemView(download_chips, item_id));
        EXPECT_FALSE(
            test_api().GetHoldingSpaceItemView(screen_capture_views, item_id));
      }
    }
  }

  // The recent files bubble should be empty and therefore hidden.
  ASSERT_FALSE(test_api().RecentFilesBubbleShown());
}

// Verifies that unpinning a pinned holding space item works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, UnpinItem) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Add enough pinned items for there to be multiple rows in the section.
  constexpr size_t kNumPinnedItems = 3u;
  for (size_t i = 0; i < kNumPinnedItems; ++i)
    AddPinnedFile();

  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  std::vector<views::View*> pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(kNumPinnedItems, pinned_file_chips.size());

  // Operate on the last `pinned_file_chip` as there was an easy to reproduce
  // bug in which unpinning a chip *not* in the top row resulted in a crash on
  // destruction due to its ink drop layer attempting to be reordered.
  views::View* pinned_file_chip = pinned_file_chips.back();

  // The pin button is only visible after mousing over the `pinned_file_chip`,
  // so move the mouse and wait for the pin button to be drawn. Note that the
  // mouse is moved over multiple events to ensure that the appropriate mouse
  // enter event is also generated.
  test::MoveMouseTo(pinned_file_chip, /*count=*/10);
  auto* pin_btn = pinned_file_chip->GetViewByID(kHoldingSpaceItemPinButtonId);
  ViewDrawnWaiter().Wait(pin_btn);

  test::Click(pin_btn);

  pinned_file_chips = test_api().GetPinnedFileChips();
  ASSERT_EQ(kNumPinnedItems - 1, pinned_file_chips.size());
}

// Verifies that previews can be toggled via context menu.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, TogglePreviews) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ASSERT_TRUE(test_api().IsShowingInShelf());

  // Initially, the default icon should be shown.
  auto* default_tray_icon = test_api().GetDefaultTrayIcon();
  ASSERT_TRUE(default_tray_icon);
  EXPECT_TRUE(default_tray_icon->GetVisible());

  auto* previews_tray_icon = test_api().GetPreviewsTrayIcon();
  ASSERT_TRUE(previews_tray_icon);
  ASSERT_TRUE(previews_tray_icon->layer());
  ASSERT_EQ(1u, previews_tray_icon->layer()->children().size());
  auto* previews_container_layer =
      previews_tray_icon->layer()->children()[0].get();
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
  ViewDrawnWaiter().Wait(previews_tray_icon);
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
  ViewDrawnWaiter().Wait(default_tray_icon);
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

// Base class for holding space UI browser tests that require in-progress
// downloads integration. NOTE: This test suite will swap out the production
// download manager with a mock instance.
class HoldingSpaceUiInProgressDownloadsBrowserTest
    : public HoldingSpaceUiBrowserTest {
 public:
  HoldingSpaceUiInProgressDownloadsBrowserTest() {
    // Use a testing factory to give us a chance to swap out the production
    // download manager for a given browser `context` with a mock prior to
    // holding space keyed service creation.
    HoldingSpaceKeyedServiceFactory::SetTestingFactory(
        base::BindLambdaForTesting([&](content::BrowserContext* context) {
          DCHECK(!download_manager_);

          // Create a mock download manager.
          download_manager_ =
              new testing::NiceMock<content::MockDownloadManager>();

          // Mock `content::DownloadManager::Shutdown()`.
          ON_CALL(*download_manager_, Shutdown)
              .WillByDefault(testing::Invoke([&]() {
                if (download_manager_->GetDelegate()) {
                  download_manager_->GetDelegate()->Shutdown();
                  download_manager_->SetDelegate(nullptr);
                }
              }));

          // Mock `content::DownloadManager::IsManagerInitialized()`.
          ON_CALL(*download_manager_, IsManagerInitialized())
              .WillByDefault(testing::Return(true));

          // Mock `content::DownloadManager::AddObserver()`.
          ON_CALL(*download_manager_, AddObserver)
              .WillByDefault(testing::Invoke(
                  &download_manager_observers_,
                  &base::ObserverList<content::DownloadManager::Observer>::
                      Unchecked::AddObserver));

          // Mock `content::DownloadManager::RemoveObserver()`.
          ON_CALL(*download_manager_, RemoveObserver)
              .WillByDefault(testing::Invoke(
                  &download_manager_observers_,
                  &base::ObserverList<content::DownloadManager::Observer>::
                      Unchecked::RemoveObserver));

          // Mock `content::DownloadManager::GetBrowserContext()`.
          ON_CALL(*download_manager_, GetBrowserContext)
              .WillByDefault(testing::Return(context));

          // Mock `content::DownloadManager::SetDelegate()`.
          ON_CALL(*download_manager_, SetDelegate)
              .WillByDefault(testing::Invoke(
                  [&](content::DownloadManagerDelegate* delegate) {
                    download_manager_delegate_ = delegate;
                  }));

          // Mock `content::DownloadManager::GetDelegate()`.
          ON_CALL(*download_manager_, GetDelegate)
              .WillByDefault(testing::Invoke(
                  [&]() { return download_manager_delegate_; }));

          // Swap out the production download manager for the mock.
          context->SetDownloadManagerForTesting(
              base::WrapUnique(download_manager_.get()));

          // Install a new download manager delegate after swapping out the
          // production download manager so it will properly register itself
          // with the mock.
          DownloadCoreServiceFactory::GetForBrowserContext(context)
              ->SetDownloadManagerDelegateForTesting(
                  std::make_unique<ChromeDownloadManagerDelegate>(
                      Profile::FromBrowserContext(context)));

          // Resume default construction sequence.
          return HoldingSpaceKeyedServiceFactory::GetDefaultTestingFactory()
              .Run(context);
        }));
  }

  ~HoldingSpaceUiInProgressDownloadsBrowserTest() override {
    HoldingSpaceKeyedServiceFactory::SetTestingFactory(base::NullCallback());
  }

  // HoldingSpaceUiBrowserTest:
  void TearDownOnMainThread() override {
    HoldingSpaceUiBrowserTest::TearDownOnMainThread();

    for (auto& observer : download_manager_observers_)
      observer.ManagerGoingDown(download_manager_);
  }

  using AshDownload = testing::NiceMock<download::MockDownloadItem>;

  // Creates an in-progress download. If `paused` is `true`, the in-progress
  // download will be paused.
  std::unique_ptr<AshDownload> CreateInProgressDownload(bool paused = false) {
    std::unique_ptr<AshDownload> in_progress_download = CreateAshDownloadItem(
        download::DownloadItem::IN_PROGRESS, /*file_path=*/CreateFile(),
        /*target_file_path=*/CreateFile(), /*received_bytes=*/0,
        /*total_bytes=*/100);
    if (paused) {
      in_progress_download->Pause();
    }
    NotifyObserversAshDownloadUpdated(in_progress_download.get());
    return in_progress_download;
  }

  // Creates a completed download.
  std::unique_ptr<AshDownload> CreateCompletedDownload() {
    // NOTE: In production, the download manager will create completed download
    // items from previous sessions during initialization, so we ignore them.
    // To match production behavior, create an in-progress download item and
    // only then update it to complete state.
    std::unique_ptr<AshDownload> completed_download = CreateAshDownloadItem(
        download::DownloadItem::IN_PROGRESS, /*file_path=*/CreateFile(),
        /*target_file_path=*/CreateFile(), /*received_bytes=*/0,
        /*total_bytes=*/100);
    ON_CALL(*completed_download, GetState())
        .WillByDefault(testing::Return(download::DownloadItem::COMPLETE));
    ON_CALL(*completed_download, GetReceivedBytes())
        .WillByDefault(testing::Return(100));
    NotifyObserversAshDownloadUpdated(completed_download.get());
    return completed_download;
  }

  // Completes the specified `in_progress_download`.
  void CompleteInProgressDownload(AshDownload* in_progress_download) {
    ON_CALL(*in_progress_download, GetState())
        .WillByDefault(testing::Return(download::DownloadItem::COMPLETE));
    ON_CALL(*in_progress_download, GetReceivedBytes())
        .WillByDefault(testing::Return(in_progress_download->GetTotalBytes()));
    NotifyObserversAshDownloadUpdated(in_progress_download);
  }

  // Pauses the specified `in_progress_download`.
  void PauseInProgressDownload(AshDownload* in_progress_download) {
    in_progress_download->Pause();
  }

  // Updates the byte counts for the specified `in_progress_download`.
  void UpdateInProgressDownloadByteCounts(AshDownload* in_progress_download,
                                          int32_t received_bytes,
                                          int32_t total_bytes) {
    ON_CALL(*in_progress_download, GetReceivedBytes())
        .WillByDefault(testing::Return(received_bytes));
    ON_CALL(*in_progress_download, GetTotalBytes())
        .WillByDefault(testing::Return(total_bytes));
    NotifyObserversAshDownloadUpdated(in_progress_download);
  }

  // Updates whether the specified `in_progress_download`  is dangerous,
  // insecure, or might be malicious.
  void UpdateInProgressDownloadIsDangerousInsecureOrMightBeMalicious(
      AshDownload* in_progress_download,
      bool is_dangerous,
      bool is_insecure,
      bool might_be_malicious) {
    ASSERT_TRUE(is_dangerous || !might_be_malicious);
    ON_CALL(*in_progress_download, GetDangerType())
        .WillByDefault(testing::Return(
            is_dangerous
                ? might_be_malicious
                      ? download::DownloadDangerType::
                            DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT
                      : download::DownloadDangerType::
                            DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE
                : download::DownloadDangerType::
                      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    ON_CALL(*in_progress_download, IsDangerous())
        .WillByDefault(testing::Return(is_dangerous));
    ON_CALL(*in_progress_download, IsInsecure())
        .WillByDefault(testing::Return(is_insecure));
    NotifyObserversAshDownloadUpdated(in_progress_download);
  }

  // Updates whether the specified `in_progress_download` is scanning.
  void UpdateInProgressDownloadIsScanning(AshDownload* in_progress_download,
                                          bool is_scanning) {
    const bool was_scanning =
        in_progress_download->GetDangerType() ==
        download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING;
    if (is_scanning != was_scanning) {
      ON_CALL(*in_progress_download, GetDangerType())
          .WillByDefault(testing::Return(
              is_scanning ? download::DownloadDangerType::
                                DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING
                          : download::DownloadDangerType::
                                DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
      ON_CALL(*in_progress_download, IsDangerous())
          .WillByDefault(testing::Return(false));
      NotifyObserversAshDownloadUpdated(in_progress_download);
    }
  }

  // Returns the target file path for the specified `download`.
  base::FilePath GetTargetFilePath(const AshDownload* download) const {
    return download->GetTargetFilePath();
  }

 private:
  // Creates and returns an Ash download item with the specified `state`,
  // `file_path`, `target_file_path`, `received_bytes`, and `total_bytes`.
  std::unique_ptr<AshDownload> CreateAshDownloadItem(
      download::DownloadItem::DownloadState state,
      const base::FilePath& file_path,
      const base::FilePath& target_file_path,
      int64_t received_bytes,
      int64_t total_bytes) {
    auto ash_download_item = std::make_unique<AshDownload>();

    content::DownloadItemUtils::AttachInfo(
        ash_download_item.get(), GetProfile(),
        /*web_contents=*/nullptr, content::GlobalRenderFrameHostId());

    // Mock `download::DownloadItem::Cancel()`.
    ON_CALL(*ash_download_item, Cancel(/*from_user=*/testing::Eq(true)))
        .WillByDefault(testing::InvokeWithoutArgs(
            [ash_download_item = ash_download_item.get()]() {
              // When a download is cancelled, the underlying file is deleted.
              const auto& file_path = ash_download_item->GetFullPath();
              if (!file_path.empty()) {
                base::ScopedAllowBlockingForTesting allow_blocking;
                ASSERT_TRUE(base::DeleteFile(file_path));
              }
              // Any subsequent calls to `download::DownloadItem::GetState()`
              // should indicate that the `mock_download_item` is cancelled.
              ON_CALL(*ash_download_item, GetState)
                  .WillByDefault(
                      testing::Return(download::DownloadItem::CANCELLED));
              // Calling `download::DownloadItem::Cancel()` results in updates.
              ash_download_item->NotifyObserversDownloadUpdated();
            }));

    // Mock `download::DownloadItem::GetETag()`.
    ON_CALL(*ash_download_item, GetETag)
        .WillByDefault(testing::ReturnRefOfCopy(std::string()));

    // Mock `download::DownloadItem::GetFullPath()`.
    ON_CALL(*ash_download_item, GetFullPath)
        .WillByDefault(testing::Invoke(
            [ash_download_item = ash_download_item.get(),
             file_path = base::FilePath(file_path)]() -> const base::FilePath& {
              return ash_download_item->GetState() ==
                             download::DownloadItem::COMPLETE
                         ? ash_download_item->GetTargetFilePath()
                         : file_path;
            }));

    // Mock `download::DownloadItem::GetGuid()`.
    ON_CALL(*ash_download_item, GetGuid)
        .WillByDefault(testing::ReturnRefOfCopy(
            base::Uuid::GenerateRandomV4().AsLowercaseString()));

    // Mock `download::DownloadItem::GetId()`.
    ON_CALL(*ash_download_item, GetId).WillByDefault(testing::Invoke([]() {
      static uint32_t kNextId = 1u;
      return kNextId++;
    }));

    // Mock `download::DownloadItem::GetLastModifiedTime()`.
    ON_CALL(*ash_download_item, GetLastModifiedTime)
        .WillByDefault(testing::ReturnRefOfCopy(std::string()));

    // Mock `download::DownloadItem::GetLastReason()`.
    ON_CALL(*ash_download_item, GetLastReason)
        .WillByDefault(
            testing::Invoke([ash_download_item = ash_download_item.get()]() {
              return ash_download_item->GetState() ==
                             download::DownloadItem::CANCELLED
                         ? download::DownloadInterruptReason::
                               DOWNLOAD_INTERRUPT_REASON_USER_CANCELED
                         : download::DownloadInterruptReason::
                               DOWNLOAD_INTERRUPT_REASON_NONE;
            }));

    // Mock `download::DownloadItem::GetOpenWhenComplete()`.
    auto open_when_complete = std::make_unique<bool>(false);
    ON_CALL(*ash_download_item, GetOpenWhenComplete)
        .WillByDefault(testing::ReturnPointee(open_when_complete.get()));

    // Mock `download::DownloadItem::GetReceivedBytes()`.
    ON_CALL(*ash_download_item, GetReceivedBytes)
        .WillByDefault(testing::Return(received_bytes));

    // Mock `download::DownloadItem::GetReceivedSlices()`.
    ON_CALL(*ash_download_item, GetReceivedSlices)
        .WillByDefault(testing::ReturnRefOfCopy(
            std::vector<download::DownloadItem::ReceivedSlice>()));

    // Mock `download::DownloadItem::GetSerializedEmbedderDownloadData()`.
    ON_CALL(*ash_download_item, GetSerializedEmbedderDownloadData)
        .WillByDefault(testing::ReturnRefOfCopy(std::string()));

    // Mock `download::DownloadItem::GetReferrerUrl()`.
    ON_CALL(*ash_download_item, GetReferrerUrl)
        .WillByDefault(testing::ReturnRefOfCopy(GURL()));

    // Mock `download::DownloadItem::GetTabUrl()`.
    ON_CALL(*ash_download_item, GetTabUrl)
        .WillByDefault(testing::ReturnRefOfCopy(GURL()));

    // Mock `download::DownloadItem::GetTabReferrerUrl()`.
    ON_CALL(*ash_download_item, GetTabReferrerUrl)
        .WillByDefault(testing::ReturnRefOfCopy(GURL()));

    // Mock `download::DownloadItem::GetState()`.
    ON_CALL(*ash_download_item, GetState).WillByDefault(testing::Return(state));

    // Mock `download::DownloadItem::GetTargetFilePath()`.
    ON_CALL(*ash_download_item, GetTargetFilePath)
        .WillByDefault(testing::ReturnRefOfCopy(target_file_path));

    // Mock `download::DownloadItem::GetTotalBytes()`.
    ON_CALL(*ash_download_item, GetTotalBytes)
        .WillByDefault(testing::Return(total_bytes));

    // Mock `download::DownloadItem::GetURL()`.
    ON_CALL(*ash_download_item, GetURL)
        .WillByDefault(testing::ReturnRefOfCopy(GURL()));

    // Mock `download::DownloadItem::GetUrlChain()`.
    ON_CALL(*ash_download_item, GetUrlChain)
        .WillByDefault(testing::ReturnRefOfCopy(std::vector<GURL>()));

    // Mock `download::DownloadItem::IsDone()`.
    ON_CALL(*ash_download_item, IsDone)
        .WillByDefault(
            testing::Invoke([ash_download_item = ash_download_item.get()]() {
              return ash_download_item->GetState() ==
                     download::DownloadItem::COMPLETE;
            }));

    // Mock `download::DownloadItem::IsPaused()`.
    auto paused = std::make_unique<bool>(false);
    ON_CALL(*ash_download_item, IsPaused)
        .WillByDefault(testing::ReturnPointee(paused.get()));

    // Create a callback which can be run to set `paused` state and which
    // mirrors production behavior by notifying observers on change.
    auto set_paused = base::BindRepeating(
        [](download::MockDownloadItem* ash_download_item, bool* paused,
           bool new_paused) {
          if (*paused != new_paused) {
            *paused = new_paused;
            ash_download_item->NotifyObserversDownloadUpdated();
          }
        },
        base::Unretained(ash_download_item.get()),
        base::Owned(std::move(paused)));

    // Mock `download::DownloadItem::Pause()`.
    ON_CALL(*ash_download_item, Pause).WillByDefault([set_paused]() {
      set_paused.Run(true);
    });

    // Mock `download::DownloadItem::Resume()`.
    ON_CALL(*ash_download_item, Resume(/*from_user=*/testing::Eq(true)))
        .WillByDefault([set_paused]() { set_paused.Run(false); });

    // Mock `download::DownloadItem::SetOpenWhenComplete()`.
    ON_CALL(*ash_download_item, SetOpenWhenComplete)
        .WillByDefault(
            [callback = base::BindRepeating(
                 [](download::MockDownloadItem* ash_download_item,
                    bool* open_when_complete, bool new_open_when_complete) {
                   if (*open_when_complete != new_open_when_complete) {
                     *open_when_complete = new_open_when_complete;
                     ash_download_item->NotifyObserversDownloadUpdated();
                   }
                 },
                 base::Unretained(ash_download_item.get()),
                 base::Owned(std::move(open_when_complete)))](
                bool new_open_when_complete) {
              callback.Run(new_open_when_complete);
            });

    // Notify observers of the created download.
    for (auto& observer : download_manager_observers_)
      observer.OnDownloadCreated(download_manager_, ash_download_item.get());

    return ash_download_item;
  }

  // Notifies observers that the specified `ash_download` has been updated.
  void NotifyObserversAshDownloadUpdated(
      download::MockDownloadItem* ash_download) {
    ash_download->NotifyObserversDownloadUpdated();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<testing::NiceMock<content::MockDownloadManager>, DanglingUntriaged>
      download_manager_ = nullptr;
  raw_ptr<content::DownloadManagerDelegate> download_manager_delegate_ =
      nullptr;
  base::ObserverList<content::DownloadManager::Observer>::Unchecked
      download_manager_observers_;
};

// Verifies that primary, secondary, and accessible text work as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiInProgressDownloadsBrowserTest,
                       PrimarySecondaryAndAccessibleText) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Force locale since strings are being verified.
  base::ScopedLocale scoped_locale("en_US.UTF-8");

  // Create an in-progress download.
  auto in_progress_download = CreateInProgressDownload();

  // Update byte counts.
  int32_t received_bytes = 0;
  int32_t total_bytes = -1;
  UpdateInProgressDownloadByteCounts(in_progress_download.get(), received_bytes,
                                     total_bytes);

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Verify the existence of a single download chip.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);

  // Wait for the download chip to be drawn with an indeterminate progress ring
  // animation.
  NextMainFrameWaiter(Shell::GetPrimaryRootWindow()->GetHost()->compositor())
      .Wait();
  EXPECT_THAT(
      HoldingSpaceAnimationRegistry::GetInstance()
          ->GetProgressRingAnimationForKey(
              ProgressIndicatorAnimationRegistry::AsAnimationKey(
                  HoldingSpaceController::Get()->model()->GetItem(
                      test_api().GetHoldingSpaceItemId(download_chips[0])))),
      Property(&ProgressRingAnimation::type,
               Eq(ProgressRingAnimation::Type::kIndeterminate)));

  // Cache pointers to the `primary_label` and `secondary_label`.
  auto* primary_label = static_cast<views::Label*>(
      download_chips[0]->GetViewByID(kHoldingSpaceItemPrimaryChipLabelId));
  auto* secondary_label = static_cast<views::Label*>(
      download_chips[0]->GetViewByID(kHoldingSpaceItemSecondaryChipLabelId));

  // The `primary_label` should always be visible and should always show the
  // lossy display name of the download's target file path.
  const auto target_file_path = GetTargetFilePath(in_progress_download.get());
  const auto target_file_name = target_file_path.BaseName().LossyDisplayName();
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);

  // Initially, no bytes have been received so `secondary_label` should display
  // `0 B` as there is no knowledge of the total number of bytes expected.
  EXPECT_TRUE(secondary_label->GetVisible());
  EXPECT_EQ(secondary_label->GetText(), u"0 B");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Downloading " + target_file_name));

  // Pause the download.
  RightClick(download_chips.at(0));
  EXPECT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kPauseItem));
  PressAndReleaseKey(ui::VKEY_RETURN);

  // When paused with no bytes received, the `secondary_label` should display
  // "Paused, 0 B" as there is still no knowledge of the total number of
  // bytes expected.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Paused, 0 B");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress and
  // that progress is paused.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download paused " + target_file_name));

  // Update received bytes.
  received_bytes = 1024 * 1024;
  UpdateInProgressDownloadByteCounts(in_progress_download.get(), received_bytes,
                                     total_bytes);

  // When paused with bytes received, the `secondary_label` should display both
  // the paused state and the number of bytes received with appropriate units.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Paused, 1,024 KB");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress and
  // that progress is paused.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download paused " + target_file_name));

  // Resume the download.
  RightClick(download_chips.at(0));
  EXPECT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kResumeItem));
  PressAndReleaseKey(ui::VKEY_RETURN);

  // If resumed with bytes received, the `secondary_label` should display only
  // the number of bytes received with appropriate units.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"1,024 KB");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Downloading " + target_file_name));

  // Update total bytes.
  total_bytes = 2 * received_bytes;
  UpdateInProgressDownloadByteCounts(in_progress_download.get(), received_bytes,
                                     total_bytes);

  // If both the number of bytes received and the total number of bytes expected
  // are known, the `secondary_label` should display both with appropriate
  // units.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"1.0/2.0 MB");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Downloading " + target_file_name));

  // Pause the download.
  RightClick(download_chips.at(0));
  EXPECT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kPauseItem));
  PressAndReleaseKey(ui::VKEY_RETURN);

  // If paused with both the number of bytes received and the total number of
  // bytes expected known, the `secondary_label` should display the paused state
  // and both received and total bytes with appropriate units.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Paused, 1.0/2.0 MB");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress and
  // that progress is paused.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download paused " + target_file_name));

  // Update received bytes to indicate that all bytes have been received.
  received_bytes = total_bytes;
  UpdateInProgressDownloadByteCounts(in_progress_download.get(), received_bytes,
                                     total_bytes);

  // Because the download has not yet been marked complete, the number of bytes
  // received will not equal the total number of expected bytes but in most
  // cases that will be imperceptible to the user due to rounding. This is to
  // prevent giving the impression of completion before download progress is
  // truly complete (which does not occur until after renaming, etc).
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Paused, 2.0/2.0 MB");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress and
  // that progress is paused.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download paused " + target_file_name));

  // Mark the download as dangerous.
  UpdateInProgressDownloadIsDangerousInsecureOrMightBeMalicious(
      in_progress_download.get(),
      /*is_dangerous=*/true,
      /*is_insecure=*/false,
      /*might_be_malicious=*/true);

  // Because the download is marked as dangerous, that should be indicated in
  // the `secondary_label` of the holding space item chip view.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Dangerous file");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(cros_tokens::kTextColorAlert)));

  // The accessible name should indicate that the download is dangerous.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download dangerous " + target_file_name));

  // Mark the download as being scanned.
  UpdateInProgressDownloadIsScanning(in_progress_download.get(), true);

  // Because the download is marked as being scanned, that should be indicated
  // in the `secondary_label` of the holding space item chip view.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Scanning");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(cros_tokens::kTextColorProminent)));

  // The accessible name should indicate that the download is being scanning.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download scanning " + target_file_name));

  // Stop scanning and mark that the download is *not* malicious.
  UpdateInProgressDownloadIsDangerousInsecureOrMightBeMalicious(
      in_progress_download.get(), /*is_dangerous=*/true,
      /*is_insecure=*/false, /*might_be_malicious=*/false);

  // Because the download is *not* malicious, the user will be able to keep/
  // discard the download via notification. That should be indicated in the
  // `secondary_label` of the holding space item chip view.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Confirm download");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(cros_tokens::kTextColorWarning)));

  // The accessible name should indicate that the download must be confirmed.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Confirm download " + target_file_name));

  // Mark the download as safe.
  UpdateInProgressDownloadIsDangerousInsecureOrMightBeMalicious(
      in_progress_download.get(),
      /*is_dangerous=*/false,
      /*is_insecure=*/false,
      /*might_be_malicious=*/false);

  // Because the download is no longer marked as dangerous, that should be
  // indicated in the `secondary_label` of the holding space item chip view.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Paused, 2.0/2.0 MB");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress and
  // that progress is paused.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download paused " + target_file_name));

  // Mark the download as insecure.
  UpdateInProgressDownloadIsDangerousInsecureOrMightBeMalicious(
      in_progress_download.get(),
      /*is_dangerous=*/false,
      /*is_insecure=*/true,
      /*might_be_malicious=*/false);

  // Because the download is marked as insecure, that should be indicated
  // in the `secondary_label` of the holding space item chip view.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Dangerous file");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(cros_tokens::kTextColorAlert)));

  // The accessible name should indicate that the download is dangerous.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download dangerous " + target_file_name));

  // Mark the download as *not* insecure.
  UpdateInProgressDownloadIsDangerousInsecureOrMightBeMalicious(
      in_progress_download.get(),
      /*is_dangerous=*/false,
      /*is_insecure=*/false,
      /*might_be_malicious=*/false);

  // Because the download is no longer marked as insecure, that should be
  // indicated in the `secondary_label` of the holding space item chip view.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Paused, 2.0/2.0 MB");
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate that the download is in progress and
  // that progress is paused.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(u"Download paused " + target_file_name));

  // Complete the download.
  CompleteInProgressDownload(in_progress_download.get());

  // When no longer in progress, the `secondary_label` should be hidden.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_FALSE(secondary_label->GetVisible());
  EXPECT_THAT(secondary_label,
              EnabledColorId(Optional(kColorAshTextColorSecondary)));

  // The accessible name should indicate the target file name.
  EXPECT_EQ(GetAccessibleName(download_chips.at(0)),
            base::UTF16ToUTF8(target_file_name));
}

// Verifies that canceling holding space items works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiInProgressDownloadsBrowserTest,
                       CancelItem) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Create an in-progress download and a completed download.
  auto in_progress_download = CreateInProgressDownload();
  auto completed_download = CreateCompletedDownload();

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  views::View* const completed_download_chip = download_chips.at(0);
  views::View* const in_progress_download_chip = download_chips.at(1);

  // Right click the `completed_download_chip`. Because the underlying download
  // is completed, the context menu should *not* contain a "Cancel" command.
  RightClick(completed_download_chip);
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kCancelItem));

  // Close the context menu and control-right click the
  // `in_progress_download_chip`. Because the `completed_download_chip` is still
  // selected and its underlying download is completed, the context menu should
  // *not* contain a "Cancel" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  RightClick(in_progress_download_chip, ui::EF_CONTROL_DOWN);
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kCancelItem));

  // Close the context menu, press the `in_progress_download_chip` and then
  // right click it. Because the `in_progress_download_chip` is the only chip
  // selected and its underlying download is in-progress, the context menu
  // should contain a "Cancel" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  test::Click(in_progress_download_chip);
  RightClick(in_progress_download_chip);
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kCancelItem));

  // Cache the holding space item IDs associated with the two download chips.
  const std::string completed_download_id =
      test_api().GetHoldingSpaceItemId(completed_download_chip);
  const std::string in_progress_download_id =
      test_api().GetHoldingSpaceItemId(in_progress_download_chip);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Press ENTER to execute the "Cancel" command, expecting and waiting for
  // the in-progress download item to be removed from the holding space model.
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        ASSERT_EQ(items[0]->id(), in_progress_download_id);
        run_loop.Quit();
      });
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  run_loop.Run();

  // Verify that there is now only a single download chip.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);

  // Because the in-progress download was canceled, only the completed download
  // chip should still be present in the UI.
  EXPECT_TRUE(test_api().GetHoldingSpaceItemView(download_chips,
                                                 completed_download_id));
  EXPECT_FALSE(test_api().GetHoldingSpaceItemView(download_chips,
                                                  in_progress_download_id));
}

// Verifies that canceling holding space items via primary action is WAI.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiInProgressDownloadsBrowserTest,
                       CancelItemViaPrimaryAction) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Create an in-progress download and a completed download.
  auto in_progress_download = CreateInProgressDownload();
  auto completed_download = CreateCompletedDownload();

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  views::View* const completed_download_chip = download_chips.at(0);
  views::View* const in_progress_download_chip = download_chips.at(1);

  // Hover over the `completed_download_chip`. Because the underlying download
  // is completed, the chip should contain a visible primary action for "Pin".
  test::MoveMouseTo(completed_download_chip, /*count=*/10);
  auto* primary_action_container = completed_download_chip->GetViewByID(
      kHoldingSpaceItemPrimaryActionContainerId);
  auto* primary_action_cancel =
      primary_action_container->GetViewByID(kHoldingSpaceItemCancelButtonId);
  auto* primary_action_pin =
      primary_action_container->GetViewByID(kHoldingSpaceItemPinButtonId);
  ViewDrawnWaiter().Wait(primary_action_container);
  EXPECT_FALSE(primary_action_cancel->GetVisible());
  EXPECT_TRUE(primary_action_pin->GetVisible());

  // Hover over the `in_progress_download_chip`. Because the underlying download
  // is in-progress, the chip should contain a visible primary action for
  // "Cancel".
  test::MoveMouseTo(in_progress_download_chip, /*count=*/10);
  primary_action_container = in_progress_download_chip->GetViewByID(
      kHoldingSpaceItemPrimaryActionContainerId);
  primary_action_cancel =
      primary_action_container->GetViewByID(kHoldingSpaceItemCancelButtonId);
  primary_action_pin =
      primary_action_container->GetViewByID(kHoldingSpaceItemPinButtonId);
  ViewDrawnWaiter().Wait(primary_action_container);
  EXPECT_TRUE(primary_action_cancel->GetVisible());
  EXPECT_FALSE(primary_action_pin->GetVisible());

  // Cache the holding space item IDs associated with the two download chips.
  const std::string completed_download_id =
      test_api().GetHoldingSpaceItemId(completed_download_chip);
  const std::string in_progress_download_id =
      test_api().GetHoldingSpaceItemId(in_progress_download_chip);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Press the `primary_action_container` to execute "Cancel", expecting and
  // waiting for the in-progress download item to be removed from the holding
  // space model.
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        ASSERT_EQ(items[0]->id(), in_progress_download_id);
        run_loop.Quit();
      });
  test::Click(primary_action_container);
  run_loop.Run();

  // Verify that there is now only a single download chip.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);

  // Because the in-progress download was canceled, only the completed download
  // chip should still be present in the UI.
  EXPECT_TRUE(test_api().GetHoldingSpaceItemView(download_chips,
                                                 completed_download_id));
  EXPECT_FALSE(test_api().GetHoldingSpaceItemView(download_chips,
                                                  in_progress_download_id));
}

// Verifies that opening in-progress download items works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiInProgressDownloadsBrowserTest,
                       OpenItemWhenComplete) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Force locale since strings are being verified.
  base::ScopedLocale scoped_locale("en_US.UTF-8");

  // Create an in-progress download.
  auto in_progress_download = CreateInProgressDownload();

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Expect a single download chip.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 1u);

  // Cache pointers to the `primary_label` and `secondary_label`.
  auto* const primary_label = static_cast<views::Label*>(
      download_chips.front()->GetViewByID(kHoldingSpaceItemPrimaryChipLabelId));
  auto* const secondary_label =
      static_cast<views::Label*>(download_chips.front()->GetViewByID(
          kHoldingSpaceItemSecondaryChipLabelId));

  // The `primary_label` should be visible and should show the lossy display
  // name of the download's target file path.
  const auto target_file_path = GetTargetFilePath(in_progress_download.get());
  const auto target_file_name = target_file_path.BaseName().LossyDisplayName();
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);

  // The `secondary_label` should also be visible and should show `0/100 B` as
  // no bytes have been received but the total number of bytes is known.
  EXPECT_TRUE(secondary_label->GetVisible());
  EXPECT_EQ(secondary_label->GetText(), u"0/100 B");

  // Double click the download chip to open the item. Because the underlying
  // item is in-progress, opening should not occur immediately but should
  // instead be queued up until download completion.
  DoubleClick(download_chips.front());

  // The `primary_label` should still be visible and should still show the
  // lossy display name of the download's target file path.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);

  // The `secondary_label` should still be visible but should have been updated
  // to reflect that the underlying download will be opened when complete.
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Open when complete");

  // Pause the download.
  PauseInProgressDownload(in_progress_download.get());

  // The `secondary_label` should still be visible but should have been updated
  // to reflect that the underlying download is paused.
  EXPECT_TRUE(secondary_label->GetVisible());
  WaitForText(secondary_label, u"Paused, 0/100 B");

  // Complete the download.
  CompleteInProgressDownload(in_progress_download.get());

  // When no longer in progress, the `secondary_label` should be hidden.
  EXPECT_TRUE(primary_label->GetVisible());
  EXPECT_EQ(primary_label->GetText(), target_file_name);
  EXPECT_FALSE(secondary_label->GetVisible());
}

// Verifies that removing holding space items works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiInProgressDownloadsBrowserTest,
                       RemoveItem) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Create an in-progress download and a completed download.
  auto in_progress_download = CreateInProgressDownload();
  auto completed_download = CreateCompletedDownload();

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  views::View* const completed_download_chip = download_chips.at(0);
  views::View* const in_progress_download_chip = download_chips.at(1);

  // Right click the `in_progress_download_chip`. Because the underlying
  // download is in-progress, the context menu should *not* contain a "Remove"
  // command.
  RightClick(in_progress_download_chip);
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Close the context menu and control-right click the
  // `completed_download_chip`. Because the `in_progress_download_chip` is still
  // selected and its underlying download is in-progress, the context menu
  // should *not* contain a "Remove" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  RightClick(completed_download_chip, ui::EF_CONTROL_DOWN);
  ASSERT_FALSE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Close the context menu, press the `completed_download_chip` and then
  // right click it. Because the `completed_download_chip` is the only chip
  // selected and its underlying download is completed, the context menu should
  // contain a "Remove" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  test::Click(completed_download_chip);
  RightClick(completed_download_chip);
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));

  // Cache the holding space item IDs associated with the two download chips.
  const std::string completed_download_id =
      test_api().GetHoldingSpaceItemId(completed_download_chip);
  const std::string in_progress_download_id =
      test_api().GetHoldingSpaceItemId(in_progress_download_chip);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Press ENTER to execute the "Remove" command, expecting and waiting for
  // the completed download item to be removed from the holding space model.
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        ASSERT_EQ(items[0]->id(), completed_download_id);
        run_loop.Quit();
      });
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  run_loop.Run();

  // Verify that there is now only a single download chip.
  download_chips = test_api().GetDownloadChips();
  EXPECT_EQ(download_chips.size(), 1u);

  // Because the completed download was canceled, only the in-progress download
  // chip should still be present in the UI.
  EXPECT_FALSE(test_api().GetHoldingSpaceItemView(download_chips,
                                                  completed_download_id));
  EXPECT_TRUE(test_api().GetHoldingSpaceItemView(download_chips,
                                                 in_progress_download_id));

  // Complete the in-progress download.
  CompleteInProgressDownload(in_progress_download.get());

  // Because the in-progress download has been completed, right clicking it
  // should now surface the "Remove" command.
  RightClick(download_chips.front());
  ASSERT_TRUE(SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem));
}

// Base class for tests of the pause or resume commands, parameterized by the
// command to use. This will either be `kPauseItem` or `kResumeItem`.
class HoldingSpaceUiPauseOrResumeBrowserTest
    : public HoldingSpaceUiInProgressDownloadsBrowserTest,
      public testing::WithParamInterface<HoldingSpaceCommandId> {
 public:
  HoldingSpaceUiPauseOrResumeBrowserTest()
      : HoldingSpaceUiInProgressDownloadsBrowserTest() {
    const HoldingSpaceCommandId command_id(GetPauseOrResumeCommandId());
    EXPECT_TRUE(command_id == HoldingSpaceCommandId::kPauseItem ||
                command_id == HoldingSpaceCommandId::kResumeItem);
  }

  // Returns either `kPauseItem` or `kResumeItem` depending on parameterization.
  HoldingSpaceCommandId GetPauseOrResumeCommandId() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceUiPauseOrResumeBrowserTest,
    testing::ValuesIn({HoldingSpaceCommandId::kPauseItem,
                       HoldingSpaceCommandId::kResumeItem}));

// Verifies that pausing or resuming holding space items works as intended.
IN_PROC_BROWSER_TEST_P(HoldingSpaceUiPauseOrResumeBrowserTest,
                       PauseOrResumeItem) {
  // Use zero animation duration so that UI updates are immediate.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Create an in-progress download which may or may not be paused depending
  // on parameterization.
  auto in_progress_download =
      CreateInProgressDownload(/*paused=*/GetPauseOrResumeCommandId() ==
                               HoldingSpaceCommandId::kResumeItem);

  // Create a completed download.
  auto completed_download = CreateCompletedDownload();

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  views::View* const completed_download_chip = download_chips.at(0);
  views::View* const in_progress_download_chip = download_chips.at(1);

  // Right click the `completed_download_chip`. Because the underlying download
  // is completed, the context menu should *not* contain a "Pause" or "Resume"
  // command.
  RightClick(completed_download_chip);
  ASSERT_FALSE(SelectMenuItemWithCommandId(GetPauseOrResumeCommandId()));

  // Close the context menu and control-right click the
  // `in_progress_download_chip`. Because the `completed_download_chip` is still
  // selected and its underlying download is completed, the context menu should
  // *not* contain a "Pause" or "Resume" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  RightClick(in_progress_download_chip, ui::EF_CONTROL_DOWN);
  ASSERT_FALSE(SelectMenuItemWithCommandId(GetPauseOrResumeCommandId()));

  // Close the context menu, press the `in_progress_download_chip` and then
  // right click it. Because the `in_progress_download_chip` is the only chip
  // selected and its underlying download is in-progress, the context menu
  // should contain a "Pause" or "Resume" command.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  test::Click(in_progress_download_chip);
  RightClick(in_progress_download_chip);
  ASSERT_TRUE(SelectMenuItemWithCommandId(GetPauseOrResumeCommandId()));

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Press ENTER to execute the "Pause" or "Resume" command, expecting and
  // waiting for the in-progress download item to be updated in the holding
  // space model.
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemUpdated)
      .WillOnce([&](const HoldingSpaceItem* item,
                    const HoldingSpaceItemUpdatedFields& updated_fields) {
        EXPECT_EQ(item->id(),
                  test_api().GetHoldingSpaceItemId(in_progress_download_chip));
        EXPECT_TRUE(updated_fields.previous_in_progress_commands);
        run_loop.Quit();
      });
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  run_loop.Run();

  // Verify that there are still two download chips.
  download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // The two download chips present should still be the original chips for the
  // completed download and the (now paused or resumed) in-progress download.
  EXPECT_EQ(download_chips.at(0), completed_download_chip);
  EXPECT_EQ(download_chips.at(1), in_progress_download_chip);
}

// Verifies that pausing or resuming holding space items via secondary action is
// working as intended.
IN_PROC_BROWSER_TEST_P(HoldingSpaceUiPauseOrResumeBrowserTest,
                       PauseOrResumeItemViaSecondaryAction) {
  // Use zero animation duration so that UI updates are immediate.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Create an in-progress download which may or may not be paused depending
  // on parameterization.
  auto in_progress_download =
      CreateInProgressDownload(/*paused=*/GetPauseOrResumeCommandId() ==
                               HoldingSpaceCommandId::kResumeItem);

  // Create a completed download.
  auto completed_download = CreateCompletedDownload();

  // Show holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());

  // Expect two download chips, one for each created download item.
  std::vector<views::View*> download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // Cache download chips. NOTE: Chips are displayed in reverse order of their
  // underlying holding space item creation.
  views::View* const completed_download_chip = download_chips.at(0);
  views::View* const in_progress_download_chip = download_chips.at(1);

  // Hover over the `completed_download_chip`. Because the underlying download
  // is completed, the chip should not contain a visible secondary action.
  test::MoveMouseTo(completed_download_chip, /*count=*/10);
  ASSERT_FALSE(completed_download_chip
                   ->GetViewByID(kHoldingSpaceItemSecondaryActionContainerId)
                   ->GetVisible());

  // Hover over the `in_progress_download_chip`. Because the underlying download
  // is in-progress, the chip should contain a visible secondary action for
  // either "Pause" or "Resume", depending on test parameterization.
  test::MoveMouseTo(in_progress_download_chip, /*count=*/10);
  auto* secondary_action_container = in_progress_download_chip->GetViewByID(
      kHoldingSpaceItemSecondaryActionContainerId);
  auto* secondary_action_pause =
      secondary_action_container->GetViewByID(kHoldingSpaceItemPauseButtonId);
  auto* secondary_action_resume =
      secondary_action_container->GetViewByID(kHoldingSpaceItemResumeButtonId);
  ViewDrawnWaiter().Wait(secondary_action_container);
  EXPECT_EQ(secondary_action_pause->GetVisible(),
            GetPauseOrResumeCommandId() == HoldingSpaceCommandId::kPauseItem);
  EXPECT_EQ(secondary_action_resume->GetVisible(),
            GetPauseOrResumeCommandId() == HoldingSpaceCommandId::kResumeItem);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  // Press the `secondary_action_container` to execute the "Pause" or "Resume"
  // command, expecting and waiting for the in-progress download item to be
  // updated in the holding space model.
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemUpdated)
      .WillOnce([&](const HoldingSpaceItem* item,
                    const HoldingSpaceItemUpdatedFields& updated_fields) {
        EXPECT_EQ(item->id(),
                  test_api().GetHoldingSpaceItemId(in_progress_download_chip));
        EXPECT_TRUE(updated_fields.previous_in_progress_commands);
        run_loop.Quit();
      });
  test::Click(secondary_action_container);
  run_loop.Run();

  // Verify that there are still two download chips.
  download_chips = test_api().GetDownloadChips();
  ASSERT_EQ(download_chips.size(), 2u);

  // The two download chips present should still be the original chips for the
  // completed download and the (now paused or resumed) in-progress download.
  EXPECT_EQ(download_chips.at(0), completed_download_chip);
  EXPECT_EQ(download_chips.at(1), in_progress_download_chip);
}

// Verifies that taking a screenshot adds a screenshot holding space item.
IN_PROC_BROWSER_TEST_F(HoldingSpaceUiBrowserTest, AddScreenshot) {
  // Verify that no screenshots exist in holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());
  EXPECT_TRUE(test_api().GetScreenCaptureViews().empty());

  test_api().Close();
  ASSERT_FALSE(test_api().IsShowing());

  // Take a screenshot using the keyboard. The screenshot will be taken using
  // the `CaptureModeController`.
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  // Move the mouse over to the browser window. The reason for that is the
  // capture mode implementation will not automatically capture the topmost
  // window unless the mouse is hovered above it.
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow());
  event_generator.MoveMouseTo(
      browser_window->GetBoundsInScreen().CenterPoint());
  PressAndReleaseKey(ui::VKEY_RETURN);

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

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
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());
  EXPECT_EQ(1u, test_api().GetScreenCaptureViews().size());
}

// Base class for tests of holding space's integration with the capture mode
// screen recording feature, parameterized by the type of recording.
class HoldingSpaceScreenRecordingUiBrowserTest
    : public HoldingSpaceUiBrowserTest,
      public testing::WithParamInterface<RecordingType> {
 public:
  RecordingType recording_type() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceScreenRecordingUiBrowserTest,
                         testing::Values(RecordingType::kGif,
                                         RecordingType::kWebM));

// Verifies that taking a screen recording adds a screen recording holding space
// item. The type of holding space item depends on the type of screen recording.
IN_PROC_BROWSER_TEST_P(HoldingSpaceScreenRecordingUiBrowserTest,
                       AddScreenRecording) {
  // Verify that no screen recordings exist in holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());
  EXPECT_TRUE(test_api().GetScreenCaptureViews().empty());

  test_api().Close();
  ASSERT_FALSE(test_api().IsShowing());
  ash::CaptureModeTestApi capture_mode_test_api;
  capture_mode_test_api.SetRecordingType(recording_type());
  capture_mode_test_api.StartForRegion(/*for_video=*/true);
  capture_mode_test_api.SetUserSelectedRegion(gfx::Rect(200, 200));
  capture_mode_test_api.PerformCapture();
  // Record a 100 ms long video.
  base::RunLoop video_recording_time;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, video_recording_time.QuitClosure(), base::Milliseconds(100));
  video_recording_time.Run();
  capture_mode_test_api.StopVideoRecording();

  // Bind an observer to watch for updates to the holding space model.
  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(HoldingSpaceController::Get()->model());

  base::RunLoop wait_for_item;
  // Expect and wait for a screen recording item to be added to holding space.
  EXPECT_CALL(mock, OnHoldingSpaceItemsAdded)
      .WillOnce([&](const std::vector<const HoldingSpaceItem*>& items) {
        ASSERT_EQ(items.size(), 1u);
        ASSERT_EQ(items[0]->type(),
                  recording_type() == RecordingType::kGif
                      ? HoldingSpaceItem::Type::kScreenRecordingGif
                      : HoldingSpaceItem::Type::kScreenRecording);
        wait_for_item.Quit();
      });
  wait_for_item.Run();

  // The video recording and / or the GIF recording progress notifications can
  // get in the way while tapping on the holding space tray button. Therefore,
  // we must wait until the notification animation completes before attempting
  // to tap on it.
  // TODO(b/275558519): This should not be needed, since the notification should
  // not overlap the shelf.
  MessagePopupAnimationWaiter(ash::Shell::GetPrimaryRootWindowController()
                                  ->shelf()
                                  ->GetStatusAreaWidget()
                                  ->notification_center_tray()
                                  ->popup_collection())
      .Wait();

  // Verify that the screen recording appears in holding space UI.
  test_api().Show();
  ASSERT_TRUE(test_api().IsShowing());
  EXPECT_EQ(1u, test_api().GetScreenCaptureViews().size());
}

// Used to check the holding space suggestion feature.
class HoldingSpaceSuggestionUiBrowserTest : public HoldingSpaceUiBrowserTest {
 public:
  HoldingSpaceSuggestionUiBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHoldingSpaceSuggestions);
  }

  // HoldingSpaceUiBrowserTest:
  void SetUpOnMainThread() override {
    HoldingSpaceUiBrowserTest::SetUpOnMainThread();

    // Initialize `local_file_directory_`.
    EXPECT_TRUE(local_file_directory_.CreateUniqueTempDirUnderPath(
        browser()->profile()->GetPath()));
    EXPECT_TRUE(browser()->profile()->GetMountPoints()->RegisterFileSystem(
        /*mount_name=*/"archive", storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), GetLocalFileMountPath()));
  }

  // Creates multiple files and suggests them through service.
  std::vector<base::FilePath> CreateFileSuggestions(size_t count) {
    using FileOpenEvent =
        file_manager::file_tasks::FileTasksObserver::FileOpenEvent;
    using FileOpenType = file_manager::file_tasks::FileTasksObserver::OpenType;

    base::ScopedAllowBlockingForTesting allow_blocking;

    std::vector<base::FilePath> paths(count);
    std::vector<FileOpenEvent> open_events;
    for (auto& path : paths) {
      EXPECT_TRUE(
          base::CreateTemporaryFileInDir(GetLocalFileMountPath(), &path));
      FileOpenEvent e;
      e.path = path;
      e.open_type = FileOpenType::kOpen;
      open_events.push_back(std::move(e));
    }

    FileSuggestKeyedServiceFactory::GetInstance()
        ->GetService(GetProfile())
        ->local_file_suggestion_provider_for_test()
        ->OnFilesOpened(open_events);
    return paths;
  }

  base::FilePath GetLocalFileMountPath() {
    return local_file_directory_.GetPath();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // The directory that hosts local files.
  base::ScopedTempDir local_file_directory_;
};

// Verifies suggestion removal through holding space item context menu.
IN_PROC_BROWSER_TEST_F(HoldingSpaceSuggestionUiBrowserTest, RemoveSuggestion) {
  // Use zero animation duration so that UI updates are immediate.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Create file suggestions and wait until the suggested files exist in the
  // holding space model.
  constexpr size_t kSuggestionFileCount = 3;
  std::vector<base::FilePath> file_paths =
      CreateFileSuggestions(kSuggestionFileCount);
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  WaitForSuggestionsInModel(
      model,
      /*expected_suggestions=*/
      {{HoldingSpaceItem::Type::kLocalSuggestion, file_paths[0]},
       {HoldingSpaceItem::Type::kLocalSuggestion, file_paths[1]},
       {HoldingSpaceItem::Type::kLocalSuggestion, file_paths[2]}});

  test_api().Show();

  // The count of suggestion chips should be equal to that of suggested files.
  std::vector<views::View*> suggestion_chips = test_api().GetSuggestionChips();
  ASSERT_EQ(suggestion_chips.size(), kSuggestionFileCount);

  // Select two suggestion chips and open context menu.
  ASSERT_FALSE(views::MenuController::GetActiveInstance());
  test::Click(suggestion_chips.front(), ui::EF_CONTROL_DOWN);
  RightClick(suggestion_chips[1], ui::EF_CONTROL_DOWN);
  ASSERT_TRUE(views::MenuController::GetActiveInstance());

  // Remove the selected suggestion chips through context menu.
  auto* menu_item =
      SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem);
  ASSERT_TRUE(menu_item);
  test::Click(menu_item);
  WaitForSuggestionsInModel(
      model, /*expected_suggestions=*/{
          {HoldingSpaceItem::Type::kLocalSuggestion, file_paths[0]}});

  // Remove the remaining suggestion item view through context menu.
  suggestion_chips = test_api().GetSuggestionChips();
  ASSERT_EQ(suggestion_chips.size(), 1u);
  ASSERT_FALSE(views::MenuController::GetActiveInstance());
  RightClick(suggestion_chips.front());
  ASSERT_TRUE(views::MenuController::GetActiveInstance());
  menu_item = SelectMenuItemWithCommandId(HoldingSpaceCommandId::kRemoveItem);
  ASSERT_TRUE(menu_item);
  test::Click(menu_item);

  // There should not be any suggestion item view left.
  EXPECT_EQ(test_api().GetSuggestionChips().size(), 0u);
}

}  // namespace ash
