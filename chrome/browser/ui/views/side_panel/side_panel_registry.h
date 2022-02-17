// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

class SidePanelRegistryObserver;

// This class is used for storing SidePanelEntries specific to a context. This
// context can be one per tab or one per window. See also SidePanelCoordinator.
class SidePanelRegistry final : public SidePanelEntryObserver {
 public:
  SidePanelRegistry();
  SidePanelRegistry(const SidePanelRegistry&) = delete;
  SidePanelRegistry& operator=(const SidePanelRegistry&) = delete;
  ~SidePanelRegistry() override;

  void AddObserver(SidePanelRegistryObserver* observer);
  void RemoveObserver(SidePanelRegistryObserver* observer);

  void Register(std::unique_ptr<SidePanelEntry> entry);

  absl::optional<SidePanelEntry::Id> last_active_entry() {
    return last_active_entry_;
  }
  std::vector<std::unique_ptr<SidePanelEntry>>& entries() { return entries_; }

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry::Id id) override;

 private:
  absl::optional<SidePanelEntry::Id> last_active_entry_;

  std::vector<std::unique_ptr<SidePanelEntry>> entries_;

  base::ObserverList<SidePanelRegistryObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
