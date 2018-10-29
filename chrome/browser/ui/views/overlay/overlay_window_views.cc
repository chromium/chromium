// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/browser/ui/views/overlay/control_image_button.h"
#include "chrome/browser/ui/views/overlay/resize_handle_button.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/picture_in_picture/picture_in_picture_control_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"
#include "ui/views/window/window_resize_utils.h"

// static
std::unique_ptr<content::OverlayWindow> content::OverlayWindow::Create(
    content::PictureInPictureWindowController* controller) {
  return base::WrapUnique(new OverlayWindowViews(controller));
}

namespace {
constexpr gfx::Size kMinWindowSize = gfx::Size(144, 100);

const int kOverlayBorderThickness = 10;

// |button_size_| scales both its width and height to be 30% the size of the
// smaller of the screen's width and height.
const float kControlRatioToWindow = 0.3;

const int kMinControlButtonSize = 48;

// Colors for the control buttons.
SkColor kBgColor = SK_ColorWHITE;
SkColor kControlIconColor = SK_ColorBLACK;

// Returns the quadrant the OverlayWindowViews is primarily in on the current
// work area.
OverlayWindowViews::WindowQuadrant GetCurrentWindowQuadrant(
    const gfx::Rect window_bounds,
    content::PictureInPictureWindowController* controller) {
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              controller->GetInitiatorWebContents()->GetTopLevelNativeWindow())
          .work_area();
  gfx::Point window_center = window_bounds.CenterPoint();

  // Check which quadrant the center of the window appears in.
  if (window_center.x() < work_area.width() / 2) {
    return (window_center.y() < work_area.height() / 2)
               ? OverlayWindowViews::WindowQuadrant::kTopLeft
               : OverlayWindowViews::WindowQuadrant::kBottomLeft;
  }
  return (window_center.y() < work_area.height() / 2)
             ? OverlayWindowViews::WindowQuadrant::kTopRight
             : OverlayWindowViews::WindowQuadrant::kBottomRight;
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
    if (window->GetCloseControlsBounds().Contains(point) ||
        window->GetFirstCustomControlsBounds().Contains(point) ||
        window->GetSecondCustomControlsBounds().Contains(point) ||
        window->GetPlayPauseControlsBounds().Contains(point)) {
      return window_component;
    }

    // Allows for dragging and resizing the window.
    return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
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
  explicit OverlayWindowWidgetDelegate(views::Widget* widget)
      : widget_(widget) {
    DCHECK(widget_);
  }
  ~OverlayWindowWidgetDelegate() override = default;

  // views::WidgetDelegate:
  bool CanResize() const override { return true; }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_NONE; }
  base::string16 GetWindowTitle() const override {
    // While the window title is not shown on the window itself, it is used to
    // identify the window on the system tray.
    return l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_TITLE_TEXT);
  }
  bool ShouldShowWindowTitle() const override { return false; }
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return new OverlayWindowFrameView(widget);
  }

 private:
  // Owns OverlayWindowWidgetDelegate.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowWidgetDelegate);
};

OverlayWindowViews::OverlayWindowViews(
    content::PictureInPictureWindowController* controller)
    : controller_(controller),
      window_background_view_(new views::View()),
      video_view_(new views::View()),
      controls_scrim_view_(new views::View()),
      controls_parent_view_(new views::View()),
      close_controls_view_(new views::CloseImageButton(this)),
#if defined(OS_CHROMEOS)
      resize_handle_view_(new views::ResizeHandleButton(this)),
#endif
      play_pause_controls_view_(new views::ToggleImageButton(this)),
      hide_controls_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(2500 /* 2.5 seconds */),
          base::BindRepeating(&OverlayWindowViews::UpdateControlsVisibility,
                              base::Unretained(this),
                              false /* is_visible */)) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = CalculateAndUpdateWindowBounds();
  params.keep_on_top = true;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;

  // Set WidgetDelegate for more control over |widget_|.
  params.delegate = new OverlayWindowWidgetDelegate(this);

  Init(params);
  SetUpViews();

  is_initialized_ = true;
}

OverlayWindowViews::~OverlayWindowViews() = default;

