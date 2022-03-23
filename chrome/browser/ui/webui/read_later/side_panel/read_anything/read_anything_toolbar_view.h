// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

// Generic View for the toolbar of the Read Anything side panel.
class ReadAnythingToolbarView : public views::View {
 public:
  ReadAnythingToolbarView();
  ReadAnythingToolbarView(const ReadAnythingToolbarView&) = delete;
  ~ReadAnythingToolbarView() override;

 private:
  void OnSettingsClicked();
  base::WeakPtrFactory<ReadAnythingToolbarView> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
