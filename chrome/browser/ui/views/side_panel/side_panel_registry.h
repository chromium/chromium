// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_

#include <memory>
#include <vector>

class SidePanelEntry;

// This class is used for storing SidePanelEntries specific to a context. This
// context can be one per tab or one per window. See also SidePanelCoordinator.
class SidePanelRegistry final {
 public:
  SidePanelRegistry();
  SidePanelRegistry(const SidePanelRegistry&) = delete;
  SidePanelRegistry& operator=(const SidePanelRegistry&) = delete;
  ~SidePanelRegistry();

  void Register(std::unique_ptr<SidePanelEntry> entry);

  std::vector<std::unique_ptr<SidePanelEntry>>& entries() { return entries_; }

 private:
  std::vector<std::unique_ptr<SidePanelEntry>> entries_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
