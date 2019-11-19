// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_TYPES_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_TYPES_H_

// An enum listing the possible actions to take on a find-in-page selection
// in the page when ending the find session.
enum class FindOnPageSelectionAction {
  kKeep,     // Translate the find selection into a normal selection.
  kClear,    // Clear the find selection.
  kActivate  // Focus and click the selected node (for links).
};

// An enum listing the possible actions to take on a find-in-page results in
// the Find box when ending the find session.
enum class FindBoxResultAction {
  kClear,  // Clear search string, ordinal and match count.
  kKeep,   // Leave the results untouched.
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_TYPES_H_
