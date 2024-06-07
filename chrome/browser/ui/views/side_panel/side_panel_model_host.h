// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_MODEL_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_MODEL_HOST_H_

#include <memory>

#include "chrome/browser/ui/views/side_panel/side_panel_model.h"
#include "ui/views/layout/box_layout_view.h"

// TODO(pbos): This should probably be a SidePanelModelHostView that
// inherits from a non-views SidePanelModelHost before submitting.
// TODO(pbos): Reevaluate whether this should actually is-a View?
class SidePanelModelHost final : public views::BoxLayoutView {
 public:
  explicit SidePanelModelHost(std::unique_ptr<SidePanelModel> model);
  SidePanelModelHost(const SidePanelModelHost&) = delete;
  SidePanelModelHost& operator=(const SidePanelModelHost&) = delete;
  ~SidePanelModelHost() override;

 private:
  void AddCard(ui::DialogModelSection* card);

  std::unique_ptr<SidePanelModel> model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_MODEL_HOST_H_
