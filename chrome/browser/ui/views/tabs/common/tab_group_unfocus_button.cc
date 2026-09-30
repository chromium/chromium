// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_unfocus_button.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

TabGroupUnfocusButton::TabGroupUnfocusButton(TabStripOrientation orientation,
                                             base::RepeatingClosure callback)
    : views::LabelButton(
          std::move(callback),
          l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON)),
      orientation_(orientation) {
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
  const bool frame_active = GetWidget() && GetWidget()->ShouldPaintAsActive();
  SkColor bg;
  SkColor fg;
  if (is_ephemeral_) {
    bg = GetColorProvider()->GetColor(
        frame_active ? ui::kColorSysOnHeaderDivider
                     : ui::kColorSysOnHeaderDividerInactive);
    fg = GetColorProvider()->GetColor(
        frame_active ? ui::kColorSysOnSurfacePrimary
                     : ui::kColorSysOnSurfacePrimaryInactive);
  } else {
    bg = GetColorProvider()->GetColor(
        GetTabGroupTabStripColorId(color_id_, frame_active));
    fg = color_utils::GetColorWithMaxContrast(bg);
  }
  if (IsMouseHovered()) {
    const SkColor hover_ink =
        GetColorProvider()->GetColor(kColorTabStripControlButtonInkDrop);
    bg = color_utils::GetResultingPaintColor(hover_ink, bg);
  }
  SetEnabledTextColors(fg);

  const float radius = TabGroupStyle::GetChipCornerRadius(orientation_);
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
