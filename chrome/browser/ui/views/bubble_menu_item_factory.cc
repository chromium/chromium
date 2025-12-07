// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_menu_item_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {

constexpr gfx::Insets kDefaultBorderInsets = gfx::Insets(12);

class BubbleMenuItemButton : public HoverButton {
  METADATA_HEADER(BubbleMenuItemButton, HoverButton)

 public:
  BubbleMenuItemButton(PressedCallback callback,
                       const ui::ImageModel& icon,
                       const std::u16string& text)
      : HoverButton(std::move(callback), icon, text) {}

  // HoverButton:
  void StateChanged(ButtonState old_state) override {
    // Explicitly override HoverButton::StateChanged so focus is not taken from
    // other elements within the same view as this button when it is hovered.
    // Ex: In the TabGroupEditorBubbleView users should be able to hover over
    // the menu items without losing focus on the title text box.
    LabelButton::StateChanged(old_state);
  }
};

BEGIN_METADATA(BubbleMenuItemButton)
END_METADATA

}  // namespace

void ConfigureBubbleMenuItem(views::Button* button, int button_id) {
  // Items within a menu should not show focus rings.
  button->SetInstallFocusRingOnFocus(false);
  views::InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(button)->GetInkDrop()->SetShowHighlightOnHover(true);
  views::InkDrop::Get(button)->GetInkDrop()->SetHoverHighlightFadeDuration(
      base::TimeDelta());
  views::InstallRectHighlightPathGenerator(button);
  button->SetID(button_id);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetBorder(views::CreateEmptyBorder(kDefaultBorderInsets));
}

std::unique_ptr<HoverButton> CreateBubbleMenuItem(
    int button_id,
    const std::u16string& name,
    views::Button::PressedCallback callback,
    const ui::ImageModel& icon,
    const std::u16string& accelerator_text) {
  auto button =
      std::make_unique<BubbleMenuItemButton>(std::move(callback), icon, name);
  ConfigureBubbleMenuItem(button.get(), button_id);

  if (!accelerator_text.empty()) {
    // Add an accelerator label to the button if it has one. The accelerator
    // should be right aligned in Left-to-Right mode and left aligned in
    // Right-to-Left mode.

    // Use BoxLayout for the button's content.
    views::BoxLayout* layout =
        button->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    // Add a spacer view to push the accelerator to the end of the view. This
    // ensures all accelerators are aligned with each other.
    button->AddChildView(std::make_unique<views::View>());
    layout->SetFlexForView(button->children().back(), 1);

    auto accelerator_label = std::make_unique<views::Label>(accelerator_text);
    accelerator_label->SetEnabledColor(ui::kColorLabelForegroundSecondary);
    accelerator_label->SetTextStyle(views::style::STYLE_BODY_4);
    accelerator_label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    button->AddChildView(std::move(accelerator_label));

    button->SetAccessibleName(name + u" " + accelerator_text);
  }

  return button;
}
