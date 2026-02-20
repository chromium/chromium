// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"

#include <optional>
#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/background.h"
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
    const SkScalar left_radius = control_button_->GetScaledCornerRadius(
        control_button_->GetLeftCornerRadius(), Edge::kLeft);
    const SkScalar right_radius = control_button_->GetScaledCornerRadius(
        control_button_->GetRightCornerRadius(), Edge::kRight);
    const SkVector radii[4] = {{left_radius,  left_radius},
                               {right_radius, right_radius},
                               {right_radius, right_radius},
                               {left_radius,  left_radius}};

    return SkPath::RRect(
        SkRRect::MakeRectRadii(gfx::RectToSkRect(rect), radii));
  }

 private:
  raw_ptr<TabStripControlButton> control_button_;
};
}  // namespace

const int TabStripControlButton::kIconSize = 16;
const gfx::Size TabStripControlButton::kButtonSize{28, 28};

TabStripControlButton::TabStripControlButton(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback callback,
    const gfx::VectorIcon& icon,
    Edge fixed_flat_edge,
    Edge animated_flat_edge)
    : TabStripControlButton(browser_window_interface,
                            std::move(callback),
                            icon,
                            std::u16string(),
                            fixed_flat_edge,
                            animated_flat_edge) {}

TabStripControlButton::TabStripControlButton(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback callback,
    const std::u16string& text,
    Edge fixed_flat_edge,
    Edge animated_flat_edge)
    : TabStripControlButton(browser_window_interface,
                            std::move(callback),
                            gfx::VectorIcon::EmptyIcon(),
                            text,
                            fixed_flat_edge,
                            animated_flat_edge) {}

TabStripControlButton::TabStripControlButton(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback callback,
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    Edge fixed_flat_edge,
    Edge animated_flat_edge)
    : views::LabelButton(std::move(callback), text),
      icon_(icon),
      fixed_flat_edge_(fixed_flat_edge),
      animated_flat_edge_(animated_flat_edge),
      browser_window_interface_(browser_window_interface) {
  SetImageCentered(true);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  foreground_frame_active_color_id_ = kColorTabForegroundInactiveFrameActive;
  foreground_frame_inactive_color_id_ =
      kColorNewTabButtonCRForegroundFrameInactive;
  background_frame_active_color_id_ = kColorNewTabButtonBackgroundFrameActive;
  background_frame_inactive_color_id_ =
      kColorNewTabButtonBackgroundFrameInactive;

  inkdrop_hover_color_id_ = kColorTabStripControlButtonInkDrop;
  inkdrop_ripple_color_id_ = kColorTabStripControlButtonInkDropRipple;

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
    SetEnabledTextColors(foreground_frame_active_color_id_);
    SetText(text);
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

void TabStripControlButton::SetInkdropHoverColorId(
    const ChromeColorIds new_color_id) {
  if (inkdrop_hover_color_id_ == new_color_id) {
    return;
  }
  inkdrop_hover_color_id_ = new_color_id;
  UpdateInkDrop();
}

void TabStripControlButton::SetInkdropRippleColorId(
    const ChromeColorIds new_color_id) {
  if (inkdrop_ripple_color_id_ == new_color_id) {
    return;
  }
  inkdrop_ripple_color_id_ = new_color_id;
  UpdateInkDrop();
}

void TabStripControlButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = icon;
  UpdateIcon();
}

void TabStripControlButton::SetText(std::u16string_view text) {
  label()->SetText(text);
  // Required for text to be visible on hover.
  // TODO(crbug.com/431015299): Fix text on hover and remove.
  label()->SetPaintToLayer();
  label()->SetSkipSubpixelRenderingOpacityCheck(true);
  label()->layer()->SetFillsBoundsOpaquely(false);
  label()->SetSubpixelRenderingEnabled(false);
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
  if (!color_provider || !IsWidgetAlive()) {
    return;
  }

  CreateToolbarInkdropCallbacks(this, inkdrop_hover_color_id_,
                                inkdrop_ripple_color_id_);
}

