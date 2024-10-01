// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

#include "base/containers/unique_ptr_adapters.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"

SidePanelRegistry::SidePanelRegistry(tabs::TabInterface* tab_interface)
    : owner_(tab_interface) {}

SidePanelRegistry::SidePanelRegistry(
    BrowserWindowInterface* browser_window_interface)
    : owner_(browser_window_interface) {}

SidePanelRegistry::~SidePanelRegistry() = default;

// static
SidePanelRegistry* SidePanelRegistry::GetDeprecated(
    content::WebContents* web_contents) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  return tab->GetTabFeatures()->side_panel_registry();
}

SidePanelEntry* SidePanelRegistry::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) {
  auto it = base::ranges::find(entries_, entry_key, &SidePanelEntry::key);
  return it == entries_.end() ? nullptr : it->get();
}

void SidePanelRegistry::ResetActiveEntry() {
  if (active_entry_.has_value()) {
    last_active_entry_ = active_entry_;
    active_entry_.reset();
  }
}

void SidePanelRegistry::ResetLastActiveEntry() {
  last_active_entry_.reset();
}

void SidePanelRegistry::ClearCachedEntryViews() {
  for (auto const& entry : entries_) {
    if (!active_entry_.has_value() || entry.get() != active_entry_.value())
      entry.get()->ClearCachedView();
  }
}

bool SidePanelRegistry::Register(std::unique_ptr<SidePanelEntry> entry) {
  if (GetEntryForKey(entry->key()))
    return false;
  // It's important to add `this` as an observer to `entry` before notifying
  // SidePanelRegistryObservers of the entry's registration because some
  // registry observers can call SidePanelEntryObserver methods for `entry`.
  entry->AddObserver(this);
  entries_.push_back(std::move(entry));
  return true;
}

bool SidePanelRegistry::Deregister(const SidePanelEntry::Key& key) {
  // An observer can trigger this to be called while a deregister for the key
  // is ongoing. An example is an observer listening to `OnSidePanelDidClose()`
  // since a sidepanel can be closed during the deregistering process.
  if (!GetEntryForKey(key) || (deregistering_entry_key_.has_value() &&
                               deregistering_entry_key_.value() == key)) {
    return false;
  }

  base::AutoReset<std::optional<SidePanelEntryKey>> deregistering_entry_key(
      &deregistering_entry_key_, key);
  DeregisterAndReturnEntry(key);
  return true;
}

std::unique_ptr<SidePanelEntry> SidePanelRegistry::DeregisterAndReturnEntry(
    const SidePanelEntry::Key& key) {
  auto* entry = GetEntryForKey(key);
  if (!entry) {
    return nullptr;
  }

  entry->RemoveObserver(this);
  if (active_entry_.has_value() &&
      entry->key() == active_entry_.value()->key()) {
    active_entry_.reset();
  }
  if (last_active_entry_.has_value() &&
      entry->key() == last_active_entry_.value()->key()) {
    last_active_entry_.reset();
  }

  // If `entry` is currently shown, then its view is owned by the browser's side
  // panel view instead of being cached.
  // SidePanelCoordinator::OnEntryWillDeregister will retrieve the view from the
  // side panel and cache it into `entry`.
  // TODO(https://crbug.com/360163254): This is nullptr in
  // BrowserWithTestWindowTest. When the test suite goes away the nullptr check
  // can be removed.
  if (auto* coordinator = GetCoordinator()) {
    coordinator->OnEntryWillDeregister(this, entry);
  }

  return RemoveEntry(entry);
}

void SidePanelRegistry::SetActiveEntry(SidePanelEntry* entry) {
  active_entry_ = entry;
}

void SidePanelRegistry::OnEntryShown(SidePanelEntry* entry) {
  active_entry_ = entry;
}

SidePanelCoordinator* SidePanelRegistry::GetCoordinator() {
  BrowserWindowInterface* browser_window_interface = nullptr;
  if (auto* ptr = std::get_if<BrowserWindowInterface*>(&owner_)) {
    browser_window_interface = *ptr;
  } else {
    browser_window_interface =
        std::get<tabs::TabInterface*>(owner_)->GetBrowserWindowInterface();
  }
  return browser_window_interface->GetFeatures().side_panel_coordinator();
}

std::unique_ptr<SidePanelEntry> SidePanelRegistry::RemoveEntry(
    SidePanelEntry* entry) {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         base::MatchesUniquePtr(entry));
  if (it == entries_.end()) {
    return nullptr;
  }
  std::unique_ptr<SidePanelEntry> return_entry = std::move(*it);
  entries_.erase(it);
  return return_entry;
}
