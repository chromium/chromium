// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"

namespace content {
class WebContents;
}  // namespace content

namespace companion {

// Controller for handling views specific logic for the CompanionTabHelper.
class CompanionSidePanelController : public CompanionTabHelper::Delegate {
 public:
  explicit CompanionSidePanelController(content::WebContents* web_contents);
  CompanionSidePanelController(const CompanionSidePanelController&) = delete;
  CompanionSidePanelController& operator=(const CompanionSidePanelController&) =
      delete;
  ~CompanionSidePanelController() override;

  // CompanionTabHelper::Delegate:
  void ShowCompanionSidePanel() override;
  void UpdateNewTabButtonState() override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_
