// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_OBSERVER_H_

#include "base/observer_list_types.h"

class SidePanelEntry;
enum class SidePanelEntryHideReason;

class SidePanelEntryObserver : public base::CheckedObserver {
 public:
  // Called when a SidePanelEntry is shown.
  virtual void OnEntryShown(SidePanelEntry* entry) {}

  // Called when a SidePanelEntry is preparing to hide, before any
  // close animations have started.
  virtual void OnEntryWillHide(SidePanelEntry* entry,
                               SidePanelEntryHideReason reason) {}

  // Called when a SidePanelEntry is shown while the hide animation is
  // in progress. This will only be called if the entry being shown is the
  // same as the entry that was being hidden. eg. bookmarks shown -> side panel
  // close animation started -> bookmarks triggered to show again which cancels
  // the hide.
  virtual void OnEntryHideCancelled(SidePanelEntry* entry) {}

  // Called when a SidePanelEntry is hidden.
  virtual void OnEntryHidden(SidePanelEntry* entry) {}

 protected:
  ~SidePanelEntryObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_OBSERVER_H_
