// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

class BrowserView;

namespace views {
class View;
}  // namespace views

class SidePanelCoordinator final {
 public:
  explicit SidePanelCoordinator(BrowserView* browser_view);
  SidePanelCoordinator(const SidePanelCoordinator&) = delete;
  SidePanelCoordinator& operator=(const SidePanelCoordinator&) = delete;
  ~SidePanelCoordinator();

  void Show();
  void Close();
  void Toggle();

 private:
  BrowserView* const browser_view_;
  SidePanelRegistry window_registry_;

  // Raw pointer to active content when showing.
  // TODO(pbos): Consider whether this needs to be a ViewTracker and either
  // switch or document expected lifetimes here.
  views::View* active_content_ = nullptr;

  // TODO(pbos): Add awareness of tab registries here. This probably needs to
  // know the tab registry it's currently monitoring.
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
