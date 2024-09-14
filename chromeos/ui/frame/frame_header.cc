// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/frame_header.h"

#include <vector>

#include "base/logging.h"  // DCHECK
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/frame_center_button.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/non_client_view.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(chromeos::FrameHeader*)

namespace chromeos {

namespace {

constexpr base::TimeDelta kFrameActivationAnimationDuration =
    base::Milliseconds(200);

DEFINE_UI_CLASS_PROPERTY_KEY(FrameHeader*, kFrameHeaderKey, nullptr)

// Returns the available bounds for the header's title given the views to the
// left and right of the title, and the font used. |left_view| should be null
// if there is no view to the left of the title.
gfx::Rect GetAvailableTitleBounds(const views::View* left_view,
                                  const views::View* right_view,
                                  int header_height) {
  // Space between the title text and the caption buttons.
  constexpr int kTitleCaptionButtonSpacing = 5;
  // Space between window icon and title text.
  constexpr int kTitleIconOffsetX = 5;
  // Space between window edge and title text, when there is no icon.
  constexpr int kTitleNoIconOffsetX = 8;

  const int x = left_view ? left_view->bounds().right() + kTitleIconOffsetX
                          : kTitleNoIconOffsetX;
  const int title_height = gfx::FontList().GetHeight();
  DCHECK_LE(right_view->height(), header_height);
  // We want to align the center points of the header and title vertically.
  // Note that we can't just do (header_height - title_height) / 2, since this
  // won't make the center points align perfectly vertically due to rounding.
  // Floor when computing the center of |header_height| and when computing the
  // center of the text.
  const int header_center_y = header_height / 2;
  const int title_center_y = title_height / 2;
  const int y = std::max(0, header_center_y - title_center_y);
  const int width =
      std::max(0, right_view->x() - kTitleCaptionButtonSpacing - x);
  return gfx::Rect(x, y, width, title_height);
}

}  // namespace

FrameHeader::FrameAnimatorView::FrameAnimatorView(views::View* parent)
    : parent_(parent) {
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  parent_->AddChildViewAt(this, 0);
  parent_->AddObserver(this);
}

FrameHeader::FrameAnimatorView::~FrameAnimatorView() {
  StopAnimation();
  // A child view should always be removed first.
  parent_->RemoveObserver(this);
}

void FrameHeader::FrameAnimatorView::StartAnimation(base::TimeDelta duration) {
  aura::Window* window =
      parent_->GetWidget() ? parent_->GetWidget()->GetNativeWindow() : nullptr;
  if (layer_owner_ || !window ||
      window->layer()->GetAnimator()->is_animating()) {
    // If the frame animation is already running or the widget
    // hasn't been initialized yet, just update the content of the
    // new layer.
    parent_->SchedulePaint();
    return;
  }

  // Make sure the this view is at the bottom of root view's children.
  parent_->ReorderChildView(this, 0);

  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      std::make_unique<ui::LayerTreeOwner>(window->RecreateLayer());
  ui::Layer* old_layer = old_layer_owner->root();
  ui::Layer* new_layer = window->layer();
  new_layer->SetName(old_layer->name());
  old_layer->SetName(old_layer->name() + ":Old");
  old_layer->SetTransform(gfx::Transform());
  // Layer in maximized / fullscreen / snapped state is set to
  // opaque, which can prevent resterizing the new layer immediately.
  old_layer->SetFillsBoundsOpaquely(false);

  layer_owner_ = std::move(old_layer_owner);

  AddLayerToRegion(old_layer, views::LayerRegion::kBelow);

  // The old layer is on top and should fade out. The new layer is given the
  // opacity as the old layer is currently targeting. This ensures that we don't
  // change the overall opacity, since it may have been set by something else.
  new_layer->SetOpacity(old_layer->GetTargetOpacity());
  {
    ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(this);
    settings.SetTransitionDuration(duration);
    old_layer->SetOpacity(0.f);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
  }
}

std::unique_ptr<ui::Layer> FrameHeader::FrameAnimatorView::RecreateLayer() {
  // A layer may be recreated for another animation (maximize/restore).
  // Just cancel the animation if that happens during animation.
  StopAnimation();
  return views::View::RecreateLayer();
}

void FrameHeader::FrameAnimatorView::OnChildViewReordered(
    views::View* observed_view,
    views::View* child) {
  // Stop animation if the child view order has changed during animation.
  StopAnimation();
}

void FrameHeader::FrameAnimatorView::OnViewBoundsChanged(
    views::View* observed_view) {
  // Stop animation if the frame size changed during animation.
  StopAnimation();
  SetBoundsRect(parent_->GetLocalBounds());
}

void FrameHeader::FrameAnimatorView::LayerDestroyed(ui::Layer* layer) {
  CHECK(!layer_owner_ || layer_owner_->root() != layer);
  views::View::LayerDestroyed(layer);
}

void FrameHeader::FrameAnimatorView::OnImplicitAnimationsCompleted() {
  // TODO(crbug.com/40054632): Remove this DCHECK if this is indeed the cause.
  DCHECK(layer_owner_);
  if (layer_owner_) {
    RemoveLayerFromRegions(layer_owner_->root());
    layer_owner_.reset();
  }
}

void FrameHeader::FrameAnimatorView::StopAnimation() {
  if (layer_owner_) {
    layer_owner_->root()->GetAnimator()->StopAnimating();
    layer_owner_.reset();
  }
}

BEGIN_METADATA(FrameHeader, FrameAnimatorView)
END_METADATA

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, public:

// static
FrameHeader* FrameHeader::Get(views::Widget* widget) {
  return widget->GetNativeView()->GetProperty(kFrameHeaderKey);
}

// static
views::View::Views FrameHeader::GetAdjustedChildrenInZOrder(
    views::NonClientFrameView* frame_view) {
  views::View::Views paint_order = frame_view->children();
  views::ClientView* client_view = frame_view->GetWidget()
                                       ? frame_view->GetWidget()->client_view()
                                       : nullptr;

  if (client_view && std::erase(paint_order, client_view)) {
    paint_order.insert(std::next(paint_order.begin(), 1), client_view);
  }

  return paint_order;
}

FrameHeader::~FrameHeader() {
  if (center_button_ && !center_button_->parent()) {
    delete center_button_;
    center_button_ = nullptr;
  }

  if (underneath_layer_owner_) {
    underneath_layer_owner_->RemoveObserver(this);
    underneath_layer_owner_ = nullptr;
  }

  auto* target_window = target_widget_->GetNativeView();
  if (target_window && target_window->GetProperty(kFrameHeaderKey) == this)
    target_window->ClearProperty(kFrameHeaderKey);
}

int FrameHeader::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleBounds().x() +
         caption_button_container_->GetMinimumSize().width() +
         (GetCenterButton() ? GetCenterButton()->GetMinimumSize().width() : 0);
}

