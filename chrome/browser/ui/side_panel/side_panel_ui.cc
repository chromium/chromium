// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_ui.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

DEFINE_USER_DATA(SidePanelUI);

// static
SidePanelUI* SidePanelUI::From(BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// static
const SidePanelUI* SidePanelUI::From(const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
