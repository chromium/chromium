// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"

#include <memory>

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/permissions/chip/multi_image_container.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionChipView, kChipElementId);

PermissionChipView::PermissionChipView(PressedCallback callback)
    : MdTextButton(std::move(callback),
                   std::u16string(),
                   views::style::CONTEXT_BUTTON_MD,
                   /*use_text_color_for_icon=*/true,
                   std::make_unique<MultiImageContainer>()) {
  SetProperty(views::kElementIdentifierKey, kChipElementId);
  views::InstallPillHighlightPathGenerator(this);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Equalizing padding on the left, right and between icon and label.
  SetImageLabelSpacing(GetLayoutConstant(LOCATION_BAR_CHIP_PADDING));
  SetCustomPadding(GetPadding());
  if (features::IsChromeRefresh2023()) {
    label()->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);
  }
  SetCornerRadius(GetCornerRadius());
  animation_ = std::make_unique<gfx::SlideAnimation>(this);

  UpdateIconAndColors();
}

PermissionChipView::~PermissionChipView() = default;

void PermissionChipView::VisibilityChanged(views::View* starting_from,
                                           bool is_visible) {
  for (Observer& observer : observers_) {
    observer.OnChipVisibilityChanged(is_visible);
  }
}

void PermissionChipView::AnimateCollapse(base::TimeDelta duration) {
  base_width_ = 0;
  animation_->SetSlideDuration(duration);
  ForceAnimateCollapse();
}

void PermissionChipView::AnimateExpand(base::TimeDelta duration) {
  base_width_ = 0;
  animation_->SetSlideDuration(duration);
  ForceAnimateExpand();
}

void PermissionChipView::AnimateToFit(base::TimeDelta duration) {
  animation_->SetSlideDuration(duration);
  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    base_width_ = label()->GetPreferredSize().width();
  } else {
    base_width_ = label()->width();
  }

  if (label()->GetPreferredSize().width() < width()) {
    // As we're collapsing, we need to make sure that the padding is not
    // animated away.
    base_width_ += GetPadding().width();
    ForceAnimateCollapse();
  } else {
    ForceAnimateExpand();
  }
}

void PermissionChipView::ResetAnimation(double value) {
  animation_->Reset(value);
  OnAnimationValueMaybeChanged();
}

gfx::Size PermissionChipView::CalculatePreferredSize() const {
  const int icon_width = GetIconViewWidth();
  const int label_width =
      label()->GetPreferredSize().width() + GetPadding().width();

  const int width =
      base_width_ +
      base::ClampRound(label_width * animation_->GetCurrentValue()) +
      icon_width;

  return gfx::Size(width, GetHeightForWidth(width));
}

void PermissionChipView::OnThemeChanged() {
  MdTextButton::OnThemeChanged();
  UpdateIconAndColors();
}

void PermissionChipView::UpdateBackgroundColor() {
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainterWithVariableRadius(
            GetBackgroundColor(), GetCornerRadii())));
}

void PermissionChipView::AnimationEnded(const gfx::Animation* animation) {
  if (animation != animation_.get()) {
    return;
  }

  OnAnimationValueMaybeChanged();

  const double value = animation_->GetCurrentValue();
  if (value == 1.0) {
    for (Observer& observer : observers_) {
      observer.OnExpandAnimationEnded();
    }
  } else if (value == 0.0) {
    for (Observer& observer : observers_) {
      observer.OnCollapseAnimationEnded();
    }
  }
}

void PermissionChipView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != animation_.get()) {
    return;
  }

  OnAnimationValueMaybeChanged();
  PreferredSizeChanged();
}

void PermissionChipView::SetUserDecision(
    permissions::PermissionAction user_decision) {
  user_decision_ = user_decision;
  UpdateIconAndColors();
}

void PermissionChipView::SetTheme(PermissionChipTheme theme) {
  theme_ = theme;
  UpdateIconAndColors();
}

void PermissionChipView::SetBlockedIconShowing(bool should_show_blocked_icon) {
  should_show_blocked_icon_ = should_show_blocked_icon;
  UpdateIconAndColors();
}

void PermissionChipView::SetPermissionPromptStyle(
    PermissionPromptStyle prompt_style) {
  prompt_style_ = prompt_style;
  UpdateIconAndColors();
}

void PermissionChipView::SetMessage(std::u16string message) {
  SetText(message);
  UpdateIconAndColors();
}

MultiImageContainer* PermissionChipView::multi_image_container() {
  return static_cast<MultiImageContainer*>(image_container());
}

ui::ImageModel PermissionChipView::GetIconImageModel() const {
  return ui::ImageModel::FromVectorIcon(GetIcon(), GetForegroundColor(),
                                        GetIconSize(), nullptr);
}

const gfx::VectorIcon& PermissionChipView::GetIcon() const {
  if (icon_) {
    return const_cast<decltype(*icon_)>(*icon_);
  }

  return gfx::kNoneIcon;
}