gfx::Rect OverlayWindowViews::CalculateAndUpdateWindowBounds() {
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              controller_->GetInitiatorWebContents()->GetTopLevelNativeWindow())
          .work_area();

  // Upper bound size of the window is 50% of the display width and height.
  max_size_ = gfx::Size(work_area.width() / 2, work_area.height() / 2);

  // Lower bound size of the window is a fixed value to allow for minimal sizes
  // on UI affordances, such as buttons.
  min_size_ = kMinWindowSize;

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

    // Update the window size to adhere to the aspect ratio.
    gfx::Rect window_rect(GetBounds().origin(), window_size);
    views::WindowResizeUtils::SizeRectToAspectRatio(
        views::HitTest::kBottomRight, aspect_ratio, min_size_, max_size_,
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
  gfx::Rect larger_window_bounds = GetBounds();
  larger_window_bounds.Inset(-1, -1);
  window_background_view_->SetSize(larger_window_bounds.size());
  window_background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  GetWindowBackgroundLayer()->SetColor(SK_ColorBLACK);

  // views::View that holds the scrim, which appears with the controls. -------
  controls_scrim_view_->SetSize(GetBounds().size());
  controls_scrim_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  GetControlsScrimLayer()->SetColor(gfx::kGoogleGrey900);
  GetControlsScrimLayer()->SetOpacity(0.43f);

  // view::View that holds the controls. --------------------------------------
  controls_parent_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  controls_parent_view_->SetSize(GetBounds().size());
  controls_parent_view_->layer()->SetFillsBoundsOpaquely(false);
  controls_parent_view_->set_owned_by_client();

  // views::View that closes the window. --------------------------------------
  close_controls_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view_->layer()->SetFillsBoundsOpaquely(false);
  close_controls_view_->set_owned_by_client();

  // view::View that holds the video. -----------------------------------------
  video_view_->SetPaintToLayer(ui::LAYER_TEXTURED);

  // views::View that toggles play/pause. -------------------------------------
  play_pause_controls_view_->SetImageAlignment(
      views::ImageButton::ALIGN_CENTER, views::ImageButton::ALIGN_MIDDLE);
  play_pause_controls_view_->SetToggled(controller_->IsPlayerActive());
  play_pause_controls_view_->set_owned_by_client();

#if defined(OS_CHROMEOS)
  // views::View that shows the affordance that the window can be resized. ----
  resize_handle_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  resize_handle_view_->layer()->SetFillsBoundsOpaquely(false);
  resize_handle_view_->set_owned_by_client();
#endif

  // Accessibility.
  play_pause_controls_view_->SetFocusForPlatform();  // Make button focusable.
  const base::string16 play_pause_accessible_button_label(
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_PLAY_PAUSE_CONTROL_ACCESSIBLE_TEXT));
  play_pause_controls_view_->SetAccessibleName(
      play_pause_accessible_button_label);
  const base::string16 play_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PLAY_CONTROL_TEXT));
  play_pause_controls_view_->SetTooltipText(play_button_label);
  const base::string16 pause_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PAUSE_CONTROL_TEXT));
  play_pause_controls_view_->SetToggledTooltipText(pause_button_label);
  play_pause_controls_view_->SetInstallFocusRingOnFocus(true);

  // Set up view::Views heirarchy. --------------------------------------------
  controls_parent_view_->AddChildView(play_pause_controls_view_.get());
  GetContentsView()->AddChildView(controls_scrim_view_.get());
  GetContentsView()->AddChildView(controls_parent_view_.get());
  GetContentsView()->AddChildView(close_controls_view_.get());
#if defined(OS_CHROMEOS)
  GetContentsView()->AddChildView(resize_handle_view_.get());
#endif

  UpdatePlayPauseControlsSize();
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

  gfx::Size letterbox_size = letterbox_region.size();
  gfx::Point origin =
      gfx::Point((window_size.width() - letterbox_size.width()) / 2,
                 (window_size.height() - letterbox_size.height()) / 2);

  video_bounds_.set_origin(origin);
  video_bounds_.set_size(letterbox_region.size());

  // Update the layout of the controls.
  UpdateControlsBounds();

  // Update the surface layer bounds to scale with window size changes.
  controller_->UpdateLayerBounds();
}

void OverlayWindowViews::UpdateControlsVisibility(bool is_visible) {
  if (always_hide_play_pause_button_ && is_visible)
    play_pause_controls_view_->SetVisible(false);

  GetCloseControlsLayer()->SetVisible(is_visible);

#if defined(OS_CHROMEOS)
  GetResizeHandleLayer()->SetVisible(is_visible);
#endif

  GetControlsScrimLayer()->SetVisible(
      (playback_state_ == kNoVideo) ? false : is_visible);
  GetControlsParentLayer()->SetVisible(
      (playback_state_ == kNoVideo) ? false : is_visible);
}

