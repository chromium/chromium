// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/rounded_corner_utils.h"
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/win/shell.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/aura/window_tree_host.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

namespace {

// Lower bound size of the window is a fixed value to allow for minimal sizes
// on UI affordances, such as buttons.
constexpr gfx::Size kMinWindowSize(260, 146);

constexpr int kOverlayBorderThickness = 10;

template <typename T>
T* AddChildView(std::vector<std::unique_ptr<views::View>>* views,
                std::unique_ptr<T> child) {
  views->push_back(std::move(child));
  return static_cast<T*>(views->back().get());
}

}  // namespace

// OverlayWindow implementation of NonClientFrameView.
class OverlayWindowFrameView : public views::NonClientFrameView {
 public:
  explicit OverlayWindowFrameView(views::Widget* widget) : widget_(widget) {}

  OverlayWindowFrameView(const OverlayWindowFrameView&) = delete;
  OverlayWindowFrameView& operator=(const OverlayWindowFrameView&) = delete;

  ~OverlayWindowFrameView() override = default;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return bounds();
  }
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    constexpr int kResizeAreaCornerSize = 16;
    int window_component = GetHTComponentForFrame(
        point, gfx::Insets(kOverlayBorderThickness), kResizeAreaCornerSize,
        kResizeAreaCornerSize, GetWidget()->widget_delegate()->CanResize());

    // The overlay controls should take and handle user interaction.
    OverlayWindowViews* window = static_cast<OverlayWindowViews*>(widget_);
    if (window->ControlsHitTestContainsPoint(point)) {
      return window_component;
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // If the resize handle is clicked on, we want to force the hit test to
    // force a resize drag.
    if (window->AreControlsVisible() &&
        window->GetResizeHandleControlsBounds().Contains(point))
      return window->GetResizeHTComponent();
#endif

    // Allows for dragging and resizing the window.
    return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
  }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    return false;
  }

 private:
  raw_ptr<views::Widget> widget_;
};

// OverlayWindow implementation of WidgetDelegate.
class OverlayWindowWidgetDelegate : public views::WidgetDelegate {
 public:
  OverlayWindowWidgetDelegate() {
    SetCanResize(true);
    SetModalType(ui::MODAL_TYPE_NONE);
    // While not shown, the title is still used to identify the window in the
    // window switcher.
    SetShowTitle(false);
    SetTitle(IDS_PICTURE_IN_PICTURE_TITLE_TEXT);
    SetOwnedByWidget(true);
  }

  OverlayWindowWidgetDelegate(const OverlayWindowWidgetDelegate&) = delete;
  OverlayWindowWidgetDelegate& operator=(const OverlayWindowWidgetDelegate&) =
      delete;

  ~OverlayWindowWidgetDelegate() override = default;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    return std::make_unique<OverlayWindowFrameView>(widget);
  }
};

// static
views::WidgetDelegate* OverlayWindowViews::CreateDelegate() {
  return new OverlayWindowWidgetDelegate();
}

// static
OverlayWindowViews::WindowQuadrant OverlayWindowViews::GetCurrentWindowQuadrant(
    const gfx::Rect window_bounds,
    content::PictureInPictureWindowController* controller) {
  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              controller->GetWebContents()->GetTopLevelNativeWindow())
          .work_area();
  const gfx::Point window_center = window_bounds.CenterPoint();

  // Check which quadrant the center of the window appears in.
  const bool top = window_center.y() < work_area.height() / 2;
  if (window_center.x() < work_area.width() / 2) {
    return top ? OverlayWindowViews::WindowQuadrant::kTopLeft
               : OverlayWindowViews::WindowQuadrant::kBottomLeft;
  }
  return top ? OverlayWindowViews::WindowQuadrant::kTopRight
             : OverlayWindowViews::WindowQuadrant::kBottomRight;
}

OverlayWindowViews::OverlayWindowViews()
    : min_size_(kMinWindowSize),
      hide_controls_timer_(
          FROM_HERE,
          base::Milliseconds(2500),
          base::BindRepeating(&OverlayWindowViews::UpdateControlsVisibility,
                              base::Unretained(this),
                              false /* is_visible */)) {
  display::Screen::GetScreen()->AddObserver(this);
}

