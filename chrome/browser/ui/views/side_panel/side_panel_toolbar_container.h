// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_CONTAINER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class BrowserView;
class SidePanelToolbarButton;
class ToolbarButton;

// Container for side panel button and pinned side panel entries shown in the
// toolbar.
class SidePanelToolbarContainer : public ToolbarIconContainerView {
 public:
  explicit SidePanelToolbarContainer(BrowserView* browser_view);
  SidePanelToolbarContainer(const SidePanelToolbarContainer&) = delete;
  SidePanelToolbarContainer& operator=(const SidePanelToolbarContainer&) =
      delete;
  ~SidePanelToolbarContainer() override;

  // Gets the side panel button for the toolbar.
  SidePanelToolbarButton* GetSidePanelButton() const;

  // Creates any pinned side panel entry toolbar buttons.
  void CreatePinnedEntryButtons();

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

 private:
  // Sorts child views to display them in the correct order (pinned buttons,
  // side panel button).
  void ReorderViews();

  const raw_ptr<BrowserView> browser_view_;

  const raw_ptr<SidePanelToolbarButton> side_panel_button_;

  std::vector<ToolbarButton*> pinned_entry_buttons_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_CONTAINER_H_
