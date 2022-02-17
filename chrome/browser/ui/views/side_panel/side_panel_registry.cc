// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"

SidePanelRegistry::SidePanelRegistry() = default;

SidePanelRegistry::~SidePanelRegistry() = default;

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

void SidePanelRegistry::OnEntryShown(SidePanelEntry::Id id) {
  last_active_entry_ = id;
}
