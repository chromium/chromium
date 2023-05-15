// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"

namespace extensions::side_panel_util {

// Defined in extension_side_panel_utils.h
void CreateSidePanelManagerForWebContents(Profile* profile,
                                          content::WebContents* web_contents) {
  ExtensionSidePanelManager::GetOrCreateForWebContents(profile, web_contents);
}

// Defined in extension_side_panel_utils.h
void ToggleExtensionSidePanel(Browser* browser,
                              const ExtensionId& extension_id) {
  SidePanelCoordinator* coordinator =
      BrowserView::GetBrowserViewForBrowser(browser)->side_panel_coordinator();

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
  if (coordinator->IsSidePanelEntryShowing(extension_key)) {
    coordinator->Close();
  } else {
    coordinator->Show(extension_key);
  }
}

}  // namespace extensions::side_panel_util
