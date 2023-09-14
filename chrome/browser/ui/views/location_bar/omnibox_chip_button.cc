// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
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

namespace {

// Padding between chip's icon and label.
constexpr int kChipImagePadding = 4;
// An extra space between chip's label and right edge.
constexpr int kExtraRightPadding = 4;

// These chrome refresh layout constants are not shared with other views.
constexpr int kChipVerticalPadding = 4;
constexpr int kChipHorizontalPadding = 6;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxChipButton, kChipElementId);

OmniboxChipButton::OmniboxChipButton(PressedCallback callback)
    : MdTextButton(std::move(callback),
                   std::u16string(),
                   views::style::CONTEXT_BUTTON_MD) {
  SetProperty(views::kElementIdentifierKey, kChipElementId);
  views::InstallPillHighlightPathGenerator(this);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Equalizing padding on the left, right and between icon and label.
  SetImageLabelSpacing(kChipImagePadding);
  if (features::IsChromeRefresh2023()) {
    SetCustomPadding(
        gfx::Insets::VH(kChipVerticalPadding, kChipHorizontalPadding));
  } else {
    SetCustomPadding(gfx::Insets::VH(
        GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING),
        GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left()));
  }
  if (features::IsChromeRefresh2023()) {
    label()->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);
  }
  SetCornerRadius(GetCornerRadius());
  animation_ = std::make_unique<gfx::SlideAnimation>(this);

  UpdateIconAndColors();
}

OmniboxChipButton::~OmniboxChipButton() = default;

void OmniboxChipButton::VisibilityChanged(views::View* starting_from,
                                          bool is_visible) {
  for (Observer& observer : observers_) {
    observer.OnChipVisibilityChanged(is_visible);
  }
}

void OmniboxChipButton::AnimateCollapse(base::TimeDelta duration) {
  base_width_ = 0;
  animation_->SetSlideDuration(duration);
  ForceAnimateCollapse();
}

void OmniboxChipButton::AnimateExpand(base::TimeDelta duration) {
  base_width_ = 0;
  animation_->SetSlideDuration(duration);
  ForceAnimateExpand();
}

void OmniboxChipButton::AnimateToFit(base::TimeDelta duration) {
  animation_->SetSlideDuration(duration);
  base_width_ = label()->width();

  if (label()->GetPreferredSize().width() < width()) {
    // As we're collapsing, we need to make sure that the padding is not
    // animated away.
    base_width_ += kChipImagePadding + kExtraRightPadding;
    ForceAnimateCollapse();
  } else {
    ForceAnimateExpand();
  }
}

void OmniboxChipButton::ResetAnimation(double value) {
  animation_->Reset(value);
  OnAnimationValueMaybeChanged();
}

gfx::Size OmniboxChipButton::CalculatePreferredSize() const {
  const int fixed_width = GetIconSize() + GetInsets().width();
  const int collapsable_width = label()->GetPreferredSize().width() +
                                kChipImagePadding + kExtraRightPadding;

  const int width =
      base_width_ +
      base::ClampRound(collapsable_width * animation_->GetCurrentValue()) +
      fixed_width;
  return gfx::Size(width, GetHeightForWidth(width));
}

void OmniboxChipButton::OnThemeChanged() {
  MdTextButton::OnThemeChanged();
  UpdateIconAndColors();
}

void OmniboxChipButton::UpdateBackgroundColor() {
  if (theme_ == OmniboxChipTheme::kIconStyle) {
    // In pre-ChromeRefresh2023 and post-ChromeRefresh2023, content settings
    // icons (which kIconStyle mimics) don't have a background.
    SetBackground(nullptr);
  } else {
    SetBackground(
        CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
            GetBackgroundColor(), GetCornerRadius())));
  }
}

void OmniboxChipButton::AnimationEnded(const gfx::Animation* animation) {
  if (animation != animation_.get())
    return;

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

void OmniboxChipButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != animation_.get()) {
    return;
  }

  OnAnimationValueMaybeChanged();
  PreferredSizeChanged();
}

void OmniboxChipButton::SetUserDecision(
    permissions::PermissionAction user_decision) {
  user_decision_ = user_decision;
  UpdateIconAndColors();
}

void OmniboxChipButton::SetTheme(OmniboxChipTheme theme) {
  theme_ = theme;
  UpdateIconAndColors();
}

void OmniboxChipButton::SetBlockedIconShowing(bool should_show_blocked_icon) {
  should_show_blocked_icon_ = should_show_blocked_icon;
  UpdateIconAndColors();
}

void OmniboxChipButton::SetPermissionPromptStyle(
    PermissionPromptStyle prompt_style) {
  prompt_style_ = prompt_style;
  UpdateIconAndColors();
}

void OmniboxChipButton::SetMessage(std::u16string message) {
  SetText(message);
  UpdateIconAndColors();
}

ui::ImageModel OmniboxChipButton::GetIconImageModel() const {
  return ui::ImageModel::FromVectorIcon(GetIcon(), GetForegroundColor(),
                                        GetIconSize(), nullptr);
}

const gfx::VectorIcon& OmniboxChipButton::GetIcon() const {
  if (icon_) {
    return const_cast<decltype(*icon_)>(*icon_);
  }

  return gfx::kNoneIcon;
}

SkColor OmniboxChipButton::GetForegroundColor() const {
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

  if (GetOmniboxChipTheme() == OmniboxChipTheme::kIconStyle) {
    return GetColorProvider()->GetColor(kColorOmniboxResultsIcon);
  }

  return GetColorProvider()->GetColor(
      GetOmniboxChipTheme() == OmniboxChipTheme::kLowVisibility
          ? kColorOmniboxChipForegroundLowVisibility
          : kColorOmniboxChipForegroundNormalVisibility);
}

SkColor OmniboxChipButton::GetBackgroundColor() const {
  DCHECK(theme_ != OmniboxChipTheme::kIconStyle);
  return GetColorProvider()->GetColor(kColorOmniboxChipBackground);
}

void OmniboxChipButton::UpdateIconAndColors() {
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

void OmniboxChipButton::ForceAnimateExpand() {
  ResetAnimation(0.0);
  animation_->Show();
}

void OmniboxChipButton::ForceAnimateCollapse() {
  ResetAnimation(1.0);
  animation_->Hide();
}

void OmniboxChipButton::OnAnimationValueMaybeChanged() {
  fully_collapsed_ = animation_->GetCurrentValue() == 0.0;
}

int OmniboxChipButton::GetIconSize() const {
  if (features::IsChromeRefresh2023()) {
    // Mimic the sizing for other trailing icons.
    if (theme_ == OmniboxChipTheme::kIconStyle) {
      return GetLayoutConstant(LOCATION_BAR_TRAILING_ICON_SIZE);
    }
    return GetLayoutConstant(LOCATION_BAR_CHIP_ICON_SIZE);
  }

  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

int OmniboxChipButton::GetCornerRadius() const {
  DCHECK(theme_ != OmniboxChipTheme::kIconStyle);
  if (features::IsChromeRefresh2023()) {
    return GetLayoutConstant(LOCATION_BAR_CHILD_CORNER_RADIUS);
  }
  return GetIconSize();
}

void OmniboxChipButton::SetChipIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;

  UpdateIconAndColors();
}

void OmniboxChipButton::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxChipButton::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(OmniboxChipButton, views::MdTextButton)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
