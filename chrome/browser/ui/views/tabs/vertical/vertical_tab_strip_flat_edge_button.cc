// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_flat_edge_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/actions/action_view_interface.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace {
class VerticalTabStripFlatEdgeButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit VerticalTabStripFlatEdgeButtonActionViewInterface(
      VerticalTabStripFlatEdgeButton* action_view)
      : views::LabelButtonActionViewInterface(action_view),
        action_view_(action_view) {}
  ~VerticalTabStripFlatEdgeButtonActionViewInterface() override = default;

  // views::LabelButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    // Calling ButtonActionViewInterface instead of
    // LabelButtonActionViewInterface to avoid the text of the button being set.
    ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    if (action_item->GetImage().IsVectorIcon()) {
      action_view_->UpdateIcon(action_item->GetImage());
    }
  }

 private:
  raw_ptr<VerticalTabStripFlatEdgeButton> action_view_ = nullptr;
};
}  // namespace

VerticalTabStripFlatEdgeButton::VerticalTabStripFlatEdgeButton() {
  ConfigureInkDropForToolbar(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                GetToolbarInkDropInsets(this), GetButtonCornerRadii()));
}

std::unique_ptr<views::ActionViewInterface>
VerticalTabStripFlatEdgeButton::GetActionViewInterface() {
  return std::make_unique<VerticalTabStripFlatEdgeButtonActionViewInterface>(
      this);
}

void VerticalTabStripFlatEdgeButton::UpdateIcon(
    const ui::ImageModel& icon_image) {
  CHECK(icon_image.IsVectorIcon());

  const ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
      *icon_image.GetVectorIcon().vector_icon(), GetForegroundColor(),
      GetLayoutConstant(LayoutConstant::kVerticalTabStripBottomButtonIconSize));

  SetImageModel(views::Button::STATE_NORMAL, image_model);
  SetImageModel(views::Button::STATE_HOVERED, image_model);
  SetImageModel(views::Button::STATE_PRESSED, image_model);
  SetImageModel(views::Button::STATE_DISABLED, image_model);
}

void VerticalTabStripFlatEdgeButton::SetInsets(const gfx::Insets& insets) {
  std::unique_ptr<views::LabelButtonBorder> border = CreateDefaultBorder();
  border->set_insets(insets);
  SetBorder(std::move(border));
}

void VerticalTabStripFlatEdgeButton::OnPaintBackground(gfx::Canvas* canvas) {
  const SkColor color = GetColorProvider()->GetColor(
      kColorVerticalTabStripBottomButtonBackground);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color);

  canvas->sk_canvas()->drawRRect(GetButtonShape(), flags);
}

bool VerticalTabStripFlatEdgeButton::GetHitTestMask(SkPath* mask) const {
  *mask = SkPath::RRect(GetButtonShape());
  return true;
}

void VerticalTabStripFlatEdgeButton::SetFlatEdge(FlatEdge flat_edge) {
  if (flat_edge_ == flat_edge) {
    return;
  }
  flat_edge_ = flat_edge;

  SetProperty(views::kHighlightPathGeneratorKey,
              std::make_unique<views::RoundRectHighlightPathGenerator>(
                  GetToolbarInkDropInsets(this), GetButtonCornerRadii()));

  SchedulePaint();
}

void VerticalTabStripFlatEdgeButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &View::NotifyViewControllerCallback, base::Unretained(this)));
}

void VerticalTabStripFlatEdgeButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

ui::ColorId VerticalTabStripFlatEdgeButton::GetForegroundColor() const {
  return GetWidget() && GetWidget()->ShouldPaintAsActive()
             ? kColorNewTabButtonCRForegroundFrameActive
             : kColorNewTabButtonCRForegroundFrameInactive;
}

gfx::RoundedCornersF VerticalTabStripFlatEdgeButton::GetButtonCornerRadii()
    const {
  int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);

  switch (flat_edge_) {
    case FlatEdge::kNone:
      return gfx::RoundedCornersF(radius, radius, radius, radius);
    case FlatEdge::kTop:
      return gfx::RoundedCornersF(0, 0, radius, radius);
    case FlatEdge::kLeft:
      return gfx::RoundedCornersF(0, radius, radius, 0);
    case FlatEdge::kBottom:
      return gfx::RoundedCornersF(radius, radius, 0, 0);
    case FlatEdge::kRight:
      return gfx::RoundedCornersF(radius, 0, 0, radius);
  }
}

SkRRect VerticalTabStripFlatEdgeButton::GetButtonShape() const {
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

BEGIN_METADATA(VerticalTabStripFlatEdgeButton)
END_METADATA
