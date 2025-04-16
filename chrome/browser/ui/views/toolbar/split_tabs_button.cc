// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_visual_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

SplitTabsToolbarButton::SplitTabsToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&SplitTabsToolbarButton::ButtonPressed,
                                        base::Unretained(this)),
                    nullptr,
                    nullptr),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey,
              kToolbarSplitTabsToolbarButtonElementId);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SPLIT_TABS));
  SetVectorIcon(kSplitSceneIcon);
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

void SplitTabsToolbarButton::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  UpdateButtonVisibility();
}

void SplitTabsToolbarButton::OnSplitTabCreated(
    std::vector<std::pair<tabs::TabInterface*, int>> tabs,
    split_tabs::SplitTabId split_id,
    TabStripModelObserver::SplitTabAddReason reason,
    split_tabs::SplitTabVisualData visual_data) {
  UpdateButtonVisibility();
}

void SplitTabsToolbarButton::OnSplitTabRemoved(
    std::vector<std::pair<tabs::TabInterface*, int>> tabs,
    split_tabs::SplitTabId split_id,
    SplitTabRemoveReason reason) {
  UpdateButtonVisibility();
}

void SplitTabsToolbarButton::ButtonPressed(const ui::Event& event) {}

void SplitTabsToolbarButton::UpdateButtonVisibility() {
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

BEGIN_METADATA(SplitTabsToolbarButton)
END_METADATA
