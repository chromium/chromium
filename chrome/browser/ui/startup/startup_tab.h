// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_

#include <vector>

#include "url/gurl.h"

// Represents tab data at startup.
struct StartupTab {
  StartupTab(const GURL& url, bool is_pinned);
  ~StartupTab();

  // The url to load.
  GURL url;

  // True if the is tab pinned.
  bool is_pinned;
};

typedef std::vector<StartupTab> StartupTabs;

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_
