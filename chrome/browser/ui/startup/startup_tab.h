// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_

#include <vector>

#include "url/gurl.h"

// Represents tab data at startup.
struct StartupTab {
  enum class Type {
    kNormal,
    // The tab is pinned.
    kPinned,
    // The url is from the LAST_AND_URLS startup pref.
    kFromLastAndUrlsStartupPref,
  };

  explicit StartupTab(const GURL& url, Type type = Type::kNormal);
  ~StartupTab();

  // The url to load.
  GURL url;

  Type type;
};

using StartupTabs = std::vector<StartupTab>;

// Indicates whether the command line arguments includes tabs to be opened on
// startup.
enum class CommandLineTabsPresent {
  kUnknown = -1,
  kNo = 0,
  kYes = 1,
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_
