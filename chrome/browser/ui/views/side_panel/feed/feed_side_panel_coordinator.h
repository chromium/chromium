// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_FEED_FEED_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_FEED_FEED_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "chrome/browser/ui/browser_user_data.h"

class Browser;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace feed {

// FeedSidePanelCoordinator handles the creation and registration of the
// feed SidePanelEntry.
class FeedSidePanelCoordinator
    : public BrowserUserData<FeedSidePanelCoordinator> {
 public:
  explicit FeedSidePanelCoordinator(Browser* browser);
  FeedSidePanelCoordinator(const FeedSidePanelCoordinator&) = delete;
  FeedSidePanelCoordinator& operator=(const FeedSidePanelCoordinator&) = delete;
  ~FeedSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  friend class BrowserUserData<FeedSidePanelCoordinator>;

  std::unique_ptr<views::View> CreateFeedWebUIView();

  BROWSER_USER_DATA_KEY_DECL();
};

}  // namespace feed

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_FEED_FEED_SIDE_PANEL_COORDINATOR_H_
