// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

#include <algorithm>
#include <variant>

#include "base/auto_reset.h"
#include "base/containers/unique_ptr_adapters.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"

SidePanelRegistry::SidePanelRegistry(tabs::TabInterface* tab_interface)
    : SidePanelEntryScope(SidePanelEntryScope::ScopeType::kTab),
      owner_(tab_interface) {
  CHECK(tab_interface);
}

SidePanelRegistry::SidePanelRegistry(
    BrowserWindowInterface* browser_window_interface)
    : SidePanelEntryScope(SidePanelEntryScope::ScopeType::kBrowser),
      owner_(browser_window_interface) {
  CHECK(browser_window_interface);
}

SidePanelRegistry::~SidePanelRegistry() = default;

// static
SidePanelRegistry* SidePanelRegistry::GetDeprecated(
    content::WebContents* web_contents) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  return tab->GetTabFeatures()->side_panel_registry();
}

SidePanelEntry* SidePanelRegistry::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) {
  auto it = std::ranges::find(entries_, entry_key, &SidePanelEntry::key);
  return it == entries_.end() ? nullptr : it->get();
}

void SidePanelRegistry::ResetActiveEntryFor(SidePanelEntry::PanelType type) {
  active_entries_[type].reset();
}

void SidePanelRegistry::ClearCachedEntryViews(SidePanelEntry::PanelType type) {
  for (auto const& entry : entries_) {
    if (entry->type() == type &&
        (!active_entries_[type].has_value() ||
         entry.get() != active_entries_[type].value())) {
      entry.get()->ClearCachedView();
    }
  }
}

bool SidePanelRegistry::Register(std::unique_ptr<SidePanelEntry> entry) {
  if (GetEntryForKey(entry->key())) {
    return false;
  }
  // It's important to add `this` as an observer to `entry` before notifying
  // SidePanelRegistryObservers of the entry's registration because some
  // registry observers can call SidePanelEntryObserver methods for `entry`.
  entry->set_scope(this);
  entry->AddObserver(this);
  entries_.push_back(std::move(entry));
  return true;
}

bool SidePanelRegistry::Deregister(const SidePanelEntry::Key& key) {
  auto* entry = GetEntryForKey(key);

  // An observer can trigger this to be called while a deregister for the key
  // is ongoing. An example is an observer listening to `OnSidePanelDidClose()`
  // since a sidepanel can be closed during the deregistering process.
  if (!entry || (deregistering_entry_key_.has_value() &&
                 deregistering_entry_key_.value() == key)) {
    return false;
  }

  base::AutoReset<std::optional<SidePanelEntryKey>> deregistering_entry_key(
      &deregistering_entry_key_, key);

  entry->RemoveObserver(this);
  entry->set_scope(nullptr);
  if (active_entries_[entry->type()].has_value() &&
      entry->key() == active_entries_[entry->type()].value()->key()) {
    active_entries_[entry->type()].reset();
  }

  // TODO(https://crbug.com/360163254): This is nullptr in
  // BrowserWithTestWindowTest. When the test suite goes away the nullptr check
  // can be removed.
  if (auto* coordinator = GetCoordinator()) {
    bool for_tab = get_scope_type() == SidePanelEntryScope::ScopeType::kTab;
    // If the entry with the same key and scope is showing, synchronously close.
    if (coordinator->IsSidePanelEntryShowing(key, for_tab)) {
      coordinator->Close(/*suppress_animations=*/true);
    }
  }

  auto it = std::find_if(entries_.begin(), entries_.end(),
                         base::MatchesUniquePtr(entry));
  if (it != entries_.end()) {
    entries_.erase(it);
  }
  return true;
}

void SidePanelRegistry::SetActiveEntry(SidePanelEntry* entry) {
  active_entries_[entry->type()] = entry;
}

std::optional<SidePanelEntry*> SidePanelRegistry::GetActiveEntryFor(
    SidePanelEntry::PanelType type) {
  return active_entries_[type];
}

void SidePanelRegistry::OnEntryShown(SidePanelEntry* entry) {
  active_entries_[entry->type()] = entry;
}

const tabs::TabInterface& SidePanelRegistry::GetTabInterface() const {
  CHECK_EQ(SidePanelEntryScope::ScopeType::kTab, get_scope_type());
  return *std::get<tabs::TabInterface*>(owner_);
}

const BrowserWindowInterface& SidePanelRegistry::GetBrowserWindowInterface()
    const {
  return get_scope_type() == SidePanelEntryScope::ScopeType::kTab
             ? *std::get<tabs::TabInterface*>(owner_)
                    ->GetBrowserWindowInterface()
             : *std::get<BrowserWindowInterface*>(owner_);
}

SidePanelCoordinator* SidePanelRegistry::GetCoordinator() {
  return GetBrowserWindowInterface().GetFeatures().side_panel_coordinator();
}
