// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
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
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget_delegate.h"

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

template <typename T>
T* AddChildView(std::vector<std::unique_ptr<views::View>>* views,
                std::unique_ptr<T> child) {
  views->push_back(std::move(child));
  return static_cast<T*>(views->back().get());
}

class WindowBackgroundView : public views::View {
 public:
  METADATA_HEADER(WindowBackgroundView);

  WindowBackgroundView() = default;
  WindowBackgroundView(const WindowBackgroundView&) = delete;
  WindowBackgroundView& operator=(const WindowBackgroundView&) = delete;
  ~WindowBackgroundView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    layer()->SetColor(GetColorProvider()->GetColor(kColorPipWindowBackground));
  }
};

BEGIN_METADATA(WindowBackgroundView, views::View)
END_METADATA

class ControlsBackgroundView : public views::View {
 public:
  METADATA_HEADER(ControlsBackgroundView);

  ControlsBackgroundView() = default;
  ControlsBackgroundView(const ControlsBackgroundView&) = delete;
  ControlsBackgroundView& operator=(const ControlsBackgroundView&) = delete;
  ~ControlsBackgroundView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const SkColor color =
        GetColorProvider()->GetColor(kColorPipWindowControlsBackground);
    layer()->SetColor(SkColorSetA(color, SK_AlphaOPAQUE));
    layer()->SetOpacity(static_cast<float>(SkColorGetA(color)) /
                        SK_AlphaOPAQUE);
  }
};

BEGIN_METADATA(ControlsBackgroundView, views::View)
END_METADATA

}  // namespace

