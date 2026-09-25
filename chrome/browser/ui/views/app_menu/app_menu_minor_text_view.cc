// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_minor_text_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"

AppMenuMinorTextView::AppMenuMinorTextView(const std::u16string& minor_text) {
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  label_ = AddChildView(
      views::Builder<views::Label>()
          .SetText(minor_text)
          .SetEnabledColor(ui::kColorAppMenuUpgradeRowSubstringForeground)
          .SetEnabled(true)
          .SetAutoColorReadabilityEnabled(false)
          .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
              0, views::LayoutProvider::Get()->GetDistanceMetric(
                     views::DISTANCE_RELATED_LABEL_HORIZONTAL) -
                     views::MenuItemView::kChildHorizontalPadding)))
          .Build());
}

AppMenuMinorTextView::~AppMenuMinorTextView() = default;

// static
void AppMenuMinorTextView::AttachTo(views::MenuItemView* menu_item,
                                    const std::u16string& minor_text) {
  CHECK(menu_item);
  auto minor_text_view = std::make_unique<AppMenuMinorTextView>(minor_text);

  menu_item->AddChildView(std::move(minor_text_view));
  menu_item->SetHighlightWhenSelectedWithChildViews(true);
  menu_item->SetTriggerActionWithNonIconChildViews(true);

  menu_item->GetViewAccessibility().SetName(
      views::MenuItemView::GetAccessibleNameForMenuItem(
          menu_item->title(), minor_text, std::nullopt));
}

BEGIN_METADATA(AppMenuMinorTextView)
END_METADATA
