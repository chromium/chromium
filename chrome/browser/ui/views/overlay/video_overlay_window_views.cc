// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_button.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_label_button.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/browser/ui/views/overlay/hang_up_button.h"
#include "chrome/browser/ui/views/overlay/minimize_button.h"
#include "chrome/browser/ui/views/overlay/overlay_window_live_caption_dialog.h"
#include "chrome/browser/ui/views/overlay/playback_image_button.h"
#include "chrome/browser/ui/views/overlay/resize_handle_button.h"
#include "chrome/browser/ui/views/overlay/simple_overlay_window_image_button.h"
#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"
#include "chrome/browser/ui/views/overlay/toggle_camera_button.h"
#include "chrome/browser/ui/views/overlay/toggle_microphone_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/format_duration.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "services/media_session/public/cpp/media_image_manager.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/rounded_corner_utils.h"
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/base/win/shell.h"
#endif

namespace {

// Lower bound size of the window is a fixed value to allow for minimal sizes
// on UI affordances, such as buttons.
constexpr gfx::Size kMinWindowSize(284, 160);

constexpr int kOverlayBorderThickness = 10;

#if BUILDFLAG(IS_CHROMEOS)
// The opacity of the resize handle control.
constexpr double kResizeHandleOpacity = 0.38;
#endif

// Size of a primary control.
constexpr gfx::Size kPrimaryControlSize(52, 52);

// Margin from the bottom of the window for primary controls.
constexpr int kPrimaryControlBottomMargin = 0;

// Size of a secondary control.
constexpr gfx::Size kSecondaryControlSize(36, 36);

// Margin from the bottom of the window for secondary controls.
constexpr int kSecondaryControlBottomMargin = 8;

// Margin between controls.
constexpr int kControlMargin = 16;

// Minimum padding between the overlay view, if shown, and the window.
constexpr gfx::Size kOverlayViewPadding(64, 46);

// Size for action buttons in the 2024 UI.
constexpr gfx::Size kActionButtonSize(28, 28);

// The amount of time the seek buttons next to the play button seek.
constexpr base::TimeDelta kSeekTime = base::Seconds(10);

// The size of the view containing the favicon.
constexpr gfx::Size kFaviconSize(24, 24);

// The size of the favicon image itself.
constexpr gfx::Size kFaviconIconSize(16, 16);

gfx::Size ScaleImageSizeToFitView(const gfx::Size& image_size,
                                  const gfx::Size& view_size) {
  const float scale =
      std::max(view_size.width() / static_cast<float>(image_size.width()),
               view_size.height() / static_cast<float>(image_size.height()));
  return gfx::ScaleToFlooredSize(image_size, scale);
}

// Converts the given bitmap to the correct color type.
gfx::ImageSkia GetCorrectColorTypeImage(const SkBitmap& bitmap) {
  if (bitmap.info().colorType() == kN32_SkColorType) {
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }
  SkImageInfo color_type_info = bitmap.info().makeColorType(kN32_SkColorType);
  SkBitmap color_type_copy;
  if (!color_type_copy.tryAllocPixels(color_type_info)) {
    return gfx::ImageSkia();
  }
  if (!bitmap.readPixels(color_type_info, color_type_copy.getPixels(),
                         color_type_copy.rowBytes(), 0, 0)) {
    return gfx::ImageSkia();
  }
  return gfx::ImageSkia::CreateFrom1xBitmap(color_type_copy);
}

// Returns the quadrant the VideoOverlayWindowViews is primarily in on the
// current work area.
VideoOverlayWindowViews::WindowQuadrant GetCurrentWindowQuadrant(
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
    return top ? VideoOverlayWindowViews::WindowQuadrant::kTopLeft
               : VideoOverlayWindowViews::WindowQuadrant::kBottomLeft;
  }
  return top ? VideoOverlayWindowViews::WindowQuadrant::kTopRight
             : VideoOverlayWindowViews::WindowQuadrant::kBottomRight;
}

template <typename T>
T* AddChildView(std::vector<std::unique_ptr<views::View>>* views,
                std::unique_ptr<T> child) {
  views->push_back(std::move(child));
  return static_cast<T*>(views->back().get());
}

bool Use2024UI() {
  return base::FeatureList::IsEnabled(
      media::kVideoPictureInPictureControlsUpdate2024);
}

class WindowBackgroundView : public views::View {
  METADATA_HEADER(WindowBackgroundView, views::View)

 public:
  WindowBackgroundView() = default;
  WindowBackgroundView(const WindowBackgroundView&) = delete;
  WindowBackgroundView& operator=(const WindowBackgroundView&) = delete;
  ~WindowBackgroundView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    layer()->SetColor(GetColorProvider()->GetColor(kColorPipWindowBackground));
  }
};

BEGIN_METADATA(WindowBackgroundView)
END_METADATA

class ControlsBackgroundView : public views::View {
  METADATA_HEADER(ControlsBackgroundView, views::View)

 public:
  ControlsBackgroundView() = default;
  ControlsBackgroundView(const ControlsBackgroundView&) = delete;
  ControlsBackgroundView& operator=(const ControlsBackgroundView&) = delete;
  ~ControlsBackgroundView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const SkColor color = GetColorProvider()->GetColor(
        Use2024UI() ? kColorPipWindowScrimFull
                    : kColorPipWindowControlsBackground);
    layer()->SetColor(SkColorSetA(color, SK_AlphaOPAQUE));
    layer()->SetOpacity(static_cast<float>(SkColorGetA(color)) /
                        SK_AlphaOPAQUE);
  }
};

BEGIN_METADATA(ControlsBackgroundView)
END_METADATA

class GradientBackground : public views::Background {
 public:
  GradientBackground(SkColor4f top_color, SkColor4f bottom_color)
      : top_color_(top_color), bottom_color_(bottom_color) {}
  GradientBackground(const GradientBackground&) = delete;
  GradientBackground& operator=(const GradientBackground&) = delete;
  ~GradientBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect draw_bounds = view->GetContentsBounds();
    const SkColor4f colors[2] = {top_color_, bottom_color_};
    const SkPoint points[2] = {
        gfx::PointToSkPoint(draw_bounds.top_center()),
        gfx::PointToSkPoint(draw_bounds.bottom_center())};
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setShader(cc::PaintShader::MakeLinearGradient(points, colors, nullptr,
                                                        2, SkTileMode::kClamp));
    canvas->DrawRect(draw_bounds, flags);
  }

  const SkColor4f top_color_;
  const SkColor4f bottom_color_;
};

}  // namespace

// OverlayWindow implementation of NonClientFrameView.
class OverlayWindowFrameView : public views::NonClientFrameView {
  METADATA_HEADER(OverlayWindowFrameView, views::NonClientFrameView)

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
    if (!bounds().Contains(point)) {
      return HTNOWHERE;
    }

    constexpr int kResizeAreaCornerSize = 16;
    int window_component = GetHTComponentForFrame(
        point, gfx::Insets(kOverlayBorderThickness), kResizeAreaCornerSize,
        kResizeAreaCornerSize, GetWidget()->widget_delegate()->CanResize());

    // The overlay controls should take and handle user interaction.
    VideoOverlayWindowViews* window =
        static_cast<VideoOverlayWindowViews*>(widget_);
    if (window->ControlsHitTestContainsPoint(point)) {
      return window_component;
    }

#if BUILDFLAG(IS_CHROMEOS)
    // If the resize handle is clicked on, we want to force the hit test to
    // force a resize drag.
    if (window->AreControlsVisible() &&
        window->GetResizeHandleControlsBounds().Contains(point)) {
      return window->GetResizeHTComponent();
    }
#endif

    // Allows for dragging and resizing the window.
    return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
  }
#if BUILDFLAG(IS_CHROMEOS)
  void UpdateWindowRoundedCorners() override {
    // The first call to  occurs in `UpdateWindowRoundedCorners()`. However, the
    // layer is initialized after the widget is initialized, hence the null
    // check.
    ui::Layer* root_view_layer = GetWidget()->GetRootView()->layer();
    if (root_view_layer) {
      aura::Window* window = GetWidget()->GetNativeWindow();
      window->SetProperty(aura::client::kWindowCornerRadiusKey,
                          chromeos::kPipRoundedCornerRadius);
      ash::SetCornerRadius(window, root_view_layer,
                           chromeos::kPipRoundedCornerRadius);
    }
  }
#endif

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    return false;
  }

 private:
  raw_ptr<views::Widget> widget_;
};

BEGIN_METADATA(OverlayWindowFrameView)
END_METADATA