void OverlayWindowViews::UpdateControlsBounds() {
  // Adding an extra pixel to width/height makes sure the scrim covers the
  // entire window when the platform has fractional scaling applied.
  gfx::Rect larger_window_bounds = GetBounds();
  larger_window_bounds.Inset(-1, -1);
  controls_scrim_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, 0), larger_window_bounds.size()));

  WindowQuadrant quadrant = GetCurrentWindowQuadrant(GetBounds(), controller_);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);
#if defined(OS_CHROMEOS)
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
#endif

  controls_parent_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, 0), GetBounds().size()));

  UpdateControlsPositions();
}

void OverlayWindowViews::UpdateButtonSize() {
  const gfx::Size window_size = GetBounds().size();
  int scaled_button_dimension =
      window_size.width() < window_size.height()
          ? window_size.width() * kControlRatioToWindow
          : window_size.height() * kControlRatioToWindow;

  int new_button_dimension =
      std::max(kMinControlButtonSize, scaled_button_dimension);

  button_size_.SetSize(new_button_dimension, new_button_dimension);
}

void OverlayWindowViews::UpdateCustomControlsSize(
    views::ControlImageButton* control_button) {
  if (!control_button)
    return;
  UpdateButtonSize();
  control_button->SetSize(button_size_);
  // TODO(sawtelle): Download the images and add them to the controls.
  // https://crbug.com/864271.
  if (control_button == first_custom_controls_view_.get()) {
    first_custom_controls_view_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kPlayArrowIcon, button_size_.width() / 2,
                              kControlIconColor));
  }
  if (control_button == second_custom_controls_view_.get()) {
    second_custom_controls_view_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kPauseIcon, button_size_.width() / 2,
                              kControlIconColor));
  }
  const gfx::ImageSkia control_background = gfx::CreateVectorIcon(
      kPictureInPictureControlBackgroundIcon, button_size_.width(), kBgColor);
  control_button->SetBackgroundImage(kBgColor, &control_background,
                                     &control_background);
}

void OverlayWindowViews::UpdatePlayPauseControlsSize() {
  UpdateButtonSize();
  play_pause_controls_view_->SetSize(button_size_);
  play_pause_controls_view_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kPlayArrowIcon, button_size_.width() / 2,
                            kControlIconColor));
  gfx::ImageSkia pause_icon = gfx::CreateVectorIcon(
      kPauseIcon, button_size_.width() / 2, kControlIconColor);
  play_pause_controls_view_->SetToggledImage(views::Button::STATE_NORMAL,
                                             &pause_icon);
  const gfx::ImageSkia play_pause_background = gfx::CreateVectorIcon(
      kPictureInPictureControlBackgroundIcon, button_size_.width(), kBgColor);
  play_pause_controls_view_->SetBackgroundImage(
      kBgColor, &play_pause_background, &play_pause_background);
}

void OverlayWindowViews::CreateCustomControl(
    std::unique_ptr<views::ControlImageButton>& control_button,
    const blink::PictureInPictureControlInfo& info,
    ControlPosition position) {
  control_button = std::make_unique<views::ControlImageButton>(this);
  controls_parent_view_->AddChildView(control_button.get());
  control_button->set_id(info.id);
  control_button->set_owned_by_client();

  // Sizing / positioning.
  control_button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                    views::ImageButton::ALIGN_MIDDLE);
  UpdateCustomControlsSize(control_button.get());
  UpdateControlsBounds();

  // Accessibility.
  base::string16 custom_button_label = base::UTF8ToUTF16(info.label);
  control_button->SetAccessibleName(custom_button_label);
  control_button->SetTooltipText(custom_button_label);
  control_button->SetInstallFocusRingOnFocus(true);
  control_button->SetFocusForPlatform();
}

bool OverlayWindowViews::HasOnlyOneCustomControl() {
  return first_custom_controls_view_ && !second_custom_controls_view_;
}

gfx::Rect OverlayWindowViews::CalculateControlsBounds(int x,
                                                      const gfx::Size& size) {
  return gfx::Rect(
      gfx::Point(x, (GetBounds().size().height() - size.height()) / 2), size);
}

