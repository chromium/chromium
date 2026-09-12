// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_chip_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/metadata/view_factory.h"

namespace {
constexpr int kChipCornerRadius = 100;
constexpr int kChipHeight = 20;
}  // namespace

ActionAppMenuChipView::ActionAppMenuChipView(const std::u16string& chip_text) {
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  chip_label_ =
      AddChildView(views::Builder<views::Label>()
                       .SetText(chip_text)
                       .SetAutoColorReadabilityEnabled(false)
                       .SetLineHeight(kChipHeight)
                       .SetBorder(views::CreateEmptyBorder(
                           ChromeLayoutProvider::Get()->GetInsetsMetric(
                               INSETS_APP_MENU_CHIP)))
                       .Build());

  UpdateColors(/*is_selected=*/false);
}

ActionAppMenuChipView::~ActionAppMenuChipView() = default;

// static
void ActionAppMenuChipView::AttachTo(views::MenuItemView* menu_item,
                                     const std::u16string& chip_text) {
  CHECK(menu_item);
  auto chip_view = std::make_unique<ActionAppMenuChipView>(chip_text);
  chip_view->UpdateColors(menu_item->IsSelected());

  ActionAppMenuChipView* chip_view_ptr = chip_view.get();
  chip_view->selected_changed_subscription_ =
      menu_item->AddSelectedChangedCallback(base::BindRepeating(
          [](views::MenuItemView* item, ActionAppMenuChipView* chip) {
            chip->UpdateColors(item->IsSelected());
          },
          menu_item, chip_view_ptr));

  const int horizontal_padding =
      ChromeLayoutProvider::Get()
          ->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM)
          .right();
  auto edge_spacing_view =
      views::Builder<views::View>()
          .SetPreferredSize(gfx::Size(horizontal_padding, 0))
          .Build();

  menu_item->AddChildView(std::move(chip_view));
  menu_item->AddChildView(std::move(edge_spacing_view));
  menu_item->SetHighlightWhenSelectedWithChildViews(true);

  menu_item->GetViewAccessibility().SetName(
      views::MenuItemView::GetAccessibleNameForMenuItem(
          menu_item->title(), chip_text, std::nullopt));
}

// Set background and text colors according to selected and hover states.
void ActionAppMenuChipView::UpdateColors(bool is_selected) {
  chip_label_->SetBackground(views::CreateRoundedRectBackground(
      is_selected ? kColorAppMenuChipBackgroundHovered
                  : kColorAppMenuChipBackground,
      kChipCornerRadius));
  chip_label_->SetEnabledColor(kColorAppMenuChipForeground);
}

BEGIN_METADATA(ActionAppMenuChipView)
END_METADATA
