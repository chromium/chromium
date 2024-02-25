// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_

#include "chrome/browser/ui/browser_user_data.h"

class Browser;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

// ReadingListSidePanelCoordinator handles the creation and registration of the
// bookmarks SidePanelEntry.
class ReadingListSidePanelCoordinator
    : public BrowserUserData<ReadingListSidePanelCoordinator> {
 public:
  explicit ReadingListSidePanelCoordinator(Browser* browser);
  ~ReadingListSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  friend class BrowserUserData<ReadingListSidePanelCoordinator>;

  std::unique_ptr<views::View> CreateReadingListWebView();

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READING_LIST_SIDE_PANEL_COORDINATOR_H_