OverlayWindowViews::~OverlayWindowViews() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

gfx::Size& OverlayWindowViews::GetNaturalSize() {
  return natural_size_;
}

gfx::Rect OverlayWindowViews::CalculateAndUpdateWindowBounds() {
  gfx::Rect work_area = GetWorkAreaForWindow();

  UpdateMaxSize(work_area);

  const gfx::Rect bounds = native_widget() ? GetRestoredBounds() : gfx::Rect();

  gfx::Size window_size = bounds.size();
  if (!has_been_shown_)
    window_size = gfx::Size(work_area.width() / 5, work_area.height() / 5);

  // Even though we define the minimum and maximum sizes for our views::Widget,
  // it's possible for the current size to be outside of those bounds
  // transiently on some platforms, so we need to cap it.
  window_size.SetToMin(max_size_);
  window_size.SetToMax(min_size_);

  // Determine the window size by fitting |natural_size_| within |window_size|,
  // keeping to |natural_size_|'s aspect ratio.
  if (!natural_size_.IsEmpty()) {
    float aspect_ratio = (float)natural_size_.width() / natural_size_.height();

    WindowQuadrant quadrant = GetCurrentWindowQuadrant(bounds, GetController());
    gfx::ResizeEdge resize_edge;
    switch (quadrant) {
      case OverlayWindowViews::WindowQuadrant::kBottomRight:
        resize_edge = gfx::ResizeEdge::kTopLeft;
        break;
      case OverlayWindowViews::WindowQuadrant::kBottomLeft:
        resize_edge = gfx::ResizeEdge::kTopRight;
        break;
      case OverlayWindowViews::WindowQuadrant::kTopLeft:
        resize_edge = gfx::ResizeEdge::kBottomRight;
        break;
      case OverlayWindowViews::WindowQuadrant::kTopRight:
        resize_edge = gfx::ResizeEdge::kBottomLeft;
        break;
    }

    // Update the window size to adhere to the aspect ratio.
    gfx::Rect window_rect(bounds.origin(), window_size);
    gfx::SizeRectToAspectRatio(resize_edge, aspect_ratio, min_size_, max_size_,
                               &window_rect);
    window_size = window_rect.size();

    UpdateLayerBoundsWithLetterboxing(window_size);
  }

  // Use the previous window origin location, if exists.
  gfx::Point origin = bounds.origin();

  int window_diff_width = work_area.right() - window_size.width();
  int window_diff_height = work_area.bottom() - window_size.height();

  // Keep a margin distance of 2% the average of the two window size
  // differences, keeping the margins consistent.
  int buffer = (window_diff_width + window_diff_height) / 2 * 0.02;

  gfx::Point default_origin =
      gfx::Point(window_diff_width - buffer, window_diff_height - buffer);

  if (has_been_shown_) {
    // Make sure window is displayed entirely in the work area.
    origin.SetToMin(default_origin);
  } else {
    origin = default_origin;
  }

  return gfx::Rect(origin, window_size);
}

gfx::Rect OverlayWindowViews::CalculateControlsBounds(int x,
                                                      const gfx::Size& size) {
  return gfx::Rect(
      gfx::Point(x, (GetRestoredBounds().size().height() - size.height()) / 2),
      size);
}

void OverlayWindowViews::DoShowInactive() {
  views::Widget::ShowInactive();
  views::Widget::SetVisibleOnAllWorkspaces(true);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros is based on Ozone/Wayland, which uses ui::PlatformWindow and
  // views::DesktopWindowTreeHostLinux.
  auto* desktop_window_tree_host =
      views::DesktopWindowTreeHostLinux::From(GetNativeWindow()->GetHost());

  // At this point, the aura surface will be created so we can set it to pip and
  // its aspect ratio. Let Exo handle adding a rounded corner decorartor.
  desktop_window_tree_host->GetWaylandExtension()->SetPip();
  desktop_window_tree_host->SetAspectRatio(gfx::SizeF(natural_size_));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For rounded corners.
  if (ash::features::IsPipRoundedCornersEnabled()) {
    ash::SetCornerRadius(GetNativeWindow(), GetRootView()->layer(),
                         ash::kPipRoundedCornerRadius);
  }
#endif

  // If this is not the first time the window is shown, this will be a no-op.
  has_been_shown_ = true;
}

