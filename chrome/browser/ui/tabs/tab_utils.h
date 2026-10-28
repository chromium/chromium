// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_UTILS_H_

#include <vector>

#include "components/tabs/public/tab_interface.h"

// Returns true if the site for `tab` is muted.
bool IsSiteMuted(tabs::TabHandle tab);

// Returns true if the sites for `tabs` are all muted.
bool AreAllSitesMuted(const std::vector<tabs::TabHandle>& tabs);

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
