// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/actions/action_view_interface.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kButtonWithLabelPadding = 5;

class TabStripFlatEdgeButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit TabStripFlatEdgeButtonActionViewInterface(
      TabStripFlatEdgeButton* action_view)
      : views::LabelButtonActionViewInterface(action_view),
        action_view_(action_view) {}
  ~TabStripFlatEdgeButtonActionViewInterface() override = default;

  // views::LabelButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    // Calling ButtonActionViewInterface instead of
    // LabelButtonActionViewInterface to avoid the text of the button being set
    // by default. We will set it manually if there is space.
    ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    action_view_->SetLabelText(std::u16string(action_item->GetText()));

    if (action_item->GetImage().IsVectorIcon()) {
      action_view_->UpdateIcon(action_item->GetImage());
    }
  }

  void OnViewChangedImpl(actions::ActionItem* action_item) override {
    ButtonActionViewInterface::OnViewChangedImpl(action_item);

    if (action_item->GetImage().IsVectorIcon()) {
      action_view_->UpdateIcon(action_item->GetImage());
    }
  }

  void InvokeActionImpl(actions::ActionItem* action_item) override {
    action_view_->NotifyWillInvokeAction();
    LabelButtonActionViewInterface::InvokeActionImpl(action_item);
  }

 private:
  raw_ptr<TabStripFlatEdgeButton> action_view_ = nullptr;
};
}  // namespace

TabStripFlatEdgeButton::TabStripFlatEdgeButton() {
  ConfigureInkDropForToolbar(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), GetButtonCornerRadii()));
  ConfigureToolbarInkdropForRefresh2023(
      this, kColorTabStripControlButtonInkDrop,
      kColorTabStripControlButtonInkDropRipple);
  SetIconSize(
      GetLayoutConstant(LayoutConstant::kVerticalTabStripComboButtonIconSize));
  SetImageLabelSpacing(kButtonWithLabelPadding);
  SetInsets(gfx::Insets());
}

TabStripFlatEdgeButton::~TabStripFlatEdgeButton() = default;

gfx::Size TabStripFlatEdgeButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int raw_button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  gfx::Size size(raw_button_size, raw_button_size);

  if (expansion_orientation_ == views::LayoutOrientation::kHorizontal) {
    size.set_width(size.width() * expansion_factor_);
  } else {
    size.set_height(size.height() * expansion_factor_);
  }
  return size;
}

std::unique_ptr<views::ActionViewInterface>
TabStripFlatEdgeButton::GetActionViewInterface() {
  return std::make_unique<TabStripFlatEdgeButtonActionViewInterface>(this);
}

void TabStripFlatEdgeButton::UpdateIcon(const ui::ImageModel& icon_image) {
  CHECK(icon_image.IsVectorIcon());

  const ui::ImageModel image_model =
      ui::ImageModel::FromVectorIcon(*icon_image.GetVectorIcon().vector_icon(),
                                     GetForegroundColor(), icon_size_);

  SetImageModel(views::Button::STATE_NORMAL, image_model);
  SetImageModel(views::Button::STATE_HOVERED, image_model);
  SetImageModel(views::Button::STATE_PRESSED, image_model);
  SetImageModel(views::Button::STATE_DISABLED, image_model);
}

void TabStripFlatEdgeButton::SetInsets(const gfx::Insets& insets) {
  std::unique_ptr<views::LabelButtonBorder> border = CreateDefaultBorder();
  border->set_insets(insets);
  SetBorder(std::move(border));
}

void TabStripFlatEdgeButton::SetIconOpacity(float opacity) {
  if (!image_container_view()->layer()) {
    image_container_view()->SetPaintToLayer();
    image_container_view()->layer()->SetFillsBoundsOpaquely(false);
  }
  image_container_view()->layer()->SetOpacity(opacity);
}

void TabStripFlatEdgeButton::SetExpansionFactor(float factor) {
  if (expansion_factor_ == factor) {
    return;
  }
  expansion_factor_ = factor;
  PreferredSizeChanged();
}

void TabStripFlatEdgeButton::SetExpansionOrientation(
    views::LayoutOrientation orientation) {
  if (expansion_orientation_ == orientation) {
    return;
  }
  expansion_orientation_ = orientation;
  PreferredSizeChanged();
}

void TabStripFlatEdgeButton::SetFlatEdgeFactor(float factor) {
  if (flat_edge_factor_ == factor) {
    return;
  }
  flat_edge_factor_ = factor;

  SetProperty(views::kHighlightPathGeneratorKey,
              std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), GetButtonCornerRadii()));
  // The ink drop doesn't automatically pick up on rounded corner changes, so │
  // we need to manually notify it here.
  views::InkDrop::Get(this)->GetInkDrop()->HostSizeChanged(size());

  SchedulePaint();
}

void TabStripFlatEdgeButton::SetShouldShowLabel(bool show_label) {
  should_show_label_ = show_label;
  if (should_show_label_) {
    label()->SetPaintToLayer();
    label()->SetSkipSubpixelRenderingOpacityCheck(true);
    label()->layer()->SetFillsBoundsOpaquely(false);
    label()->SetSubpixelRenderingEnabled(false);
  }
}

base::CallbackListSubscription
TabStripFlatEdgeButton::RegisterWillInvokeActionCallback(
    base::RepeatingClosure callback) {
  return will_invoke_action_callback_list_.Add(std::move(callback));
}

void TabStripFlatEdgeButton::NotifyWillInvokeAction() {
  will_invoke_action_callback_list_.Notify();
}

