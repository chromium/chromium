// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_VIEW_STATE_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_VIEW_STATE_OBSERVER_H_

#include "base/observer_list_types.h"

class SidePanelViewStateObserver : public base::CheckedObserver {
 public:
  // Called after opening the SidePanel.
  virtual void OnSidePanelDidOpen() {}

  // Called when the side panel was in the process of closing, but a call to
  // open the side panel interrupted the closing process.
  virtual void OnSidePanelCloseInterrupted() {}

  // Called after closing the SidePanel.
  virtual void OnSidePanelDidClose() {}

 protected:
  ~SidePanelViewStateObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_VIEW_STATE_OBSERVER_H_