void OverlayWindowViews::UpdateControlsPositions() {
  int mid_window_x = GetBounds().size().width() / 2;

  // The controls should always be centered, regardless of how many there are.
  // When there are only two controls, make them symmetric from the center.
  //  __________________________
  // |                          |
  // |                          |
  // |        [1]   [P]         |
  // |                          |
  // |__________________________|
  if (HasOnlyOneCustomControl()) {
    play_pause_controls_view_->SetBoundsRect(
        CalculateControlsBounds(mid_window_x, button_size_));
    first_custom_controls_view_->SetBoundsRect(CalculateControlsBounds(
        mid_window_x - button_size_.width(), button_size_));
    return;
  }

  // Place the play / pause control in the center of the window. If both custom
  // controls are specified, place them on either side to maintain the balance,
  // from left to right.
  //  __________________________
  // |                          |
  // |                          |
  // |     [1]   [P]   [2]      |
  // |                          |
  // |__________________________|
  play_pause_controls_view_->SetBoundsRect(CalculateControlsBounds(
      mid_window_x - button_size_.width() / 2, button_size_));

  if (first_custom_controls_view_ && second_custom_controls_view_) {
    first_custom_controls_view_->SetBoundsRect(CalculateControlsBounds(
        mid_window_x - button_size_.width() / 2 - button_size_.width(),
        button_size_));
    second_custom_controls_view_->SetBoundsRect(CalculateControlsBounds(
        mid_window_x + button_size_.width() / 2, button_size_));
  }
}

bool OverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void OverlayWindowViews::Close() {
  views::Widget::Close();
}

void OverlayWindowViews::Show() {
  views::Widget::Show();

  // If this is not the first time the window is shown, this will be a no-op.
  has_been_shown_ = true;
}

void OverlayWindowViews::Hide() {
  views::Widget::Hide();
}

bool OverlayWindowViews::IsVisible() const {
  return views::Widget::IsVisible();
}

bool OverlayWindowViews::IsAlwaysOnTop() const {
  return true;
}

ui::Layer* OverlayWindowViews::GetLayer() {
  return views::Widget::GetLayer();
}

gfx::Rect OverlayWindowViews::GetBounds() const {
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
  // TODO(apacible): have machine state for controls visibility.
  bool controls_parent_layer_visible = GetControlsParentLayer()->visible();

  playback_state_ = playback_state;

  switch (playback_state_) {
    case kPlaying:
      play_pause_controls_view_->SetToggled(true);
      controls_parent_view_->SetVisible(true);
      video_view_->SetVisible(true);
      GetControlsParentLayer()->SetVisible(controls_parent_layer_visible);
      break;
    case kPaused:
      play_pause_controls_view_->SetToggled(false);
      controls_parent_view_->SetVisible(true);
      video_view_->SetVisible(true);
      GetControlsParentLayer()->SetVisible(controls_parent_layer_visible);
      break;
    case kNoVideo:
      controls_scrim_view_->SetVisible(false);
      controls_parent_view_->SetVisible(false);
      video_view_->SetVisible(false);
      GetControlsParentLayer()->SetVisible(false);
      break;
  }
}

void OverlayWindowViews::SetAlwaysHidePlayPauseButton(bool is_visible) {
  always_hide_play_pause_button_ = !is_visible;
}

void OverlayWindowViews::SetPictureInPictureCustomControls(
    const std::vector<blink::PictureInPictureControlInfo>& controls) {
  // Clear any existing controls.
  first_custom_controls_view_.reset();
  second_custom_controls_view_.reset();

  if (controls.size() > 0)
    CreateCustomControl(first_custom_controls_view_, controls[0],
                        ControlPosition::kLeft);
  if (controls.size() > 1)
    CreateCustomControl(second_custom_controls_view_, controls[1],
                        ControlPosition::kRight);
}

ui::Layer* OverlayWindowViews::GetWindowBackgroundLayer() {
  return window_background_view_->layer();
}

ui::Layer* OverlayWindowViews::GetVideoLayer() {
  return video_view_->layer();
}

gfx::Rect OverlayWindowViews::GetVideoBounds() {
  return video_bounds_;
}

void OverlayWindowViews::OnNativeBlur() {
  // Controls should be hidden when there is no more focus on the window. This
  // is used for tabbing and touch interactions. For mouse interactions, the
  // window cannot be blurred before the ui::ET_MOUSE_EXITED event is handled.
  if (is_initialized_)
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
  if (is_initialized_)
    UpdateControlsVisibility(false);

  // Update the existing |window_bounds_| when the window moves. This allows
  // the window to reappear with the same origin point when a new video is
  // shown.
  window_bounds_ = GetBounds();

#if defined(OS_CHROMEOS)
  // Update the positioning of some icons when the window is moved.
  WindowQuadrant quadrant = GetCurrentWindowQuadrant(GetBounds(), controller_);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
#endif
}

