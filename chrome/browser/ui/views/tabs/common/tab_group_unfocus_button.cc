// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_unfocus_button.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_group.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kIconSize = 16;
}  // namespace

TabGroupUnfocusButton::TabGroupUnfocusButton(Delegate& delegate,
                                             TabStripOrientation orientation)
    : views::LabelButton(
          base::BindRepeating(&Delegate::ExitFocusMode,
                              base::Unretained(&delegate)),
          l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON)),
      delegate_(delegate),
      orientation_(orientation) {
  SetAppearDisabledInInactiveWidget(false);
  SetProperty(views::kElementIdentifierKey, kUnfocusTabGroupButtonElementId);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label()->SetElideBehavior(gfx::NO_ELIDE);
  UpdateBorder();
  views::FocusRing::Install(this);
  UpdateAccessibleName();
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

void TabGroupUnfocusButton::SetFocusAnimationState(
    const TabGroupFocusAnimationState& state) {
  if (focus_animation_state_ == state) {
    return;
  }
  const bool offset_changed =
      focus_animation_state_.content_horizontal_offset !=
      state.content_horizontal_offset;
  focus_animation_state_ = state;
  if (offset_changed) {
    UpdateBorder();
  }
  UpdateColors();
}

void TabGroupUnfocusButton::UpdateBorder() {
  gfx::Insets insets =
      orientation_ == TabStripOrientation::kHorizontal
          ? TabGroupStyle::GetInsetsForFocusedUnfocusChip()
          : gfx::Insets::VH(
                0, TabGroupStyle::GetInsetsForHeaderChip(orientation_).left());
  if (orientation_ == TabStripOrientation::kVertical && !is_collapsed_ &&
      focus_animation_state_.content_horizontal_offset != 0) {
    insets.set_left(std::max(
        0, insets.left() + focus_animation_state_.content_horizontal_offset));
  }
  SetBorder(views::CreateEmptyBorder(insets));
}

void TabGroupUnfocusButton::UpdateColors() {
  if (!GetColorProvider()) {
    return;
  }
  const SkColor text_color = GetTextColor();
  SetEnabledTextColors(text_color);

  if (is_collapsed_) {
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      kFilterTiltShiftIcon,
                      focus_animation_state_.blend_color_id.has_value()
                          ? GetForegroundColor()
                          : text_color,
                      kIconSize));
  }

  SetBackground(views::CreateRoundedRectBackground(
      GetBackgroundColor(),
      TabGroupStyle::GetUnfocusButtonCornerRadii(
          orientation_, focus_animation_state_.progress)));
}

SkColor TabGroupUnfocusButton::GetBaseBackgroundColor() const {
  if (!GetColorProvider()) {
    return gfx::kPlaceholderColor;
  }
  const bool frame_active = GetWidget() && GetWidget()->ShouldPaintAsActive();
  SkColor bg_color =
      is_ephemeral_
          ? GetColorProvider()->GetColor(
                frame_active
                    ? kColorTabGroupEphemeralHeaderBackgroundFrameActive
                    : kColorTabGroupEphemeralHeaderBackgroundFrameInactive)
          : GetColorProvider()->GetColor(
                GetTabGroupTabStripColorId(color_id_, frame_active));
  if (!is_ephemeral_ && focus_animation_state_.blend_color_id.has_value() &&
      focus_animation_state_.color_blend > 0.0f) {
    const SkColor target_bg_color =
        GetColorProvider()->GetColor(GetTabGroupTabStripColorId(
            *focus_animation_state_.blend_color_id, frame_active));
    bg_color = color_utils::AlphaBlend(target_bg_color, bg_color,
                                       focus_animation_state_.color_blend);
  }
  return bg_color;
}

SkColor TabGroupUnfocusButton::GetBackgroundColor() const {
  if (!GetColorProvider()) {
    return gfx::kPlaceholderColor;
  }
  SkColor bg_color = GetBaseBackgroundColor();
  if (IsMouseHovered()) {
    const SkColor hover_ink =
        GetColorProvider()->GetColor(ui::kColorSysStateHoverOnSubtle);
    bg_color = color_utils::GetResultingPaintColor(hover_ink, bg_color);
  }
  return bg_color;
}

SkColor TabGroupUnfocusButton::GetForegroundColor() const {
  if (!GetColorProvider()) {
    return gfx::kPlaceholderColor;
  }
  if (is_ephemeral_) {
    const bool frame_active = GetWidget() && GetWidget()->ShouldPaintAsActive();
    return GetColorProvider()->GetColor(
        frame_active ? kColorTabGroupEphemeralHeaderForegroundFrameActive
                     : kColorTabGroupEphemeralHeaderForegroundFrameInactive);
  }
  return color_utils::GetColorWithMaxContrast(GetBaseBackgroundColor());
}

SkColor TabGroupUnfocusButton::GetTextColor() const {
  const SkColor fg_color = GetForegroundColor();
  const float content_opacity = focus_animation_state_.content_opacity;
  if (content_opacity >= 1.0f) {
    return fg_color;
  }
  return SkColorSetA(fg_color, base::ClampRound<U8CPU>(SkColorGetA(fg_color) *
                                                       content_opacity));
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
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON));
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(kFilterTiltShiftIcon,
                                                 GetTextColor(), kIconSize));
  } else {
    SetText(l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON));
    SetTooltipText(std::u16string());
    SetImageModel(views::Button::STATE_NORMAL, ui::ImageModel());
  }
  // SetText() resets the accessible name, so re-apply it.
  UpdateAccessibleName();
}

void TabGroupUnfocusButton::UpdateAccessibleName() {
  if (!delegate_->IsValid()) {
    return;
  }
  GetViewAccessibility().SetName(
      delegate_->GetTabGroup().is_ephemeral()
          ? l10n_util::GetPluralStringFUTF16(
                IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON_EPHEMERAL_AX_LABEL,
                delegate_->GetTabGroup().tab_count())
          : l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_UNFOCUS_BUTTON_AX_LABEL));
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