// OverlayWindow implementation of WidgetDelegate.
class OverlayWindowWidgetDelegate : public views::WidgetDelegate {
 public:
  OverlayWindowWidgetDelegate() {
    SetCanResize(true);
    SetModalType(ui::mojom::ModalType::kNone);
    // While not shown, the title is still used to identify the window in the
    // window switcher.
    SetShowTitle(false);
    SetTitle(IDS_PICTURE_IN_PICTURE_TITLE_TEXT);
    SetOwnedByWidget(OwnedByWidgetPassKey());
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
std::unique_ptr<VideoOverlayWindowViews> VideoOverlayWindowViews::Create(
    content::VideoPictureInPictureWindowController* controller) {
  // Can't use make_unique(), which doesn't have access to the private
  // constructor. It's important that the constructor be private, because it
  // doesn't initialize the object fully.
  auto overlay_window =
      base::WrapUnique(new VideoOverlayWindowViews(controller));

  // The 2024 updated controls use dark mode colors.
  if (Use2024UI()) {
    overlay_window->SetColorModeOverride(
        ui::ColorProviderKey::ColorMode::kDark);
  }

  overlay_window->CalculateAndUpdateWindowBounds();
  overlay_window->SetUpViews();

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  // Just to have any non-empty bounds as required by Init(). The window is
  // resized to fit the video that is embedded right afterwards, anyway.
  params.bounds = gfx::Rect(overlay_window->GetMinimumSize());
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  params.name = "PictureInPictureWindow";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.delegate = new OverlayWindowWidgetDelegate();

#if BUILDFLAG(IS_CHROMEOS)
  params.init_properties_container.SetProperty(chromeos::kAppTypeKey,
                                               chromeos::AppType::BROWSER);
#endif

  overlay_window->Init(std::move(params));
  overlay_window->OnRootViewReady();

#if BUILDFLAG(IS_WIN)
  std::wstring app_user_model_id;
  Browser* browser = chrome::FindBrowserWithTab(controller->GetWebContents());
  if (browser) {
    const base::FilePath& profile_path = browser->profile()->GetPath();
    // Set the window app id to GetAppUserModelIdForApp if the original window
    // is an app window, GetAppUserModelIdForBrowser if it's a browser window.
    app_user_model_id =
        browser->is_type_app()
            ? shell_integration::win::GetAppUserModelIdForApp(
                  base::UTF8ToWide(browser->app_name()), profile_path)
            : shell_integration::win::GetAppUserModelIdForBrowser(profile_path);
    if (!app_user_model_id.empty()) {
      ui::win::SetAppIdForWindow(
          app_user_model_id,
          overlay_window->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
    }
  }

  InputScope input_scope = overlay_window->GetController()
                                   ->GetWebContents()
                                   ->GetRenderWidgetHostView()
                                   ->GetTextInputClient()
                                   ->ShouldDoLearning()
                               ? IS_DEFAULT
                               : IS_PRIVATE;

  ui::tsf_inputscope::SetInputScope(
      overlay_window->GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
      input_scope);

#endif  // BUILDFLAG(IS_WIN)

  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  if (tracker) {
    tracker->OnPictureInPictureWidgetOpened(overlay_window.get());
  }

  return overlay_window;
}

// static
std::unique_ptr<content::VideoOverlayWindow>
content::VideoOverlayWindow::Create(
    content::VideoPictureInPictureWindowController* controller) {
  return VideoOverlayWindowViews::Create(controller);
}

VideoOverlayWindowViews::VideoOverlayWindowViews(
    content::VideoPictureInPictureWindowController* controller)
    : controller_(controller),
      min_size_(kMinWindowSize),
      hide_controls_timer_(
          FROM_HERE,
          base::Milliseconds(2500),
          base::BindRepeating(
              &VideoOverlayWindowViews::UpdateControlsVisibility,
              base::Unretained(this),
              false /* is_visible */)),
      enable_controls_after_move_timer_(
          FROM_HERE,
          VideoOverlayWindowViews::kControlHideDelayAfterMove,
          base::BindRepeating(
              &VideoOverlayWindowViews::ReEnableControlsAfterMove,
              base::Unretained(this))) {
  display::Screen::GetScreen()->AddObserver(this);
}

VideoOverlayWindowViews::~VideoOverlayWindowViews() {
  if (overlay_view_) {
    overlay_view_->RemoveObserver(this);
  }
  display::Screen::GetScreen()->RemoveObserver(this);
}

gfx::Size& VideoOverlayWindowViews::GetNaturalSize() {
  return natural_size_;
}

gfx::Rect VideoOverlayWindowViews::CalculateAndUpdateWindowBounds() {
  gfx::Rect work_area = GetWorkAreaForWindow();

  UpdateMaxSize(work_area);

  const gfx::Rect bounds = GetBounds();

  gfx::Size window_size = bounds.size();
  if (!has_been_shown_) {
    window_size = gfx::Size(work_area.width() / 5, work_area.height() / 5);
  }

  // Even though we define the minimum and maximum sizes for our views::Widget,
  // it's possible for the current size to be outside of those bounds
  // transiently on some platforms, so we need to cap it.
  window_size.SetToMin(max_size_);
  window_size.SetToMax(GetMinimumSize());

  // Determine the window size by fitting |natural_size_| within |window_size|,
  // keeping to |natural_size_|'s aspect ratio.
  if (!natural_size_.IsEmpty()) {
    float aspect_ratio = (float)natural_size_.width() / natural_size_.height();

    WindowQuadrant quadrant = GetCurrentWindowQuadrant(bounds, GetController());
    gfx::ResizeEdge resize_edge;
    switch (quadrant) {
      case WindowQuadrant::kBottomRight:
        resize_edge = gfx::ResizeEdge::kTopLeft;
        break;
      case WindowQuadrant::kBottomLeft:
        resize_edge = gfx::ResizeEdge::kTopRight;
        break;
      case WindowQuadrant::kTopLeft:
        resize_edge = gfx::ResizeEdge::kBottomRight;
        break;
      case WindowQuadrant::kTopRight:
        resize_edge = gfx::ResizeEdge::kBottomLeft;
        break;
    }

    // Update the window size to adhere to the aspect ratio.
    gfx::Rect window_rect(bounds.origin(), window_size);
    gfx::SizeRectToAspectRatio(resize_edge, aspect_ratio, GetMinimumSize(),
                               max_size_, &window_rect);
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

void VideoOverlayWindowViews::OnNativeFocus() {
  UpdateControlsVisibility(true);
  views::Widget::OnNativeFocus();
}

void VideoOverlayWindowViews::OnNativeBlur() {
  // Controls should be hidden when there is no more focus on the window. This
  // is used for tabbing and touch interactions. For mouse interactions, the
  // window cannot be blurred before the ui::EventType::kMouseExited event is
  // handled.
  UpdateControlsVisibility(false);

  views::Widget::OnNativeBlur();
}

gfx::Size VideoOverlayWindowViews::GetMinimumSize() const {
  if (IsOverlayViewShown()) {
    // Make sure that our minimum is sufficiently large to enclose the bubble,
    // plus some margin to make it look nicer.
    gfx::Size overlay_size =
        overlay_view_->GetBubbleSize() + kOverlayViewPadding;
    overlay_size.SetToMax(min_size_);
    return overlay_size;
  }
  return min_size_;
}

gfx::Size VideoOverlayWindowViews::GetMaximumSize() const {
  return max_size_;
}

void VideoOverlayWindowViews::OnNativeWidgetMove() {
  // Hide the controls when the window is moving. The controls will reappear
  // when the user interacts with the window again. Only called once, at the
  // start of movement because we do not want to clobber updates from other
  // requesters.
  if (!is_moving_) {
    UpdateControlsVisibility(false);
  }

  is_moving_ = true;
  enable_controls_after_move_timer_.Reset();

  // Update the maximum size of the widget in case we have moved to another
  // window.
  UpdateMaxSize(GetWorkAreaForWindow());

#if BUILDFLAG(IS_CHROMEOS)
  // Update the positioning of some icons when the window is moved.
  WindowQuadrant quadrant =
      GetCurrentWindowQuadrant(GetBounds(), GetController());

  // In the 2024 UI, there are icons on both sides of the top bar, so moving the
  // close button doesn't make sense. Only move it when using the older
  // controls.
  if (!Use2024UI()) {
    close_controls_view_->SetPosition(GetBounds().size(), quadrant);
  }

  UpdateResizeHandleBounds(quadrant);
#endif

  views::Widget::OnNativeWidgetMove();
}

void VideoOverlayWindowViews::OnNativeWidgetSizeChanged(
    const gfx::Size& new_size) {
  // Hide the controls when the window is being resized. The controls will
  // reappear when the user interacts with the window again.
  UpdateControlsVisibility(false);

  // Update the view layers to scale to |new_size|.
  UpdateLayerBoundsWithLetterboxing(new_size);

  views::Widget::OnNativeWidgetSizeChanged(new_size);
}

void VideoOverlayWindowViews::OnKeyEvent(ui::KeyEvent* event) {
  // Every time a user uses a keyboard to interact on the window, restart the
  // timer to automatically hide the controls.
  hide_controls_timer_.Reset();

  // Any keystroke will make the controls visible, if not already. The Tab key
  // needs to be handled separately.
  // If the controls are already visible, this is a no-op.
  if (event->type() == ui::EventType::kKeyPressed ||
      event->key_code() == ui::VKEY_TAB) {
    UpdateControlsVisibility(true);
  }

// On Windows, the Alt+F4 keyboard combination closes the window. Only handle
// closure on key press so Close() is not called a second time when the key
// is released.
#if BUILDFLAG(IS_WIN)
  if (event->type() == ui::EventType::kKeyPressed && event->IsAltDown() &&
      event->key_code() == ui::VKEY_F4) {
    CloseAndPauseIfAvailable();
    event->SetHandled();
  }
#endif  // BUILDFLAG(IS_WIN)

  // If there is no focus affordance on the buttons and play/pause button is
  // visible, only handle space key for TogglePlayPause().
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (!focused_view && event->type() == ui::EventType::kKeyPressed &&
      event->key_code() == ui::VKEY_SPACE && show_play_pause_button_) {
    TogglePlayPause();
    event->SetHandled();
  }

  views::Widget::OnKeyEvent(event);
}

void VideoOverlayWindowViews::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
      // Only show the media controls when the mouse is hovering over the
      // window.
    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseEntered:
      UpdateControlsVisibility(true);
      break;

    case ui::EventType::kMouseExited: {
      // On Windows, ui::EventType::kMouseExited is triggered when hovering over
      // the media controls because of the HitTest. This check ensures the
      // controls are visible if the mouse is still over the window.
      // We also check that the user isn't currently dragging the progress bar,
      // since setting visibility to false during the drag will prevent the drag
      // from functioning properly (and we'll lose the drag end).
      const bool should_update_control_visibility =
          !GetWindowBackgroundView()->bounds().Contains(event->location()) &&
          progress_view_drag_state_ ==
              global_media_controls::DragState::kDragEnded;
      if (should_update_control_visibility) {
        UpdateControlsVisibility(false);
      }
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

bool VideoOverlayWindowViews::OnGestureEventHandledOrIgnored(
    ui::GestureEvent* event) {
  if (event->type() != ui::EventType::kGestureTap) {
    return true;
  }

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

void VideoOverlayWindowViews::ReEnableControlsAfterMove() {
  is_moving_ = false;

  if (queued_controls_visibility_status_) {
    UpdateControlsVisibility(*queued_controls_visibility_status_);
  }
  queued_controls_visibility_status_.reset();
}

void VideoOverlayWindowViews::ForceControlsVisibleForTesting(bool visible) {
  force_controls_visible_ = visible;
  UpdateControlsVisibility(visible);
}

void VideoOverlayWindowViews::StopForcingControlsVisibleForTesting() {
  force_controls_visible_.reset();
}

bool VideoOverlayWindowViews::AreControlsVisible() const {
  return GetControlsContainerView()->GetVisible();
}

void VideoOverlayWindowViews::UpdateControlsVisibility(bool is_visible) {
  if (is_moving_) {
    queued_controls_visibility_status_ = is_visible;
    return;
  }

  // We should not change visibility while the progress bar is being dragged, as
  // this gets us stuck in the `kDragStarted` state.
  if (progress_view_drag_state_ ==
      global_media_controls::DragState::kDragStarted) {
    return;
  }

  // If the overlay view is shown, then the other controls are always hidden.
  GetControlsContainerView()->SetVisible(
      !IsOverlayViewShown() && force_controls_visible_.value_or(is_visible));
}

void VideoOverlayWindowViews::UpdateControlsBounds() {
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
      base::BindOnce(&VideoOverlayWindowViews::OnUpdateControlsBounds,
                     base::Unretained(this)));
}

bool VideoOverlayWindowViews::IsLayoutPendingForTesting() const {
  return update_controls_bounds_timer_ &&
         update_controls_bounds_timer_->IsRunning();
}

void VideoOverlayWindowViews::OnDisplayMetricsChanged(
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

void VideoOverlayWindowViews::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  // If the visibility is changing due to a parent view/widget, then we don't
  // care about it.
  if (starting_view != overlay_view_) {
    return;
  }

  // The visibility of `overlay_view_` affects our minimum size.
  OnSizeConstraintsChanged();
}

void VideoOverlayWindowViews::OnAutoPipSettingOverlayViewHidden() {
  // If there is an existing overlay view, remove it now.
  RemoveOverlayViewIfExists();
}

gfx::Rect VideoOverlayWindowViews::GetWorkAreaForWindow() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(
          native_widget() && IsVisible()
              ? GetNativeWindow()
              : GetController()->GetWebContents()->GetTopLevelNativeWindow())
      .work_area();
}

void VideoOverlayWindowViews::UpdateMaxSize(const gfx::Rect& work_area) {
  // An empty |work_area| is not valid, but it is sometimes reported as a
  // transient value.
  if (work_area.IsEmpty()) {
    return;
  }

  auto new_max_size =
      gfx::Size(work_area.width() * 0.8, work_area.height() * 0.8);

  // Ensure |new_max_size| is not smaller than |min_size_|, or else we will
  // crash.
  new_max_size.SetToMax(min_size_);

  // Make sure we only run the logic to update the current size if the maximum
  // size actually changes. Running it unconditionally means also running it
  // when DPI <-> pixel computations introduce off-by-1 errors, which leads to
  // incorrect window sizing/positioning.
  if (new_max_size == max_size_) {
    return;
  }

  max_size_ = new_max_size;

  if (!native_widget()) {
    return;
  }

  // native_widget() is required for OnSizeConstraintsChanged.
  OnSizeConstraintsChanged();

  if (GetBounds().width() <= max_size_.width() &&
      GetBounds().height() <= max_size_.height()) {
    return;
  }

  gfx::Size clamped_size = GetBounds().size();
  clamped_size.SetToMin(max_size_);
  SetSize(clamped_size);
}

bool VideoOverlayWindowViews::ControlsHitTestContainsPoint(
    const gfx::Point& point) {
  if (overlay_view_) {
    // Let the overlay view consume this event if it wants to.  If not, then
    // ignore any of our controls as well.  This will still permit dragging the
    // window by any parts that aren't consumed by the overlay view.
    gfx::Point point_in_screen =
        views::View::ConvertPointToScreen(non_client_view(), point);
    return overlay_view_->WantsEvent(point_in_screen);
  }

  if (!AreControlsVisible()) {
    return false;
  }
  if (GetBackToTabControlsBounds().Contains(point) ||
      GetSkipAdControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point) ||
      GetMinimizeControlsBounds().Contains(point) ||
      GetPlayPauseControlsBounds().Contains(point) ||
      GetReplay10SecondsButtonBounds().Contains(point) ||
      GetForward10SecondsButtonBounds().Contains(point) ||
      GetNextTrackControlsBounds().Contains(point) ||
      GetPreviousTrackControlsBounds().Contains(point) ||
      GetToggleMicrophoneButtonBounds().Contains(point) ||
      GetToggleCameraButtonBounds().Contains(point) ||
      GetHangUpButtonBounds().Contains(point) ||
      GetPreviousSlideControlsBounds().Contains(point) ||
      GetNextSlideControlsBounds().Contains(point) ||
      GetProgressViewBounds().Contains(point) ||
      GetLiveCaptionButtonBounds().Contains(point) ||
      GetLiveCaptionDialogBounds().Contains(point)) {
    return true;
  }
  return false;
}

content::PictureInPictureWindowController*
VideoOverlayWindowViews::GetController() const {
  return controller_;
}

views::View* VideoOverlayWindowViews::GetWindowBackgroundView() const {
  return window_background_view_;
}

views::View* VideoOverlayWindowViews::GetControlsContainerView() const {
  return controls_container_view_;
}

void VideoOverlayWindowViews::SetUpViews() {
  // View that is displayed when video is hidden. ------------------------------
  // Adding an extra pixel to width/height makes sure controls background cover
  // entirely window when platform has fractional scale applied.
  auto window_background_view = std::make_unique<WindowBackgroundView>();
  auto video_view = std::make_unique<views::View>();
  auto controls_scrim_view = std::make_unique<ControlsBackgroundView>();
  auto controls_container_view = std::make_unique<views::View>();
  auto close_controls_view = std::make_unique<CloseImageButton>(
      base::BindRepeating(&VideoOverlayWindowViews::CloseAndPauseIfAvailable,
                          base::Unretained(this)));

  auto play_pause_controls_view =
      std::make_unique<PlaybackImageButton>(base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) { overlay->TogglePlayPause(); },
          base::Unretained(this)));

  // These controls may be different (or even nonexistent) depending on whether
  // the 2024 updated UI is enabled.
  std::unique_ptr<views::View> playback_controls_container_view;
  std::unique_ptr<views::View> vc_controls_container_view;
  std::unique_ptr<views::View> controls_top_scrim_view;
  std::unique_ptr<views::View> controls_bottom_scrim_view;
  std::unique_ptr<views::ImageView> favicon_view;
  std::unique_ptr<views::Label> origin;
  std::unique_ptr<OverlayWindowMinimizeButton> minimize_button;
  std::unique_ptr<OverlayWindowBackToTabButton> back_to_tab_button;
  std::unique_ptr<BackToTabLabelButton> back_to_tab_label_button;
  std::unique_ptr<SimpleOverlayWindowImageButton> replay_10_seconds_button;
  std::unique_ptr<SimpleOverlayWindowImageButton> forward_10_seconds_button;
  std::unique_ptr<SimpleOverlayWindowImageButton> previous_track_controls_view;
  std::unique_ptr<SimpleOverlayWindowImageButton> next_track_controls_view;
  std::unique_ptr<SimpleOverlayWindowImageButton> previous_slide_controls_view;
  std::unique_ptr<SimpleOverlayWindowImageButton> next_slide_controls_view;
  std::unique_ptr<SkipAdLabelButton> skip_ad_controls_view;
  std::unique_ptr<ToggleMicrophoneButton> toggle_microphone_button;
  std::unique_ptr<ToggleCameraButton> toggle_camera_button;
  std::unique_ptr<HangUpButton> hang_up_button;
  std::unique_ptr<global_media_controls::MediaProgressView> progress_view;
  std::unique_ptr<views::Label> timestamp;
  std::unique_ptr<views::Label> live_status;
  std::unique_ptr<SimpleOverlayWindowImageButton> live_caption_button;
  std::unique_ptr<OverlayWindowLiveCaptionDialog> live_caption_dialog;

  if (Use2024UI()) {
    play_pause_controls_view->SetSize({kCenterButtonSize, kCenterButtonSize});
    playback_controls_container_view = std::make_unique<views::View>();
    vc_controls_container_view = std::make_unique<views::View>();
    controls_top_scrim_view = std::make_unique<views::View>();
    controls_top_scrim_view->SetBackground(std::make_unique<GradientBackground>(
        SkColor4f::FromColor(
            GetColorProvider()->GetColor(kColorPipWindowScrimTopGradientStart)),
        SkColor4f::FromColor(
            GetColorProvider()->GetColor(kColorPipWindowScrimTopGradientEnd))));
    controls_bottom_scrim_view = std::make_unique<views::View>();
    controls_bottom_scrim_view->SetBackground(
        std::make_unique<GradientBackground>(
            SkColor4f::FromColor(GetColorProvider()->GetColor(
                kColorPipWindowScrimBottomGradientStart)),
            SkColor4f::FromColor(GetColorProvider()->GetColor(
                kColorPipWindowScrimBottomGradientEnd))));
    favicon_view = std::make_unique<views::ImageView>();
    favicon_view->SetSize(kFaviconSize);
    origin = std::make_unique<views::Label>(std::u16string(),
                                            views::style::CONTEXT_LABEL,
                                            views::style::STYLE_BODY_4);
    origin->SetEnabledColor(ui::kColorSysOnSurface);
    origin->SetBackgroundColor(SK_ColorTRANSPARENT);
    origin->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    origin->SetElideBehavior(gfx::ELIDE_HEAD);
    minimize_button = std::make_unique<
        OverlayWindowMinimizeButton>(base::BindRepeating(
        [](VideoOverlayWindowViews* overlay) {
          PictureInPictureWindowManager::GetInstance()
              ->ExitPictureInPictureViaWindowUi(
                  PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly);
        },
        base::Unretained(this)));
    back_to_tab_button =
        std::make_unique<OverlayWindowBackToTabButton>(base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              PictureInPictureWindowManager::GetInstance()
                  ->ExitPictureInPictureViaWindowUi(
                      PictureInPictureWindowManager::UiBehavior::
                          kCloseWindowAndFocusOpener);
            },
            base::Unretained(this)));
    replay_10_seconds_button = std::make_unique<SimpleOverlayWindowImageButton>(
        base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->Replay10Seconds();
            },
            base::Unretained(this)),
        vector_icons::kReplay10Icon,
        l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_REPLAY_10_TEXT));
    replay_10_seconds_button->SetSize(kActionButtonSize);
    forward_10_seconds_button =
        std::make_unique<SimpleOverlayWindowImageButton>(
            base::BindRepeating(
                [](VideoOverlayWindowViews* overlay) {
                  overlay->Forward10Seconds();
                },
                base::Unretained(this)),
            vector_icons::kForward10Icon,
            l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_FORWARD_10_TEXT));
    forward_10_seconds_button->SetSize(kActionButtonSize);
    previous_track_controls_view =
        std::make_unique<SimpleOverlayWindowImageButton>(
            base::BindRepeating(
                [](VideoOverlayWindowViews* overlay) {
                  if (overlay->show_previous_track_button_) {
                    overlay->controller_->PreviousTrack();
                  } else if (overlay->show_previous_slide_button_) {
                    overlay->controller_->PreviousSlide();
                  }
                },
                base::Unretained(this)),
            vector_icons::kSkipPreviousIcon,
            l10n_util::GetStringUTF16(
                IDS_PICTURE_IN_PICTURE_PREVIOUS_TRACK_CONTROL_ACCESSIBLE_TEXT));
    previous_track_controls_view->SetSize(kActionButtonSize);
    next_track_controls_view = std::make_unique<SimpleOverlayWindowImageButton>(
        base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              if (overlay->show_next_track_button_) {
                overlay->controller_->NextTrack();
              } else if (overlay->show_next_slide_button_) {
                overlay->controller_->NextSlide();
              }
            },
            base::Unretained(this)),
        vector_icons::kSkipNextIcon,
        l10n_util::GetStringUTF16(
            IDS_PICTURE_IN_PICTURE_NEXT_TRACK_CONTROL_ACCESSIBLE_TEXT));
    next_track_controls_view->SetSize(kActionButtonSize);
    // `base::Unretained()` is okay here since we own the progress view.
    progress_view = std::make_unique<global_media_controls::MediaProgressView>(
        /*use_squiggly_line=*/false,
        /*playing_foreground_color_id=*/ui::kColorSysPrimary,
        /*playing_background_color_id=*/ui::kColorSysStateDisabledContainer,
        /*paused_foreground_color_id=*/ui::kColorSysStateDisabledContainer,
        /*paused_background_color_id=*/ui::kColorSysStateDisabledContainer,
        /*focus_ring_color_id=*/ui::kColorSysStateFocusRing,
        /*drag_state_change_callback=*/
        base::BindRepeating(
            &VideoOverlayWindowViews::OnProgressDragStateChanged,
            base::Unretained(this)),
        /*playback_state_change_for_dragging_callback=*/
        base::BindRepeating(
            &VideoOverlayWindowViews::ChangePlaybackStateForProgressDrag,
            base::Unretained(this)),
        /*seek_callback=*/
        base::BindRepeating(
            &VideoOverlayWindowViews::SeekForProgressBarInteraction,
            base::Unretained(this)),
        /*on_update_progress_callback=*/
        base::BindRepeating(
            &VideoOverlayWindowViews::OnProgressViewUpdateCurrentTime,
            base::Unretained(this)));
    timestamp = std::make_unique<views::Label>(std::u16string(),
                                               views::style::CONTEXT_LABEL,
                                               views::style::STYLE_BODY_4);
    timestamp->SetEnabledColor(ui::kColorSysOnSurfaceSubtle);
    timestamp->SetBackgroundColor(SK_ColorTRANSPARENT);
    timestamp->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    live_status = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_LIVE_STATUS_TEXT),
        views::style::CONTEXT_LABEL, views::style::STYLE_CAPTION_BOLD);
    live_status->SetEnabledColor(ui::kColorSysTonalContainer);
    live_status->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 4)));
    live_status->SetBackground(
        views::CreateRoundedRectBackground(ui::kColorSysOnTonalContainer, 4));
    live_status->SetVisible(false);
    live_caption_button = std::make_unique<SimpleOverlayWindowImageButton>(
        base::BindRepeating(
            &VideoOverlayWindowViews::OnLiveCaptionButtonPressed,
            base::Unretained(this)),
        vector_icons::kLiveCaptionOnIcon,
        l10n_util::GetStringUTF16(
            IDS_PICTURE_IN_PICTURE_LIVE_CAPTION_CONTROL_TEXT));
    live_caption_button->SetSize(kActionButtonSize);
    live_caption_dialog = std::make_unique<OverlayWindowLiveCaptionDialog>(
        Profile::FromBrowserContext(
            controller_->GetWebContents()->GetBrowserContext()));
    live_caption_dialog->SetVisible(false);
    toggle_microphone_button =
        std::make_unique<ToggleMicrophoneButton>(base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->controller_->ToggleMicrophone();
            },
            base::Unretained(this)));
    toggle_microphone_button->SetVisible(false);
    toggle_microphone_button->SetSize(kActionButtonSize);
    toggle_camera_button =
        std::make_unique<ToggleCameraButton>(base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->controller_->ToggleCamera();
            },
            base::Unretained(this)));
    toggle_camera_button->SetVisible(false);
    toggle_camera_button->SetSize(kActionButtonSize);
    hang_up_button = std::make_unique<HangUpButton>(base::BindRepeating(
        [](VideoOverlayWindowViews* overlay) {
          overlay->controller_->HangUp();
        },
        base::Unretained(this)));
    hang_up_button->SetSize({kCenterButtonSize, kCenterButtonSize});
    hang_up_button->SetVisible(false);
  } else {
    back_to_tab_label_button =
        std::make_unique<BackToTabLabelButton>(base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              PictureInPictureWindowManager::GetInstance()
                  ->ExitPictureInPictureViaWindowUi(
                      PictureInPictureWindowManager::UiBehavior::
                          kCloseWindowAndFocusOpener);
            },
            base::Unretained(this)));
    previous_track_controls_view =
        std::make_unique<SimpleOverlayWindowImageButton>(
            base::BindRepeating(
                [](VideoOverlayWindowViews* overlay) {
                  overlay->controller_->PreviousTrack();
                },
                base::Unretained(this)),
            vector_icons::kMediaPreviousTrackIcon,
            l10n_util::GetStringUTF16(
                IDS_PICTURE_IN_PICTURE_PREVIOUS_TRACK_CONTROL_ACCESSIBLE_TEXT));
    next_track_controls_view = std::make_unique<SimpleOverlayWindowImageButton>(
        base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->controller_->NextTrack();
            },
            base::Unretained(this)),
        vector_icons::kMediaNextTrackIcon,
        l10n_util::GetStringUTF16(
            IDS_PICTURE_IN_PICTURE_NEXT_TRACK_CONTROL_ACCESSIBLE_TEXT));
    previous_slide_controls_view =
        std::make_unique<SimpleOverlayWindowImageButton>(
            base::BindRepeating(
                [](VideoOverlayWindowViews* overlay) {
                  overlay->controller_->PreviousSlide();
                },
                base::Unretained(this)),
            vector_icons::kMediaPreviousTrackIcon,
            l10n_util::GetStringUTF16(
                IDS_PICTURE_IN_PICTURE_PREVIOUS_SLIDE_CONTROL_ACCESSIBLE_TEXT));
    next_slide_controls_view = std::make_unique<SimpleOverlayWindowImageButton>(
        base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->controller_->NextSlide();
            },
            base::Unretained(this)),
        vector_icons::kMediaNextTrackIcon,
        l10n_util::GetStringUTF16(
            IDS_PICTURE_IN_PICTURE_NEXT_SLIDE_CONTROL_ACCESSIBLE_TEXT));
    skip_ad_controls_view =
        std::make_unique<SkipAdLabelButton>(base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->controller_->SkipAd();
            },
            base::Unretained(this)));
    toggle_microphone_button =
        std::make_unique<ToggleMicrophoneButton>(base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->controller_->ToggleMicrophone();
            },
            base::Unretained(this)));
    toggle_camera_button =
        std::make_unique<ToggleCameraButton>(base::BindRepeating(
            [](VideoOverlayWindowViews* overlay) {
              overlay->controller_->ToggleCamera();
            },
            base::Unretained(this)));
    hang_up_button = std::make_unique<HangUpButton>(base::BindRepeating(
        [](VideoOverlayWindowViews* overlay) {
          overlay->controller_->HangUp();
        },
        base::Unretained(this)));
  }

