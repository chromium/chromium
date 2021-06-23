// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_image_button.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_label_button.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/browser/ui/views/overlay/hang_up_button.h"
#include "chrome/browser/ui/views/overlay/playback_image_button.h"
#include "chrome/browser/ui/views/overlay/resize_handle_button.h"
#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"
#include "chrome/browser/ui/views/overlay/toggle_camera_button.h"
#include "chrome/browser/ui/views/overlay/toggle_microphone_button.h"
#include "chrome/browser/ui/views/overlay/track_image_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "ui/aura/window.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/win/shell.h"
#endif

namespace {

// Lower bound size of the window is a fixed value to allow for minimal sizes
// on UI affordances, such as buttons.
constexpr gfx::Size kMinWindowSize(260, 146);

constexpr int kOverlayBorderThickness = 10;

// The opacity of the controls scrim.
constexpr double kControlsScrimOpacity = 0.6;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The opacity of the resize handle control.
constexpr double kResizeHandleOpacity = 0.38;
#endif

// Size of a primary control.
constexpr gfx::Size kPrimaryControlSize(36, 36);

// Margin from the bottom of the window for primary controls.
constexpr int kPrimaryControlBottomMargin = 8;

// Size of a secondary control.
constexpr gfx::Size kSecondaryControlSize(20, 20);

// Margin from the bottom of the window for secondary controls.
constexpr int kSecondaryControlBottomMargin = 16;

// Margin between controls.
constexpr int kControlMargin = 32;

// Returns the quadrant the OverlayWindowViews is primarily in on the current
// work area.
OverlayWindowViews::WindowQuadrant GetCurrentWindowQuadrant(
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
        point, kOverlayBorderThickness, kOverlayBorderThickness,
        kResizeAreaCornerSize, kResizeAreaCornerSize,
        GetWidget()->widget_delegate()->CanResize());

    // The media controls should take and handle user interaction.
    OverlayWindowViews* window = static_cast<OverlayWindowViews*>(widget_);
    if (window->AreControlsVisible() &&
        (window->GetBackToTabControlsBounds().Contains(point) ||
         window->GetSkipAdControlsBounds().Contains(point) ||
         window->GetCloseControlsBounds().Contains(point) ||
         window->GetPlayPauseControlsBounds().Contains(point) ||
         window->GetNextTrackControlsBounds().Contains(point) ||
         window->GetPreviousTrackControlsBounds().Contains(point) ||
         window->GetToggleMicrophoneButtonBounds().Contains(point) ||
         window->GetToggleCameraButtonBounds().Contains(point) ||
         window->GetHangUpButtonBounds().Contains(point))) {
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

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowFrameView);
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
  ~OverlayWindowWidgetDelegate() override = default;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    return std::make_unique<OverlayWindowFrameView>(widget);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayWindowWidgetDelegate);
};

// static
std::unique_ptr<OverlayWindowViews> OverlayWindowViews::Create(
    content::PictureInPictureWindowController* controller) {
  // Can't use make_unique(), which doesn't have access to the private
  // constructor. It's important that the constructor be private, because it
  // doesn't initialize the object fully.
  auto overlay_window = base::WrapUnique(new OverlayWindowViews(controller));

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Just to have any non-empty bounds as required by Init(). The window is
  // resized to fit the video that is embedded right afterwards, anyway.
  params.bounds = gfx::Rect(overlay_window->GetMinimumSize());
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  params.name = "PictureInPictureWindow";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.delegate = new OverlayWindowWidgetDelegate();

  overlay_window->Init(std::move(params));
  overlay_window->OnRootViewReady();

#if defined(OS_WIN)
  std::wstring app_user_model_id;
  Browser* browser =
      chrome::FindBrowserWithWebContents(controller->GetWebContents());
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
#endif  // defined(OS_WIN)

  return overlay_window;
}

// static
std::unique_ptr<content::OverlayWindow> content::OverlayWindow::Create(
    content::PictureInPictureWindowController* controller) {
  return OverlayWindowViews::Create(controller);
}

OverlayWindowViews::OverlayWindowViews(
    content::PictureInPictureWindowController* controller)
    : controller_(controller),
      min_size_(kMinWindowSize),
      hide_controls_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(2500),
          base::BindRepeating(&OverlayWindowViews::UpdateControlsVisibility,
                              base::Unretained(this),
                              false /* is_visible */)) {
  CalculateAndUpdateWindowBounds();
  SetUpViews();
}

