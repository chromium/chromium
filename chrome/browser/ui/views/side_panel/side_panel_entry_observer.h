// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

class SidePanelEntryObserver : public base::CheckedObserver {
 public:
  // Called when a SidePanelEntry is shown.
  virtual void OnEntryShown(SidePanelEntry* entry) {}

  // Called when a SidePanelEntry is hidden.
  virtual void OnEntryHidden(SidePanelEntry* entry) {}

  // Called when a SidePanelEntry's icon is updated.
  virtual void OnEntryIconUpdated(SidePanelEntry* entry) {}

 protected:
  ~SidePanelEntryObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_OBSERVER_H_