#if BUILDFLAG(IS_CHROMEOS)
  auto resize_handle_view =
      std::make_unique<ResizeHandleButton>(views::Button::PressedCallback());
#endif

  window_background_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  window_background_view->layer()->SetName("WindowBackgroundView");

  // view::View that holds the video. -----------------------------------------
  video_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  video_view->layer()->SetMasksToBounds(true);
  video_view->layer()->SetFillsBoundsOpaquely(false);
  video_view->layer()->SetName("VideoView");

  // views::View that holds the scrim, which appears with the controls. -------
  controls_scrim_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  controls_scrim_view->layer()->SetName("ControlsScrimView");

  // views::View that is a parent of all the controls. Makes hiding and showing
  // all the controls at once easier.
  controls_container_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  controls_container_view->layer()->SetFillsBoundsOpaquely(false);
  controls_container_view->layer()->SetName("ControlsContainerView");

  // views::View that closes the window. --------------------------------------
  close_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view->layer()->SetFillsBoundsOpaquely(false);
  close_controls_view->layer()->SetName("CloseControlsView");

  if (Use2024UI()) {
    // Contains controls for playback. ----------------------------------------
    playback_controls_container_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    playback_controls_container_view->layer()->SetFillsBoundsOpaquely(false);
    playback_controls_container_view->layer()->SetName(
        "PlaybackControlsContainerView");

    // Contains controls for video conferencing. ------------------------------
    vc_controls_container_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    vc_controls_container_view->layer()->SetFillsBoundsOpaquely(false);
    vc_controls_container_view->layer()->SetName("VcControlsContainerView");

    // The scrim for the top controls. ----------------------------------------
    controls_top_scrim_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    controls_top_scrim_view->layer()->SetFillsBoundsOpaquely(false);
    controls_top_scrim_view->layer()->SetName("ControlsTopScrimView");

    // The scrim for the bottom controls. -------------------------------------
    controls_bottom_scrim_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    controls_bottom_scrim_view->layer()->SetFillsBoundsOpaquely(false);
    controls_bottom_scrim_view->layer()->SetName("ControlsBottomScrimView");

    // views::View that displays the website's favicon. -----------------------
    favicon_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    favicon_view->layer()->SetFillsBoundsOpaquely(false);
    favicon_view->layer()->SetName("FaviconView");

    // Displays the source title (website's origin or extension/PWA name). ----
    origin->SetPaintToLayer(ui::LAYER_TEXTURED);
    origin->layer()->SetFillsBoundsOpaquely(false);
    origin->layer()->SetName("Origin");

    // views::View that closes the window without pausing. --------------------
    minimize_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    minimize_button->layer()->SetFillsBoundsOpaquely(false);
    minimize_button->layer()->SetName("OverlayWindowMinimizeButton");

    // views::View that closes the window and focuses initiator tab. ----------
    back_to_tab_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    back_to_tab_button->layer()->SetFillsBoundsOpaquely(false);
    back_to_tab_button->layer()->SetName("BackToTabControlsView");
  } else {
    // views::View that closes the window and focuses initiator tab. ----------
    CHECK(back_to_tab_label_button);
    back_to_tab_label_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    back_to_tab_label_button->layer()->SetFillsBoundsOpaquely(false);
    back_to_tab_label_button->layer()->SetName("BackToTabControlsView");
  }

  // views::View that holds the previous-track image button. ------------------
  previous_track_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  previous_track_controls_view->layer()->SetFillsBoundsOpaquely(false);
  previous_track_controls_view->layer()->SetName("PreviousTrackControlsView");

  // views::View that toggles play/pause/replay. ------------------------------
  play_pause_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  play_pause_controls_view->layer()->SetFillsBoundsOpaquely(false);
  play_pause_controls_view->layer()->SetName("PlayPauseControlsView");
  play_pause_controls_view->SetPlaybackState(
      controller_->IsPlayerActive() ? kPlaying : kPaused);

  // views::View that holds the next-track image button. ----------------------
  next_track_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  next_track_controls_view->layer()->SetFillsBoundsOpaquely(false);
  next_track_controls_view->layer()->SetName("NextTrackControlsView");

  if (Use2024UI()) {
    replay_10_seconds_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    replay_10_seconds_button->layer()->SetFillsBoundsOpaquely(false);
    replay_10_seconds_button->layer()->SetName("Replay10SecondsButton");

    forward_10_seconds_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    forward_10_seconds_button->layer()->SetFillsBoundsOpaquely(false);
    forward_10_seconds_button->layer()->SetName("Forward10SecondsButton");

    progress_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    progress_view->layer()->SetFillsBoundsOpaquely(false);
    progress_view->layer()->SetName("ProgressView");

    timestamp->SetPaintToLayer(ui::LAYER_TEXTURED);
    timestamp->layer()->SetFillsBoundsOpaquely(false);
    timestamp->layer()->SetName("Timestamp");

    live_status->SetPaintToLayer(ui::LAYER_TEXTURED);
    live_status->layer()->SetFillsBoundsOpaquely(false);
    live_status->layer()->SetName("LiveStatus");

    live_caption_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    live_caption_button->layer()->SetFillsBoundsOpaquely(false);
    live_caption_button->layer()->SetName("LiveCaptionButton");

    live_caption_dialog->SetPaintToLayer(ui::LAYER_TEXTURED);
    live_caption_dialog->layer()->SetFillsBoundsOpaquely(false);
    live_caption_dialog->layer()->SetName("LiveCaptionDialog");
  } else {
    // views::View that holds the skip-ad label button.
    // -------------------------
    skip_ad_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    skip_ad_controls_view->layer()->SetFillsBoundsOpaquely(true);
    skip_ad_controls_view->layer()->SetName("SkipAdControlsView");

    previous_slide_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    previous_slide_controls_view->layer()->SetFillsBoundsOpaquely(false);
    previous_slide_controls_view->layer()->SetName("PreviousSlideButton");

    next_slide_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    next_slide_controls_view->layer()->SetFillsBoundsOpaquely(false);
    next_slide_controls_view->layer()->SetName("NextSlideButton");
  }

  toggle_microphone_button->SetPaintToLayer(ui::LAYER_TEXTURED);
  toggle_microphone_button->layer()->SetFillsBoundsOpaquely(false);
  toggle_microphone_button->layer()->SetName("ToggleMicrophoneButton");

  toggle_camera_button->SetPaintToLayer(ui::LAYER_TEXTURED);
  toggle_camera_button->layer()->SetFillsBoundsOpaquely(false);
  toggle_camera_button->layer()->SetName("ToggleCameraButton");

  hang_up_button->SetPaintToLayer(ui::LAYER_TEXTURED);
  hang_up_button->layer()->SetFillsBoundsOpaquely(false);
  hang_up_button->layer()->SetName("HangUpButton");