OverlayWindowViews::~OverlayWindowViews() = default;

gfx::Rect OverlayWindowViews::CalculateAndUpdateWindowBounds() {
  gfx::Rect work_area = GetWorkAreaForWindow();

  UpdateMaxSize(work_area);

  gfx::Size window_size = window_bounds_.size();
  if (!has_been_shown_) {
    window_size = gfx::Size(work_area.width() / 5, work_area.height() / 5);
    window_size.set_width(std::min(
        max_size_.width(), std::max(min_size_.width(), window_size.width())));
    window_size.set_height(
        std::min(max_size_.height(),
                 std::max(min_size_.height(), window_size.height())));
  }

  // Determine the window size by fitting |natural_size_| within
  // |window_size|, keeping to |natural_size_|'s aspect ratio.
  if (!window_size.IsEmpty() && !natural_size_.IsEmpty()) {
    float aspect_ratio = (float)natural_size_.width() / natural_size_.height();

    WindowQuadrant quadrant =
        GetCurrentWindowQuadrant(GetBounds(), controller_);
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
    gfx::Size min_size = min_size_;
    gfx::Size max_size = max_size_;
    gfx::Rect window_rect(GetBounds().origin(), window_size);
    gfx::SizeRectToAspectRatio(resize_edge, aspect_ratio, min_size, max_size,
                               &window_rect);
    window_size.SetSize(window_rect.width(), window_rect.height());

    UpdateLayerBoundsWithLetterboxing(window_size);
  }

  // Use the previous window origin location, if exists.
  gfx::Point origin = window_bounds_.origin();

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

  window_bounds_ = gfx::Rect(origin, window_size);
  return window_bounds_;
}

