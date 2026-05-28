// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_BAR_VISIBILITY_STATE_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_BAR_VISIBILITY_STATE_H_

namespace bookmarks {

// Enum which specifies the visibility state of the bookmark bar.
// Used for the `kNtpSimplificationBookmarkBar` feature.
// These values are persisted to a syncable pref. Values should not be
// renumbered.
enum class BookmarkBarVisibilityState {
  // The bookmark bar will be visible on every page.
  kAlwaysShow = 0,
  // The bookmark bar will be visible only on the new tab page.
  kOnlyShowOnNtp = 1,
  // The bookmark bar will not be visible on any page.
  kAlwaysHide = 2,
  kMaxValue = kAlwaysHide,
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_BAR_VISIBILITY_STATE_H_
