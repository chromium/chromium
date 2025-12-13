// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_UTILS_H_

#include <vector>

class TabStripModel;

// Returns true if the site at |index| in |tab_strip| is muted.
bool IsSiteMuted(const TabStripModel& tab_strip, const int index);

// Returns true if the sites at the |indices| in |tab_strip| are all muted.
bool AreAllSitesMuted(const TabStripModel& tab_strip,
                      const std::vector<int>& indices);

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
