// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ref.h"

class Profile;
class SidePanelRegistry;
class TabStripModel;

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
  const raw_ref<Profile> profile_;
  const raw_ref<TabStripModel> tab_strip_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