// static
std::unique_ptr<VideoOverlayWindowViews> VideoOverlayWindowViews::Create(
    content::VideoPictureInPictureWindowController* controller) {
  // Can't use make_unique(), which doesn't have access to the private
  // constructor. It's important that the constructor be private, because it
  // doesn't initialize the object fully.
  auto overlay_window =
      base::WrapUnique(new VideoOverlayWindowViews(controller));

  overlay_window->CalculateAndUpdateWindowBounds();
  overlay_window->SetUpViews();

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
  params.delegate = OverlayWindowViews::CreateDelegate();

  overlay_window->Init(std::move(params));
  overlay_window->OnRootViewReady();

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

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
    : controller_(controller) {}

VideoOverlayWindowViews::~VideoOverlayWindowViews() = default;

bool VideoOverlayWindowViews::ControlsHitTestContainsPoint(
    const gfx::Point& point) {
  if (!AreControlsVisible())
    return false;
  if (GetBackToTabControlsBounds().Contains(point) ||
      GetSkipAdControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point) ||
      GetPlayPauseControlsBounds().Contains(point) ||
      GetNextTrackControlsBounds().Contains(point) ||
      GetPreviousTrackControlsBounds().Contains(point) ||
      GetToggleMicrophoneButtonBounds().Contains(point) ||
      GetToggleCameraButtonBounds().Contains(point) ||
      GetHangUpButtonBounds().Contains(point)) {
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
  auto close_controls_view =
      std::make_unique<CloseImageButton>(base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) {
            // Only pause the video if play/pause is available.
            const bool should_pause_video = overlay->show_play_pause_button_;
            overlay->controller_->Close(should_pause_video);
            overlay->RecordButtonPressed(OverlayWindowControl::kClose);
          },
          base::Unretained(this)));

  std::unique_ptr<BackToTabImageButton> back_to_tab_image_button;
  std::unique_ptr<BackToTabLabelButton> back_to_tab_label_button;
  auto back_to_tab_callback = base::BindRepeating(
      [](VideoOverlayWindowViews* overlay) {
        overlay->controller_->CloseAndFocusInitiator();
        overlay->RecordButtonPressed(OverlayWindowControl::kBackToTab);
      },
      base::Unretained(this));
  if (base::FeatureList::IsEnabled(media::kMediaSessionWebRTC)) {
    back_to_tab_label_button =
        std::make_unique<BackToTabLabelButton>(std::move(back_to_tab_callback));
  } else {
    back_to_tab_image_button =
        std::make_unique<BackToTabImageButton>(std::move(back_to_tab_callback));
  }

  auto previous_track_controls_view = std::make_unique<TrackImageButton>(
      base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) {
            overlay->controller_->PreviousTrack();
            overlay->RecordButtonPressed(OverlayWindowControl::kPreviousTrack);
          },
          base::Unretained(this)),
      vector_icons::kMediaPreviousTrackIcon,
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_PREVIOUS_TRACK_CONTROL_ACCESSIBLE_TEXT));
  auto play_pause_controls_view =
      std::make_unique<PlaybackImageButton>(base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) {
            overlay->TogglePlayPause();
            overlay->RecordButtonPressed(OverlayWindowControl::kPlayPause);
          },
          base::Unretained(this)));
  auto next_track_controls_view = std::make_unique<TrackImageButton>(
      base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) {
            overlay->controller_->NextTrack();
            overlay->RecordButtonPressed(OverlayWindowControl::kNextTrack);
          },
          base::Unretained(this)),
      vector_icons::kMediaNextTrackIcon,
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_NEXT_TRACK_CONTROL_ACCESSIBLE_TEXT));
  auto skip_ad_controls_view =
      std::make_unique<SkipAdLabelButton>(base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) {
            overlay->controller_->SkipAd();
            overlay->RecordButtonPressed(OverlayWindowControl::kSkipAd);
          },
          base::Unretained(this)));
  auto toggle_microphone_button =
      std::make_unique<ToggleMicrophoneButton>(base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) {
            overlay->controller_->ToggleMicrophone();
            overlay->RecordButtonPressed(
                OverlayWindowControl::kToggleMicrophone);
          },
          base::Unretained(this)));
  auto toggle_camera_button =
      std::make_unique<ToggleCameraButton>(base::BindRepeating(
          [](VideoOverlayWindowViews* overlay) {
            overlay->controller_->ToggleCamera();
            overlay->RecordButtonPressed(OverlayWindowControl::kToggleCamera);
          },
          base::Unretained(this)));
  auto hang_up_button = std::make_unique<HangUpButton>(base::BindRepeating(
      [](VideoOverlayWindowViews* overlay) {
        overlay->controller_->HangUp();
        overlay->RecordButtonPressed(OverlayWindowControl::kHangUp);
      },
      base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

void VideoOverlayWindowViews::OnRootViewReady() {
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

void VideoOverlayWindowViews::UpdateLayerBoundsWithLetterboxing(
    gfx::Size window_size) {
  // This is the case when the window is initially created or the video surface
  // id has not been embedded.
  if (!native_widget() || GetBounds().IsEmpty() || GetNaturalSize().IsEmpty())
    return;

  gfx::Rect letterbox_region = media::ComputeLetterboxRegion(
      gfx::Rect(gfx::Point(0, 0), window_size), GetNaturalSize());
  if (letterbox_region.IsEmpty())
    return;

  // To avoid black stripes in the window when integer window dimensions don't
  // correspond to the video aspect ratio exactly (e.g. 854x480 for 16:9
  // video) force the letterbox region size to be equal to the window size.
  const float aspect_ratio =
      static_cast<float>(GetNaturalSize().width()) / GetNaturalSize().height();
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
  if (video_view_->layer()->has_external_content())
    video_view_->layer()->SetSurfaceSize(video_bounds.size());

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

  WindowQuadrant quadrant =
      OverlayWindowViews::GetCurrentWindowQuadrant(GetBounds(), controller_);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);

  if (back_to_tab_label_button_)
    back_to_tab_label_button_->SetWindowSize(GetBounds().size());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  UpdateResizeHandleBounds(quadrant);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void VideoOverlayWindowViews::UpdateResizeHandleBounds(
    WindowQuadrant quadrant) {
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
  GetNativeWindow()->SetProperty(
      ash::kWindowPipResizeHandleBoundsKey,
      new gfx::Rect(GetResizeHandleControlsBounds()));
}
#endif

bool VideoOverlayWindowViews::IsActive() {
  return views::Widget::IsActive();
}

bool VideoOverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void VideoOverlayWindowViews::Close() {
  views::Widget::Close();
  MaybeUnregisterFrameSinkHierarchy();
}

void VideoOverlayWindowViews::ShowInactive() {
  DoShowInactive();
}

void VideoOverlayWindowViews::Hide() {
  OverlayWindowViews::Hide();
  MaybeUnregisterFrameSinkHierarchy();
}

bool VideoOverlayWindowViews::IsVisible() {
  return views::Widget::IsVisible();
}

bool VideoOverlayWindowViews::IsVisible() const {
  return views::Widget::IsVisible();
}

bool VideoOverlayWindowViews::IsAlwaysOnTop() {
  return true;
}

gfx::Rect VideoOverlayWindowViews::GetBounds() {
  return views::Widget::GetRestoredBounds();
}

void VideoOverlayWindowViews::UpdateNaturalSize(const gfx::Size& natural_size) {
  DoUpdateNaturalSize(natural_size);
}

void VideoOverlayWindowViews::SetPlaybackState(PlaybackState playback_state) {
  playback_state_for_testing_ = playback_state;
  play_pause_controls_view_->SetPlaybackState(playback_state);
}

void VideoOverlayWindowViews::SetPlayPauseButtonVisibility(bool is_visible) {
  if (show_play_pause_button_ == is_visible)
    return;

  show_play_pause_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetSkipAdButtonVisibility(bool is_visible) {
  if (show_skip_ad_button_ == is_visible)
    return;

  show_skip_ad_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetNextTrackButtonVisibility(bool is_visible) {
  if (show_next_track_button_ == is_visible)
    return;

  show_next_track_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetPreviousTrackButtonVisibility(
    bool is_visible) {
  if (show_previous_track_button_ == is_visible)
    return;

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
  if (show_toggle_microphone_button_ == is_visible)
    return;

  show_toggle_microphone_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetToggleCameraButtonVisibility(bool is_visible) {
  if (show_toggle_camera_button_ == is_visible)
    return;

  show_toggle_camera_button_ = is_visible;
  UpdateControlsBounds();
}

void VideoOverlayWindowViews::SetHangUpButtonVisibility(bool is_visible) {
  if (show_hang_up_button_ == is_visible)
    return;

  show_hang_up_button_ = is_visible;
  UpdateControlsBounds();
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

void VideoOverlayWindowViews::OnNativeWidgetMove() {
  OverlayWindowViews::OnNativeWidgetMove();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Update the positioning of some icons when the window is moved.
  WindowQuadrant quadrant =
      GetCurrentWindowQuadrant(GetRestoredBounds(), GetController());
  close_controls_view_->SetPosition(GetRestoredBounds().size(), quadrant);
  UpdateResizeHandleBounds(quadrant);
#endif
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

void VideoOverlayWindowViews::OnKeyEvent(ui::KeyEvent* event) {
  // If there is no focus affordance on the buttons and play/pause button is
  // visible, only handle space key for TogglePlayPause().
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (!focused_view && event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_SPACE && show_play_pause_button_) {
    TogglePlayPause();
    event->SetHandled();
  }

  OverlayWindowViews::OnKeyEvent(event);
}

void VideoOverlayWindowViews::OnGestureEvent(ui::GestureEvent* event) {
  if (OverlayWindowViews::OnGestureEventHandledOrIgnored(event))
    return;

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

gfx::Rect VideoOverlayWindowViews::GetBackToTabControlsBounds() {
  if (back_to_tab_image_button_)
    return back_to_tab_image_button_->GetMirroredBounds();

  DCHECK(back_to_tab_label_button_);
  return back_to_tab_label_button_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetSkipAdControlsBounds() {
  return skip_ad_controls_view_->GetMirroredBounds();
}

gfx::Rect VideoOverlayWindowViews::GetCloseControlsBounds() {
  return close_controls_view_->GetMirroredBounds();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
gfx::Rect VideoOverlayWindowViews::GetResizeHandleControlsBounds() {
  return resize_handle_view_->GetMirroredBounds();
}
#endif

gfx::Rect VideoOverlayWindowViews::GetPlayPauseControlsBounds() {
  return play_pause_controls_view_->GetMirroredBounds();
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

PlaybackImageButton*
VideoOverlayWindowViews::play_pause_controls_view_for_testing() const {
  return play_pause_controls_view_;
}

TrackImageButton*
VideoOverlayWindowViews::next_track_controls_view_for_testing() const {
  return next_track_controls_view_;
}

TrackImageButton*
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

BackToTabLabelButton*
VideoOverlayWindowViews::back_to_tab_label_button_for_testing() const {
  return back_to_tab_label_button_;
}

CloseImageButton* VideoOverlayWindowViews::close_button_for_testing() const {
  return close_controls_view_;
}

gfx::Point VideoOverlayWindowViews::close_image_position_for_testing() const {
  return close_controls_view_->origin();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

cc::Layer* VideoOverlayWindowViews::GetLayerForTesting() {
  return GetRootView()->layer()->cc_layer_for_testing();  // IN-TEST
}

const viz::FrameSinkId* VideoOverlayWindowViews::GetCurrentFrameSinkId() const {
  if (auto* surface = video_view_->layer()->GetSurfaceId())
    return &surface->frame_sink_id();

  return nullptr;
}

void VideoOverlayWindowViews::MaybeUnregisterFrameSinkHierarchy() {
  if (has_registered_frame_sink_hierarchy_) {
    DCHECK(GetCurrentFrameSinkId());
    GetCompositor()->RemoveChildFrameSink(*GetCurrentFrameSinkId());
    has_registered_frame_sink_hierarchy_ = false;
  }
}
