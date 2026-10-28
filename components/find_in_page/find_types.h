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

// Outcome of a Find-in-Page user session, recorded once per session when the
// session ends (provided at least one active search completed).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(FindInPageSessionOutcome)
enum class FindSessionOutcome {
  // Every completed search in the session found at least one match, and the
  // session ended with a non-empty query.
  kMatchedWithoutSeeingZeroMatch = 0,
  // At least one search in the session yielded zero matches, but the session
  // ended on a non-empty query with one or more matches.
  kMatchedAfterSeeingZeroMatch = 1,
  // The session ended on a non-empty query with zero matches.
  kEndedWithZeroMatches = 2,
  // Every completed search in the session found at least one match, and the
  // user cleared the search box before ending the session.
  kEndedWithEmptyQueryWithoutSeeingZeroMatch = 3,
  // At least one search in the session yielded zero matches, and the user
  // cleared the search box before ending the session.
  kEndedWithEmptyQueryAfterSeeingZeroMatch = 4,
  kMaxValue = kEndedWithEmptyQueryAfterSeeingZeroMatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:FindInPageSessionOutcome)

}  // namespace find_in_page

#endif  // COMPONENTS_FIND_IN_PAGE_FIND_TYPES_H_
