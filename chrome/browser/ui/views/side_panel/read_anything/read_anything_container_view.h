// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "ui/views/view.h"

class ReadAnythingToolbarView;
class ReadAnythingUI;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingContainerView
//
//  A class that holds all of the Read Anything UI. This includes a toolbar,
//  which is a View, and the Read Anything contents pane, which is a WebUI.
//  This class is created by the ReadAnythingCoordinator and owned by the Side
//  Panel View. It has the same lifetime as the Side Panel view.
//
class ReadAnythingContainerView : public views::View {
 public:
  ReadAnythingContainerView(
      std::unique_ptr<ReadAnythingToolbarView> toolbar,
      std::unique_ptr<SidePanelWebUIViewT<ReadAnythingUI>> content);
  ReadAnythingContainerView(const ReadAnythingContainerView&) = delete;
  ReadAnythingContainerView& operator=(const ReadAnythingContainerView&) =
      delete;
  ~ReadAnythingContainerView() override;
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_
