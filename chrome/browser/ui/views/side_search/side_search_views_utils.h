// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_VIEWS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_VIEWS_UTILS_H_

class Browser;

namespace side_search {

// Returns true if the side panel is open to the side search feature. This is
// used by both the independent and unified side panel implementations.
bool IsSideSearchToggleOpen(Browser* browser);

}  // namespace side_search

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_VIEWS_UTILS_H_
