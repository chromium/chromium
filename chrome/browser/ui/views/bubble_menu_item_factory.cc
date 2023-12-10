// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_menu_item_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {

constexpr gfx::Insets kDefaultBorderInsets = gfx::Insets(12);

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
    const ui::ImageModel& icon) {
  auto button = std::make_unique<HoverButton>(std::move(callback), icon, name);
  ConfigureBubbleMenuItem(button.get(), button_id);
  button->SetBorder(views::CreateEmptyBorder(kDefaultBorderInsets));
  return button;
}
