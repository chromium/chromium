// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include <cstddef>

#include "base/time/time.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/painter.h"

namespace {

// Padding between chip's icon and label.
constexpr int kChipImagePadding = 4;
// An extra space between chip's label and right edge.
constexpr int kExtraRightPadding = 4;

}  // namespace

OmniboxChipButton::OmniboxChipButton(PressedCallback callback)
    : MdTextButton(std::move(callback),
                   std::u16string(),
                   views::style::CONTEXT_BUTTON_MD) {
  views::InstallPillHighlightPathGenerator(this);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Equalizing padding on the left, right and between icon and label.
  SetImageLabelSpacing(kChipImagePadding);
  SetCustomPadding(gfx::Insets::VH(
      GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING),
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left()));
  SetCornerRadius(GetIconSize());
  constexpr auto kAnimationDuration = base::Milliseconds(350);
  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetSlideDuration(kAnimationDuration);

  UpdateIconAndColors();
}

OmniboxChipButton::~OmniboxChipButton() = default;

void OmniboxChipButton::VisibilityChanged(views::View* starting_from,
                                          bool is_visible) {
  for (Observer& observer : observers_) {
    observer.OnChipVisibilityChanged(is_visible);
  }
}

void OmniboxChipButton::AnimateCollapse(base::TimeDelta kAnimationDuration) {
  base_width_ = 0;
  animation_->SetSlideDuration(kAnimationDuration);
  ForceAnimateCollapse();
}

void OmniboxChipButton::AnimateExpand(base::TimeDelta kAnimationDuration) {
  base_width_ = 0;
  animation_->SetSlideDuration(kAnimationDuration);
  ForceAnimateExpand();
}

void OmniboxChipButton::AnimateToFit(base::TimeDelta kAnimationDuration) {
  animation_->SetSlideDuration(kAnimationDuration);
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
  fully_collapsed_ = value == 0.0;
  animation_->Reset(value);
}

gfx::Size OmniboxChipButton::CalculatePreferredSize() const {
  const int fixed_width = GetIconSize() + GetInsets().width();
  const int collapsable_width = label()->GetPreferredSize().width() +
                                kChipImagePadding + kExtraRightPadding;

  const double animation_value =
      force_expanded_for_testing_ ? 1.0 : animation_->GetCurrentValue();
  const int width = base_width_ +
                    std::round(collapsable_width * animation_value) +
                    fixed_width;
  return gfx::Size(width, GetHeightForWidth(width));
}

void OmniboxChipButton::OnThemeChanged() {
  MdTextButton::OnThemeChanged();
  UpdateIconAndColors();
}

void OmniboxChipButton::UpdateBackgroundColor() {
  if (theme_ == OmniboxChipTheme::kIconStyle) {
    SetBackground(nullptr);
  } else {
    SetBackground(
        CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
            GetBackgroundColor(), GetIconSize())));
  }
}

void OmniboxChipButton::AnimationEnded(const gfx::Animation* animation) {
  if (animation != animation_.get())
    return;

  fully_collapsed_ = animation->GetCurrentValue() != 1.0;

  if (animation->GetCurrentValue() == 1.0) {
    for (Observer& observer : observers_) {
      observer.OnExpandAnimationEnded();
    }
  }

  if (animation->GetCurrentValue() == 0.0) {
    for (Observer& observer : observers_) {
      observer.OnCollapseAnimationEnded();
    }
  }
}

void OmniboxChipButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == animation_.get())
    PreferredSizeChanged();
}

void OmniboxChipButton::SetTheme(OmniboxChipTheme theme) {
  theme_ = theme;
  UpdateIconAndColors();
}

void OmniboxChipButton::SetMessage(std::u16string message) {
  SetText(message);
  UpdateIconAndColors();
}

ui::ImageModel OmniboxChipButton::GetIconImageModel() const {
  return ui::ImageModel::FromVectorIcon(GetIcon(), GetTextAndIconColor(),
                                        GetIconSize(), nullptr);
}

const gfx::VectorIcon& OmniboxChipButton::GetIcon() const {
  if (icon_) {
    return const_cast<decltype(*icon_)>(*icon_);
  }

  return gfx::kNoneIcon;
}

void OmniboxChipButton::ForceAnimateExpand() {
  ResetAnimation(0.0);
  animation_->Show();
}

void OmniboxChipButton::ForceAnimateCollapse() {
  ResetAnimation(1.0);
  animation_->Hide();
}

int OmniboxChipButton::GetIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

void OmniboxChipButton::UpdateIconAndColors() {
  if (!GetWidget())
    return;
  SetEnabledTextColors(GetTextAndIconColor());
  SetImageModel(views::Button::STATE_NORMAL, GetIconImageModel());
}

SkColor OmniboxChipButton::GetTextAndIconColor() const {
  if (theme_ == OmniboxChipTheme::kIconStyle)
    return GetColorProvider()->GetColor(kColorOmniboxResultsIcon);

  return GetColorProvider()->GetColor(
      theme_ == OmniboxChipTheme::kLowVisibility
          ? kColorOmniboxChipForegroundLowVisibility
          : kColorOmniboxChipForegroundNormalVisibility);
}

SkColor OmniboxChipButton::GetBackgroundColor() const {
  DCHECK(theme_ != OmniboxChipTheme::kIconStyle);
  return GetColorProvider()->GetColor(kColorOmniboxChipBackground);
}

void OmniboxChipButton::SetForceExpandedForTesting(
    bool force_expanded_for_testing) {
  force_expanded_for_testing_ = force_expanded_for_testing;
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
