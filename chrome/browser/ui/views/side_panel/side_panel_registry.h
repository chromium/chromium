// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

namespace content {
class WebContents;
}  // namespace content

class SidePanelRegistryObserver;

// This class is used for storing SidePanelEntries specific to a context. This
// context can be one per tab or one per window. See also SidePanelCoordinator.
class SidePanelRegistry final : public base::SupportsUserData::Data,
                                public SidePanelEntryObserver {
 public:
  SidePanelRegistry();
  SidePanelRegistry(const SidePanelRegistry&) = delete;
  SidePanelRegistry& operator=(const SidePanelRegistry&) = delete;
  ~SidePanelRegistry() override;

  // Gets the contextual registry for the tab associated with |web_contents|.
  // Can return null for non-tab contents.
  static SidePanelRegistry* Get(content::WebContents* web_contents);

  SidePanelEntry* GetEntryForId(SidePanelEntry::Id entry_id);
  void ResetActiveEntry();

  // Clear cached view for all owned entries.
  void ClearCachedEntryViews();

  void AddObserver(SidePanelRegistryObserver* observer);
  void RemoveObserver(SidePanelRegistryObserver* observer);

  void Register(std::unique_ptr<SidePanelEntry> entry);
  void Deregister(SidePanelEntry::Id id);

  absl::optional<SidePanelEntry*> active_entry() { return active_entry_; }
  std::vector<std::unique_ptr<SidePanelEntry>>& entries() { return entries_; }

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* id) override;

 private:
  void RemoveEntry(SidePanelEntry* entry);

  // The last active entry hosted in the side panel used to determine what entry
  // should be visible. This is reset by the coordinator when the panel is
  // closed. When there are multiple registries, this may not be the entry
  // currently visible in the side panel.
  absl::optional<SidePanelEntry*> active_entry_;

  std::vector<std::unique_ptr<SidePanelEntry>> entries_;

  base::ObserverList<SidePanelRegistryObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
