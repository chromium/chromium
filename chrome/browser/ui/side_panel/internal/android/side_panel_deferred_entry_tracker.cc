// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_deferred_entry_tracker.h"

#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"

SidePanelDeferredEntryTracker::SidePanelDeferredEntryTracker(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(browser_);
}

SidePanelDeferredEntryTracker::~SidePanelDeferredEntryTracker() = default;

void SidePanelDeferredEntryTracker::AddActiveEntries(DeferredReason reason) {
  auto* tab_list = TabListInterface::From(browser_);
  CHECK(tab_list);

  if (tabs::TabInterface* tab = tab_list->GetActiveTab()) {
    if (auto* tab_scoped_registry = SidePanelRegistry::From(tab)) {
      if (auto tab_scoped_entry = tab_scoped_registry->GetActiveEntry()) {
        AddEntry(SidePanelUIBase::UniqueKey{tab->GetHandle(),
                                            (*tab_scoped_entry)->key()},
                 reason);
      }
    }
  }

  if (auto* window_scoped_registry = SidePanelRegistry::From(browser_)) {
    if (auto window_scoped_entry = window_scoped_registry->GetActiveEntry()) {
      AddEntry(SidePanelUIBase::UniqueKey{std::nullopt,
                                          (*window_scoped_entry)->key()},
               reason);
    }
  }
}

void SidePanelDeferredEntryTracker::AddEntry(
    const SidePanelUIBase::UniqueKey& key,
    DeferredReason reason) {
  if (key.tab_handle) {
    tab_scoped_deferred_entries_.insert_or_assign(
        key.tab_handle.value(), DeferredEntry{.key = key, .reason = reason});
    SidePanelRegistry* registry = nullptr;
    if (tabs::TabInterface* tab = key.tab_handle.value().Get()) {
      registry = SidePanelRegistry::From(tab);
    }
    if (registry && registry->GetActiveEntry() &&
        (*registry->GetActiveEntry())->key() == key.key) {
      registry->ResetActiveEntry();
    }
  } else {
    window_scoped_deferred_entry_ = DeferredEntry{.key = key, .reason = reason};
    SidePanelRegistry* registry = SidePanelRegistry::From(browser_);
    if (registry && registry->GetActiveEntry() &&
        (*registry->GetActiveEntry())->key() == key.key) {
      registry->ResetActiveEntry();
    }
  }
}

std::optional<SidePanelDeferredEntryTracker::DeferredEntry>
SidePanelDeferredEntryTracker::GetTabOrWindowScopedEntry(
    const tabs::TabHandle& tab_handle) const {
  // 1. Check for tab-scoped deferred entry.
  if (auto tab_scoped_entry = GetTabScopedEntry(tab_handle)) {
    return tab_scoped_entry;
  }

  // 2. Check for window-scoped deferred entry.
  if (window_scoped_deferred_entry_) {
    return *window_scoped_deferred_entry_;
  }

  return std::nullopt;
}

std::optional<SidePanelDeferredEntryTracker::DeferredEntry>
SidePanelDeferredEntryTracker::GetTabScopedEntry(
    const tabs::TabHandle& tab_handle) const {
  auto it = tab_scoped_deferred_entries_.find(tab_handle);
  if (it != tab_scoped_deferred_entries_.end()) {
    return it->second;
  }

  return std::nullopt;
}

void SidePanelDeferredEntryTracker::ClearEntry(
    const SidePanelUIBase::UniqueKey& key) {
  if (key.tab_handle) {
    tab_scoped_deferred_entries_.erase(key.tab_handle.value());
  } else {
    window_scoped_deferred_entry_.reset();
  }
}

void SidePanelDeferredEntryTracker::ClearTabScopedEntry(
    const tabs::TabHandle& tab_handle) {
  tab_scoped_deferred_entries_.erase(tab_handle);
}

void SidePanelDeferredEntryTracker::ClearAllEntries() {
  tab_scoped_deferred_entries_.clear();
  window_scoped_deferred_entry_.reset();
}