void FrameHeader::PaintHeader(gfx::Canvas* canvas) {
  painted_ = true;
  DoPaintHeader(canvas);
}

void FrameHeader::LayoutHeader() {
  LayoutHeaderInternal();
  // Default to the header height; owning code may override via
  // SetHeaderHeightForPainting().
  painted_height_ = GetHeaderHeight();
}

void FrameHeader::InvalidateLayout() {
  view_->InvalidateLayout();
}

int FrameHeader::GetHeaderHeight() const {
  return caption_button_container_->height();
}

int FrameHeader::GetHeaderHeightForPainting() const {
  return painted_height_;
}

void FrameHeader::SetHeaderHeightForPainting(int height) {
  painted_height_ = height;
}

void FrameHeader::SchedulePaintForTitle() {
  view_->SchedulePaintInRect(view_->GetMirroredRect(GetTitleBounds()));
}

void FrameHeader::SetPaintAsActive(bool paint_as_active) {
  // No need to animate if already active.
  const bool already_active = (mode_ == Mode::MODE_ACTIVE);

  if (already_active == paint_as_active)
    return;

  mode_ = paint_as_active ? MODE_ACTIVE : MODE_INACTIVE;

  // The frame has no content yet to animatie.
  if (painted_)
    StartTransitionAnimation(kFrameActivationAnimationDuration);

  caption_button_container_->SetPaintAsActive(paint_as_active);
  if (back_button_)
    back_button_->SetPaintAsActive(paint_as_active);
  if (center_button_)
    center_button_->SetPaintAsActive(paint_as_active);

  UpdateFrameColors();
}

