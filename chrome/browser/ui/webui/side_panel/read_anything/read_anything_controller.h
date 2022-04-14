// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_toolbar_view.h"

class ReadAnythingController : public ReadAnythingToolbarView::Delegate {
 public:
  explicit ReadAnythingController(ReadAnythingModel* model);
  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  virtual ~ReadAnythingController();

 private:
  // ReadAnythingToolbarView::Delegate:
  void OnFontChoiceChanged(int new_choice) override;

  const raw_ptr<ReadAnythingModel> model_;
  std::vector<ReadAnythingModel::Observer*> observers_;

  base::WeakPtrFactory<ReadAnythingController> weak_pointer_factory_{this};
};
#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
