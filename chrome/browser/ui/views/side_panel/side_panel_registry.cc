// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

#include "base/containers/cxx20_erase.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "content/public/browser/web_contents.h"

const char kSidePanelRegistryKey[] = "side_panel_registry_key";

SidePanelRegistry::SidePanelRegistry() = default;

SidePanelRegistry::~SidePanelRegistry() = default;

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

SidePanelEntry* SidePanelRegistry::GetEntryForId(SidePanelEntry::Id entry_id) {
  auto it =
      std::find_if(entries_.begin(), entries_.end(),
                   [entry_id](const std::unique_ptr<SidePanelEntry>& entry) {
                     return entry.get()->id() == entry_id;
                   });
  return it == entries_.end() ? nullptr : it->get();
}

void SidePanelRegistry::ResetActiveEntry() {
  active_entry_.reset();
}

void SidePanelRegistry::ClearCachedEntryViews() {
  for (auto const& entry : entries_)
    entry.get()->ClearCachedView();
}

void SidePanelRegistry::AddObserver(SidePanelRegistryObserver* observer) {
  observers_.AddObserver(observer);
}

void SidePanelRegistry::RemoveObserver(SidePanelRegistryObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SidePanelRegistry::Register(std::unique_ptr<SidePanelEntry> entry) {
  for (SidePanelRegistryObserver& observer : observers_)
    observer.OnEntryRegistered(entry.get());
  entry->AddObserver(this);
  entries_.push_back(std::move(entry));
}

void SidePanelRegistry::Deregister(SidePanelEntry::Id id) {
  for (auto const& entry : entries_) {
    if (entry.get()->id() == id) {
      entry.get()->RemoveObserver(this);
      if (active_entry_.has_value() &&
          entry.get()->id() == active_entry_.value()->id()) {
        active_entry_.reset();
      }
      for (SidePanelRegistryObserver& observer : observers_) {
        observer.OnEntryWillDeregister(entry.get());
      }
      RemoveEntry(entry.get());
      return;
    }
  }
}

void SidePanelRegistry::OnEntryShown(SidePanelEntry* entry) {
  active_entry_ = entry;
}

void SidePanelRegistry::RemoveEntry(SidePanelEntry* entry) {
  base::EraseIf(entries_, base::MatchesUniquePtr(entry));
}
