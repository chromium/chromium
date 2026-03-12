// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UTIL_H_

#include "ui/base/class_property.h"

class SidePanelContentProxy;

class SidePanelUtil {
 public:
  // Gets the SidePanelContentProxy for the provided view. If one does not
  // exist, this creates one indicating the view is available.
  static SidePanelContentProxy* GetSidePanelContentProxy(
      ui::PropertyHandler* content_view);
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UTIL_H_