void FrameHeader::OnShowStateChanged(ui::mojom::WindowShowState show_state) {
  if (show_state == ui::mojom::WindowShowState::kMinimized) {
    return;
  }

  LayoutHeaderInternal();
}

void FrameHeader::OnFloatStateChanged() {
  LayoutHeaderInternal();
}

void FrameHeader::SetHeaderCornerRadius(int radius) {
  if (radius == corner_radius_) {
    return;
  }

  corner_radius_ = radius;
  view_->SchedulePaint();
}

void FrameHeader::SetLeftHeaderView(views::View* left_header_view) {
  left_header_view_ = left_header_view;
}

void FrameHeader::SetBackButton(views::FrameCaptionButton* back_button) {
  back_button_ = back_button;
  if (back_button_) {
    back_button_->SetBackgroundColor(GetCurrentFrameColor());
    back_button_->SetImage(views::CAPTION_BUTTON_ICON_BACK,
                           views::FrameCaptionButton::Animate::kNo,
                           chromeos::kWindowControlBackIcon);
  }
}

void FrameHeader::SetCenterButton(chromeos::FrameCenterButton* center_button) {
  DCHECK(!center_button_);
  center_button_ = center_button;
  if (center_button_)
    center_button_->SetBackgroundColor(GetCurrentFrameColor());
}

views::FrameCaptionButton* FrameHeader::GetBackButton() const {
  return back_button_;
}

chromeos::FrameCenterButton* FrameHeader::GetCenterButton() const {
  return center_button_;
}

const chromeos::CaptionButtonModel* FrameHeader::GetCaptionButtonModel() const {
  return caption_button_container_->model();
}

void FrameHeader::SetFrameTextOverride(
    const std::u16string& frame_text_override) {
  frame_text_override_ = frame_text_override;
  SchedulePaintForTitle();
}

SkPath FrameHeader::GetWindowMaskForFrameHeader(const gfx::Size& size) {
  return SkPath();
}

ui::ColorId FrameHeader::GetColorIdForCurrentMode() const {
  return mode_ == MODE_ACTIVE ? ui::kColorFrameActive : ui::kColorFrameInactive;
}

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, protected:

FrameHeader::FrameHeader(views::Widget* target_widget, views::View* view)
    : target_widget_(target_widget), view_(view) {
  DCHECK(target_widget);
  DCHECK(view);
  UpdateFrameHeaderKey();
  frame_animator_ = new FrameAnimatorView(view);
}

void FrameHeader::UpdateFrameHeaderKey() {
  target_widget_->GetNativeView()->SetProperty(kFrameHeaderKey, this);
}

void FrameHeader::OnLayerRecreated(ui::Layer* old_layer) {
  if (underneath_layer_owner_) {
    frame_animator_->RemoveLayerFromRegionsKeepInLayerTree(old_layer);
    frame_animator_->AddLayerToRegion(underneath_layer_owner_->layer(),
                                      views::LayerRegion::kBelow);
  }
}

void FrameHeader::AddLayerBeneath(ui::LayerOwner* layer_owner) {
  if (layer_owner) {
    underneath_layer_owner_ = layer_owner;
    // A relationship between the layer_owner's layer and animation view is
    // created, we need to observe the layer_owner in case of the layer gets
    // recreated.
    layer_owner->AddObserver(this);
    frame_animator_->AddLayerToRegion(layer_owner->layer(),
                                      views::LayerRegion::kBelow);
  }
}

void FrameHeader::RemoveLayerBeneath() {
  if (underneath_layer_owner_) {
    frame_animator_->RemoveLayerFromRegionsKeepInLayerTree(
        underneath_layer_owner_->layer());
    underneath_layer_owner_->RemoveObserver(this);
    underneath_layer_owner_ = nullptr;
  }
}

gfx::Rect FrameHeader::GetPaintedBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

