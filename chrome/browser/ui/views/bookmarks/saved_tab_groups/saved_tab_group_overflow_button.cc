// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"

SavedTabGroupOverflowButton::SavedTabGroupOverflowButton(
    PressedCallback callback)
    : views::MenuButton(std::move(callback)) {
  SetAccessibilityProperties(
      ax::mojom::Role::kMenu,
      l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS_CHEVRON));
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_SAVED_TAB_GROUPS_OVERFLOW_BUTTON_TOOLTIP));
  SetFlipCanvasOnPaintForRTLUI(true);
  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  SetProperty(views::kElementIdentifierKey,
              kSavedTabGroupOverflowButtonElementId);
}

SavedTabGroupOverflowButton::~SavedTabGroupOverflowButton() = default;

void SavedTabGroupOverflowButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  views::MenuButton::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kMenu;
  node_data->SetNameChecked(
      l10n_util::GetStringUTF8(IDS_ACCNAME_SAVED_TAB_GROUPS_CHEVRON));
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
  const gfx::VectorIcon& icon = features::IsChromeRefresh2023()
                                    ? kBookmarkbarOverflowRefreshIcon
                                    : kOverflowChevronIcon;
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, overflow_color));
  return;
}

BEGIN_METADATA(SavedTabGroupOverflowButton, views::MenuButton)
END_METADATA
