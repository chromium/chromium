// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_unfocus_button.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kIconSize = 16;
}  // namespace

TabGroupUnfocusButton::TabGroupUnfocusButton(TabStripOrientation orientation,
                                             base::RepeatingClosure callback)
    : views::LabelButton(
          std::move(callback),
          l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON)),
      orientation_(orientation) {
  SetAppearDisabledInInactiveWidget(false);
  SetProperty(views::kElementIdentifierKey, kUnfocusTabGroupButtonElementId);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  const gfx::Insets insets =
      orientation_ == TabStripOrientation::kHorizontal
          ? TabGroupStyle::GetInsetsForFocusedUnfocusChip()
          : gfx::Insets::VH(
                0, TabGroupStyle::GetInsetsForHeaderChip(orientation_).left());
  SetBorder(views::CreateEmptyBorder(insets));
  views::FocusRing::Install(this);
}

TabGroupUnfocusButton::~TabGroupUnfocusButton() = default;

gfx::Size TabGroupUnfocusButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::LabelButton::CalculatePreferredSize(available_size);
  if (orientation_ == TabStripOrientation::kHorizontal) {
    size.set_height(TabGroupStyle::GetFocusedChipHeight());
  }
  return size;
}

void TabGroupUnfocusButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::LabelButton::OnBoundsChanged(previous_bounds);
  UpdateCollapsedState();
}

void TabGroupUnfocusButton::SetGroupColorData(
    tab_groups::TabGroupColorId color_id,
    bool is_ephemeral) {
  color_id_ = color_id;
  is_ephemeral_ = is_ephemeral;
  UpdateColors();
}

void TabGroupUnfocusButton::UpdateColors() {
  if (!GetColorProvider()) {
    return;
  }
  const SkColor bg = GetBackgroundColor();
  const SkColor fg = GetForegroundColor();
  SetEnabledTextColors(fg);

  if (is_collapsed_) {
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kFilterTiltShiftIcon, fg, kIconSize));
  }

  const float radius = TabGroupStyle::GetFocusedChipCornerRadius(orientation_);
  constexpr float kFlatRadius = 2.0f;
  gfx::RoundedCornersF radii;
  if (orientation_ == TabStripOrientation::kVertical) {
    radii = gfx::RoundedCornersF(radius, radius, kFlatRadius, kFlatRadius);
  } else {
    radii =
        base::i18n::IsRTL()
            ? gfx::RoundedCornersF(kFlatRadius, radius, radius, kFlatRadius)
            : gfx::RoundedCornersF(radius, kFlatRadius, kFlatRadius, radius);
  }
  SetBackground(views::CreateRoundedRectBackground(bg, radii));
}

SkColor TabGroupUnfocusButton::GetBackgroundColor() const {
  if (!GetColorProvider()) {
    return gfx::kPlaceholderColor;
  }
  const bool frame_active = GetWidget() && GetWidget()->ShouldPaintAsActive();
  SkColor bg = is_ephemeral_
                   ? GetColorProvider()->GetColor(
                         frame_active ? ui::kColorSysOnHeaderDivider
                                      : ui::kColorSysOnHeaderDividerInactive)
                   : GetColorProvider()->GetColor(
                         GetTabGroupTabStripColorId(color_id_, frame_active));
  if (IsMouseHovered()) {
    const SkColor hover_ink =
        GetColorProvider()->GetColor(ui::kColorSysStateHoverOnSubtle);
    bg = color_utils::GetResultingPaintColor(hover_ink, bg);
  }
  return bg;
}

SkColor TabGroupUnfocusButton::GetForegroundColor() const {
  if (!GetColorProvider()) {
    return gfx::kPlaceholderColor;
  }
  const bool frame_active = GetWidget() && GetWidget()->ShouldPaintAsActive();
  if (is_ephemeral_) {
    return GetColorProvider()->GetColor(
        frame_active ? ui::kColorSysOnSurfacePrimary
                     : ui::kColorSysOnSurfacePrimaryInactive);
  }
  const SkColor bg = GetColorProvider()->GetColor(
      GetTabGroupTabStripColorId(color_id_, frame_active));
  return color_utils::GetColorWithMaxContrast(bg);
}

void TabGroupUnfocusButton::UpdateCollapsedState() {
  const int collapsed_width =
      tabs::kVerticalTabStripCollapsedWidth -
      2 * GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);
  const bool is_collapsed = orientation_ == TabStripOrientation::kVertical &&
                            width() > 0 && width() <= collapsed_width;
  if (is_collapsed_ == is_collapsed) {
    return;
  }
  is_collapsed_ = is_collapsed;
  UpdateTextAndIcon();
}

void TabGroupUnfocusButton::UpdateTextAndIcon() {
  if (is_collapsed_) {
    SetText(std::u16string());
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON));
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      kFilterTiltShiftIcon, GetForegroundColor(), kIconSize));
  } else {
    SetText(l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON));
    SetImageModel(views::Button::STATE_NORMAL, ui::ImageModel());
  }
}

void TabGroupUnfocusButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();
  UpdateColors();
}

void TabGroupUnfocusButton::OnMouseEntered(const ui::MouseEvent& event) {
  views::LabelButton::OnMouseEntered(event);
  UpdateColors();
}

void TabGroupUnfocusButton::OnMouseExited(const ui::MouseEvent& event) {
  views::LabelButton::OnMouseExited(event);
  UpdateColors();
}

void TabGroupUnfocusButton::AddedToWidget() {
  views::LabelButton::AddedToWidget();
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &TabGroupUnfocusButton::UpdateColors, base::Unretained(this)));
  UpdateColors();
}

void TabGroupUnfocusButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
  views::LabelButton::RemovedFromWidget();
}

BEGIN_METADATA(TabGroupUnfocusButton)
END_METADATA
