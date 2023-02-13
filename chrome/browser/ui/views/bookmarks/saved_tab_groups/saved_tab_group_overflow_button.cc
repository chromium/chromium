// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"

SavedTabGroupOverflowButton::SavedTabGroupOverflowButton(
    PressedCallback callback)
    : views::MenuButton(std::move(callback), u"") {
  // TODO(crbug/1415488): Change the accessible name and tooltip text to IDS
  // strings when UX gives the final strings to use. Can use
  // IDS_BOOKMARK_BAR_OVERFLOW_BUTTON_TOOLTIP as a template for the tooltip
  // text.
  SetAccessibleName(u"Saved Group Overflow Accessible Name");
  SetTooltipText(u"Hidden saved groups");
  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  views::InstallPillHighlightPathGenerator(this);
}

SavedTabGroupOverflowButton::~SavedTabGroupOverflowButton() = default;

void SavedTabGroupOverflowButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // If the button would have no name, avoid crashing by setting the name
  // explicitly empty.
  if (GetAccessibleName().empty()) {
    node_data->SetNameExplicitlyEmpty();
  }

  // TODO(crbug/1415488): Add an IDS string instead of the hard coded value. Can
  // use IDS_ACCNAME_BOOKMARKS_CHEVRON as a template.
  views::MenuButton::GetAccessibleNodeData(node_data);
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                "Menu containing hidden saved groups");
}

std::unique_ptr<views::LabelButtonBorder>
SavedTabGroupOverflowButton::CreateDefaultBorder() const {
  auto border = std::make_unique<views::LabelButtonBorder>();
  border->set_insets(ChromeLayoutProvider::Get()->GetInsetsMetric(
      INSETS_BOOKMARKS_BAR_BUTTON));
  return border;
}

void SavedTabGroupOverflowButton::OnThemeChanged() {
  views::MenuButton::OnThemeChanged();

  ui::ColorProvider* color_provider = GetColorProvider();
  const SkColor overflow_color =
      color_provider->GetColor(kColorBookmarkButtonIcon);
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kOverflowChevronIcon, overflow_color));
  return;
}
