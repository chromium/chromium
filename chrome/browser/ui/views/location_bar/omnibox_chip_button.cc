// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"

#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_impl_macros.h"

OmniboxChipButton::OmniboxChipButton(PressedCallback callback,
                                     int button_context)
    : MdTextButton(std::move(callback), std::u16string(), button_context) {
  views::InstallPillHighlightPathGenerator(this);
  SetCornerRadius(GetIconSize());
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Equalizing padding on the left, right and between icon and label.
  SetImageLabelSpacing(
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left());
  SetCustomPadding(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING),
                  GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left()));

  constexpr auto kAnimationDuration = base::TimeDelta::FromMilliseconds(350);
  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetSlideDuration(kAnimationDuration);
}

OmniboxChipButton::~OmniboxChipButton() = default;

void OmniboxChipButton::AnimateCollapse() {
  constexpr auto kAnimationDuration = base::TimeDelta::FromMilliseconds(250);
  animation_->SetSlideDuration(kAnimationDuration);
  animation_->Hide();
}

void OmniboxChipButton::AnimateExpand() {
  constexpr auto kAnimationDuration = base::TimeDelta::FromMilliseconds(350);
  animation_->SetSlideDuration(kAnimationDuration);
  animation_->Show();
}

void OmniboxChipButton::ResetAnimation(double value) {
  animation_->Reset(value);
}

void OmniboxChipButton::SetIcon(const gfx::VectorIcon* icon) {
  icon_ = icon;
  UpdateColors();
}

void OmniboxChipButton::SetExpandAnimationEndedCallback(
    base::RepeatingCallback<void()> callback) {
  expand_animation_ended_callback_ = callback;
}

gfx::Size OmniboxChipButton::CalculatePreferredSize() const {
  const int fixed_width = GetIconSize() + GetInsets().width();
  const int collapsable_width =
      label()->GetPreferredSize().width() + GetInsets().right();
  const double animation_value =
      force_expanded_for_testing_ ? 1.0 : animation_->GetCurrentValue();
  const int width =
      std::round(collapsable_width * animation_value) + fixed_width;
  return gfx::Size(width, GetHeightForWidth(width));
}

void OmniboxChipButton::OnThemeChanged() {
  View::OnThemeChanged();
  UpdateColors();
}

void OmniboxChipButton::AnimationEnded(const gfx::Animation* animation) {
  if (animation != animation_.get())
    return;

  fully_collapsed_ = animation->GetCurrentValue() != 1.0;
  if (animation->GetCurrentValue() == 1.0)
    expand_animation_ended_callback_.Run();
}

void OmniboxChipButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == animation_.get())
    PreferredSizeChanged();
}

void OmniboxChipButton::SetTheme(Theme theme) {
  theme_ = theme;
  UpdateColors();
}

void OmniboxChipButton::SetProminent(bool is_prominent) {
  views::MdTextButton::SetProminent(is_prominent);
  UpdateColors();
}

int OmniboxChipButton::GetIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

void OmniboxChipButton::UpdateColors() {
  if (!icon_)
    return;

  SetEnabledTextColors(GetForegroundColor());
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*icon_, GetForegroundColor(),
                                               GetIconSize()));
  SetBgColorOverride(GetBackgroundColor());
}

SkColor OmniboxChipButton::GetMainColor() {
  ui::NativeTheme* native_theme = GetNativeTheme();
  switch (theme_) {
    case Theme::kBlue:
      // TODO(crbug.com/1003612): ui::NativeTheme::kColorId_ProminentButtonColor
      // does not always represent the blue color we need, but it is OK to use
      // for now.
      return native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_ProminentButtonColor);
  }
}

SkColor OmniboxChipButton::GetNeutralColor() {
  return views::style::GetColor(*this, label()->GetTextContext(),
                                views::style::STYLE_DIALOG_BUTTON_DEFAULT);
}

SkColor OmniboxChipButton::GetForegroundColor() {
  return GetProminent() ? GetNeutralColor() : GetMainColor();
}

SkColor OmniboxChipButton::GetBackgroundColor() {
  return GetProminent() ? GetMainColor() : GetNeutralColor();
}

void OmniboxChipButton::SetForceExpandedForTesting(
    bool force_expanded_for_testing) {
  force_expanded_for_testing_ = force_expanded_for_testing;
}

BEGIN_METADATA(OmniboxChipButton, views::MdTextButton)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
