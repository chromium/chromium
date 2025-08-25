// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_ui_base.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

SidePanelUIBase::SidePanelUIBase(Browser* browser)
    : browser_(browser),
      window_registry_(std::make_unique<SidePanelRegistry>(browser)),
      waiter_(std::make_unique<SidePanelEntryWaiter>()) {
  browser_->tab_strip_model()->AddObserver(this);
}

SidePanelUIBase::~SidePanelUIBase() = default;

void SidePanelUIBase::Show(
    SidePanelEntry::Id entry_id,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  Show(SidePanelEntry::Key(entry_id), open_trigger);
}

void SidePanelUIBase::Show(
    SidePanelEntry::Key entry_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(entry_key);
  CHECK(unique_key.has_value());
  Show(unique_key.value(), open_trigger, /*suppress_animations=*/false);
}

std::optional<SidePanelUIBase::UniqueKey> SidePanelUIBase::GetUniqueKeyForKey(
    const SidePanelEntry::Key& entry_key) const {
  // For tab-scoped side panels.
  if (GetActiveContextualRegistry() &&
      GetActiveContextualRegistry()->GetEntryForKey(entry_key)) {
    return UniqueKey{browser_->GetActiveTabInterface()->GetHandle(), entry_key};
  }

  // For window-scoped side panels.
  if (window_registry_->GetEntryForKey(entry_key)) {
    return UniqueKey{/*tab_handle=*/std::nullopt, entry_key};
  }
  return std::nullopt;
}

SidePanelRegistry* SidePanelUIBase::GetActiveContextualRegistry() const {
  if (browser_->tab_strip_model()->empty()) {
    return nullptr;
  }
  return browser_->GetActiveTabInterface()
      ->GetTabFeatures()
      ->side_panel_registry();
}

void SidePanelUIBase::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {}
