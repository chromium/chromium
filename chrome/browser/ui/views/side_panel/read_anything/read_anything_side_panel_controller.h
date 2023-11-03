// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_

#include "chrome/browser/ui/side_panel/read_anything/read_anything_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

// A per-tab class that facilitates the showing of the Read Anything side panel.
class ReadAnythingSidePanelController : public ReadAnythingTabHelper::Delegate,
                                        public SidePanelEntryObserver {
 public:
  explicit ReadAnythingSidePanelController(content::WebContents* web_contents);
  ReadAnythingSidePanelController(const ReadAnythingSidePanelController&) =
      delete;
  ReadAnythingSidePanelController& operator=(
      const ReadAnythingSidePanelController&) = delete;
  ~ReadAnythingSidePanelController() override;

  // ReadAnythingTabHelper::Delegate:
  void CreateAndRegisterEntry() override;
  void DeregisterEntry() override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  // Creates the container view and all its child views for side panel entry.
  std::unique_ptr<views::View> CreateContainerView();

  const raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_
