// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"

#include <memory>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabsToolbarButton,
                                      kSplitTabButtonMenu);

SplitTabsToolbarButton::SplitTabsToolbarButton(Browser* browser)
    : ToolbarButton(
          base::BindRepeating(&SplitTabsToolbarButton::ButtonPressed,
                              base::Unretained(this)),
          std::make_unique<SplitTabMenuModel>(browser->tab_strip_model()),
          nullptr),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey,
              kToolbarSplitTabsToolbarButtonElementId);
  set_menu_identifier(kSplitTabButtonMenu);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SPLIT_TABS));
  pin_state_.Init(
      prefs::kPinSplitTabButton, browser_->profile()->GetPrefs(),
      base::BindRepeating(&SplitTabsToolbarButton::UpdateButtonVisibility,
                          base::Unretained(this)));
  UpdateButtonVisibility();
  browser->tab_strip_model()->AddObserver(this);
}

SplitTabsToolbarButton::~SplitTabsToolbarButton() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool SplitTabsToolbarButton::ShouldShowMenu() {
  return browser_->tab_strip_model()->GetActiveTab()->IsSplit();
}

void SplitTabsToolbarButton::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    UpdateButtonVisibility();
  }
}

void SplitTabsToolbarButton::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kAdded ||
      change.type == SplitTabChange::Type::kRemoved ||
      change.type == SplitTabChange::Type::kContentsChanged) {
    UpdateButtonVisibility();
  }
}

void SplitTabsToolbarButton::ButtonPressed(const ui::Event& event) {
  TabStripModel* const tab_strip_model = browser_->tab_strip_model();
  tabs::TabInterface* const active_tab = tab_strip_model->GetActiveTab();

  if (active_tab->IsSplit()) {
    ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
  } else {
    chrome::NewSplitTab(browser_);
  }
}

void SplitTabsToolbarButton::UpdateButtonVisibility() {
  UpdateButtonIcon();
  if (pin_state_.GetValue()) {
    SetVisible(true);
    return;
  }

  bool should_show_button = false;
  TabStripModel* const tab_strip_model = browser_->tab_strip_model();
  if (tab_strip_model) {
    tabs::TabInterface* const active_tab = tab_strip_model->GetActiveTab();
    should_show_button = active_tab && active_tab->IsSplit();
  }

  SetVisible(should_show_button);
}

void SplitTabsToolbarButton::UpdateButtonIcon() {
  TabStripModel* const tab_strip_model = browser_->tab_strip_model();
  tabs::TabInterface* const active_tab = tab_strip_model->GetActiveTab();

  if (active_tab && active_tab->IsSplit()) {
    const split_tabs::SplitTabActiveLocation location =
        split_tabs::GetLastActiveTabLocation(tab_strip_model,
                                             active_tab->GetSplit().value());
    constexpr auto icons =
        base::MakeFixedFlatMap<split_tabs::SplitTabActiveLocation,
                               const gfx::VectorIcon*>({
            {split_tabs::SplitTabActiveLocation::kStart, &kSplitSceneLeftIcon},
            {split_tabs::SplitTabActiveLocation::kEnd, &kSplitSceneRightIcon},
            {split_tabs::SplitTabActiveLocation::kTop, &kSplitSceneUpIcon},
            {split_tabs::SplitTabActiveLocation::kBottom, &kSplitSceneDownIcon},
        });
    SetVectorIcon(*icons.at(location));
  } else {
    SetVectorIcon(kSplitSceneIcon);
  }
}

const std::optional<ToolbarButton::VectorIcons>&
SplitTabsToolbarButton::GetIconsForTesting() {
  return GetVectorIcons();
}

BEGIN_METADATA(SplitTabsToolbarButton)
END_METADATA
