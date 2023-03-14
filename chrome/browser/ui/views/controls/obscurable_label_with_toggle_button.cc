// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/obscurable_label_with_toggle_button.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

ObscurableLabelWithToggleButton::ObscurableLabelWithToggleButton(
    const std::u16string& obscured_value,
    const std::u16string& revealed_value,
    const std::u16string& toggle_button_tooltip,
    const std::u16string& toggle_button_toggled_tooltip)
    : obscured_value_(obscured_value), revealed_value_(revealed_value) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  value_ = AddChildView(std::make_unique<views::Label>(
      obscured_value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  value_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  value_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  value_->SetMultiLine(true);

  toggle_obscured_ = AddChildView(std::make_unique<views::ToggleImageButton>(
      base::BindRepeating(&ObscurableLabelWithToggleButton::ToggleValueObscured,
                          base::Unretained(this))));
  toggle_obscured_->SetImageHorizontalAlignment(
      views::ImageButton::ALIGN_CENTER);
  toggle_obscured_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  toggle_obscured_->SetToggled(false);
  views::SetImageFromVectorIconWithColorId(toggle_obscured_, views::kEyeIcon,
                                           ui::kColorIcon,
                                           ui::kColorIconDisabled);
  views::SetToggledImageFromVectorIconWithColorId(
      toggle_obscured_, views::kEyeCrossedIcon, ui::kColorIcon,
      ui::kColorIconDisabled);
  toggle_obscured_->SetTooltipText(toggle_button_tooltip);
  toggle_obscured_->SetToggledTooltipText(toggle_button_toggled_tooltip);
}

ObscurableLabelWithToggleButton::~ObscurableLabelWithToggleButton() = default;

views::Label* ObscurableLabelWithToggleButton::value() {
  return value_;
}

views::ToggleImageButton* ObscurableLabelWithToggleButton::toggle_obscured() {
  return toggle_obscured_;
}

void ObscurableLabelWithToggleButton::ToggleValueObscured() {
  const bool was_revealed = toggle_obscured_->GetToggled();
  toggle_obscured_->SetToggled(!was_revealed);
  value_->SetText(was_revealed ? obscured_value_ : revealed_value_);
}

BEGIN_METADATA(ObscurableLabelWithToggleButton, views::BoxLayoutView)
END_METADATA
