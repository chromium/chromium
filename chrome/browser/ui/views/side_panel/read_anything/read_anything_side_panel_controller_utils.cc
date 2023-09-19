// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/browser/web_contents_observer.h"

void ShowReadAnythingSidePanel(Browser* browser) {
  SidePanelUI* side_panel_ui = SidePanelUI::GetSidePanelUIForBrowser(browser);
  if (!side_panel_ui) {
    return;
  }
  side_panel_ui->Show(SidePanelEntryId::kReadAnything,
                      SidePanelOpenTrigger::kReadAnythingContextMenu);
}

bool IsReadAnythingEntryShowing(Browser* browser) {
  SidePanelUI* side_panel_ui = SidePanelUI::GetSidePanelUIForBrowser(browser);
  if (!side_panel_ui) {
    return false;
  }
  return side_panel_ui->IsSidePanelShowing() &&
         (side_panel_ui->GetCurrentEntryId() ==
          SidePanelEntryId::kReadAnything);
}

void CreateAndRegisterReadAnythingEntry(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto* registry = SidePanelRegistry::Get(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!registry || !browser) {
    return;
  }
  // If the web contents is already registered to read anything, do nothing.
  if (registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))) {
    return;
  }

  auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);
  coordinator->CreateAndRegisterSidePanelEntry(registry);
}
