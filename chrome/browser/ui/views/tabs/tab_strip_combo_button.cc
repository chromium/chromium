// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_combo_button.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

TabStripComboButton::TabStripComboButton(BrowserWindowInterface* browser,
                                         TabStrip* tab_strip) {
  std::unique_ptr<TabStripControlButton> new_tab_button =
      std::make_unique<TabStripControlButton>(
          tab_strip->controller(),
          base::BindRepeating(&TabStrip::NewTabButtonPressed,
                              base::Unretained(tab_strip)),
          vector_icons::kAddIcon,
          base::i18n::IsRTL() ? Edge::kLeft : Edge::kRight);
  new_tab_button->SetProperty(views::kElementIdentifierKey,
                              kNewTabButtonElementId);

  if (features::HasTabstripComboButtonWithBackground()) {
    new_tab_button->SetForegroundFrameActiveColorId(
        kColorNewTabButtonForegroundFrameActive);
    new_tab_button->SetForegroundFrameInactiveColorId(
        kColorNewTabButtonForegroundFrameInactive);
    new_tab_button->SetBackgroundFrameActiveColorId(
        kColorNewTabButtonCRBackgroundFrameActive);
    new_tab_button->SetBackgroundFrameInactiveColorId(
        kColorNewTabButtonCRBackgroundFrameInactive);
  }

  new_tab_button_ = AddChildView(std::move(new_tab_button));

  new_tab_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
  new_tab_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));

#if BUILDFLAG(IS_LINUX)
  // The New Tab Button can be middle-clicked on Linux.
  new_tab_button_->SetTriggerableEventFlags(
      new_tab_button_->GetTriggerableEventFlags() | ui::EF_MIDDLE_MOUSE_BUTTON);
#endif

  std::unique_ptr<TabSearchContainer> tab_search_container =
      std::make_unique<TabSearchContainer>(
          tab_strip->controller(), browser->GetTabStripModel(), true, this,
          browser, browser->GetFeatures().tab_declutter_controller());
  tab_search_container->SetProperty(views::kCrossAxisAlignmentKey,
                                    views::LayoutAlignment::kCenter);

  tab_search_container_ = AddChildView(std::move(tab_search_container));
  tab_search_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0, GetLayoutConstant(TAB_STRIP_PADDING)));

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

BEGIN_METADATA(TabStripComboButton)
END_METADATA
