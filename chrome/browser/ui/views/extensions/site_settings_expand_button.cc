// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/site_settings_expand_button.h"

#include <memory>

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"

SiteSettingsExpandButton::SiteSettingsExpandButton(PressedCallback callback)
    : HoverButton(std::move(callback),
                  ui::ImageModel::FromVectorIcon(
                      kWebIcon,
                      ui::kColorIcon,
                      ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE)),
                  std::u16string()) {
  views::Builder<SiteSettingsExpandButton>(this)
      .SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_USER_SETTINGS_TITLE))
      .AddChild(
          views::Builder<views::StyledLabel>()
              .SetText(l10n_util::GetStringUTF16(
                  IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_USER_SETTINGS_TITLE))
              // Hover the whole button when hovering the styled label.
              .SetCanProcessEventsWithinSubtree(false)
              // Space between hover button icon and the styled label includes
              // icon spacing to align with other buttons in the menu.
              .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
                  0,
                  ChromeLayoutProvider::Get()->GetDistanceMetric(
                      DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN) +
                      ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_EXTENSIONS_MENU_ICON_SPACING),
                  0, 0)))
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)))
      .AddChild(views::Builder<views::ImageView>()
                    .CopyAddressTo(&icon_)
                    .SetImage(ui::ImageModel::FromVectorIcon(
                        vector_icons::kCaretDownIcon, ui::kColorIcon,
                        gfx::GetDefaultSizeOfVectorIcon(
                            vector_icons::kCaretDownIcon)))
                    .SetPaintToLayer()
                    // Hover the whole button when hovering the image view.
                    .SetCanProcessEventsWithinSubtree(false)
                    .CustomConfigure(base::BindOnce([](views::ImageView* view) {
                      view->layer()->SetFillsBoundsOpaquely(false);
                    })))
      .BuildChildren();

  // Set the layout manager to ignore the ink_drop_container to ensure the ink
  // drop tracks the bounds of its parent. This is done outside the builder
  // since it doesn't allow for layout manager customization.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetChildViewIgnoredByLayout(ink_drop_container(), true);

  // By default, the icon is in not expanded stated.
  SetIcon(false);
}
SiteSettingsExpandButton::~SiteSettingsExpandButton() = default;

void SiteSettingsExpandButton::SetIcon(bool expand_state) {
  const auto& vector_icon =
      expand_state ? vector_icons::kCaretUpIcon : vector_icons::kCaretDownIcon;
  int icon_size = gfx::GetDefaultSizeOfVectorIcon(vector_icon);
  icon_->SetImage(
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorIcon, icon_size));
}

BEGIN_METADATA(SiteSettingsExpandButton, HoverButton)
END_METADATA