void OverlayWindowViews::SetUpViews() {
  // views::View that is displayed when video is hidden. ----------------------
  // Adding an extra pixel to width/height makes sure controls background cover
  // entirely window when platform has fractional scale applied.
  auto window_background_view = std::make_unique<views::View>();
  auto video_view = std::make_unique<views::View>();
  auto controls_scrim_view = std::make_unique<views::View>();
  auto controls_container_view = std::make_unique<views::View>();
  auto close_controls_view =
      std::make_unique<views::CloseImageButton>(base::BindRepeating(
          [](OverlayWindowViews* overlay) {
            overlay->controller_->Close(/*should_pause_video=*/true);
            overlay->RecordButtonPressed(OverlayWindowControl::kClose);
          },
          base::Unretained(this)));

  std::unique_ptr<views::BackToTabImageButton> back_to_tab_image_button =
      nullptr;
  std::unique_ptr<BackToTabLabelButton> back_to_tab_label_button = nullptr;
  auto back_to_tab_callback = base::BindRepeating(
      [](OverlayWindowViews* overlay) {
        overlay->controller_->CloseAndFocusInitiator();
        overlay->RecordButtonPressed(OverlayWindowControl::kBackToTab);
      },
      base::Unretained(this));
  if (base::FeatureList::IsEnabled(media::kMediaSessionWebRTC)) {
    back_to_tab_label_button =
        std::make_unique<BackToTabLabelButton>(std::move(back_to_tab_callback));
  } else {
    back_to_tab_image_button = std::make_unique<views::BackToTabImageButton>(
        std::move(back_to_tab_callback));
  }

  auto previous_track_controls_view = std::make_unique<views::TrackImageButton>(
      base::BindRepeating(
          [](OverlayWindowViews* overlay) {
            overlay->controller_->PreviousTrack();
            overlay->RecordButtonPressed(OverlayWindowControl::kPreviousTrack);
          },
          base::Unretained(this)),
      vector_icons::kMediaPreviousTrackIcon,
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_PREVIOUS_TRACK_CONTROL_ACCESSIBLE_TEXT));
  auto play_pause_controls_view =
      std::make_unique<views::PlaybackImageButton>(base::BindRepeating(
          [](OverlayWindowViews* overlay) {
            overlay->TogglePlayPause();
            overlay->RecordButtonPressed(OverlayWindowControl::kPlayPause);
          },
          base::Unretained(this)));
  auto next_track_controls_view = std::make_unique<views::TrackImageButton>(
      base::BindRepeating(
          [](OverlayWindowViews* overlay) {
            overlay->controller_->NextTrack();
            overlay->RecordButtonPressed(OverlayWindowControl::kNextTrack);
          },
          base::Unretained(this)),
      vector_icons::kMediaNextTrackIcon,
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_NEXT_TRACK_CONTROL_ACCESSIBLE_TEXT));
  auto skip_ad_controls_view =
      std::make_unique<views::SkipAdLabelButton>(base::BindRepeating(
          [](OverlayWindowViews* overlay) {
            overlay->controller_->SkipAd();
            overlay->RecordButtonPressed(OverlayWindowControl::kSkipAd);
          },
          base::Unretained(this)));
  auto toggle_microphone_button =
      std::make_unique<ToggleMicrophoneButton>(base::BindRepeating(
          [](OverlayWindowViews* overlay) {
            overlay->controller_->ToggleMicrophone();
            overlay->RecordButtonPressed(
                OverlayWindowControl::kToggleMicrophone);
          },
          base::Unretained(this)));
  auto toggle_camera_button =
      std::make_unique<ToggleCameraButton>(base::BindRepeating(
          [](OverlayWindowViews* overlay) {
            overlay->controller_->ToggleCamera();
            overlay->RecordButtonPressed(OverlayWindowControl::kToggleCamera);
          },
          base::Unretained(this)));
  auto hang_up_button = std::make_unique<HangUpButton>(base::BindRepeating(
      [](OverlayWindowViews* overlay) {
        overlay->controller_->HangUp();
        overlay->RecordButtonPressed(OverlayWindowControl::kHangUp);
      },
      base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto resize_handle_view = std::make_unique<views::ResizeHandleButton>(
      views::Button::PressedCallback());
#endif

  window_background_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  window_background_view->layer()->SetName("WindowBackgroundView");
  window_background_view->layer()->SetColor(SK_ColorBLACK);

  // view::View that holds the video. -----------------------------------------
  video_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  video_view->layer()->SetMasksToBounds(true);
  video_view->layer()->SetFillsBoundsOpaquely(false);
  video_view->layer()->SetName("VideoView");

  // views::View that holds the scrim, which appears with the controls. -------
  controls_scrim_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  controls_scrim_view->layer()->SetName("ControlsScrimView");
  controls_scrim_view->layer()->SetColor(gfx::kGoogleGrey900);
  controls_scrim_view->layer()->SetOpacity(kControlsScrimOpacity);

  // views::View that is a parent of all the controls. Makes hiding and showing
  // all the controls at once easier.
  controls_container_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  controls_container_view->layer()->SetFillsBoundsOpaquely(false);
  controls_container_view->layer()->SetName("ControlsContainerView");

  // views::View that closes the window. --------------------------------------
  close_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view->layer()->SetFillsBoundsOpaquely(false);
  close_controls_view->layer()->SetName("CloseControlsView");

  // views::View that closes the window and focuses initiator tab. ------------
  if (back_to_tab_image_button) {
    back_to_tab_image_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    back_to_tab_image_button->layer()->SetFillsBoundsOpaquely(false);
    back_to_tab_image_button->layer()->SetName("BackToTabControlsView");
  } else {
    DCHECK(back_to_tab_label_button);
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

  // views::View that holds the skip-ad label button. -------------------------
  skip_ad_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  skip_ad_controls_view->layer()->SetFillsBoundsOpaquely(true);
  skip_ad_controls_view->layer()->SetName("SkipAdControlsView");

  toggle_microphone_button->SetPaintToLayer(ui::LAYER_TEXTURED);
  toggle_microphone_button->layer()->SetFillsBoundsOpaquely(false);
  toggle_microphone_button->layer()->SetName("ToggleMicrophoneButton");

  toggle_camera_button->SetPaintToLayer(ui::LAYER_TEXTURED);
  toggle_camera_button->layer()->SetFillsBoundsOpaquely(false);
  toggle_camera_button->layer()->SetName("ToggleCameraButton");

  hang_up_button->SetPaintToLayer(ui::LAYER_TEXTURED);
  hang_up_button->layer()->SetFillsBoundsOpaquely(false);
  hang_up_button->layer()->SetName("HangUpButton");

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  close_controls_view_ =
      controls_container_view->AddChildView(std::move(close_controls_view));

  if (back_to_tab_image_button) {
    back_to_tab_image_button_ = controls_container_view->AddChildView(
        std::move(back_to_tab_image_button));
  } else {
    DCHECK(back_to_tab_label_button);
    back_to_tab_label_button_ = controls_container_view->AddChildView(
        std::move(back_to_tab_label_button));
  }

  previous_track_controls_view_ = controls_container_view->AddChildView(
      std::move(previous_track_controls_view));
  play_pause_controls_view_ = controls_container_view->AddChildView(
      std::move(play_pause_controls_view));
  next_track_controls_view_ = controls_container_view->AddChildView(
      std::move(next_track_controls_view));
  skip_ad_controls_view_ =
      controls_container_view->AddChildView(std::move(skip_ad_controls_view));
  toggle_microphone_button_ = controls_container_view->AddChildView(
      std::move(toggle_microphone_button));
  toggle_camera_button_ =
      controls_container_view->AddChildView(std::move(toggle_camera_button));
  hang_up_button_ =
      controls_container_view->AddChildView(std::move(hang_up_button));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  resize_handle_view_ =
      controls_container_view->AddChildView(std::move(resize_handle_view));
#endif
  controls_container_view_ =
      AddChildView(&view_holder_, std::move(controls_container_view));
}

void OverlayWindowViews::OnRootViewReady() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetNativeWindow()->SetProperty(ash::kWindowPipTypeKey, true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  GetRootView()->SetPaintToLayer(ui::LAYER_TEXTURED);
  GetRootView()->layer()->SetName("RootView");
  GetRootView()->layer()->SetMasksToBounds(true);

  views::View* const contents_view = GetContentsView();
  for (std::unique_ptr<views::View>& child : view_holder_)
    contents_view->AddChildView(std::move(child));
  view_holder_.clear();

  // Don't show the controls until the mouse hovers over the window.
  UpdateControlsVisibility(false);
}

void OverlayWindowViews::UpdateLayerBoundsWithLetterboxing(
    gfx::Size window_size) {
  // This is the case when the window is initially created or the video surface
  // id has not been embedded.
  if (window_bounds_.size().IsEmpty() || natural_size_.IsEmpty())
    return;

  gfx::Rect letterbox_region = media::ComputeLetterboxRegion(
      gfx::Rect(gfx::Point(0, 0), window_size), natural_size_);
  if (letterbox_region.IsEmpty())
    return;

  // To avoid black stripes in the window when integer window dimensions don't
  // correspond to the video aspect ratio exactly (e.g. 854x480 for 16:9
  // video) force the letterbox region size to be equal to the window size.
  const float aspect_ratio =
      static_cast<float>(natural_size_.width()) / natural_size_.height();
  if (aspect_ratio > 1 && window_size.height() == letterbox_region.height()) {
    const int height_from_width =
        base::ClampRound(window_size.width() / aspect_ratio);
    if (height_from_width == window_size.height())
      letterbox_region.set_width(window_size.width());
  } else if (aspect_ratio <= 1 &&
             window_size.width() == letterbox_region.width()) {
    const int width_from_height =
        base::ClampRound(window_size.height() * aspect_ratio);
    if (width_from_height == window_size.width())
      letterbox_region.set_height(window_size.height());
  }

  gfx::Size letterbox_size = letterbox_region.size();
  gfx::Point origin =
      gfx::Point((window_size.width() - letterbox_size.width()) / 2,
                 (window_size.height() - letterbox_size.height()) / 2);

  video_bounds_.set_origin(origin);
  video_bounds_.set_size(letterbox_region.size());

  // Update the layout of the controls.
  UpdateControlsBounds();

  // Update the surface layer bounds to scale with window size changes.
  window_background_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, 0), GetBounds().size()));
  video_view_->SetBoundsRect(video_bounds_);
  if (video_view_->layer()->has_external_content())
    video_view_->layer()->SetSurfaceSize(video_bounds_.size());

  // Notify the controller that the bounds have changed.
  controller_->UpdateLayerBounds();
}