void OverlayWindowViews::DoUpdateNaturalSize(const gfx::Size& natural_size) {
  DCHECK(!natural_size.IsEmpty());
  natural_size_ = natural_size;
  SetAspectRatio(gfx::SizeF(natural_size_));

  // Update the views::Widget bounds to adhere to sizing spec. This will also
  // update the layout of the controls.
  SetBounds(CalculateAndUpdateWindowBounds());
}

void OverlayWindowViews::OnNativeFocus() {
  UpdateControlsVisibility(true);
  views::Widget::OnNativeFocus();
}

void OverlayWindowViews::OnNativeBlur() {
  // Controls should be hidden when there is no more focus on the window. This
  // is used for tabbing and touch interactions. For mouse interactions, the
  // window cannot be blurred before the ui::ET_MOUSE_EXITED event is handled.
  UpdateControlsVisibility(false);

  views::Widget::OnNativeBlur();
}

gfx::Size OverlayWindowViews::GetMinimumSize() const {
  return min_size_;
}

gfx::Size OverlayWindowViews::GetMaximumSize() const {
  return max_size_;
}

void OverlayWindowViews::OnNativeWidgetMove() {
  // Hide the controls when the window is moving. The controls will reappear
  // when the user interacts with the window again.
  UpdateControlsVisibility(false);

  // Update the maximum size of the widget in case we have moved to another
  // window.
  UpdateMaxSize(GetWorkAreaForWindow());
}

void OverlayWindowViews::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  // Hide the controls when the window is being resized. The controls will
  // reappear when the user interacts with the window again.
  UpdateControlsVisibility(false);

  // Update the view layers to scale to |new_size|.
  UpdateLayerBoundsWithLetterboxing(new_size);

  views::Widget::OnNativeWidgetSizeChanged(new_size);
}

void OverlayWindowViews::OnNativeWidgetWorkspaceChanged() {
  // TODO(apacible): Update sizes and maybe resize the current
  // Picture-in-Picture window. Currently, switching between workspaces on linux
  // does not trigger this function. http://crbug.com/819673
}

void OverlayWindowViews::OnKeyEvent(ui::KeyEvent* event) {
  // Every time a user uses a keyboard to interact on the window, restart the
  // timer to automatically hide the controls.
  hide_controls_timer_.Reset();

  // Any keystroke will make the controls visible, if not already. The Tab key
  // needs to be handled separately.
  // If the controls are already visible, this is a no-op.
  if (event->type() == ui::ET_KEY_PRESSED ||
      event->key_code() == ui::VKEY_TAB) {
    UpdateControlsVisibility(true);
  }

// On Windows, the Alt+F4 keyboard combination closes the window. Only handle
// closure on key press so Close() is not called a second time when the key
// is released.
#if BUILDFLAG(IS_WIN)
  if (event->type() == ui::ET_KEY_PRESSED && event->IsAltDown() &&
      event->key_code() == ui::VKEY_F4) {
    GetController()->Close(true /* should_pause_video */);
    event->SetHandled();
  }
#endif  // BUILDFLAG(IS_WIN)

  views::Widget::OnKeyEvent(event);
}

void OverlayWindowViews::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
// Only show the media controls when the mouse is hovering over the window.
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
      UpdateControlsVisibility(true);
      break;

    case ui::ET_MOUSE_EXITED: {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // On Lacros, the |event| will always occur within
      // |window_background_view_| despite the mouse exiting the respective
      // surface so always hide the controls.
      const bool should_update_control_visibility = true;
#else
      // On Windows, ui::ET_MOUSE_EXITED is triggered when hovering over the
      // media controls because of the HitTest. This check ensures the controls
      // are visible if the mouse is still over the window.
      const bool should_update_control_visibility =
          !GetWindowBackgroundView()->bounds().Contains(event->location());
#endif
      if (should_update_control_visibility)
        UpdateControlsVisibility(false);
      break;
    }

    default:
      break;
  }

  // If the user interacts with the window using a mouse, stop the timer to
  // automatically hide the controls.
  hide_controls_timer_.Reset();

  views::Widget::OnMouseEvent(event);
}

