// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"

class Profile;
class SidePanelEntryScope;
class SidePanelRegistry;
class TabStripModel;

namespace views {
class View;
}  // namespace views

// ReadingListSidePanelCoordinator handles the creation and registration of the
// bookmarks SidePanelEntry.
class ReadingListSidePanelCoordinator {
 public:
  ReadingListSidePanelCoordinator(Profile* profile,
                                  TabStripModel* tab_strip_model);
  ReadingListSidePanelCoordinator(const ReadingListSidePanelCoordinator&) =
      delete;
  ReadingListSidePanelCoordinator& operator=(
      const ReadingListSidePanelCoordinator&) = delete;
  ~ReadingListSidePanelCoordinator();

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  std::unique_ptr<views::View> CreateReadingListWebView(
      SidePanelEntryScope& scope);

  const raw_ptr<Profile> profile_;
  const raw_ptr<TabStripModel> tab_strip_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