SkColor PermissionChipView::GetForegroundColor() const {
  if (GetPermissionChipTheme() ==
      PermissionChipTheme::kInUseActivityIndicator) {
    return GetColorProvider()->GetColor(
        kColorOmniboxChipInUseActivityIndicatorForeground);
  }

  if (GetPermissionChipTheme() ==
      PermissionChipTheme::kBlockedActivityIndicator) {
    return GetColorProvider()->GetColor(
        kColorOmniboxChipBlockedActivityIndicatorForeground);
  }

  if (features::IsChromeRefresh2023()) {
    // 1. Default to the system primary color.
    SkColor text_and_icon_color = GetColorProvider()->GetColor(
        kColorOmniboxChipForegroundNormalVisibility);

    // 2. Then update the color if the quiet chip is showing.
    if (GetPermissionPromptStyle() == PermissionPromptStyle::kQuietChip) {
      text_and_icon_color = GetColorProvider()->GetColor(
          kColorOmniboxChipForegroundLowVisibility);
    }

    // 3. Then update the color based on the user decision.
    // TODO(dljames): There is potentially a bug here if there exists a case
    // where a quiet chip can be shown on a GRANTED_ONCE permission action.
    // In that case the color should stay kColorOmniboxChipTextDefaultCR23.
    switch (GetUserDecision()) {
      case permissions::PermissionAction::GRANTED:
      case permissions::PermissionAction::GRANTED_ONCE:
        text_and_icon_color = GetColorProvider()->GetColor(
            kColorOmniboxChipForegroundNormalVisibility);
        break;
      case permissions::PermissionAction::DENIED:
      case permissions::PermissionAction::DISMISSED:
      case permissions::PermissionAction::IGNORED:
      case permissions::PermissionAction::REVOKED:
        text_and_icon_color = GetColorProvider()->GetColor(
            kColorOmniboxChipForegroundLowVisibility);
        break;
      case permissions::PermissionAction::NUM:
        break;
    }

    // 4. Then update the color based on if the icon is blocked or not.
    if (ShouldShowBlockedIcon()) {
      text_and_icon_color = GetColorProvider()->GetColor(
          kColorOmniboxChipForegroundLowVisibility);
    }

    return text_and_icon_color;
  }

  return GetColorProvider()->GetColor(
      GetPermissionChipTheme() == PermissionChipTheme::kLowVisibility
          ? kColorOmniboxChipForegroundLowVisibility
          : kColorOmniboxChipForegroundNormalVisibility);
}

SkColor PermissionChipView::GetBackgroundColor() const {
  if (GetPermissionChipTheme() ==
      PermissionChipTheme::kInUseActivityIndicator) {
    return GetColorProvider()->GetColor(
        kColorOmniboxChipInUseActivityIndicatorBackground);
  }

  if (GetPermissionChipTheme() ==
      PermissionChipTheme::kBlockedActivityIndicator) {
    return GetColorProvider()->GetColor(
        kColorOmniboxChipBlockedActivityIndicatorBackground);
  }

  return GetColorProvider()->GetColor(kColorOmniboxChipBackground);
}

void PermissionChipView::UpdateIconAndColors() {
  if (!GetWidget()) {
    return;
  }
  SetEnabledTextColors(GetForegroundColor());
  SetImageModel(views::Button::STATE_NORMAL, GetIconImageModel());
  if (features::IsChromeRefresh2023()) {
    ConfigureInkDropForRefresh2023(this, kColorOmniboxChipInkDropHover,
                                   kColorOmniboxChipInkDropRipple);
  }
}

void PermissionChipView::ForceAnimateExpand() {
  ResetAnimation(0.0);
  animation_->Show();
}

void PermissionChipView::ForceAnimateCollapse() {
  ResetAnimation(1.0);
  animation_->Hide();
}

void PermissionChipView::OnAnimationValueMaybeChanged() {
  fully_collapsed_ = animation_->GetCurrentValue() == 0.0;
}

int PermissionChipView::GetIconSize() const {
  if (features::IsChromeRefresh2023()) {
    return GetLayoutConstant(LOCATION_BAR_CHIP_ICON_SIZE);
  }

  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

int PermissionChipView::GetCornerRadius() const {
  if (features::IsChromeRefresh2023()) {
    return GetLayoutConstant(LOCATION_BAR_CHILD_CORNER_RADIUS);
  }
  return GetIconSize();
}

gfx::RoundedCornersF PermissionChipView::GetCornerRadii() const {
  const int leading_radius = GetCornerRadius();
  // If the chips' divider is visible, the left/trailing side of the request
  // chip should be rectangular.
  const int trailing_radius = is_divider_visible_ ? 0 : leading_radius;

  return gfx::RoundedCornersF(trailing_radius, leading_radius, leading_radius,
                              trailing_radius);
}

gfx::Insets PermissionChipView::GetPadding() const {
  if (features::IsChromeRefresh2023()) {
    return gfx::Insets(GetLayoutConstant(LOCATION_BAR_CHIP_PADDING));
  } else {
    return gfx::Insets::VH(
        GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING),
        GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left());
  }
}

void PermissionChipView::SetChipIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;

  UpdateIconAndColors();
}

void PermissionChipView::SetChipIcon(const gfx::VectorIcon* icon) {
  icon_ = icon;

  UpdateIconAndColors();
}

void PermissionChipView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PermissionChipView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PermissionChipView::UpdateForDividerVisibility(bool is_divider_visible,
                                                    int divider_arc_width) {
  is_divider_visible_ = is_divider_visible;

  UpdateBackgroundColor();

  // The request chip should move under the divider arc if the divider is
  // visible.
  gfx::Insets margin = is_divider_visible
                           ? gfx::Insets::TLBR(0, -divider_arc_width, 0, 0)
                           : gfx::Insets();
  SetProperty(views::kMarginsKey, margin);

  gfx::Insets padding = GetPadding();
  if (is_divider_visible) {
    // Set a left padding to move the request chip's icon to the right.
    padding += gfx::Insets::TLBR(0, divider_arc_width, 0, 0);
  }
  SetCustomPadding(padding);

  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), GetCornerRadii()));
}

int PermissionChipView::GetIconViewWidth() const {
  return GetIconSize() + GetInsets().width();
}

BEGIN_METADATA(PermissionChipView)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