#if BUILDFLAG(IS_CHROMEOS)
  // views::View that shows the affordance that the window can be resized. ----
  resize_handle_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  resize_handle_view->layer()->SetFillsBoundsOpaquely(false);
  resize_handle_view->layer()->SetName("ResizeHandleView");
  resize_handle_view->layer()->SetOpacity(kResizeHandleOpacity);
#endif

  // Set up view::Views hierarchy. --------------------------------------------
  window_background_view_ =
      AddChildView(&view_holder_, std::move(window_background_view));
  video_view_ = AddChildView(&view_holder_, std::move(video_view));
  controls_scrim_view_ =
      controls_container_view->AddChildView(std::move(controls_scrim_view));
  if (Use2024UI()) {
    controls_top_scrim_view_ = controls_container_view->AddChildView(
        std::move(controls_top_scrim_view));
    controls_bottom_scrim_view_ = controls_container_view->AddChildView(
        std::move(controls_bottom_scrim_view));
    playback_controls_container_view_ = controls_container_view->AddChildView(
        std::move(playback_controls_container_view));
    vc_controls_container_view_ = controls_container_view->AddChildView(
        std::move(vc_controls_container_view));
  }

  // For the 2024 UI, playback and VC controls are in separate containers.
  views::View* playback_container =
      Use2024UI() ? playback_controls_container_view_.get()
                  : controls_container_view.get();
  views::View* vc_container = Use2024UI() ? vc_controls_container_view_.get()
                                          : controls_container_view.get();

  close_controls_view_ =
      controls_container_view->AddChildView(std::move(close_controls_view));
  if (Use2024UI()) {
    // Initialize the favicon view with the default icon.
    favicon_view_ =
        controls_container_view->AddChildView(std::move(favicon_view));
    UpdateFavicon(gfx::ImageSkia());

    origin_ = controls_container_view->AddChildView(std::move(origin));
    minimize_button_ =
        controls_container_view->AddChildView(std::move(minimize_button));
    back_to_tab_button_ =
        controls_container_view->AddChildView(std::move(back_to_tab_button));
  } else {
    CHECK(back_to_tab_label_button);
    back_to_tab_label_button_ = controls_container_view->AddChildView(
        std::move(back_to_tab_label_button));
  }
  previous_track_controls_view_ =
      playback_container->AddChildView(std::move(previous_track_controls_view));
  if (!Use2024UI()) {
    previous_slide_controls_view_ = controls_container_view->AddChildView(
        std::move(previous_slide_controls_view));
  }
  play_pause_controls_view_ =
      playback_container->AddChildView(std::move(play_pause_controls_view));

  if (Use2024UI()) {
    replay_10_seconds_button_ = playback_controls_container_view_->AddChildView(
        std::move(replay_10_seconds_button));
    forward_10_seconds_button_ =
        playback_controls_container_view_->AddChildView(
            std::move(forward_10_seconds_button));

    progress_view_ = playback_controls_container_view_->AddChildView(
        std::move(progress_view));

    timestamp_ =
        playback_controls_container_view_->AddChildView(std::move(timestamp));

    live_status_ =
        playback_controls_container_view_->AddChildView(std::move(live_status));

    live_caption_button_ = playback_controls_container_view_->AddChildView(
        std::move(live_caption_button));

    live_caption_dialog_ =
        controls_container_view->AddChildView(std::move(live_caption_dialog));
  }

  next_track_controls_view_ =
      playback_container->AddChildView(std::move(next_track_controls_view));
  if (!Use2024UI()) {
    next_slide_controls_view_ = controls_container_view->AddChildView(
        std::move(next_slide_controls_view));
    skip_ad_controls_view_ =
        controls_container_view->AddChildView(std::move(skip_ad_controls_view));
  }
  toggle_microphone_button_ =
      vc_container->AddChildView(std::move(toggle_microphone_button));
  toggle_camera_button_ =
      vc_container->AddChildView(std::move(toggle_camera_button));
  hang_up_button_ = vc_container->AddChildView(std::move(hang_up_button));
