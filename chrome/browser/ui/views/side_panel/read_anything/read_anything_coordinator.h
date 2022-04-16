// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_

#include "chrome/browser/ui/browser_user_data.h"

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"

class Browser;
class ReadAnythingContainerView;
class ReadAnythingController;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

class ReadAnythingCoordinator
    : public BrowserUserData<ReadAnythingCoordinator> {
 public:
  explicit ReadAnythingCoordinator(Browser* browser);
  ReadAnythingCoordinator(const ReadAnythingCoordinator&) = delete;
  ReadAnythingCoordinator& operator=(const ReadAnythingCoordinator&) = delete;
  ~ReadAnythingCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);
  ReadAnythingPageHandler::Delegate* GetPageHandlerDelegate();

  void AddModelObserver(ReadAnythingModel::Observer* observer);
  void RemoveModelObserver(ReadAnythingModel::Observer* observer);

 private:
  friend class BrowserUserData<ReadAnythingCoordinator>;

  std::unique_ptr<views::View> GetContainerView();

  std::unique_ptr<ReadAnythingModel> model_;
  std::unique_ptr<ReadAnythingController> controller_;
  std::unique_ptr<ReadAnythingContainerView> container_view_;

  BROWSER_USER_DATA_KEY_DECL();
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
