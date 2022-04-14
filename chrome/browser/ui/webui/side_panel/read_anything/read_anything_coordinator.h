// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_model.h"

class Browser;
class ReadAnythingController;
class ReadAnythingContainerView;

class ReadAnythingCoordinator {
 public:
  explicit ReadAnythingCoordinator(Browser* browser);
  ReadAnythingCoordinator(const ReadAnythingCoordinator&) = delete;
  ReadAnythingCoordinator& operator=(const ReadAnythingCoordinator&) = delete;
  ~ReadAnythingCoordinator();

  std::unique_ptr<ReadAnythingContainerView> GetContainerView();

  void AddObserver(ReadAnythingModel::Observer* observer) {
    model_->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingModel::Observer* observer) {
    model_->RemoveObserver(observer);
  }

 private:
  std::unique_ptr<ReadAnythingModel> model_;
  std::unique_ptr<ReadAnythingController> controller_;
  std::unique_ptr<ReadAnythingContainerView> container_view_;
};
#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
