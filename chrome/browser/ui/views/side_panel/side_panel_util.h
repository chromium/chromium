// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_

class Browser;
class SidePanelRegistry;

class SidePanelUtil {
 public:
  static void PopulateGlobalEntries(Browser* browser,
                                    SidePanelRegistry* global_registry);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
