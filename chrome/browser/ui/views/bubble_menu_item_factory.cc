// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_menu_item_factory.h"

#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {

constexpr gfx::Insets kDefaultBorderInsets = gfx::Insets(12);

class BubbleMenuItem : public views::LabelButton {
 public:
  METADATA_HEADER(BubbleMenuItem);
  BubbleMenuItem(PressedCallback callback,
                 const std::u16string& text,
                 int button_context,
                 int button_id)
      : LabelButton(callback, text, button_context) {
    ConfigureBubbleMenuItem(this, button_id);
  }

  void OnThemeChanged() override {
    LabelButton::OnThemeChanged();
    SetInkDropBaseColor(HoverButton::GetInkDropColor(this));
  }
};

BEGIN_METADATA(BubbleMenuItem, views::LabelButton)
END_METADATA

}  // namespace

void ConfigureBubbleMenuItem(views::Button* button, int button_id) {
  // Items within a menu should not show focus rings.
  button->SetInstallFocusRingOnFocus(false);
  button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  button->GetInkDrop()->SetShowHighlightOnFocus(true);
  button->GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());
  views::InstallRectHighlightPathGenerator(button);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetID(button_id);
}

std::unique_ptr<views::LabelButton> CreateBubbleMenuItem(
    int button_id,
    const std::u16string& name,
    views::Button::PressedCallback callback) {
  auto button = std::make_unique<BubbleMenuItem>(
      callback, name, views::style::CONTEXT_BUTTON, button_id);

  button->SetButtonController(std::make_unique<HoverButtonController>(
      button.get(), std::move(callback),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          button.get())));
  button->SetBorder(views::CreateEmptyBorder(kDefaultBorderInsets));

  return button;
}
