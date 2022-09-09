// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PINNED_TAB_TEST_UTILS_H_
#define CHROME_BROWSER_UI_TABS_PINNED_TAB_TEST_UTILS_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/startup/startup_tab.h"

class PinnedTabTestUtils {
 public:
  PinnedTabTestUtils() = delete;
  PinnedTabTestUtils(const PinnedTabTestUtils&) = delete;
  PinnedTabTestUtils& operator=(const PinnedTabTestUtils&) = delete;

  // Converts a set of Tabs into a string. The format is a space separated list
  // of urls. If the tab is an app, ':app' is appended, and if the tab is
  // pinned, ':pinned' is appended.
  static std::string TabsToString(const std::vector<StartupTab>& values);
};

#endif  // CHROME_BROWSER_UI_TABS_PINNED_TAB_TEST_UTILS_H_