#if BUILDFLAG(IS_CHROMEOS)
  resize_handle_view_ =
      controls_container_view->AddChildView(std::move(resize_handle_view));
#endif
  controls_container_view_ =
      AddChildView(&view_holder_, std::move(controls_container_view));
}

void VideoOverlayWindowViews::OnRootViewReady() {
#if BUILDFLAG(IS_CHROMEOS)
  GetNativeWindow()->SetProperty(ash::kWindowPipTypeKey, true);
  highlight_border_overlay_ = std::make_unique<HighlightBorderOverlay>(this);
#endif  // BUILDFLAG(IS_CHROMEOS)

  GetRootView()->SetPaintToLayer(ui::LAYER_TEXTURED);
  GetRootView()->layer()->SetName("RootView");
  GetRootView()->layer()->SetMasksToBounds(true);

  views::View* const contents_view = GetContentsView();
  for (std::unique_ptr<views::View>& child : view_holder_) {
    contents_view->AddChildView(std::move(child));
  }
  view_holder_.clear();

  // Don't show the controls until the mouse hovers over the window.
  UpdateControlsVisibility(false);
}

void VideoOverlayWindowViews::UpdateLayerBoundsWithLetterboxing(
    gfx::Size window_size) {
  // This is the case when the window is initially created or the video surface
  // id has not been embedded.
  if (!native_widget() || GetBounds().IsEmpty() || GetNaturalSize().IsEmpty()) {
    return;
  }

  gfx::Rect letterbox_region = media::ComputeLetterboxRegion(
      gfx::Rect(gfx::Point(0, 0), window_size), GetNaturalSize());
  if (letterbox_region.IsEmpty()) {
    return;
  }

  // To avoid black stripes in the window when integer window dimensions don't
  // correspond to the video aspect ratio exactly (e.g. 854x480 for 16:9
  // video) force the letterbox region size to be equal to the window size.
  const float aspect_ratio =
      static_cast<float>(GetNaturalSize().width()) / GetNaturalSize().height();
  if (aspect_ratio > 1 && window_size.height() == letterbox_region.height()) {
    const int height_from_width =
        base::ClampRound(window_size.width() / aspect_ratio);
    if (height_from_width == window_size.height()) {
      letterbox_region.set_width(window_size.width());
    }
  } else if (aspect_ratio <= 1 &&
             window_size.width() == letterbox_region.width()) {
    const int width_from_height =
        base::ClampRound(window_size.height() * aspect_ratio);
    if (width_from_height == window_size.width()) {
      letterbox_region.set_height(window_size.height());
    }
  }

  const gfx::Rect video_bounds(
      gfx::Point((window_size.width() - letterbox_region.size().width()) / 2,
                 (window_size.height() - letterbox_region.size().height()) / 2),
      letterbox_region.size());

  // Update the layout of the controls.
  UpdateControlsBounds();

  // Update the surface layer bounds to scale with window size changes.
  window_background_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, 0), GetBounds().size()));
  video_view_->SetBoundsRect(video_bounds);
  if (video_view_->layer()->has_external_content()) {
    video_view_->layer()->SetSurfaceSize(video_bounds.size());
  }

  if (IsOverlayViewShown()) {
    overlay_view_->SetBoundsRect(gfx::Rect(GetBounds().size()));
  }

  // Notify the controller that the bounds have changed.
  controller_->UpdateLayerBounds();
}

void VideoOverlayWindowViews::OnUpdateControlsBounds() {
  controls_container_view_->SetSize(GetBounds().size());

  // Adding an extra pixel to width/height makes sure the scrim covers the
  // entire window when the platform has fractional scaling applied.
  gfx::Rect larger_window_bounds = gfx::Rect(GetBounds().size());
  larger_window_bounds.Inset(-1);
  controls_scrim_view_->SetBoundsRect(larger_window_bounds);

  WindowQuadrant quadrant = GetCurrentWindowQuadrant(GetBounds(), controller_);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);

#if BUILDFLAG(IS_CHROMEOS)
  UpdateResizeHandleBounds(quadrant);
