// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SEARCH_FEATURE_H_
#define CHROME_BROWSER_UI_TAB_SEARCH_FEATURE_H_

namespace features {
// This function has been pulled out of ui_features.h to prevent a circular
// dependency. This file will be deleted once the rehoming of the tab search
// feature is complete.
bool HasTabSearchToolbarButton();
}  // namespace features

#endif  // CHROME_BROWSER_UI_TAB_SEARCH_FEATURE_H_