void OverlayWindowViews::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  // Hide the controls when the window is being resized. The controls will
  // reappear when the user interacts with the window again.
  if (is_initialized_)
    UpdateControlsVisibility(false);

  // Update the view layers to scale to |new_size|.
  UpdateCustomControlsSize(first_custom_controls_view_.get());
  UpdateCustomControlsSize(second_custom_controls_view_.get());
  UpdatePlayPauseControlsSize();
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

// On Mac, the space key event isn't automatically handled. Only handle space
// for TogglePlayPause() since tabbing between the buttons is not supported and
// there is no focus affordance on the buttons.
#if defined(OS_MACOSX)
  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_SPACE) {
    TogglePlayPause();
    event->SetHandled();
  }
#endif  // OS_MACOSX

// On Windows, the Alt+F4 keyboard combination closes the window. Only handle
// closure on key press so Close() is not called a second time when the key
// is released.
#if defined(OS_WIN)
  if (event->type() == ui::ET_KEY_PRESSED && event->IsAltDown() &&
      event->key_code() == ui::VKEY_F4) {
    controller_->Close(true /* should_pause_video */,
                       true /* should_reset_pip_player */);
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
      if (!GetVideoBounds().Contains(event->location()))
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
  if (!GetControlsScrimLayer()->visible()) {
    UpdateControlsVisibility(true);
    return;
  }

  if (GetCloseControlsBounds().Contains(event->location())) {
    controller_->Close(true /* should_pause_video */,
                       true /* should_reset_pip_player */);
    event->SetHandled();
  } else if (GetPlayPauseControlsBounds().Contains(event->location())) {
    TogglePlayPause();
    event->SetHandled();
  }

  views::Widget::OnGestureEvent(event);
}

void OverlayWindowViews::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == close_controls_view_.get())
    controller_->Close(true /* should_pause_video */,
                       true /* should_reset_pip_player */);

  if (sender == play_pause_controls_view_.get())
    TogglePlayPause();

  if (sender == first_custom_controls_view_.get())
    controller_->CustomControlPressed(first_custom_controls_view_->id());

  if (sender == second_custom_controls_view_.get())
    controller_->CustomControlPressed(second_custom_controls_view_->id());
}

gfx::Rect OverlayWindowViews::GetCloseControlsBounds() {
  return close_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetPlayPauseControlsBounds() {
  return play_pause_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetFirstCustomControlsBounds() {
  if (!first_custom_controls_view_)
    return gfx::Rect();
  return first_custom_controls_view_->GetMirroredBounds();
}

gfx::Rect OverlayWindowViews::GetSecondCustomControlsBounds() {
  if (!second_custom_controls_view_)
    return gfx::Rect();
  return second_custom_controls_view_->GetMirroredBounds();
}

ui::Layer* OverlayWindowViews::GetControlsScrimLayer() {
  return controls_scrim_view_->layer();
}

ui::Layer* OverlayWindowViews::GetCloseControlsLayer() {
  return close_controls_view_->layer();
}

ui::Layer* OverlayWindowViews::GetResizeHandleLayer() {
  return resize_handle_view_->layer();
}

ui::Layer* OverlayWindowViews::GetControlsParentLayer() {
  return controls_parent_view_->layer();
}

void OverlayWindowViews::TogglePlayPause() {
  // Retrieve expected active state based on what command was sent in
  // TogglePlayPause() since the IPC message may not have been propogated
  // the media player yet.
  bool is_active = controller_->TogglePlayPause();
  play_pause_controls_view_->SetToggled(is_active);
}

views::ToggleImageButton*
OverlayWindowViews::play_pause_controls_view_for_testing() const {
  return play_pause_controls_view_.get();
}

gfx::Point OverlayWindowViews::close_image_position_for_testing() const {
  return close_controls_view_->origin();
}

gfx::Point OverlayWindowViews::resize_handle_position_for_testing() const {
  return resize_handle_view_->origin();
}

views::View* OverlayWindowViews::controls_parent_view_for_testing() const {
  return controls_parent_view_.get();
}

OverlayWindowViews::PlaybackState
OverlayWindowViews::playback_state_for_testing() const {
  return playback_state_;
}