bool OverlayWindowViews::OnGestureEventHandledOrIgnored(
    ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return true;

  // Every time a user taps on the window, restart the timer to automatically
  // hide the controls.
  hide_controls_timer_.Reset();

  // If the controls were not shown, make them visible. All controls related
  // layers are expected to have the same visibility.
  // TODO(apacible): This placeholder logic should be updated with touchscreen
  // specific investigation. https://crbug/854373
  if (!AreControlsVisible()) {
    UpdateControlsVisibility(true);
    return true;
  }
  return false;
}

void OverlayWindowViews::RecordTapGesture(OverlayWindowControl window_control) {
  UMA_HISTOGRAM_ENUMERATION("PictureInPictureWindow.TapGesture",
                            window_control);
}

void OverlayWindowViews::RecordButtonPressed(
    OverlayWindowControl window_control) {
  UMA_HISTOGRAM_ENUMERATION("PictureInPictureWindow.ButtonPressed",
                            window_control);
}

void OverlayWindowViews::ForceControlsVisibleForTesting(bool visible) {
  ForceControlsVisible(visible);
}

void OverlayWindowViews::ForceControlsVisible(bool visible) {
  force_controls_visible_ = visible;
  UpdateControlsVisibility(visible);
}

bool OverlayWindowViews::AreControlsVisible() const {
  return GetControlsContainerView()->GetVisible();
}

void OverlayWindowViews::UpdateControlsVisibility(bool is_visible) {
  GetControlsContainerView()->SetVisible(
      force_controls_visible_.value_or(is_visible));
}

void OverlayWindowViews::UpdateControlsBounds() {
  // If controls are hidden, let's update controls bounds immediately.
  // Otherwise, wait a bit before updating controls bounds to avoid too many
  // changes happening too quickly.
  if (!AreControlsVisible()) {
    OnUpdateControlsBounds();
    return;
  }

  update_controls_bounds_timer_ = std::make_unique<base::OneShotTimer>();
  update_controls_bounds_timer_->Start(
      FROM_HERE, base::Seconds(1),
      base::BindOnce(&OverlayWindowViews::OnUpdateControlsBounds,
                     base::Unretained(this)));
}

bool OverlayWindowViews::IsLayoutPendingForTesting() const {
  return update_controls_bounds_timer_ &&
         update_controls_bounds_timer_->IsRunning();
}

void OverlayWindowViews::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Some display metric changes, such as display scaling, can affect the work
  // area, so max size needs to be updated.
  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA &&
      display.id() == display::Screen::GetScreen()
                          ->GetDisplayNearestWindow(GetNativeWindow())
                          .id()) {
    UpdateMaxSize(GetWorkAreaForWindow());
  }
}

gfx::Rect OverlayWindowViews::GetWorkAreaForWindow() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(
          native_widget() && IsVisible()
              ? GetNativeWindow()
              : GetController()->GetWebContents()->GetTopLevelNativeWindow())
      .work_area();
}

void OverlayWindowViews::UpdateMaxSize(const gfx::Rect& work_area) {
  // An empty |work_area| is not valid, but it is sometimes reported as a
  // transient value.
  if (work_area.IsEmpty())
    return;

  auto new_max_size = gfx::Size(work_area.width() / 2, work_area.height() / 2);

  // Ensure |new_max_size| is not smaller than |min_size_|, or else we will
  // crash.
  new_max_size.SetToMax(min_size_);

  // Make sure we only run the logic to update the current size if the maximum
  // size actually changes. Running it unconditionally means also running it
  // when DPI <-> pixel computations introduce off-by-1 errors, which leads to
  // incorrect window sizing/positioning.
  if (new_max_size == max_size_)
    return;

  max_size_ = new_max_size;

  if (!native_widget())
    return;

  // native_widget() is required for OnSizeConstraintsChanged.
  OnSizeConstraintsChanged();

  if (GetRestoredBounds().width() <= max_size_.width() &&
      GetRestoredBounds().height() <= max_size_.height()) {
    return;
  }

  gfx::Size clamped_size = GetRestoredBounds().size();
  clamped_size.SetToMin(max_size_);
  SetSize(clamped_size);
}
