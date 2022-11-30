// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_CHANGE_TYPE_H_
#define CHROME_BROWSER_UI_TABS_TAB_CHANGE_TYPE_H_

// Enumeration of possible changes to tab state used by the UI.
enum class TabChangeType {
  // Everything changed.
  kAll,

  // Only the loading state changed.
  kLoadingOnly,
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_CHANGE_TYPE_H_