#endif

  // The 2024 updated UI lays out controls differently.
  if (Use2024UI()) {
    constexpr int kTopControlsHeight = 34;
    constexpr int kBottomControlsHeight = 64;
    constexpr int kTopScrimHeight = 160;
    constexpr int kBottomScrimHeight = 160;
    constexpr int kFaviconLeftMargin = 8;
    constexpr int kFaviconTopMargin = 5;
    constexpr int kFaviconRightMargin = 4;
    constexpr int kOriginTopMargin = 5;
    constexpr int kOriginHeight = 24;
    constexpr int kOriginRightMargin = 80;
    constexpr int kProgressBarHeight = 26;
    constexpr int kCenterControlMargin = 16;
    constexpr int kControlHorizontalMargin = 8;
    constexpr int kBottomControlsHorizontalMargin = 8;
    constexpr int kBottomControlsVerticalMargin = 4;
    constexpr int kTimestampHorizontalMargin = 16;
    constexpr int kTimestampVerticalMargin = 10;
    constexpr int kTimestampHeight = 16;

    gfx::Rect bounds = GetBounds();
    bounds.set_origin({0, 0});

    // The top bar takes up the first `kTopControlsHeight` pixels of vertical
    // space.
    gfx::Rect top_controls_bounds(bounds.x(), bounds.y(), bounds.width(),
                                  kTopControlsHeight);

    // The bottom controls take up the last `kBottomControlsHeight` pixels of
    // vertical space.
    gfx::Rect bottom_controls_bounds(
        bounds.x(), bounds.y() + bounds.height() - kBottomControlsHeight,
        bounds.width(), kBottomControlsHeight);

    // The rest of the vertical space is used for the middle controls area.
    gfx::Rect middle_controls_bounds = gfx::BoundingRect(
        top_controls_bounds.bottom_left(), bottom_controls_bounds.top_right());

    playback_controls_container_view_->SetSize(bounds.size());
    vc_controls_container_view_->SetSize(bounds.size());
    controls_top_scrim_view_->SetBoundsRect(
        {top_controls_bounds.x(), top_controls_bounds.y(),
         top_controls_bounds.width(), kTopScrimHeight});
    controls_bottom_scrim_view_->SetBoundsRect(
        {bottom_controls_bounds.x(),
         bottom_controls_bounds.y() + bottom_controls_bounds.height() -
             kBottomScrimHeight,
         bottom_controls_bounds.width(), kBottomScrimHeight});

    gfx::Rect favicon_view_bounds({top_controls_bounds.x() + kFaviconLeftMargin,
                                   top_controls_bounds.y() + kFaviconTopMargin},
                                  kFaviconSize);
    favicon_view_->SetPosition(favicon_view_bounds.origin());

    gfx::Point origin_position(
        favicon_view_bounds.right() + kFaviconRightMargin, kOriginTopMargin);
    origin_->SetPosition(origin_position);
    origin_->SetSize(
        {top_controls_bounds.width() - origin_position.x() - kOriginRightMargin,
         kOriginHeight});

    minimize_button_->SetPosition(GetBounds().size(), quadrant);
    back_to_tab_button_->SetPosition(GetBounds().size(), quadrant);

    // Positioning of the middle row of controls.
    const gfx::Point center_control_position(
        middle_controls_bounds.CenterPoint().x() - kCenterButtonSize / 2,
        middle_controls_bounds.CenterPoint().y() - kCenterButtonSize / 2);
    const gfx::Point center_left_control_position(
        center_control_position.x() - kCenterControlMargin -
            kActionButtonSize.width(),
        middle_controls_bounds.CenterPoint().y() -
            kActionButtonSize.height() / 2);
    const gfx::Point center_right_control_position(
        center_control_position.x() + kCenterButtonSize + kCenterControlMargin,
        middle_controls_bounds.CenterPoint().y() -
            kActionButtonSize.height() / 2);

    // If any VC control is visible, then we will hide the playback controls
    // and just show VC controls.
    hang_up_button_->SetVisible(show_hang_up_button_);
    toggle_camera_button_->SetVisible(show_toggle_camera_button_);
    toggle_microphone_button_->SetVisible(show_toggle_microphone_button_);
    if (show_toggle_camera_button_ || show_toggle_microphone_button_ ||
        show_hang_up_button_) {
      hang_up_button_->SetPosition(center_control_position);
      toggle_camera_button_->SetPosition(center_left_control_position);
      toggle_microphone_button_->SetPosition(center_right_control_position);
      vc_controls_container_view_->SetVisible(true);
      playback_controls_container_view_->SetVisible(false);
      return;
    }
    playback_controls_container_view_->SetVisible(true);
    vc_controls_container_view_->SetVisible(false);

    play_pause_controls_view_->SetPosition(center_control_position);
    replay_10_seconds_button_->SetPosition(center_left_control_position);
    forward_10_seconds_button_->SetPosition(center_right_control_position);

    // The previous and next track buttons are placed on the top left/right
    // edges of the bottom controls area.
    previous_track_controls_view_->SetPosition(
        {bottom_controls_bounds.x() + kBottomControlsHorizontalMargin +
             kControlHorizontalMargin,
         bottom_controls_bounds.y() + kBottomControlsVerticalMargin});
    next_track_controls_view_->SetPosition(
        {bottom_controls_bounds.x() + bottom_controls_bounds.width() -
             (kBottomControlsHorizontalMargin + kControlHorizontalMargin +
              kActionButtonSize.width()),
         bottom_controls_bounds.y() + kBottomControlsVerticalMargin});

    // The previous and next track buttons are always both visible if at least
    // one of them is visible.
    const bool should_enable_prev =
        show_previous_track_button_ || show_previous_slide_button_;
    const bool should_enable_next =
        show_next_track_button_ || show_next_slide_button_;
    const bool should_show_prev_next = should_enable_prev || should_enable_next;
    previous_track_controls_view_->SetVisible(should_show_prev_next);
    next_track_controls_view_->SetVisible(should_show_prev_next);
    previous_track_controls_view_->SetEnabled(should_enable_prev);
    next_track_controls_view_->SetEnabled(should_enable_next);

    // The progress bars should take up all the space that is left after the
    // previous and next buttons. Here we calculate how much horizontal space
    // one of those buttons takes up and use that to calculate the width and x
    // position of the progress view.
    constexpr int kPreviousNextTrackWidthPlusHorizontalMargins =
        kBottomControlsHorizontalMargin + (2 * kControlHorizontalMargin) +
        kActionButtonSize.width();
    const int used_horizontal_space_left_of_progress_bar =
        should_show_prev_next
            ? kPreviousNextTrackWidthPlusHorizontalMargins
            : kBottomControlsHorizontalMargin + kControlHorizontalMargin;
    progress_view_->SetPosition(
        {bottom_controls_bounds.x() +
             used_horizontal_space_left_of_progress_bar,
         bottom_controls_bounds.y() + kBottomControlsVerticalMargin});
    progress_view_->SetSize(
        {bounds.width() - (2 * used_horizontal_space_left_of_progress_bar),
         kProgressBarHeight});

    gfx::Point timestamp_position(
        bottom_controls_bounds.x() + kTimestampHorizontalMargin,
        bottom_controls_bounds.y() + bottom_controls_bounds.height() -
            kTimestampVerticalMargin - kTimestampHeight);
    const int max_timestamp_width =
        bottom_controls_bounds.width() - (2 * kTimestampHorizontalMargin);
    timestamp_->SetPosition(timestamp_position);
    timestamp_->SetSize({max_timestamp_width, kTimestampHeight});
    timestamp_->SetVisible(!is_live_);

    live_status_->SetPosition(timestamp_position);
    live_status_->SetMaximumWidthSingleLine(max_timestamp_width);
    live_status_->SetSize(
        {live_status_->GetPreferredSize({max_timestamp_width, kTimestampHeight})
             .width(),
         kTimestampHeight});
    live_status_->SetVisible(is_live_);

    gfx::Rect live_caption_button_bounds(
        bottom_controls_bounds.right() - kBottomControlsHorizontalMargin -
            kActionButtonSize.width(),
        bottom_controls_bounds.bottom() - kBottomControlsVerticalMargin -
            kActionButtonSize.height(),
        live_caption_button_->width(), live_caption_button_->height());

    live_caption_button_->SetPosition(live_caption_button_bounds.origin());

    live_caption_dialog_->SetPosition(
        {live_caption_button_bounds.right() - live_caption_dialog_->width(),
         live_caption_button_bounds.y() - live_caption_dialog_->height()});

    // The play/pause button and replay/forward 10 seconds buttons should not be
    // visible while dragging the progress bar or for live media.
    const bool is_dragging_progress_bar =
        progress_view_drag_state_ ==
        global_media_controls::DragState::kDragStarted;
    play_pause_controls_view_->SetVisible(show_play_pause_button_ &&
                                          !is_dragging_progress_bar);
    replay_10_seconds_button_->SetVisible(!is_dragging_progress_bar &&
                                          !is_live_);
    forward_10_seconds_button_->SetVisible(!is_dragging_progress_bar &&
                                           !is_live_);

    return;
  }

  CHECK(back_to_tab_label_button_);
  back_to_tab_label_button_->SetWindowSize(GetBounds().size());

  skip_ad_controls_view_->SetPosition(GetBounds().size());

  // Following controls order matters:
  // #1 Previous track
  // #2 Previous slide
  // #3 Play/Pause
  // #4 Next track
  // #5 Next slide
  // #6 Toggle microphone
  // #7 Toggle camera
  // #8 Hang up
  std::vector<views::ImageButton*> visible_controls_views;
  if (show_previous_track_button_) {
    visible_controls_views.push_back(previous_track_controls_view_);
  }
  if (show_previous_slide_button_) {
    visible_controls_views.push_back(previous_slide_controls_view_);
  }
  if (show_play_pause_button_) {
    visible_controls_views.push_back(play_pause_controls_view_);
  }
  if (show_next_track_button_) {
    visible_controls_views.push_back(next_track_controls_view_);
  }
  if (show_next_slide_button_) {
    visible_controls_views.push_back(next_slide_controls_view_);
  }
  if (show_toggle_microphone_button_) {
    visible_controls_views.push_back(toggle_microphone_button_);
  }
  if (show_toggle_camera_button_) {
    visible_controls_views.push_back(toggle_camera_button_);
  }
  if (show_hang_up_button_) {
    visible_controls_views.push_back(hang_up_button_);
  }

  if (visible_controls_views.size() > 4) {
    visible_controls_views.resize(4);
  }

  int mid_window_x = GetBounds().size().width() / 2;
  int primary_control_y = GetBounds().size().height() -
                          kPrimaryControlSize.height() -
                          kPrimaryControlBottomMargin;
  int secondary_control_y = GetBounds().size().height() -
                            kSecondaryControlSize.height() -
                            kSecondaryControlBottomMargin;

  switch (visible_controls_views.size()) {
    case 0:
      break;
    case 1: {
      /* | --- --- [ ] --- --- | */
      visible_controls_views[0]->SetSize(kSecondaryControlSize);
      visible_controls_views[0]->SetPosition(
          gfx::Point(mid_window_x - kSecondaryControlSize.width() / 2,
                     secondary_control_y));
      break;
    }
    case 2: {
      /* | ----- [ ] [ ] ----- | */
      visible_controls_views[0]->SetSize(kSecondaryControlSize);
      visible_controls_views[0]->SetPosition(gfx::Point(
          mid_window_x - kControlMargin / 2 - kSecondaryControlSize.width(),
          secondary_control_y));

      visible_controls_views[1]->SetSize(kSecondaryControlSize);
      visible_controls_views[1]->SetPosition(
          gfx::Point(mid_window_x + kControlMargin / 2, secondary_control_y));
      break;
    }
    case 3: {
      /* | --- [ ] [ ] [ ] --- | */
      // Middle control is primary only if it's play/pause control.
      if (visible_controls_views[1] == play_pause_controls_view_) {
        visible_controls_views[0]->SetSize(kSecondaryControlSize);
        visible_controls_views[0]->SetPosition(
            gfx::Point(mid_window_x - kPrimaryControlSize.width() / 2 -
                           kControlMargin - kSecondaryControlSize.width(),
                       secondary_control_y));

        visible_controls_views[1]->SetSize(kPrimaryControlSize);
        visible_controls_views[1]->SetPosition(gfx::Point(
            mid_window_x - kPrimaryControlSize.width() / 2, primary_control_y));

        visible_controls_views[2]->SetSize(kSecondaryControlSize);
        visible_controls_views[2]->SetPosition(gfx::Point(
            mid_window_x + kPrimaryControlSize.width() / 2 + kControlMargin,
            secondary_control_y));
      } else {
        visible_controls_views[0]->SetSize(kSecondaryControlSize);
        visible_controls_views[0]->SetPosition(
            gfx::Point(mid_window_x - kSecondaryControlSize.width() / 2 -
                           kControlMargin - kSecondaryControlSize.width(),
                       secondary_control_y));

        visible_controls_views[1]->SetSize(kSecondaryControlSize);
        visible_controls_views[1]->SetPosition(
            gfx::Point(mid_window_x - kSecondaryControlSize.width() / 2,
                       secondary_control_y));

        visible_controls_views[2]->SetSize(kSecondaryControlSize);
        visible_controls_views[2]->SetPosition(gfx::Point(
            mid_window_x + kSecondaryControlSize.width() / 2 + kControlMargin,
            secondary_control_y));
      }
      break;
    }
    case 4: {
      /* | - [ ] [ ] [ ] [ ] - | */
      visible_controls_views[0]->SetSize(kSecondaryControlSize);
      visible_controls_views[0]->SetPosition(
          gfx::Point(mid_window_x - kControlMargin * 3 / 2 -
                         kSecondaryControlSize.width() * 2,
                     secondary_control_y));

      visible_controls_views[1]->SetSize(kSecondaryControlSize);
      visible_controls_views[1]->SetPosition(gfx::Point(
          mid_window_x - kControlMargin / 2 - kSecondaryControlSize.width(),
          secondary_control_y));

      visible_controls_views[2]->SetSize(kSecondaryControlSize);
      visible_controls_views[2]->SetPosition(
          gfx::Point(mid_window_x + kControlMargin / 2, secondary_control_y));

      visible_controls_views[3]->SetSize(kSecondaryControlSize);
      visible_controls_views[3]->SetPosition(gfx::Point(
          mid_window_x + kControlMargin * 3 / 2 + kSecondaryControlSize.width(),
          secondary_control_y));
      break;
    }
    default:
      NOTREACHED();
  }

  // This will actually update the visibility of a control that was just added
  // or removed, see SetPlayPauseButtonVisibility(), etc.
  previous_track_controls_view_->SetVisible(show_previous_track_button_);
  play_pause_controls_view_->SetVisible(show_play_pause_button_);
  next_track_controls_view_->SetVisible(show_next_track_button_);
  skip_ad_controls_view_->SetVisible(show_skip_ad_button_);
  toggle_microphone_button_->SetVisible(show_toggle_microphone_button_);
  toggle_camera_button_->SetVisible(show_toggle_camera_button_);
  hang_up_button_->SetVisible(show_hang_up_button_);
  previous_slide_controls_view_->SetVisible(show_previous_slide_button_);
  next_slide_controls_view_->SetVisible(show_next_slide_button_);
}