void TabStripFlatEdgeButton::OnPaintBackground(gfx::Canvas* canvas) {
  const SkColor color = GetColorProvider()->GetColor(GetBackgroundColor());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color);

  canvas->sk_canvas()->drawRRect(GetButtonShape(), flags);
}

void TabStripFlatEdgeButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();
  const std::optional<ui::ImageModel>& model =
      GetImageModel(views::Button::STATE_NORMAL);
  if (model && model->IsVectorIcon()) {
    UpdateIcon(*model);
  }
  if (should_show_label_) {
    UpdateLabelColor();
  }
}

bool TabStripFlatEdgeButton::GetHitTestMask(SkPath* mask) const {
  *mask = SkPath::RRect(GetButtonShape());
  return true;
}

void TabStripFlatEdgeButton::SetFlatEdge(FlatEdge flat_edge) {
  if (flat_edge_ == flat_edge) {
    return;
  }
  flat_edge_ = flat_edge;

  SetProperty(views::kHighlightPathGeneratorKey,
              std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), GetButtonCornerRadii()));

  SchedulePaint();
}

void TabStripFlatEdgeButton::SetIconSize(int icon_size) {
  if (icon_size_ == icon_size) {
    return;
  }
  icon_size_ = icon_size;

  const std::optional<ui::ImageModel>& model =
      GetImageModel(views::Button::STATE_NORMAL);
  if (model && model->IsVectorIcon()) {
    UpdateIcon(*model);
  }
}

void TabStripFlatEdgeButton::SetLabelText(const std::u16string& text) {
  if (label_text_ == text) {
    return;
  }
  label_text_ = text;
  UpdateLabel(width() > CalculatePreferredSize({}).width());
}

void TabStripFlatEdgeButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateLabel(width() > CalculatePreferredSize({}).width());
}

void TabStripFlatEdgeButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &TabStripFlatEdgeButton::OnThemeChanged, base::Unretained(this)));
}

void TabStripFlatEdgeButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

ui::ColorId TabStripFlatEdgeButton::GetForegroundColor() const {
  return GetWidget() && GetWidget()->ShouldPaintAsActive()
             ? kColorTabSearchButtonCRForegroundFrameActive
             : kColorTabSearchButtonCRForegroundFrameInactive;
}

ui::ColorId TabStripFlatEdgeButton::GetBackgroundColor() const {
  return GetWidget() && GetWidget()->ShouldPaintAsActive()
             ? kColorNewTabButtonCRBackgroundFrameActive
             : kColorNewTabButtonCRBackgroundFrameInactive;
}

gfx::RoundedCornersF TabStripFlatEdgeButton::GetButtonCornerRadii() const {
  constexpr float kFlatRadius = 2.0f;
  constexpr float kRoundedRadius = 10.0f;
  float flat_radius = kFlatRadius + ((kRoundedRadius - kFlatRadius) *
                                     (1.0f - flat_edge_factor_));

  switch (flat_edge_) {
    case FlatEdge::kNone:
      return gfx::RoundedCornersF(kRoundedRadius, kRoundedRadius,
                                  kRoundedRadius, kRoundedRadius);
    case FlatEdge::kTop:
      return gfx::RoundedCornersF(flat_radius, flat_radius, kRoundedRadius,
                                  kRoundedRadius);
    case FlatEdge::kLeft:
      return gfx::RoundedCornersF(flat_radius, kRoundedRadius, kRoundedRadius,
                                  flat_radius);
    case FlatEdge::kBottom:
      return gfx::RoundedCornersF(kRoundedRadius, kRoundedRadius, flat_radius,
                                  flat_radius);
    case FlatEdge::kRight:
      return gfx::RoundedCornersF(kRoundedRadius, flat_radius, flat_radius,
                                  kRoundedRadius);
  }
}

SkRRect TabStripFlatEdgeButton::GetButtonShape() const {
  const gfx::RoundedCornersF corners = GetButtonCornerRadii();
  const SkRect rect = gfx::RectToSkRect(GetLocalBounds());

  SkVector radii[4];
  radii[0] = {corners.upper_left(), corners.upper_left()};
  radii[1] = {corners.upper_right(), corners.upper_right()};
  radii[2] = {corners.lower_right(), corners.lower_right()};
  radii[3] = {corners.lower_left(), corners.lower_left()};

  SkRRect rrect;
  rrect.setRectRadii(rect, radii);
  return rrect;
}

void TabStripFlatEdgeButton::UpdateLabel(bool should_show) {
  if (!should_show_label_) {
    return;
  }
  if (should_show == GetText().empty()) {
    SetHorizontalAlignment(should_show ? gfx::ALIGN_LEFT : gfx::ALIGN_CENTER);
    SetInsets(should_show ? gfx::Insets::VH(0, kButtonWithLabelPadding)
                          : gfx::Insets());
  }
  auto label = should_show ? label_text_ : std::u16string();
  if (label != GetText()) {
    SetText(label);
  }
  if (should_show) {
    UpdateLabelColor();
  }
}

void TabStripFlatEdgeButton::UpdateLabelColor() {
  const ui::ColorId color_id = GetForegroundColor();
  SetTextColor(views::Button::STATE_NORMAL, color_id);
  SetTextColor(views::Button::STATE_HOVERED, color_id);
  SetTextColor(views::Button::STATE_PRESSED, color_id);
  SetTextColor(views::Button::STATE_DISABLED, color_id);
}

BEGIN_METADATA(TabStripFlatEdgeButton)
END_METADATA
