// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/view_class_properties.h"

namespace {
static constexpr int kUIUpdateIconSize = 20;
}  // namespace

namespace tab_groups {

SavedTabGroupOverflowButton::SavedTabGroupOverflowButton(
    PressedCallback callback)
    : views::MenuButton(std::move(callback)) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_GROUPS_EVERYTHING));
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TAB_GROUPS_EVERYTHING_BUTTON_TOOLTIP));
  SetFlipCanvasOnPaintForRTLUI(true);
  ConfigureInkDrop(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  SetProperty(views::kElementIdentifierKey,
              kSavedTabGroupOverflowButtonElementId);

  const gfx::VectorIcon& icon = features::IsRoundedIconsEnabled()
                                    ? kGridViewIcon
                                    : kSavedTabGroupBarEverythingOldIcon;
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, kColorBookmarkButtonIcon,
                                               kUIUpdateIconSize));
  SetImageModel(views::Button::STATE_DISABLED,
                ui::ImageModel::FromVectorIcon(icon, ui::kColorIconDisabled,
                                               kUIUpdateIconSize));
}

SavedTabGroupOverflowButton::~SavedTabGroupOverflowButton() = default;

std::unique_ptr<views::LabelButtonBorder>
SavedTabGroupOverflowButton::CreateDefaultBorder() const {
  auto border = std::make_unique<views::LabelButtonBorder>();
  border->set_insets(ChromeLayoutProvider::Get()->GetInsetsMetric(
      INSETS_BOOKMARKS_BAR_BUTTON));
  return border;
}

BEGIN_METADATA(SavedTabGroupOverflowButton)
END_METADATA

}  // namespace tab_groups
