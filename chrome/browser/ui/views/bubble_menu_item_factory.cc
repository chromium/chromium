// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include <memory>

#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {

constexpr gfx::Insets kDefaultBorderInsets = gfx::Insets(12);

class BubbleMenuItem : public HoverButton {
 public:
  METADATA_HEADER(BubbleMenuItem);
  BubbleMenuItem(PressedCallback callback,
                 const std::u16string& text,
                 int button_id)
      : HoverButton(callback, text) {
    ConfigureBubbleMenuItem(this, button_id);
  }

  BubbleMenuItem(PressedCallback callback,
                 const std::u16string& text,
                 int button_id,
                 const gfx::VectorIcon* icon)
      : HoverButton(callback, ui::ImageModel::FromVectorIcon(*icon), text) {
    ConfigureBubbleMenuItem(this, button_id);
  }

  void OnThemeChanged() override {
    HoverButton::OnThemeChanged();
    views::InkDrop::Get(this)->SetBaseColor(HoverButton::GetInkDropColor(this));
  }
};

BEGIN_METADATA(BubbleMenuItem, HoverButton)
END_METADATA

}  // namespace

void ConfigureBubbleMenuItem(views::Button* button, int button_id) {
  // Items within a menu should not show focus rings.
  button->SetInstallFocusRingOnFocus(false);
  views::InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(button)->GetInkDrop()->SetShowHighlightOnFocus(true);
  views::InkDrop::Get(button)->GetInkDrop()->SetHoverHighlightFadeDuration(
      base::TimeDelta());
  views::InstallRectHighlightPathGenerator(button);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetID(button_id);
}

std::unique_ptr<HoverButton> CreateBubbleMenuItem(
    int button_id,
    const std::u16string& name,
    views::Button::PressedCallback callback,
    const gfx::VectorIcon* icon) {
  std::unique_ptr<BubbleMenuItem> button;
  if (icon == nullptr) {
    button = std::make_unique<BubbleMenuItem>(callback, name, button_id);
  } else {
    button = std::make_unique<BubbleMenuItem>(callback, name, button_id, icon);
  }

  button->SetBorder(views::CreateEmptyBorder(kDefaultBorderInsets));

  return button;
}
