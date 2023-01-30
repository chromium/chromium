// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_OBSERVER_H_

#include "base/observer_list_types.h"

class SidePanelEntry;
class SidePanelRegistry;

class SidePanelRegistryObserver : public base::CheckedObserver {
 public:
  // Called when a SidePanelEntry is added to the registry.
  virtual void OnEntryRegistered(SidePanelRegistry* registry,
                                 SidePanelEntry* entry) {}

  // Called immediately before a SidePanelEntry is being removed from the given
  // registry.
  virtual void OnEntryWillDeregister(SidePanelRegistry* registry,
                                     SidePanelEntry* entry) {}

  // Called when a SidePanelEntry's icon has been updated.
  virtual void OnEntryIconUpdated(SidePanelEntry* entry) {}

  // Called when the `registry` is being destroyed.
  virtual void OnRegistryDestroying(SidePanelRegistry* registry) {}

 protected:
  ~SidePanelRegistryObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_OBSERVER_H_