void TabStripControlButton::UpdateColors() {
  const auto* const color_provider = GetColorProvider();
  if (!color_provider || !IsWidgetAlive()) {
    return;
  }

  SetEnabledTextColors(foreground_frame_active_color_id_);
  UpdateBackground();
  UpdateInkDrop();
  UpdateIcon();
  SchedulePaint();
}

void TabStripControlButton::UpdateBackground() {
  const auto* const color_provider = GetColorProvider();
  if (!color_provider || !IsWidgetAlive()) {
    return;
  }

  BrowserFrameView* const browser_frame_view = GetBrowserFrameView();
  const std::optional<int> bg_id =
      browser_frame_view ? browser_frame_view->GetCustomBackgroundId(
                               BrowserFrameActiveState::kUseCurrent)
                         : std::nullopt;

  // Paint the background as transparent for image based themes.
  if (bg_id.has_value() && paint_transparent_for_custom_image_theme_) {
    SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  } else {
    const float right_corner_radius =
        GetScaledCornerRadius(GetRightCornerRadius(), Edge::kRight);
    const float left_corner_radius =
        GetScaledCornerRadius(GetLeftCornerRadius(), Edge::kLeft);
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
  if (fixed_flat_edge_ == edge) {
    return flat_corner_radius;
  } else if (animated_flat_edge_ == edge) {
    return ((initial_radius - flat_corner_radius) * flat_edge_factor_) +
           flat_corner_radius;
  } else {
    return initial_radius;
  }
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
  const bool extend_to_top = IsFrameCondensed();

  const SkScalar bottom_left_radius = GetLeftCornerRadius();
  const SkScalar bottom_right_radius = GetRightCornerRadius();
  const SkScalar top_left_radius = extend_to_top ? 0.0f : bottom_left_radius;
  const SkScalar top_right_radius = extend_to_top ? 0.0f : bottom_right_radius;

  const SkScalar scaled_bottom_left_radius =
      GetScaledCornerRadius(bottom_left_radius, Edge::kLeft);
  const SkScalar scaled_bottom_right_radius =
      GetScaledCornerRadius(bottom_right_radius, Edge::kRight);
  const SkScalar scaled_top_left_radius =
      GetScaledCornerRadius(top_left_radius, Edge::kLeft);
  const SkScalar scaled_top_right_radius =
      GetScaledCornerRadius(top_right_radius, Edge::kRight);
  const SkVector radii[4] = {
      {scaled_top_left_radius, scaled_top_left_radius},
      {scaled_top_right_radius, scaled_top_right_radius},
      {scaled_bottom_right_radius, scaled_bottom_right_radius},
      {scaled_bottom_left_radius, scaled_bottom_left_radius}};

  gfx::Rect rect = GetContentsBounds();
  if (extend_to_top) {
    rect.SetVerticalBounds(0, rect.bottom());
  }

  *mask = SkPath::RRect(SkRRect::MakeRectRadii(gfx::RectToSkRect(rect), radii));

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

bool TabStripControlButton::IsFrameCondensed() const {
  BrowserFrameView* const browser_frame_view = GetBrowserFrameView();
  return browser_frame_view ? browser_frame_view->IsFrameCondensed() : false;
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

bool TabStripControlButton::IsWidgetAlive() const {
  const views::Widget* widget = GetWidget();
  return widget && !widget->IsClosed();
}

BrowserFrameView* TabStripControlButton::GetBrowserFrameView() const {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface_);
  // 'browser_view' can be null during startup before the BrowserView is added
  // to a widget and is associated to `browser_window_interface_`
  if (!browser_view) {
    return nullptr;
  }

  return browser_view->browser_widget()->GetFrameView();
}

void TabStripControlButton::SetLeftRightCornerRadii(int left, int right) {
  left_corner_radius_ = left;
  right_corner_radius_ = right;
  UpdateBackground();
}

BEGIN_METADATA(TabStripControlButton)
END_METADATA
