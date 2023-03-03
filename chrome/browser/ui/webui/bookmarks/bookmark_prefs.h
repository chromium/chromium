// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARK_PREFS_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARK_PREFS_H_

class PrefRegistrySimple;

namespace bookmarks_webui {

namespace prefs {

// Integer which specifies the sort order for the bookmarks side panel list, as
// an index in the list of possible sort orders.
extern const char kBookmarksSortOrder[];

// Integer which specifies the view type for the bookmarks side panel list, as
// an index in the list of possible view types.
extern const char kBookmarksViewType[];

}  // namespace prefs

// Registers user preferences related to bookmarks.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace bookmarks_webui

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARK_PREFS_H_
