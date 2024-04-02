// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/header_view.h"

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/caption_buttons/frame_back_button.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/frame_center_button.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace chromeos {

using ::chromeos::DefaultFrameHeader;
using ::chromeos::kFrameActiveColorKey;
using ::chromeos::kFrameInactiveColorKey;

// The view used to draw the content (background and title string)
// of the header. This is a separate view so that it can use
// different scaling strategy than the rest of the frame such
// as caption buttons.
class HeaderView::HeaderContentView : public views::View {
  METADATA_HEADER(HeaderContentView, views::View)

 public:
  explicit HeaderContentView(HeaderView* header_view)
      : header_view_(header_view) {}

  HeaderContentView(const HeaderContentView&) = delete;
  HeaderContentView& operator=(const HeaderContentView&) = delete;

  ~HeaderContentView() override = default;

  // views::View:
  views::PaintInfo::ScaleType GetPaintScaleType() const override {
    return scale_type_;
  }
  void OnPaint(gfx::Canvas* canvas) override {
    header_view_->PaintHeaderContent(canvas);
  }

  void SetScaleType(views::PaintInfo::ScaleType scale_type) {
    scale_type_ = scale_type;
  }

 private:
  raw_ptr<HeaderView> header_view_;
  views::PaintInfo::ScaleType scale_type_ =
      views::PaintInfo::ScaleType::kScaleWithEdgeSnapping;
};

BEGIN_METADATA(HeaderView, HeaderContentView)
END_METADATA

HeaderView::HeaderView(views::Widget* target_widget,
                       views::NonClientFrameView* frame_view)
    : target_widget_(target_widget) {
  header_content_view_ =
      AddChildView(std::make_unique<HeaderContentView>(this));

  caption_button_container_ =
      AddChildView(std::make_unique<chromeos::FrameCaptionButtonContainerView>(
          target_widget_));

  frame_header_ = std::make_unique<DefaultFrameHeader>(
      target_widget,
      (frame_view ? static_cast<views::View*>(frame_view) : this),
      caption_button_container_);
}

void HeaderView::Init() {
  UpdateBackButton();
  UpdateCenterButton();
  frame_header_->UpdateFrameColors();

  aura::Window* window = target_widget_->GetNativeWindow();
  window_observation_.Observe(window);
  display_observer_.emplace(this);
}

HeaderView::~HeaderView() = default;

void HeaderView::SchedulePaintForTitle() {
  frame_header_->SchedulePaintForTitle();
}

void HeaderView::ResetWindowControls() {
  caption_button_container_->ResetWindowControls();
}

int HeaderView::GetPreferredOnScreenHeight() {
  if (in_immersive_mode_) {
    return static_cast<int>(GetPreferredHeight() *
                            fullscreen_visible_fraction_);
  }

  return (target_widget_ && target_widget_->IsFullscreen())
             ? 0
             : GetPreferredHeight();
}

int HeaderView::GetPreferredHeight() {
  // Calculating the preferred height requires at least one layout.
  if (!did_layout_)
    DeprecatedLayoutImmediately();
  return frame_header_->GetHeaderHeightForPainting();
}

int HeaderView::GetMinimumWidth() const {
  return frame_header_->GetMinimumHeaderWidth();
}

void HeaderView::SetAvatarIcon(const gfx::ImageSkia& avatar) {
  const bool show = !avatar.isNull();
  if (!show) {
    if (!avatar_icon_)
      return;
    delete avatar_icon_;
    avatar_icon_ = nullptr;
  } else {
    DCHECK_EQ(avatar.width(), avatar.height());
    if (!avatar_icon_) {
      avatar_icon_ = new views::ImageView();
      AddChildView(avatar_icon_.get());
    }
    avatar_icon_->SetImage(avatar);
  }
  frame_header_->SetLeftHeaderView(avatar_icon_);
  DeprecatedLayoutImmediately();
}

void HeaderView::UpdateCaptionButtons() {
  caption_button_container_->ResetWindowControls();
  caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);

  UpdateBackButton();
  UpdateCenterButton();

  DeprecatedLayoutImmediately();
}