void FrameHeader::UpdateCaptionButtonColors(
    std::optional<ui::ColorId> icon_color_id) {
  const SkColor frame_color = GetCurrentFrameColor();
  if (caption_button_container_->window_controls_overlay_enabled()) {
    caption_button_container_->SetBackground(
        views::CreateSolidBackground(frame_color));
  }

  if (icon_color_id.has_value()) {
    caption_button_container_->SetButtonIconColor(*icon_color_id);
    if (back_button_) {
      back_button_->SetIconColorId(*icon_color_id);
    }
    if (center_button_) {
      center_button_->SetIconColorId(*icon_color_id);
    }
    return;
  }
  caption_button_container_->SetButtonBackgroundColor(frame_color);
  if (back_button_) {
    back_button_->SetBackgroundColor(frame_color);
  }
  if (center_button_) {
    center_button_->SetBackgroundColor(frame_color);
  }
}

void FrameHeader::PaintTitleBar(gfx::Canvas* canvas) {
  std::u16string text = frame_text_override_;
  views::WidgetDelegate* target_widget_delegate =
      target_widget_->widget_delegate();
  if (text.empty() && target_widget_delegate &&
      target_widget_delegate->ShouldShowWindowTitle()) {
    text = target_widget_delegate->GetWindowTitle();
  }

  if (!text.empty()) {
    int flags = gfx::Canvas::NO_SUBPIXEL_RENDERING;
    if (target_widget_delegate->ShouldCenterWindowTitleText())
      flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
    canvas->DrawStringRectWithFlags(text, gfx::FontList(), GetTitleColor(),
                                    view_->GetMirroredRect(GetTitleBounds()),
                                    flags);
  }
}

void FrameHeader::SetCaptionButtonContainer(
    chromeos::FrameCaptionButtonContainerView* caption_button_container) {
  caption_button_container_ = caption_button_container;

  // Perform layout to ensure the container height is correct.
  LayoutHeaderInternal();
}

void FrameHeader::StartTransitionAnimation(base::TimeDelta duration) {
  frame_animator_->StartAnimation(duration);

  frame_animator_->SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, private:

void FrameHeader::LayoutHeaderInternal() {
  // The animator's position can change when the frame is moved from overlay.
  // Make sure the animator view is at the bottom.
  view_->ReorderChildView(frame_animator_, 0);

  caption_button_container()->UpdateButtonsImageAndTooltip();

  caption_button_container()->SetButtonSize(
      views::GetCaptionButtonLayoutSize(GetButtonLayoutSize()));

  const gfx::Size caption_button_container_size =
      caption_button_container()->GetPreferredSize({});
  caption_button_container()->SetBounds(
      view_->width() - caption_button_container_size.width(), 0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  caption_button_container()->DeprecatedLayoutImmediately();

  int origin = 0;
  if (back_button_) {
    gfx::Size size = back_button_->GetPreferredSize({});
    back_button_->SetBounds(0, 0, size.width(),
                            caption_button_container_size.height());
    origin = back_button_->bounds().right();
  }

  if (left_header_view_) {
    // Vertically center the left header view (typically the window icon) with
    // respect to the caption button container.
    const gfx::Size icon_size(left_header_view_->GetPreferredSize({}));
    const int icon_offset_y = (GetHeaderHeight() - icon_size.height()) / 2;
    constexpr int kLeftViewXInset = 9;
    left_header_view_->SetBounds(kLeftViewXInset + origin, icon_offset_y,
                                 icon_size.width(), icon_size.height());
    origin = left_header_view_->bounds().right();
  }

  if (center_button_) {
    constexpr int kCenterButtonSpacing = 5;
    const int full_width = center_button_->GetPreferredSize({}).width();
    const int begin = std::max((view_->width() - full_width) / 2,
                               origin + kCenterButtonSpacing);
    const int end = std::max(
        begin, std::min((view_->width() + full_width) / 2,
                        caption_button_container_->x() - kCenterButtonSpacing));
    center_button_->SetBounds(begin, 0, end - begin,
                              caption_button_container_size.height());
  }
}

gfx::Rect FrameHeader::GetTitleBounds() const {
  views::View* left_view =
      left_header_view_ ? left_header_view_.get() : back_button_.get();
  return GetAvailableTitleBounds(left_view, caption_button_container_,
                                 GetHeaderHeight());
}

}  // namespace chromeos
