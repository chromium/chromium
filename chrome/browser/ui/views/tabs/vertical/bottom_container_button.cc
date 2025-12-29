// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/bottom_container_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
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
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace {
class BottomContainerButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit BottomContainerButtonActionViewInterface(
      BottomContainerButton* action_view)
      : views::LabelButtonActionViewInterface(action_view),
        action_view_(action_view) {}
  ~BottomContainerButtonActionViewInterface() override = default;

  // views::LabelButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    // Calling ButtonActionViewInterface instead of
    // LabelButtonActionViewInterface to avoid the text of the button being set.
    ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    action_view_->SetImageModel(action_view_->GetState(),
                                action_item->GetImage());
  }

 private:
  raw_ptr<BottomContainerButton> action_view_;
};
}  // namespace

BottomContainerButton::BottomContainerButton() {
  ConfigureInkDropForToolbar(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                GetToolbarInkDropInsets(this), GetButtonCornerRadii()));
}

std::unique_ptr<views::ActionViewInterface>
BottomContainerButton::GetActionViewInterface() {
  return std::make_unique<BottomContainerButtonActionViewInterface>(this);
}

void BottomContainerButton::OnPaintBackground(gfx::Canvas* canvas) {
  const SkColor color = GetColorProvider()->GetColor(
      kColorVerticalTabStripBottomButtonBackground);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color);

  canvas->sk_canvas()->drawRRect(GetButtonShape(), flags);
}

bool BottomContainerButton::GetHitTestMask(SkPath* mask) const {
  *mask = SkPath::RRect(GetButtonShape());
  return true;
}

void BottomContainerButton::SetFlatEdge(FlatEdge flat_edge) {
  if (flat_edge_ == flat_edge) {
    return;
  }
  flat_edge_ = flat_edge;

  SetProperty(views::kHighlightPathGeneratorKey,
              std::make_unique<views::RoundRectHighlightPathGenerator>(
                  GetToolbarInkDropInsets(this), GetButtonCornerRadii()));

  SchedulePaint();
}

gfx::RoundedCornersF BottomContainerButton::GetButtonCornerRadii() const {
  int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);

  switch (flat_edge_) {
    case FlatEdge::kNone:
      return gfx::RoundedCornersF(radius, radius, radius, radius);
    case FlatEdge::kTop:
      return gfx::RoundedCornersF(0, 0, radius, radius);
    case FlatEdge::kBottom:
      return gfx::RoundedCornersF(radius, radius, 0, 0);
  }
}

SkRRect BottomContainerButton::GetButtonShape() const {
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

BEGIN_METADATA(BottomContainerButton)
END_METADATA
