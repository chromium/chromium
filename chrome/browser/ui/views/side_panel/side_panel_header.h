// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_H_

#include "ui/views/view.h"

// SidePanelHeader is a view with custom Layout override to draw on top of the
// Side Panel border. The header is added as a separate view over the side panel
// border so it can process events since the border cannot process events.
class SidePanelHeader : public views::View {
 public:
  SidePanelHeader();

  void Layout(PassKey) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_H_
