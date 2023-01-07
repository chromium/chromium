// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FIND_IN_PAGE_FIND_TYPES_H_
#define COMPONENTS_FIND_IN_PAGE_FIND_TYPES_H_

namespace find_in_page {

// An enum listing the possible actions to take on a find-in-page selection
// in the page when ending the find session.
enum class SelectionAction {
  kKeep,     // Translate the find selection into a normal selection.
  kClear,    // Clear the find selection.
  kActivate  // Focus and click the selected node (for links).
};

// An enum listing the possible actions to take on a find-in-page results in
// the Find box when ending the find session.
enum class ResultAction {
  kClear,  // Clear search string, ordinal and match count.
  kKeep,   // Leave the results untouched.
};

}  // namespace find_in_page

#endif  // COMPONENTS_FIND_IN_PAGE_FIND_TYPES_H_
