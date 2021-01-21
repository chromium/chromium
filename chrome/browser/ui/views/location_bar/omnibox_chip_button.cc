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
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_impl_macros.h"

OmniboxChipButton::OmniboxChipButton(PressedCallback callback,
                                     int button_context)
    : MdTextButton(std::move(callback), base::string16(), button_context) {
  views::InstallPillHighlightPathGenerator(this);
  SetProminent(true);
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

void OmniboxChipButton::ResetAnimation() {
  animation_->Reset();
}

void OmniboxChipButton::SetIcon(const gfx::VectorIcon* icon) {
  icon_ = icon;
  UpdateIconAndTextColor();
}

void OmniboxChipButton::SetExpandAnimationEndedCallback(
    base::RepeatingCallback<void()> callback) {
  expand_animation_ended_callback_ = callback;
}

bool OmniboxChipButton::GetFullyCollapsed() const {
  return fully_collapsed_;
}

gfx::Size OmniboxChipButton::CalculatePreferredSize() const {
  const int fixed_width = GetIconSize() + GetInsets().width();
  const int collapsable_width =
      label()->GetPreferredSize().width() + GetInsets().right();
  const int width =
      std::round(collapsable_width * animation_->GetCurrentValue()) +
      fixed_width;
  return gfx::Size(width, GetHeightForWidth(width));
}

void OmniboxChipButton::OnThemeChanged() {
  View::OnThemeChanged();
  UpdateIconAndTextColor();
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

int OmniboxChipButton::GetIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

void OmniboxChipButton::UpdateIconAndTextColor() {
  // Set label and icon color to be the same color.
  SkColor enabled_text_color = views::style::GetColor(
      *this, label()->GetTextContext(),
      GetProminent() ? views::style::STYLE_DIALOG_BUTTON_DEFAULT
                     : views::style::STYLE_PRIMARY);
  if (icon_) {
    SetEnabledTextColors(enabled_text_color);
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(*icon_, enabled_text_color,
                                                 GetIconSize()));
  }
}

BEGIN_METADATA(OmniboxChipButton, views::MdTextButton)
ADD_READONLY_PROPERTY_METADATA(bool, FullyCollapsed)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
