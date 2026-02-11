// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SEARCH_FEATURE_H_
#define CHROME_BROWSER_UI_TAB_SEARCH_FEATURE_H_

namespace features {
// This function has been pulled out of ui_features.h to prevent a circular
// dependency. This file will be deleted once the rehoming of the tab search
// feature is complete.
//
// Note: This will return true if it is possible for the tab search toolbar
// button to exist based on feature flags. This means it may return true when
// there is no tab search toolbar button, eg. if the vertical tab strip is
// enabled and currently displayed. Prefer using GetTabSearchPosition in
// tab_strip_prefs.h wherever possible.
bool HasTabSearchToolbarButton();
}  // namespace features

#endif  // CHROME_BROWSER_UI_TAB_SEARCH_FEATURE_H_