void HeaderView::SetWidthInPixels(int width_in_pixels) {
  frame_header_->SetWidthInPixels(width_in_pixels);
  // If the width is given in pixels, use uniform scaling
  // so that UndoDeviceScaleFactor can correctly undo the scaling.
  header_content_view_->SetScaleType(
      width_in_pixels > 0
          ? views::PaintInfo::ScaleType::kUniformScaling
          : views::PaintInfo::ScaleType::kScaleWithEdgeSnapping);
}

void HeaderView::SetHeaderCornerRadius(int radius) {
  frame_header_->SetHeaderCornerRadius(radius);
}

void HeaderView::Layout(PassKey) {
  did_layout_ = true;
  header_content_view_->SetBoundsRect(GetLocalBounds());
  frame_header_->LayoutHeader();
}

void HeaderView::ChildPreferredSizeChanged(views::View* child) {
  if (child != caption_button_container_)
    return;

  // May be null during view initialization.
  if (parent())
    parent()->DeprecatedLayoutImmediately();
}

bool HeaderView::IsDrawn() const {
  if (is_drawn_override_)
    return true;
  return views::View::IsDrawn();
}

void HeaderView::OnWindowPropertyChanged(aura::Window* window,
                                         const void* key,
                                         intptr_t old) {
  if (!target_widget_)
    return;

  DCHECK_EQ(target_widget_->GetNativeWindow(), window);

  if (key == aura::client::kAvatarIconKey) {
    gfx::ImageSkia* const avatar_icon =
        window->GetProperty(aura::client::kAvatarIconKey);
    SetAvatarIcon(avatar_icon ? *avatar_icon : gfx::ImageSkia());
  } else if (key == kFrameActiveColorKey || key == kFrameInactiveColorKey) {
    frame_header_->UpdateFrameColors();
  } else if (key == aura::client::kShowStateKey) {
    frame_header_->OnShowStateChanged(
        window->GetProperty(aura::client::kShowStateKey));
  } else if (key == chromeos::kWindowStateTypeKey) {
    // Float state is an ash specific state that changes the header UI. It isn't
    // a show state so we need to watch the window state type key as well.
    if (window->GetProperty(chromeos::kWindowStateTypeKey) ==
            chromeos::WindowStateType::kFloated ||
        static_cast<chromeos::WindowStateType>(old) ==
            chromeos::WindowStateType::kFloated) {
      frame_header_->OnFloatStateChanged();
    }
  }
}

void HeaderView::OnWindowDestroying(aura::Window* window) {
  DCHECK(window_observation_.IsObservingSource(window));
  window_observation_.Reset();
  display_observer_.reset();

  // A HeaderView may outlive the target widget.
  target_widget_ = nullptr;
}

void HeaderView::OnDisplayMetricsChanged(const display::Display& display,
                                         uint32_t changed_metrics) {
  // When the display is rotated, the frame header may have invalid snap icons.
  // For example, rotating from landscape display to portrait display layout
  // should update snap icons from left/right arrows to upward/downward arrows
  // for top and bottom snaps.
  if ((changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION) &&
      frame_header_) {
    CHECK(target_widget_);
    frame_header_->LayoutHeader();
  }
}

void HeaderView::OnDisplayTabletStateChanged(display::TabletState state) {
  switch (state) {
    case display::TabletState::kInTabletMode:
      UpdateCaptionButtonsVisibility();
      caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);
      parent()->DeprecatedLayoutImmediately();
      if (target_widget_) {
        target_widget_->non_client_view()->DeprecatedLayoutImmediately();
      }
      break;
    case display::TabletState::kInClamshellMode:
      UpdateCaptionButtonsVisibility();
      caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);
      parent()->DeprecatedLayoutImmediately();
      if (target_widget_)
        target_widget_->non_client_view()->DeprecatedLayoutImmediately();
      break;
    case display::TabletState::kEnteringTabletMode:
      break;
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

views::View* HeaderView::avatar_icon() const {
  return avatar_icon_;
}

void HeaderView::SetShouldPaintHeader(bool paint) {
  if (should_paint_ == paint)
    return;

  should_paint_ = paint;
  UpdateCaptionButtonsVisibility();
  SchedulePaint();
}

