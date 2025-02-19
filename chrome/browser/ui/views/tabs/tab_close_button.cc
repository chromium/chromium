// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_close_button.h"

#include <map>
#include <memory>
#include <vector>

#include "base/hash/hash.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_class_properties.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace {
constexpr int kIconSize = 16;
constexpr gfx::Size kButtonSize = {28, 28};
}  // namespace

TabCloseButton::TabCloseButton(PressedCallback pressed_callback,
                               MouseEventCallback mouse_event_callback)
    : views::LabelButton(std::move(pressed_callback)),
      mouse_event_callback_(std::move(mouse_event_callback)) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetHighlightOpacity(0.16f);
  views::InkDrop::Get(this)->SetVisibleOpacity(0.14f);

  SetImageCentered(true);

  // Disable animation so that the hover indicator shows up immediately to help
  // avoid mis-clicks.
  SetAnimationDuration(base::TimeDelta());
  views::InkDrop::Get(this)->GetInkDrop()->SetHoverHighlightFadeDuration(
      base::TimeDelta());

  // The ink drop highlight path is the same as the focus ring highlight path,
  // but needs to be explicitly mirrored for RTL.
  // TODO(http://crbug.com/1056490): Make ink drops in RTL work the same way as
  // focus rings.
  auto ink_drop_highlight_path =
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets());
  ink_drop_highlight_path->set_use_contents_bounds(true);
  ink_drop_highlight_path->set_use_mirrored_rect(true);
  views::HighlightPathGenerator::Install(this,
                                         std::move(ink_drop_highlight_path));

  SetInstallFocusRingOnFocus(true);
  // TODO(http://crbug.com/1056490): Once this bug is solved and explicit
  // mirroring for ink drops is not needed, we can combine these two.
  auto ring_highlight_path =
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets());
  ring_highlight_path->set_use_contents_bounds(true);
  views::FocusRing::Get(this)->SetPathGenerator(std::move(ring_highlight_path));

  UpdateIcon();
}

TabCloseButton::~TabCloseButton() = default;

TabStyle::TabColors TabCloseButton::GetColors() const {
  return colors_;
}

void TabCloseButton::SetColors(TabStyle::TabColors colors) {
  if (colors == colors_) {
    return;
  }
  colors_ = std::move(colors);
  views::InkDrop::Get(this)->SetBaseColor(
      color_utils::GetColorWithMaxContrast(colors_.background_color));
  views::FocusRing::Get(this)->SetColorId(
      colors_.close_button_focus_ring_color);

  UpdateIcon();

  OnPropertyChanged(&colors_, views::kPropertyEffectsPaint);
}

views::View* TabCloseButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tab close button has no children, so tooltip handler should be the same
  // as the event handler. In addition, a hit test has to be performed for the
  // point (as GetTooltipHandlerForPoint() is responsible for it).
  if (!HitTestPoint(point)) {
    return nullptr;
  }
  return GetEventHandlerForPoint(point);
}

bool TabCloseButton::OnMousePressed(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);

  bool handled = LabelButton::OnMousePressed(event);
  // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
  // sees them.
  return !event.IsMiddleMouseButton() && handled;
}

void TabCloseButton::OnMouseReleased(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseReleased(event);
}

void TabCloseButton::OnMouseMoved(const ui::MouseEvent& event) {
  mouse_event_callback_.Run(this, event);
  Button::OnMouseMoved(event);
}

void TabCloseButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  LabelButton::OnGestureEvent(event);
  event->SetHandled();
}

gfx::Size TabCloseButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return kButtonSize;
}

views::View* TabCloseButton::TargetForRect(views::View* root,
                                           const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect)) {
    return ViewTargeterDelegate::TargetForRect(root, rect);
  }

  // Ignore the padding set on the button.
  gfx::Rect contents_bounds = GetMirroredRect(GetContentsBounds());

#if defined(USE_AURA)
  // Include the padding in hit-test for touch events.
  // TODO(pkasting): It seems like touch events would generate rects rather
  // than points and thus use the TargetForRect() call above.  If this is
  // reached, it may be from someone calling GetEventHandlerForPoint() while a
  // touch happens to be occurring.  In such a case, maybe we don't want this
  // code to run?  It's possible this block should be removed, or maybe this
  // whole function deleted.  Note that in these cases, we should probably
  // also remove the padding on the close button bounds (see Tab::Layout()), as
  // it will be pointless.
  if (aura::Env::GetInstance()->is_touch_down()) {
    contents_bounds = GetLocalBounds();
  }
#endif

  return contents_bounds.Intersects(rect) ? this : parent();
}

bool TabCloseButton::GetHitTestMask(SkPath* mask) const {
  // We need to define this so hit-testing won't include the border region.
  mask->addRect(gfx::RectToSkRect(GetMirroredRect(GetContentsBounds())));
  return true;
}
void TabCloseButton::UpdateIcon() {
  const auto& icon = kCloseTabChromeRefreshIcon;

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, colors_.foreground_color,
                                               kIconSize));
  SetImageModel(views::Button::STATE_HOVERED,
                ui::ImageModel::FromVectorIcon(icon, colors_.foreground_color,
                                               kIconSize));
  SetImageModel(views::Button::STATE_PRESSED,
                ui::ImageModel::FromVectorIcon(icon, colors_.foreground_color,
                                               kIconSize));
}

BEGIN_METADATA(TabCloseButton)
ADD_PROPERTY_METADATA(TabStyle::TabColors, Colors)
END_METADATA