void OverlayWindowViews::UpdateControlsVisibility(bool is_visible) {
  controls_container_view_->SetVisible(
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
      FROM_HERE, base::TimeDelta::FromSeconds(1),
      base::BindOnce(&OverlayWindowViews::OnUpdateControlsBounds,
                     base::Unretained(this)));
}

void OverlayWindowViews::OnUpdateControlsBounds() {
  controls_container_view_->SetSize(GetBounds().size());

  // Adding an extra pixel to width/height makes sure the scrim covers the
  // entire window when the platform has fractional scaling applied.
  gfx::Rect larger_window_bounds = gfx::Rect(GetBounds().size());
  larger_window_bounds.Inset(-1, -1);
  controls_scrim_view_->SetBoundsRect(larger_window_bounds);

  WindowQuadrant quadrant = GetCurrentWindowQuadrant(GetBounds(), controller_);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);

  if (back_to_tab_label_button_)
    back_to_tab_label_button_->SetWindowSize(GetBounds().size());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
#endif

  skip_ad_controls_view_->SetPosition(GetBounds().size());

  // Following controls order matters:
  // #1 Back to tab
  // #2 Previous track
  // #3 Play/Pause
  // #4 Next track
  // #5 Toggle microphone
  // #6 Toggle camera
  // #7 Hang up
  std::vector<views::ImageButton*> visible_controls_views;
  if (back_to_tab_image_button_)
    visible_controls_views.push_back(back_to_tab_image_button_);
  if (show_previous_track_button_)
    visible_controls_views.push_back(previous_track_controls_view_);
  if (show_play_pause_button_)
    visible_controls_views.push_back(play_pause_controls_view_);
  if (show_next_track_button_)
    visible_controls_views.push_back(next_track_controls_view_);
  if (show_toggle_microphone_button_)
    visible_controls_views.push_back(toggle_microphone_button_);
  if (show_toggle_camera_button_)
    visible_controls_views.push_back(toggle_camera_button_);
  if (show_hang_up_button_)
    visible_controls_views.push_back(hang_up_button_);

  if (visible_controls_views.size() > 4)
    visible_controls_views.resize(4);

  int mid_window_x = GetBounds().size().width() / 2;
  int primary_control_y = GetBounds().size().height() -
                          kPrimaryControlSize.height() -
                          kPrimaryControlBottomMargin;
  int secondary_control_y = GetBounds().size().height() -
                            kSecondaryControlSize.height() -
                            kSecondaryControlBottomMargin;

  switch (visible_controls_views.size()) {
    case 0:
      DCHECK(back_to_tab_label_button_);
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
}

