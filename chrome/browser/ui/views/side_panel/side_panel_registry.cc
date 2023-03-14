// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

#include "base/containers/cxx20_erase.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"

const char kSidePanelRegistryKey[] = "side_panel_registry_key";

SidePanelRegistry::SidePanelRegistry() = default;

SidePanelRegistry::~SidePanelRegistry() {
  for (SidePanelRegistryObserver& observer : observers_) {
    observer.OnRegistryDestroying(this);
  }
}

// static
SidePanelRegistry* SidePanelRegistry::Get(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  SidePanelRegistry* registry = static_cast<SidePanelRegistry*>(
      web_contents->GetUserData(kSidePanelRegistryKey));
  if (!registry) {
    auto new_registry = std::make_unique<SidePanelRegistry>();
    registry = new_registry.get();
    web_contents->SetUserData(kSidePanelRegistryKey, std::move(new_registry));
  }
  return registry;
}

SidePanelEntry* SidePanelRegistry::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) {
  auto it = base::ranges::find(entries_, entry_key, &SidePanelEntry::key);
  return it == entries_.end() ? nullptr : it->get();
}

void SidePanelRegistry::ResetActiveEntry() {
  active_entry_.reset();
}

void SidePanelRegistry::ClearCachedEntryViews() {
  for (auto const& entry : entries_) {
    if (!active_entry_.has_value() || entry.get() != active_entry_.value())
      entry.get()->ClearCachedView();
  }
}

void SidePanelRegistry::AddObserver(SidePanelRegistryObserver* observer) {
  observers_.AddObserver(observer);
}

void SidePanelRegistry::RemoveObserver(SidePanelRegistryObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool SidePanelRegistry::Register(std::unique_ptr<SidePanelEntry> entry) {
  if (GetEntryForKey(entry->key()))
    return false;
  for (SidePanelRegistryObserver& observer : observers_)
    observer.OnEntryRegistered(this, entry.get());
  entry->AddObserver(this);
  entries_.push_back(std::move(entry));
  return true;
}

bool SidePanelRegistry::Deregister(const SidePanelEntry::Key& key) {
  if (!GetEntryForKey(key)) {
    return false;
  }

  DeregisterAndReturnView(key);
  return true;
}

std::unique_ptr<views::View> SidePanelRegistry::DeregisterAndReturnView(
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

  // If `entry` is currently shown, then its view is owned by the browser's side
  // panel view instead of being cached.
  // SidePanelCoordinator::OnEntryWillDeregister will retrieve the view from the
  // side panel and cache it into `entry`.
  for (SidePanelRegistryObserver& observer : observers_) {
    observer.OnEntryWillDeregister(this, entry);
  }

  std::unique_ptr<views::View> entry_view =
      entry->CachedView() ? entry->GetContent() : nullptr;

  RemoveEntry(entry);
  return entry_view;
}

void SidePanelRegistry::SetActiveEntry(SidePanelEntry* entry) {
  active_entry_ = entry;
}

void SidePanelRegistry::OnEntryShown(SidePanelEntry* entry) {
  active_entry_ = entry;
}

void SidePanelRegistry::OnEntryIconUpdated(SidePanelEntry* entry) {
  for (SidePanelRegistryObserver& observer : observers_)
    observer.OnEntryIconUpdated(entry);
}

void SidePanelRegistry::RemoveEntry(SidePanelEntry* entry) {
  base::EraseIf(entries_, base::MatchesUniquePtr(entry));
}
