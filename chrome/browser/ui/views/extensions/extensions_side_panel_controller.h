// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_SIDE_PANEL_CONTROLLER_H_

#include "base/memory/checked_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

class SidePanel;

// A class that manages hosting the extension WebContents in the left aligned
// side panel of the browser window.
// TODO(crbug.com/1197555): Remove this once the experiment has concluded.
class ExtensionsSidePanelController : public content::WebContentsObserver {
 public:
  ExtensionsSidePanelController(SidePanel* side_panel,
                                content::BrowserContext* browser_context);
  ExtensionsSidePanelController(const ExtensionsSidePanelController&) = delete;
  ExtensionsSidePanelController& operator=(
      const ExtensionsSidePanelController&) = delete;
  ~ExtensionsSidePanelController() override;

 private:
  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  CheckedPtr<SidePanel> side_panel_;
  CheckedPtr<views::WebView> web_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_SIDE_PANEL_CONTROLLER_H_
