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
    SetCustomPadding(
        gfx::Insets::VH(kChipVerticalPadding, kChipHorizontalPadding));
    label()->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);
  SetCornerRadius(GetCornerRadius());
  animation_ = std::make_unique<gfx::SlideAnimation>(this);

  UpdateIconAndColors();
}

OmniboxChipButton::~OmniboxChipButton() = default;

void OmniboxChipButton::VisibilityChanged(views::View* starting_from,
                                          bool is_visible) {
  observers_.Notify(&Observer::OnChipVisibilityChanged, is_visible);
}

void OmniboxChipButton::AnimateCollapse(base::TimeDelta duration) {
  animation_->SetSlideDuration(duration);
  ForceAnimateCollapse();
}

void OmniboxChipButton::AnimateExpand(base::TimeDelta duration) {
  animation_->SetSlideDuration(duration);
  ForceAnimateExpand();
}

void OmniboxChipButton::ResetAnimation(double value) {
  animation_->Reset(value);
  OnAnimationValueMaybeChanged();
}

// TODO(crbug.com/40232718): Respect `available_size`.
gfx::Size OmniboxChipButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int fixed_width = GetIconSize() + GetInsets().width();
  constexpr int extra_width = kChipImagePadding + kExtraRightPadding;
  views::SizeBound available_width = std::max<views::SizeBound>(
      0, available_size.width() - fixed_width - extra_width);
  const int label_width =
      label()->GetPreferredSize(views::SizeBounds(available_width, {})).width();
  const int collapsable_width = label_width + extra_width;

  const int width =
      base::ClampRound(collapsable_width * animation_->GetCurrentValue()) +
      fixed_width;
  return views::LabelButton::CalculatePreferredSize(
      views::SizeBounds(width, {}));
}

void OmniboxChipButton::OnThemeChanged() {
  MdTextButton::OnThemeChanged();
  UpdateIconAndColors();
}

void OmniboxChipButton::UpdateBackgroundColor() {
  if (theme_ == OmniboxChipTheme::kIconStyle) {
    SetBackground(nullptr);
  } else {
    SkColor color = GetColorProvider()->GetColor(GetBackgroundColorId());
    SetBackground(CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(color, GetCornerRadius())));
  }
}

void OmniboxChipButton::AnimationEnded(const gfx::Animation* animation) {
  if (animation != animation_.get())
    return;

  OnAnimationValueMaybeChanged();
}

void OmniboxChipButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != animation_.get()) {
    return;
  }

  OnAnimationValueMaybeChanged();
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
  return ui::ImageModel::FromVectorIcon(GetIcon(), GetForegroundColorId(),
                                        GetIconSize(), nullptr);
}

const gfx::VectorIcon& OmniboxChipButton::GetIcon() const {
  if (icon_) {
    return const_cast<decltype(*icon_)>(*icon_);
  }

  return gfx::kNoneIcon;
}

ui::ColorId OmniboxChipButton::GetForegroundColorId() const {
  return kColorOmniboxChipForegroundNormalVisibility;
}

ui::ColorId OmniboxChipButton::GetBackgroundColorId() const {
  DCHECK(theme_ != OmniboxChipTheme::kIconStyle);
  return kColorOmniboxChipBackground;
}

void OmniboxChipButton::UpdateIconAndColors() {
  if (!GetWidget()) {
    return;
  }
  SetEnabledTextColorIds(GetForegroundColorId());
  SetImageModel(views::Button::STATE_NORMAL, GetIconImageModel());
    ConfigureInkDropForRefresh2023(this, kColorOmniboxChipInkDropHover,
                                   kColorOmniboxChipInkDropRipple);
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
    // Mimic the sizing for other trailing icons.
    return GetLayoutConstant((theme_ == OmniboxChipTheme::kIconStyle)
                                 ? LOCATION_BAR_TRAILING_ICON_SIZE
                                 : LOCATION_BAR_CHIP_ICON_SIZE);
}

int OmniboxChipButton::GetCornerRadius() const {
  DCHECK(theme_ != OmniboxChipTheme::kIconStyle);
    return GetLayoutConstant(LOCATION_BAR_CHILD_CORNER_RADIUS);
}

void OmniboxChipButton::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxChipButton::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(OmniboxChipButton)
ADD_READONLY_PROPERTY_METADATA(int, IconSize)
END_METADATA