#if BUILDFLAG(IS_CHROMEOS)
void VideoOverlayWindowViews::UpdateResizeHandleBounds(
    WindowQuadrant quadrant) {
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
  GetNativeWindow()->SetProperty(
      ash::kWindowPipResizeHandleBoundsKey,
      new gfx::Rect(GetResizeHandleControlsBounds()));
}
#endif

bool VideoOverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void VideoOverlayWindowViews::Close() {
  views::Widget::Close();
  MaybeUnregisterFrameSinkHierarchy();
}

void VideoOverlayWindowViews::ShowInactive() {
  views::Widget::ShowInactive();
  views::Widget::SetVisibleOnAllWorkspaces(true);
#if BUILDFLAG(IS_CHROMEOS)
  non_client_view()->frame_view()->UpdateWindowRoundedCorners();
#endif

  // If there is an existing overlay view, remove it now.
  RemoveOverlayViewIfExists();

  // TODO(crbug.com/40278613): Confirm whether the anchor should remain as
  // FLOAT.
  auto overlay_view =
      get_overlay_view_cb_
          ? get_overlay_view_cb_.Run()
          : PictureInPictureWindowManager::GetInstance()->GetOverlayView(
                window_background_view_, views::BubbleBorder::Arrow::FLOAT);
  // Re-add it if needed.
  if (overlay_view) {
    overlay_view_ = GetContentsView()->AddChildView(std::move(overlay_view));
    overlay_view_->views::View::AddObserver(this);
    overlay_view_->set_delegate(this);
    // Also update the bounds, since that's already happened for everything
    // else, potentially, during widget resize.
    overlay_view_->SetBoundsRect(gfx::Rect(GetBounds().size()));
    overlay_view_->ShowBubble(GetNativeView());
    SetBounds(CalculateAndUpdateWindowBounds());
  }

  // If this is not the first time the window is shown, this will be a no-op.
  has_been_shown_ = true;
}

void VideoOverlayWindowViews::Hide() {
  // If there is an existing overlay view, remove it now.
  RemoveOverlayViewIfExists();
  views::Widget::Hide();
  MaybeUnregisterFrameSinkHierarchy();
}

bool VideoOverlayWindowViews::IsVisible() const {
  return views::Widget::IsVisible();
}

gfx::Rect VideoOverlayWindowViews::GetBounds() {
  if (!native_widget()) {
    return gfx::Rect();
  }

  return base::FeatureList::IsEnabled(media::kUseWindowBoundsForPip)
             ? GetWindowBoundsInScreen()
             : GetRestoredBounds();
}

void VideoOverlayWindowViews::UpdateNaturalSize(const gfx::Size& natural_size) {
  DCHECK(!natural_size.IsEmpty());
  natural_size_ = natural_size;
  SetAspectRatio(gfx::SizeF(natural_size_));

  // Update the views::Widget bounds to adhere to sizing spec. This will also
  // update the layout of the controls.
  SetBounds(CalculateAndUpdateWindowBounds());
}

void VideoOverlayWindowViews::SetPlaybackState(PlaybackState playback_state) {
  playback_state_for_testing_ = playback_state;
  play_pause_controls_view_->SetPlaybackState(playback_state);
}

