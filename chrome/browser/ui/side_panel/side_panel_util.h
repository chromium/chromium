// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UTIL_H_

#include <string_view>

#include "ui/base/class_property.h"

class BrowserWindowInterface;
class SidePanelContentProxy;
class SidePanelEntry;

class SidePanelUtil {
 public:
  // Gets the SidePanelContentProxy for the provided view. If one does not
  // exist, this creates one indicating the view is available.
  static SidePanelContentProxy* GetSidePanelContentProxy(
      ui::PropertyHandler* content_view);

  // Gets the title text for the side panel entry.
  static std::u16string_view GetTitleText(SidePanelEntry* entry,
                                          BrowserWindowInterface* browser);
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_UTIL_H_
