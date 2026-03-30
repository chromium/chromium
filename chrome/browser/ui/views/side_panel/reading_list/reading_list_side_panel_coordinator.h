// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;
class SidePanelRegistry;
class TabStripModel;

// ReadingListSidePanelCoordinator handles the creation and registration of the
// bookmarks SidePanelEntry.
class ReadingListSidePanelCoordinator {
 public:
  DECLARE_USER_DATA(ReadingListSidePanelCoordinator);

  ReadingListSidePanelCoordinator(BrowserWindowInterface* interface,
                                  Profile* profile,
                                  TabStripModel* tab_strip_model);
  ReadingListSidePanelCoordinator(const ReadingListSidePanelCoordinator&) =
      delete;
  ReadingListSidePanelCoordinator& operator=(
      const ReadingListSidePanelCoordinator&) = delete;
  ~ReadingListSidePanelCoordinator();

  static ReadingListSidePanelCoordinator* From(
      BrowserWindowInterface* interface);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  const raw_ref<Profile> profile_;
  const raw_ref<TabStripModel> tab_strip_model_;

  ui::ScopedUnownedUserData<ReadingListSidePanelCoordinator>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
