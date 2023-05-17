// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/browser.h"

const int SidePanelUI::kUserDataKey;

SidePanelUI* SidePanelUI::GetSidePanelUIForBrowser(Browser* browser) {
  CHECK(browser);
  return static_cast<SidePanelUI*>(browser->GetUserData(&kUserDataKey));
}

void SidePanelUI::SetSidePanelUIForBrowser(
    Browser* browser,
    std::unique_ptr<SidePanelUI> side_panel_ui) {
  CHECK(browser);
  browser->SetUserData(&kUserDataKey, std::move(side_panel_ui));
}

void SidePanelUI::RemoveSidePanelUIForBrowser(Browser* browser) {
  CHECK(browser);
  browser->RemoveUserData(&kUserDataKey);
}