gfx::Rect OverlayWindowViews::CalculateControlsBounds(int x,
                                                      const gfx::Size& size) {
  return gfx::Rect(
      gfx::Point(x, (GetBounds().size().height() - size.height()) / 2), size);
}

bool OverlayWindowViews::IsActive() {
  return views::Widget::IsActive();
}

bool OverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void OverlayWindowViews::Close() {
  views::Widget::Close();

  if (auto* frame_sink_id = GetCurrentFrameSinkId())
    GetCompositor()->RemoveChildFrameSink(*frame_sink_id);
}

void OverlayWindowViews::ShowInactive() {
  if (back_to_tab_label_button_) {
    back_to_tab_label_button_->SetText(url_formatter::FormatUrl(
        controller_->GetWebContents()->GetLastCommittedURL(),
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  }

  views::Widget::ShowInactive();
  views::Widget::SetVisibleOnAllWorkspaces(true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For rounded corners.
  if (ash::features::IsPipRoundedCornersEnabled()) {
    decorator_ = std::make_unique<ash::RoundedCornerDecorator>(
        GetNativeWindow(), GetNativeWindow(), GetRootView()->layer(),
        ash::kPipRoundedCornerRadius);
  }
#endif

  // If this is not the first time the window is shown, this will be a no-op.
  has_been_shown_ = true;
}

void OverlayWindowViews::Hide() {
  views::Widget::Hide();
}

bool OverlayWindowViews::IsVisible() {
  return views::Widget::IsVisible();
}

bool OverlayWindowViews::IsVisible() const {
  return views::Widget::IsVisible();
}

bool OverlayWindowViews::IsAlwaysOnTop() {
  return true;
}

gfx::Rect OverlayWindowViews::GetBounds() {
  return views::Widget::GetRestoredBounds();
}

void OverlayWindowViews::UpdateVideoSize(const gfx::Size& natural_size) {
  DCHECK(!natural_size.IsEmpty());
  natural_size_ = natural_size;
  SetAspectRatio(gfx::SizeF(natural_size_));

  // Update the views::Widget bounds to adhere to sizing spec. This will also
  // update the layout of the controls.
  SetBounds(CalculateAndUpdateWindowBounds());
}

void OverlayWindowViews::SetPlaybackState(PlaybackState playback_state) {
  playback_state_for_testing_ = playback_state;
  play_pause_controls_view_->SetPlaybackState(playback_state);
}

void OverlayWindowViews::SetPlayPauseButtonVisibility(bool is_visible) {
  if (show_play_pause_button_ == is_visible)
    return;

  show_play_pause_button_ = is_visible;
  UpdateControlsBounds();
}

void OverlayWindowViews::SetSkipAdButtonVisibility(bool is_visible) {
  if (show_skip_ad_button_ == is_visible)
    return;

  show_skip_ad_button_ = is_visible;
  UpdateControlsBounds();
}

void OverlayWindowViews::SetNextTrackButtonVisibility(bool is_visible) {
  if (show_next_track_button_ == is_visible)
    return;

  show_next_track_button_ = is_visible;
  UpdateControlsBounds();
}

void OverlayWindowViews::SetPreviousTrackButtonVisibility(bool is_visible) {
  if (show_previous_track_button_ == is_visible)
    return;

  show_previous_track_button_ = is_visible;
  UpdateControlsBounds();
}

void OverlayWindowViews::SetMicrophoneMuted(bool muted) {
  toggle_microphone_button_->SetMutedState(muted);
}

void OverlayWindowViews::SetCameraState(bool turned_on) {
  toggle_camera_button_->SetCameraState(turned_on);
}

void OverlayWindowViews::SetToggleMicrophoneButtonVisibility(bool is_visible) {
  if (show_toggle_microphone_button_ == is_visible)
    return;

  show_toggle_microphone_button_ = is_visible;
  UpdateControlsBounds();
}

void OverlayWindowViews::SetToggleCameraButtonVisibility(bool is_visible) {
  if (show_toggle_camera_button_ == is_visible)
    return;

  show_toggle_camera_button_ = is_visible;
  UpdateControlsBounds();
}

void OverlayWindowViews::SetHangUpButtonVisibility(bool is_visible) {
  if (show_hang_up_button_ == is_visible)
    return;

  show_hang_up_button_ = is_visible;
  UpdateControlsBounds();
}

void OverlayWindowViews::SetSurfaceId(const viz::SurfaceId& surface_id) {
  // TODO(https://crbug.com/925346): We also want to unregister the page that
  // used to embed the video as its parent.
  if (!GetCurrentFrameSinkId()) {
    GetCompositor()->AddChildFrameSink(surface_id.frame_sink_id());
  } else if (*GetCurrentFrameSinkId() != surface_id.frame_sink_id()) {
    GetCompositor()->RemoveChildFrameSink(*GetCurrentFrameSinkId());
    GetCompositor()->AddChildFrameSink(surface_id.frame_sink_id());
  }

  video_view_->layer()->SetShowSurface(
      surface_id, GetBounds().size(), SK_ColorBLACK,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      true /* stretch_content_to_fill_bounds */);
}

void OverlayWindowViews::OnNativeBlur() {
  // Controls should be hidden when there is no more focus on the window. This
  // is used for tabbing and touch interactions. For mouse interactions, the
  // window cannot be blurred before the ui::ET_MOUSE_EXITED event is handled.
  UpdateControlsVisibility(false);

  views::Widget::OnNativeBlur();
}

void OverlayWindowViews::OnNativeWidgetDestroyed() {
  controller_->OnWindowDestroyed();
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

  // Update the existing |window_bounds_| when the window moves. This allows
  // the window to reappear with the same origin point when a new video is
  // shown.
  window_bounds_ = GetBounds();

  // Update the maximum size of the widget in case we have moved to another
  // window.
  UpdateMaxSize(GetWorkAreaForWindow());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Update the positioning of some icons when the window is moved.
  WindowQuadrant quadrant = GetCurrentWindowQuadrant(GetBounds(), controller_);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
#endif
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

  // If there is no focus affordance on the buttons, only handle space key to
  // for TogglePlayPause().
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (!focused_view && event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_SPACE) {
    TogglePlayPause();
    event->SetHandled();
  }

// On Windows, the Alt+F4 keyboard combination closes the window. Only handle
// closure on key press so Close() is not called a second time when the key
// is released.
#if defined(OS_WIN)
  if (event->type() == ui::ET_KEY_PRESSED && event->IsAltDown() &&
      event->key_code() == ui::VKEY_F4) {
    controller_->Close(true /* should_pause_video */);
    event->SetHandled();
  }
#endif  // OS_WIN

  views::Widget::OnKeyEvent(event);
}

void OverlayWindowViews::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
// Only show the media controls when the mouse is hovering over the window.
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
      UpdateControlsVisibility(true);
      break;

    case ui::ET_MOUSE_EXITED:
      // On Windows, ui::ET_MOUSE_EXITED is triggered when hovering over the
      // media controls because of the HitTest. This check ensures the controls
      // are visible if the mouse is still over the window.
      if (!video_bounds_.Contains(event->location()))
        UpdateControlsVisibility(false);
      break;

    default:
      break;
  }

  // If the user interacts with the window using a mouse, stop the timer to
  // automatically hide the controls.
  hide_controls_timer_.Reset();

  views::Widget::OnMouseEvent(event);
}

void OverlayWindowViews::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;

  // Every time a user taps on the window, restart the timer to automatically
  // hide the controls.
  hide_controls_timer_.Reset();

  // If the controls were not shown, make them visible. All controls related
  // layers are expected to have the same visibility.
  // TODO(apacible): This placeholder logic should be updated with touchscreen
  // specific investigation. https://crbug/854373
  if (!AreControlsVisible()) {
    UpdateControlsVisibility(true);
    return;
  }

  if (GetBackToTabControlsBounds().Contains(event->location())) {
    controller_->CloseAndFocusInitiator();
    RecordTapGesture(OverlayWindowControl::kBackToTab);
    event->SetHandled();
  } else if (GetSkipAdControlsBounds().Contains(event->location())) {
    controller_->SkipAd();
    RecordTapGesture(OverlayWindowControl::kSkipAd);
    event->SetHandled();
  } else if (GetCloseControlsBounds().Contains(event->location())) {
    controller_->Close(true /* should_pause_video */);
    RecordTapGesture(OverlayWindowControl::kClose);
    event->SetHandled();
  } else if (GetPlayPauseControlsBounds().Contains(event->location())) {
    TogglePlayPause();
    RecordTapGesture(OverlayWindowControl::kPlayPause);
    event->SetHandled();
  } else if (GetNextTrackControlsBounds().Contains(event->location())) {
    controller_->NextTrack();
    RecordTapGesture(OverlayWindowControl::kNextTrack);
    event->SetHandled();
  } else if (GetPreviousTrackControlsBounds().Contains(event->location())) {
    controller_->PreviousTrack();
    RecordTapGesture(OverlayWindowControl::kPreviousTrack);
    event->SetHandled();
  } else if (GetToggleMicrophoneButtonBounds().Contains(event->location())) {
    controller_->ToggleMicrophone();
    RecordTapGesture(OverlayWindowControl::kToggleMicrophone);
    event->SetHandled();
  } else if (GetToggleCameraButtonBounds().Contains(event->location())) {
    controller_->ToggleCamera();
    RecordTapGesture(OverlayWindowControl::kToggleCamera);
    event->SetHandled();
  } else if (GetHangUpButtonBounds().Contains(event->location())) {
    controller_->HangUp();
    RecordTapGesture(OverlayWindowControl::kHangUp);
    event->SetHandled();
  }
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

gfx::Rect OverlayWindowViews::GetBackToTabControlsBounds() {
  if (back_to_tab_image_button_)
    return back_to_tab_image_button_->GetMirroredBounds();

  DCHECK(back_to_tab_label_button_);
  return back_to_tab_label_button_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetSkipAdControlsBounds() {
  return skip_ad_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetCloseControlsBounds() {
  return close_controls_view_->GetMirroredBounds();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
gfx::Rect OverlayWindowViews::GetResizeHandleControlsBounds() {
  return resize_handle_view_->GetMirroredBounds();
}
#endif

gfx::Rect OverlayWindowViews::GetPlayPauseControlsBounds() {
  return play_pause_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetNextTrackControlsBounds() {
  return next_track_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetPreviousTrackControlsBounds() {
  return previous_track_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetToggleMicrophoneButtonBounds() {
  return toggle_microphone_button_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetToggleCameraButtonBounds() {
  return toggle_camera_button_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetHangUpButtonBounds() {
  return hang_up_button_->GetMirroredBounds();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
int OverlayWindowViews::GetResizeHTComponent() const {
  return resize_handle_view_->GetHTComponent();
}
#endif

bool OverlayWindowViews::AreControlsVisible() const {
  return controls_container_view_->GetVisible();
}

void OverlayWindowViews::ForceControlsVisibleForTesting(bool visible) {
  force_controls_visible_ = visible;
  UpdateControlsVisibility(visible);
}

bool OverlayWindowViews::IsLayoutPendingForTesting() const {
  return update_controls_bounds_timer_ &&
         update_controls_bounds_timer_->IsRunning();
}

gfx::Rect OverlayWindowViews::GetWorkAreaForWindow() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(
          native_widget() && IsVisible()
              ? GetNativeWindow()
              : controller_->GetWebContents()->GetTopLevelNativeWindow())
      .work_area();
}

void OverlayWindowViews::UpdateMaxSize(const gfx::Rect& work_area) {
  // An empty |work_area| is not valid, but it is sometimes reported as a
  // transient value.
  if (work_area.IsEmpty())
    return;

  max_size_ = gfx::Size(work_area.width() / 2, work_area.height() / 2);

  if (!native_widget())
    return;

  // native_widget() is required for OnSizeConstraintsChanged.
  OnSizeConstraintsChanged();

  if (window_bounds_.width() <= max_size_.width() &&
      window_bounds_.height() <= max_size_.height()) {
    return;
  }

  SetSize(max_size_);
}

void OverlayWindowViews::TogglePlayPause() {
  // Retrieve expected active state based on what command was sent in
  // TogglePlayPause() since the IPC message may not have been propagated
  // the media player yet.
  bool is_active = controller_->TogglePlayPause();
  play_pause_controls_view_->SetPlaybackState(is_active ? kPlaying : kPaused);
}

views::PlaybackImageButton*
OverlayWindowViews::play_pause_controls_view_for_testing() const {
  return play_pause_controls_view_;
}

views::TrackImageButton*
OverlayWindowViews::next_track_controls_view_for_testing() const {
  return next_track_controls_view_;
}

views::TrackImageButton*
OverlayWindowViews::previous_track_controls_view_for_testing() const {
  return previous_track_controls_view_;
}

views::SkipAdLabelButton*
OverlayWindowViews::skip_ad_controls_view_for_testing() const {
  return skip_ad_controls_view_;
}

ToggleMicrophoneButton*
OverlayWindowViews::toggle_microphone_button_for_testing() const {
  return toggle_microphone_button_;
}

ToggleCameraButton* OverlayWindowViews::toggle_camera_button_for_testing()
    const {
  return toggle_camera_button_;
}

HangUpButton* OverlayWindowViews::hang_up_button_for_testing() const {
  return hang_up_button_;
}

BackToTabLabelButton* OverlayWindowViews::back_to_tab_label_button_for_testing()
    const {
  return back_to_tab_label_button_;
}

gfx::Point OverlayWindowViews::close_image_position_for_testing() const {
  return close_controls_view_->origin();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
gfx::Point OverlayWindowViews::resize_handle_position_for_testing() const {
  return resize_handle_view_->origin();
}
#endif

OverlayWindowViews::PlaybackState
OverlayWindowViews::playback_state_for_testing() const {
  return playback_state_for_testing_;
}

ui::Layer* OverlayWindowViews::video_layer_for_testing() const {
  return video_view_->layer();
}

cc::Layer* OverlayWindowViews::GetLayerForTesting() {
  return GetRootView()->layer()->cc_layer_for_testing();
}

const viz::FrameSinkId* OverlayWindowViews::GetCurrentFrameSinkId() const {
  if (auto* surface = video_view_->layer()->GetSurfaceId())
    return &surface->frame_sink_id();

  return nullptr;
}