views::FrameCaptionButton* HeaderView::GetBackButton() {
  return frame_header_->GetBackButton();
}

void HeaderView::OnImmersiveRevealStarted() {
  fullscreen_visible_fraction_ = 0;

  add_layer_for_immersive_ = !layer();
  if (add_layer_for_immersive_)
    SetPaintToLayer();
  // AppWindow may call this before being added to the widget.
  // https://crbug.com/825260.
  if (layer()->parent()) {
    // The immersive layer should always be top.
    layer()->parent()->StackAtTop(layer());
  }
  parent()->DeprecatedLayoutImmediately();
}

void HeaderView::OnImmersiveRevealEnded() {
  fullscreen_visible_fraction_ = 0;
  if (add_layer_for_immersive_)
    DestroyLayer();
  parent()->DeprecatedLayoutImmediately();
}

void HeaderView::OnImmersiveFullscreenEntered() {
  in_immersive_mode_ = true;
  parent()->InvalidateLayout();
  if (!immersive_mode_changed_callback_.is_null())
    immersive_mode_changed_callback_.Run();
}

void HeaderView::OnImmersiveFullscreenExited() {
  in_immersive_mode_ = false;
  fullscreen_visible_fraction_ = 0;
  if (add_layer_for_immersive_)
    DestroyLayer();
  parent()->InvalidateLayout();
  if (!immersive_mode_changed_callback_.is_null())
    immersive_mode_changed_callback_.Run();
}

void HeaderView::SetVisibleFraction(double visible_fraction) {
  if (fullscreen_visible_fraction_ != visible_fraction) {
    fullscreen_visible_fraction_ = visible_fraction;
    parent()->DeprecatedLayoutImmediately();
  }
}

std::vector<gfx::Rect> HeaderView::GetVisibleBoundsInScreen() const {
  // TODO(pkotwicz): Implement views::View::ConvertRectToScreen().
  base::AutoReset<bool> reset(&is_drawn_override_, true);
  gfx::Rect visible_bounds(GetVisibleBounds());
  gfx::Point visible_origin_in_screen(visible_bounds.origin());
  views::View::ConvertPointToScreen(this, &visible_origin_in_screen);
  std::vector<gfx::Rect> bounds_in_screen;
  bounds_in_screen.push_back(
      gfx::Rect(visible_origin_in_screen, visible_bounds.size()));
  return bounds_in_screen;
}

void HeaderView::Relayout() {
  parent()->DeprecatedLayoutImmediately();
}

void HeaderView::PaintHeaderContent(gfx::Canvas* canvas) {
  if (!should_paint_ || !target_widget_)
    return;

  frame_header_->PaintHeader(canvas);
}

void HeaderView::UpdateBackButton() {
  bool has_back_button = caption_button_container_->model()->IsVisible(
      views::CAPTION_BUTTON_ICON_BACK);
  views::FrameCaptionButton* back_button = frame_header_->GetBackButton();
  if (has_back_button) {
    if (!back_button) {
      back_button = new chromeos::FrameBackButton();
      AddChildView(back_button);
      frame_header_->SetBackButton(back_button);
    }
    back_button->SetEnabled(caption_button_container_->model()->IsEnabled(
        views::CAPTION_BUTTON_ICON_BACK));
  } else {
    delete back_button;
    frame_header_->SetBackButton(nullptr);
  }
}

void HeaderView::UpdateCenterButton() {
  bool is_center_button_visible = caption_button_container_->model()->IsVisible(
      views::CAPTION_BUTTON_ICON_CENTER);
  auto* center_button = frame_header_->GetCenterButton();
  if (!center_button)
    return;
  if (is_center_button_visible) {
    if (!center_button->parent())
      AddChildView(center_button);
    center_button->SetVisible(true);
  } else {
    center_button->SetVisible(false);
  }
}

void HeaderView::UpdateCaptionButtonsVisibility() {
  if (!target_widget_)
    return;

  caption_button_container_->SetVisible(should_paint_);
}

BEGIN_METADATA(HeaderView)
END_METADATA

}  // namespace chromeos
