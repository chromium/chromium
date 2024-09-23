// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"

#include <utility>

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/highlight_path_generator.h"

using std::make_unique;

namespace {
class ControlButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit ControlButtonHighlightPathGenerator(
      TabStripControlButton* control_button)
      : control_button_(control_button) {}

  // HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    gfx::Rect rect(view->GetContentsBounds());

    SkPath path;
    const int corner_radius = control_button_->GetCornerRadius();
    const SkScalar left_radius =
        control_button_->GetScaledCornerRadius(corner_radius, Edge::kLeft);
    const SkScalar right_radius =
        control_button_->GetScaledCornerRadius(corner_radius, Edge::kRight);
    const SkScalar radii[8] = {left_radius,  left_radius,  right_radius,
                               right_radius, right_radius, right_radius,
                               left_radius,  left_radius};
    path.addRoundRect(gfx::RectToSkRect(rect), radii);
    return path;
  }

 private:
  raw_ptr<TabStripControlButton> control_button_;
};
}  // namespace

const int TabStripControlButton::kIconSize = 16;
const gfx::Size TabStripControlButton::kButtonSize{28, 28};
const gfx::VectorIcon kEmptyIcon;

TabStripControlButton::TabStripControlButton(
    TabStripController* tab_strip_controller,
    PressedCallback callback,
    const gfx::VectorIcon& icon,
    Edge flat_edge)
    : TabStripControlButton(tab_strip_controller,
                            std::move(callback),
                            icon,
                            std::u16string(),
                            flat_edge) {}

TabStripControlButton::TabStripControlButton(
    TabStripController* tab_strip_controller,
    PressedCallback callback,
    const std::u16string& text,
    Edge flat_edge)
    : TabStripControlButton(tab_strip_controller,
                            std::move(callback),
                            kEmptyIcon,
                            text,
                            flat_edge) {}

TabStripControlButton::TabStripControlButton(
    TabStripController* tab_strip_controller,
    PressedCallback callback,
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    Edge flat_edge)
    : views::LabelButton(std::move(callback), text),
      icon_(icon),
      flat_edge_(flat_edge),
      tab_strip_controller_(tab_strip_controller) {
  SetImageCentered(true);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  foreground_frame_active_color_id_ = kColorTabForegroundInactiveFrameActive;
  foreground_frame_inactive_color_id_ =
      kColorNewTabButtonCRForegroundFrameInactive;
  background_frame_active_color_id_ = kColorNewTabButtonBackgroundFrameActive;
  background_frame_inactive_color_id_ =
      kColorNewTabButtonBackgroundFrameInactive;

  UpdateIcon();
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetLayerRegion(views::LayerRegion::kAbove);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<ControlButtonHighlightPathGenerator>(this));
  UpdateInkDrop();
  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);

  if (text.size() > 0) {
    SetEnabledTextColorIds(foreground_frame_active_color_id_);
    // Required for text to be visible on hover
    label()->SetPaintToLayer();
    label()->SetSkipSubpixelRenderingOpacityCheck(true);
    label()->layer()->SetFillsBoundsOpaquely(false);
    label()->SetSubpixelRenderingEnabled(false);
  }
}

void TabStripControlButton::SetForegroundFrameActiveColorId(
    ui::ColorId new_color_id) {
  foreground_frame_active_color_id_ = new_color_id;
  UpdateColors();
}
void TabStripControlButton::SetForegroundFrameInactiveColorId(
    ui::ColorId new_color_id) {
  foreground_frame_inactive_color_id_ = new_color_id;
  UpdateColors();
}
void TabStripControlButton::SetBackgroundFrameActiveColorId(
    ui::ColorId new_color_id) {
  background_frame_active_color_id_ = new_color_id;
  UpdateColors();
}
void TabStripControlButton::SetBackgroundFrameInactiveColorId(
    ui::ColorId new_color_id) {
  background_frame_inactive_color_id_ = new_color_id;
  UpdateColors();
}

void TabStripControlButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = icon;
  UpdateIcon();
}

ui::ColorId TabStripControlButton::GetBackgroundColor() {
  return (GetWidget() && GetWidget()->ShouldPaintAsActive())
             ? background_frame_active_color_id_
             : background_frame_inactive_color_id_;
}

ui::ColorId TabStripControlButton::GetForegroundColor() {
  return (GetWidget() && GetWidget()->ShouldPaintAsActive())
             ? foreground_frame_active_color_id_
             : foreground_frame_inactive_color_id_;
}