void VideoOverlayWindowViews::SetPlayPauseButtonVisibility(bool is_visible) {
  if (show_play_pause_button_ == is_visible) {
    return;
  }

  show_play_pause_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetSkipAdButtonVisibility(bool is_visible) {
  if (show_skip_ad_button_ == is_visible) {
    return;
  }

  show_skip_ad_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetPreviousSlideButtonVisibility(
    bool is_visible) {
  if (show_previous_slide_button_ == is_visible) {
    return;
  }

  show_previous_slide_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetNextSlideButtonVisibility(bool is_visible) {
  if (show_next_slide_button_ == is_visible) {
    return;
  }

  show_next_slide_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetNextTrackButtonVisibility(bool is_visible) {
  if (show_next_track_button_ == is_visible) {
    return;
  }

  show_next_track_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetPreviousTrackButtonVisibility(
    bool is_visible) {
  if (show_previous_track_button_ == is_visible) {
    return;
  }

  show_previous_track_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetMicrophoneMuted(bool muted) {
  toggle_microphone_button_->SetMutedState(muted);
}

void VideoOverlayWindowViews::SetCameraState(bool turned_on) {
  toggle_camera_button_->SetCameraState(turned_on);
}

void VideoOverlayWindowViews::SetToggleMicrophoneButtonVisibility(
    bool is_visible) {
  if (show_toggle_microphone_button_ == is_visible) {
    return;
  }

  show_toggle_microphone_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetToggleCameraButtonVisibility(bool is_visible) {
  if (show_toggle_camera_button_ == is_visible) {
    return;
  }

  show_toggle_camera_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetHangUpButtonVisibility(bool is_visible) {
  if (show_hang_up_button_ == is_visible) {
    return;
  }

  show_hang_up_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetMediaPosition(
    const media_session::MediaPosition& position) {
  if (!Use2024UI()) {
    return;
  }
  position_ = position;
  progress_view_->UpdateProgress(position);
  UpdateTimestampLabel(position_.GetPosition(), position_.duration());
}

void VideoOverlayWindowViews::SetSourceTitle(
    const std::u16string& source_title) {
  if (Use2024UI()) {
    origin_->SetText(source_title);
  }
}

void VideoOverlayWindowViews::SetFaviconImages(
    const std::vector<media_session::MediaImage>& images) {
  if (!Use2024UI()) {
    return;
  }

  media_session::MediaImageManager manager(gfx::kFaviconSize,
                                           gfx::kFaviconSize);
  std::optional<media_session::MediaImage> image = manager.SelectImage(images);

  if (!image) {
    UpdateFavicon(gfx::ImageSkia());
    return;
  }

  controller_->GetMediaImage(
      *image, gfx::kFaviconSize, gfx::kFaviconSize,
      base::BindOnce(&VideoOverlayWindowViews::OnFaviconReceived,
                     weak_factory_.GetWeakPtr()));
}

void VideoOverlayWindowViews::SetSurfaceId(const viz::SurfaceId& surface_id) {
  // The PiP window may have a previous surface set. If the window stays open
  // since then, we need to unregister the previous frame sink; otherwise the
  // surface frame sink should already be removed when the window closed.
  MaybeUnregisterFrameSinkHierarchy();

  // Add the new frame sink to the PiP window and set the surface.
  GetCompositor()->AddChildFrameSink(surface_id.frame_sink_id());
  has_registered_frame_sink_hierarchy_ = true;
  video_view_->layer()->SetShowSurface(
      surface_id, GetBounds().size(),
      GetColorProvider()->GetColor(kColorPipWindowBackground),
      cc::DeadlinePolicy::UseDefaultDeadline(),
      true /* stretch_content_to_fill_bounds */);
}

void VideoOverlayWindowViews::OnNativeWidgetDestroying() {
  views::Widget::OnNativeWidgetDestroying();
  MaybeUnregisterFrameSinkHierarchy();
}

void VideoOverlayWindowViews::OnNativeWidgetDestroyed() {
  views::Widget::OnNativeWidgetDestroyed();
  controller_->OnWindowDestroyed(
      /*should_pause_video=*/show_play_pause_button_);
}

// When the PiP window is moved to different displays on Chrome OS, we need to
// re-parent the frame sink since the compositor will change. After
// OnNativeWidgetRemovingFromCompositor() is called, the window layer containing
// the compositor will be removed in Window::RemoveChildImpl(), and
// OnNativeWidgetAddedToCompositor() is called once another compositor is added.
void VideoOverlayWindowViews::OnNativeWidgetAddedToCompositor() {
  if (!has_registered_frame_sink_hierarchy_ && GetCurrentFrameSinkId()) {
    GetCompositor()->AddChildFrameSink(*GetCurrentFrameSinkId());
    has_registered_frame_sink_hierarchy_ = true;
  }
}

void VideoOverlayWindowViews::OnNativeWidgetRemovingFromCompositor() {
  MaybeUnregisterFrameSinkHierarchy();
}

void VideoOverlayWindowViews::OnGestureEvent(ui::GestureEvent* event) {
  if (OnGestureEventHandledOrIgnored(event)) {
    return;
  }

  if (GetBackToTabControlsBounds().Contains(event->location())) {
    controller_->CloseAndFocusInitiator();
    event->SetHandled();
  } else if (GetSkipAdControlsBounds().Contains(event->location())) {
    controller_->SkipAd();
    event->SetHandled();
  } else if (GetCloseControlsBounds().Contains(event->location())) {
    CloseAndPauseIfAvailable();
    event->SetHandled();
  } else if (GetMinimizeControlsBounds().Contains(event->location())) {
    PictureInPictureWindowManager::GetInstance()
        ->ExitPictureInPictureViaWindowUi(
            PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly);
    event->SetHandled();
  } else if (GetPlayPauseControlsBounds().Contains(event->location())) {
    TogglePlayPause();
    event->SetHandled();
  } else if (GetNextTrackControlsBounds().Contains(event->location())) {
    controller_->NextTrack();
    event->SetHandled();
  } else if (GetPreviousTrackControlsBounds().Contains(event->location())) {
    controller_->PreviousTrack();
    event->SetHandled();
  } else if (GetToggleMicrophoneButtonBounds().Contains(event->location())) {
    controller_->ToggleMicrophone();
    event->SetHandled();
  } else if (GetToggleCameraButtonBounds().Contains(event->location())) {
    controller_->ToggleCamera();
    event->SetHandled();
  } else if (GetHangUpButtonBounds().Contains(event->location())) {
    controller_->HangUp();
    event->SetHandled();
  } else if (GetLiveCaptionButtonBounds().Contains(event->location())) {
    OnLiveCaptionButtonPressed();
    event->SetHandled();
  }
}

gfx::Rect VideoOverlayWindowViews::GetBackToTabControlsBounds() {
  if (Use2024UI()) {
    return back_to_tab_button_->GetMirroredBounds();
  }
  CHECK(back_to_tab_label_button_);
  return back_to_tab_label_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetSkipAdControlsBounds() {
  // The 2024 UI does not yet have a skip ad button implemented.
  if (Use2024UI()) {
    return gfx::Rect();
  }
  return skip_ad_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetCloseControlsBounds() {
  return close_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetMinimizeControlsBounds() {
  if (!Use2024UI()) {
    return gfx::Rect();
  }
  return minimize_button_->GetMirroredBounds();
}

#if BUILDFLAG(IS_CHROMEOS)
gfx::Rect VideoOverlayWindowViews::GetResizeHandleControlsBounds() {
  return resize_handle_view_->GetMirroredBounds();
}
#endif

gfx::Rect VideoOverlayWindowViews::GetPlayPauseControlsBounds() {
  return play_pause_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetReplay10SecondsButtonBounds() {
  if (!Use2024UI()) {
    return gfx::Rect();
  }
  return replay_10_seconds_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetForward10SecondsButtonBounds() {
  if (!Use2024UI()) {
    return gfx::Rect();
  }
  return forward_10_seconds_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetNextTrackControlsBounds() {
  return next_track_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetPreviousTrackControlsBounds() {
  return previous_track_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetToggleMicrophoneButtonBounds() {
  return toggle_microphone_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetToggleCameraButtonBounds() {
  return toggle_camera_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetHangUpButtonBounds() {
  return hang_up_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetPreviousSlideControlsBounds() {
  // The 2024 UI combines the previous slide button with the previous track
  // button.
  if (Use2024UI()) {
    return gfx::Rect();
  }
  return previous_slide_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetNextSlideControlsBounds() {
  // The 2024 UI combines the next slide button with the next track button.
  if (Use2024UI()) {
    return gfx::Rect();
  }
  return next_slide_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetProgressViewBounds() {
  if (!Use2024UI()) {
    return gfx::Rect();
  }
  return progress_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetLiveCaptionButtonBounds() {
  if (!Use2024UI()) {
    return gfx::Rect();
  }
  return live_caption_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetLiveCaptionDialogBounds() {
  if (!Use2024UI() || !live_caption_dialog_->GetVisible()) {
    return gfx::Rect();
  }
  return live_caption_dialog_->GetMirroredBounds();
}

#if BUILDFLAG(IS_CHROMEOS)
int VideoOverlayWindowViews::GetResizeHTComponent() const {
  return resize_handle_view_->GetHTComponent();
}
#endif

void VideoOverlayWindowViews::TogglePlayPause() {
  // Retrieve expected active state based on what command was sent in
  // TogglePlayPause() since the IPC message may not have been propagated
  // the media player yet.
  bool is_active = controller_->TogglePlayPause();
  play_pause_controls_view_->SetPlaybackState(is_active ? kPlaying : kPaused);
}

void VideoOverlayWindowViews::Replay10Seconds() {
  controller_->SeekTo(
      std::max(base::Seconds(0), position_.GetPosition() - kSeekTime));
}

void VideoOverlayWindowViews::Forward10Seconds() {
  controller_->SeekTo(
      std::min(position_.GetPosition() + kSeekTime, position_.duration()));
}

void VideoOverlayWindowViews::CloseAndPauseIfAvailable() {
  // Only pause the video if play/pause is available.
  const bool should_pause_video = !!show_play_pause_button_;
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPictureViaWindowUi(
      should_pause_video
          ? PictureInPictureWindowManager::UiBehavior::kCloseWindowAndPauseVideo
          : PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly);
}

PlaybackImageButton*
VideoOverlayWindowViews::play_pause_controls_view_for_testing() const {
  return play_pause_controls_view_;
}

SimpleOverlayWindowImageButton*
VideoOverlayWindowViews::replay_10_seconds_button_for_testing() const {
  return replay_10_seconds_button_;
}

SimpleOverlayWindowImageButton*
VideoOverlayWindowViews::forward_10_seconds_button_for_testing() const {
  return forward_10_seconds_button_;
}

SimpleOverlayWindowImageButton*
VideoOverlayWindowViews::next_track_controls_view_for_testing() const {
  return next_track_controls_view_;
}

SimpleOverlayWindowImageButton*
VideoOverlayWindowViews::previous_track_controls_view_for_testing() const {
  return previous_track_controls_view_;
}

SkipAdLabelButton* VideoOverlayWindowViews::skip_ad_controls_view_for_testing()
    const {
  return skip_ad_controls_view_;
}

ToggleMicrophoneButton*
VideoOverlayWindowViews::toggle_microphone_button_for_testing() const {
  return toggle_microphone_button_;
}

ToggleCameraButton* VideoOverlayWindowViews::toggle_camera_button_for_testing()
    const {
  return toggle_camera_button_;
}

HangUpButton* VideoOverlayWindowViews::hang_up_button_for_testing() const {
  return hang_up_button_;
}

SimpleOverlayWindowImageButton*
VideoOverlayWindowViews::next_slide_controls_view_for_testing() const {
  return next_slide_controls_view_;
}

SimpleOverlayWindowImageButton*
VideoOverlayWindowViews::previous_slide_controls_view_for_testing() const {
  return previous_slide_controls_view_;
}

global_media_controls::MediaProgressView*
VideoOverlayWindowViews::progress_view_for_testing() const {
  return progress_view_;
}

views::Label* VideoOverlayWindowViews::timestamp_for_testing() const {
  return timestamp_;
}

views::Label* VideoOverlayWindowViews::live_status_for_testing() const {
  return live_status_;
}

SimpleOverlayWindowImageButton*
VideoOverlayWindowViews::live_caption_button_for_testing() const {
  return live_caption_button_;
}

OverlayWindowLiveCaptionDialog*
VideoOverlayWindowViews::live_caption_dialog_for_testing() const {
  return live_caption_dialog_;
}

views::ImageView* VideoOverlayWindowViews::favicon_view_for_testing() const {
  return favicon_view_;
}

views::Label* VideoOverlayWindowViews::origin_for_testing() const {
  return origin_;
}

CloseImageButton* VideoOverlayWindowViews::close_button_for_testing() const {
  return close_controls_view_;
}

OverlayWindowMinimizeButton*
VideoOverlayWindowViews::minimize_button_for_testing() const {
  return minimize_button_;
}

OverlayWindowBackToTabButton*
VideoOverlayWindowViews::back_to_tab_button_for_testing() const {
  return back_to_tab_button_;
}

gfx::Point VideoOverlayWindowViews::close_image_position_for_testing() const {
  return close_controls_view_->origin();
}

#if BUILDFLAG(IS_CHROMEOS)
gfx::Point VideoOverlayWindowViews::resize_handle_position_for_testing() const {
  return resize_handle_view_->origin();
}
#endif

VideoOverlayWindowViews::PlaybackState
VideoOverlayWindowViews::playback_state_for_testing() const {
  return playback_state_for_testing_;
}

ui::Layer* VideoOverlayWindowViews::video_layer_for_testing() const {
  return video_view_->layer();
}

const viz::FrameSinkId* VideoOverlayWindowViews::GetCurrentFrameSinkId() const {
  if (auto* surface = video_view_->layer()->GetSurfaceId()) {
    return &surface->frame_sink_id();
  }

  return nullptr;
}

void VideoOverlayWindowViews::MaybeUnregisterFrameSinkHierarchy() {
  if (has_registered_frame_sink_hierarchy_) {
    DCHECK(GetCurrentFrameSinkId());
    GetCompositor()->RemoveChildFrameSink(*GetCurrentFrameSinkId());
    has_registered_frame_sink_hierarchy_ = false;
  }
}

bool VideoOverlayWindowViews::IsOverlayViewShown() const {
  return overlay_view_ && overlay_view_->GetVisible();
}

void VideoOverlayWindowViews::RemoveOverlayViewIfExists() {
  if (overlay_view_) {
    // Remove and delete the outgoing view.  Note the trailing `T` on the method
    // name -- this removes `overlay_view_` and returns a unique_ptr to it which
    // we then discard.  Without the `T`, it returns nothing and frees nothing.
    overlay_view_->RemoveObserver(this);
    GetContentsView()->RemoveChildViewT(overlay_view_.ExtractAsDangling());
    OnSizeConstraintsChanged();
  }
}

void VideoOverlayWindowViews::OnProgressDragStateChanged(
    global_media_controls::DragState drag_state) {
  progress_view_drag_state_ = drag_state;
  OnUpdateControlsBounds();
}

void VideoOverlayWindowViews::ChangePlaybackStateForProgressDrag(
    global_media_controls::PlaybackStateChangeForDragging
        playback_state_change) {
  if (playback_state_change ==
      global_media_controls::PlaybackStateChangeForDragging::
          kPauseForDraggingStarted) {
    controller_->Pause();
  } else {
    controller_->Play();
  }
}

void VideoOverlayWindowViews::SeekForProgressBarInteraction(
    double seek_progress) {
  controller_->SeekTo(seek_progress * position_.duration());
}

void VideoOverlayWindowViews::OnProgressViewUpdateCurrentTime(
    base::TimeDelta current_time) {
  UpdateTimestampLabel(current_time, position_.duration());
}

void VideoOverlayWindowViews::UpdateTimestampLabel(base::TimeDelta current_time,
                                                   base::TimeDelta duration) {
  bool was_live = is_live_;
  is_live_ = duration.is_max();

  if (!is_live_) {
    timestamp_->SetText(base::StrCat(
        {global_media_controls::GetFormattedDuration(current_time), u" / ",
         global_media_controls::GetFormattedDuration(duration)}));
  }
  if (was_live != is_live_) {
    OnUpdateControlsBounds();
  }
}

void VideoOverlayWindowViews::OnLiveCaptionButtonPressed() {
  live_caption_dialog_->SetVisible(!live_caption_dialog_->GetVisible());
}

void VideoOverlayWindowViews::OnFaviconReceived(const SkBitmap& image) {
  UpdateFavicon(GetCorrectColorTypeImage(image));
}

void VideoOverlayWindowViews::UpdateFavicon(const gfx::ImageSkia& favicon) {
  if (favicon.isNull()) {
    favicon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kGlobeIcon, ui::kColorSysOnSurface,
        kFaviconIconSize.width()));
  } else {
    favicon_view_->SetImageSize(
        ScaleImageSizeToFitView(favicon.size(), kFaviconIconSize));
    favicon_view_->SetImage(ui::ImageModel::FromImageSkia(favicon));
  }
}
