// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/rich_radio_button.h"

#include <utility>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace extensions {

// TODO(http://crbug.com/461806299):
// - 'Escape' key won't close the dialog.
// - Make accessibility work correctly.

RichRadioButton::RichRadioButton(const ui::ImageModel& image,
                                 const std::u16string& title,
                                 const std::u16string& description,
                                 int group_id,
                                 base::RepeatingClosure on_selected_callback)
    : views::Button(base::BindRepeating(
          [](RichRadioButton* view) { view->radio_button_->SetChecked(true); },
          this)) {
  SetFocusBehavior(FocusBehavior::NEVER);
  views::Builder<RichRadioButton>(this)
      .SetLayoutManager(std::make_unique<views::FlexLayout>())
      .SetAccessibleName(title)
      .CustomConfigure(base::BindOnce([](RichRadioButton* view) {
        static_cast<views::FlexLayout*>(view->GetLayoutManager())
            ->SetOrientation(views::LayoutOrientation::kHorizontal)
            .SetDefault(views::kMarginsKey, gfx::Insets(10));
      }))
      .AddChildren(
          views::Builder<views::ImageView>().SetImage(image),
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetMainAxisAlignment(views::LayoutAlignment::kStart)
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
                               .WithWeight(1))
              .AddChildren(
                  views::Builder<views::Label>()
                      .SetText(title)
                      .SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS)
                      .SetHorizontalAlignment(
                          gfx::HorizontalAlignment::ALIGN_LEFT),
                  views::Builder<views::Label>()
                      .SetText(description)
                      .SetTextStyle(views::style::STYLE_BODY_4)
                      .SetHorizontalAlignment(
                          gfx::HorizontalAlignment::ALIGN_LEFT)),
          views::Builder<views::RadioButton>(
              std::make_unique<views::RadioButton>(std::u16string(), group_id))
              .CopyAddressTo(&radio_button_)
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification().WithAlignment(
                               views::LayoutAlignment::kEnd))
              .SetAccessibleName(
                  title, ax::mojom::NameFrom::kAttributeExplicitlyEmpty))
      .BuildChildren();

  subscription_ = radio_button_->AddCheckedChangedCallback(base::BindRepeating(
      [](views::RadioButton* button, base::RepeatingClosure closure) {
        if (button->GetChecked()) {
          closure.Run();
        }
      },
      radio_button_, std::move(on_selected_callback)));
}

RichRadioButton::~RichRadioButton() = default;

bool RichRadioButton::GetCheckedForTesting() const {
  return radio_button_->GetChecked();
}

BEGIN_METADATA(RichRadioButton)
END_METADATA

}  // namespace extensions
