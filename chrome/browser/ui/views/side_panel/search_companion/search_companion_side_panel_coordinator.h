// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

class Browser;

namespace views {
class View;
}  // namespace views

// SearchCompanionSidePanelCoordinator handles the creation and registration of
// the search companion SidePanelEntry.
class SearchCompanionSidePanelCoordinator
    : public BrowserUserData<SearchCompanionSidePanelCoordinator>,
      public TabStripModelObserver {
 public:
  explicit SearchCompanionSidePanelCoordinator(Browser* browser);
  SearchCompanionSidePanelCoordinator(
      const SearchCompanionSidePanelCoordinator&) = delete;
  SearchCompanionSidePanelCoordinator& operator=(
      const SearchCompanionSidePanelCoordinator&) = delete;
  ~SearchCompanionSidePanelCoordinator() override;

  void CreateAndRegisterEntriesForExistingWebContents(
      TabStripModel* tab_strip_model);

  bool Show();
  BrowserView* GetBrowserView();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  raw_ptr<Browser> browser_;

  friend class BrowserUserData<SearchCompanionSidePanelCoordinator>;

  std::unique_ptr<SidePanelEntry> CreateCompanionEntry();

  std::unique_ptr<views::View> CreateCompanionWebView();

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_COORDINATOR_H_