void TabStripControlButton::UpdateIcon() {
  if (icon_->is_empty()) {
    return;
  }

  const ui::ImageModel icon_image_model = ui::ImageModel::FromVectorIcon(
      icon_.get(), GetForegroundColor(), kIconSize);

  SetImageModel(views::Button::STATE_NORMAL, icon_image_model);
  SetImageModel(views::Button::STATE_HOVERED, icon_image_model);
  SetImageModel(views::Button::STATE_PRESSED, icon_image_model);
}

void TabStripControlButton::UpdateInkDrop() {
  const auto* const color_provider = GetColorProvider();

  if (!color_provider) {
    return;
  }

  CreateToolbarInkdropCallbacks(this, kColorTabStripControlButtonInkDrop,
                                kColorTabStripControlButtonInkDropRipple);
}

void TabStripControlButton::UpdateColors() {
  const auto* const color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  SetEnabledTextColorIds(foreground_frame_active_color_id_);
  UpdateBackground();
  UpdateInkDrop();
  UpdateIcon();
  SchedulePaint();
}

void TabStripControlButton::UpdateBackground() {
  const auto* const color_provider = GetColorProvider();

  if (!color_provider) {
    return;
  }

  const std::optional<int> bg_id = tab_strip_controller_->GetCustomBackgroundId(
      BrowserFrameActiveState::kUseCurrent);

  // Paint the background as transparent for image based themes.
  if (bg_id.has_value() && paint_transparent_for_custom_image_theme_) {
    SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  } else {
    const float right_corner_radius =
        GetScaledCornerRadius(GetCornerRadius(), Edge::kRight);
    const float left_corner_radius =
        GetScaledCornerRadius(GetCornerRadius(), Edge::kLeft);
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainterWithVariableRadius(
            color_provider->GetColor(GetBackgroundColor()),
            gfx::RoundedCornersF(left_corner_radius, right_corner_radius,
                                 right_corner_radius, left_corner_radius),
            GetInsets())));
  }
}

int TabStripControlButton::GetCornerRadius() const {
  return TabStripControlButton::kButtonSize.width() / 2;
}

int TabStripControlButton::GetFlatCornerRadius() const {
  return 0;
}

float TabStripControlButton::GetScaledCornerRadius(float initial_radius,
                                                   Edge edge) const {
  const int flat_corner_radius = GetFlatCornerRadius();
  return flat_edge_ == edge
             ? ((initial_radius - flat_corner_radius) * flat_edge_factor_) +
                   flat_corner_radius
             : initial_radius;
}

void TabStripControlButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &TabStripControlButton::UpdateColors, base::Unretained(this)));
  UpdateColors();
}

void TabStripControlButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void TabStripControlButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();
  UpdateColors();
}

bool TabStripControlButton::GetHitTestMask(SkPath* mask) const {
  const bool extend_to_top = tab_strip_controller_->IsFrameCondensed();

  const SkScalar bottom_radius = GetCornerRadius();
  const SkScalar top_radius = extend_to_top ? 0.0f : bottom_radius;
  const SkScalar bottom_left_radius =
      GetScaledCornerRadius(bottom_radius, Edge::kLeft);
  const SkScalar bottom_right_radius =
      GetScaledCornerRadius(bottom_radius, Edge::kRight);
  const SkScalar top_left_radius =
      GetScaledCornerRadius(top_radius, Edge::kLeft);
  const SkScalar top_right_radius =
      GetScaledCornerRadius(top_radius, Edge::kRight);
  const SkScalar radii[8] = {top_left_radius,     top_left_radius,
                             top_right_radius,    top_right_radius,
                             bottom_right_radius, bottom_right_radius,
                             bottom_left_radius,  bottom_left_radius};

  gfx::Rect rect = GetContentsBounds();
  if (extend_to_top) {
    rect.SetVerticalBounds(0, rect.bottom());
  }

  mask->addRoundRect(gfx::RectToSkRect(rect), radii);

  return true;
}

gfx::Size TabStripControlButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = TabStripControlButton::kButtonSize;
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void TabStripControlButton::NotifyClick(const ui::Event& event) {
  LabelButton::NotifyClick(event);
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::ACTION_TRIGGERED);
}

void TabStripControlButton::SetFlatEdgeFactor(float factor) {
  flat_edge_factor_ = factor;
  UpdateBackground();
  // The ink drop doesn't automatically pick up on rounded corner changes, so
  // we need to manually notify it here.
  // TODO(crbug.com/332937585): Clean up once this is no longer necessary or
  // there is a better API for updating.
  views::InkDrop::Get(this)->GetInkDrop()->HostSizeChanged(size());
}

void TabStripControlButton::AnimateToStateForTesting(
    views::InkDropState state) {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(state);
}

BEGIN_METADATA(TabStripControlButton)
END_METADATA
