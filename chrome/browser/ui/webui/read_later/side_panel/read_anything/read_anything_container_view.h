// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_

#include "ui/views/view.h"

class Browser;

// Generic View to hold the entirety of the "Read Anything" side panel.
class ReadAnythingContainerView : public views::View {
 public:
  explicit ReadAnythingContainerView(Browser* browser);
  ReadAnythingContainerView(const ReadAnythingContainerView&) = delete;
  ~ReadAnythingContainerView() override;
};
#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_
