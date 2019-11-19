// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_

#include <vector>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;

namespace bookmarks {
class BookmarkNode;
struct BookmarkNodeData;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class ImageSkia;
}

namespace ui {
class DropTargetEvent;
}

namespace chrome {

// Returns the bookmarkable URL for |web_contents|.
// This is normally the current URL, but when the page is the Instant Extended
// New Tab Page, the precise current URL may reflect various flags or other
// implementation details that don't represent data we should store
// in the bookmark.  In this case we instead return a URL that always
// means "NTP" instead of the current URL.
GURL GetURLToBookmark(content::WebContents* web_contents);

// Fills in the URL and title for a bookmark of |web_contents|.
void GetURLAndTitleToBookmark(content::WebContents* web_contents,
                              GURL* url,
                              base::string16* title);

// Toggles whether the bookmark bar is shown only on the new tab page or on
// all tabs. This is a preference modifier, not a visual modifier.
void ToggleBookmarkBarWhenVisible(content::BrowserContext* browser_context);

// Returns a formatted version of |url| appropriate to display to a user.
// When re-parsing this URL, clients should call url_formatter::FixupURL().
base::string16 FormatBookmarkURLForDisplay(const GURL& url);

// Returns whether the Apps shortcut is enabled. If true, then the visibility
// of the Apps shortcut should be controllable via an item in the bookmark
// context menu.
bool IsAppsShortcutEnabled(Profile* profile);

// Returns true if the Apps shortcut should be displayed in the bookmark bar.
bool ShouldShowAppsShortcutInBookmarkBar(Profile* profile);

// Whether the menu item and shortcut to bookmark a tab should be removed from
// the user interface.
bool ShouldRemoveBookmarkThisTabUI(Profile* profile);

// Whether the menu item and shortcut to bookmark all tabs should be removed
// from the user interface.
bool ShouldRemoveBookmarkAllTabsUI(Profile* profile);

// Returns the drag operations for the specified node.
int GetBookmarkDragOperation(content::BrowserContext* browser_context,
                             const bookmarks::BookmarkNode* node);

// Calculates the drop operation given |source_operations| and the ideal
// set of drop operations (|operations|). This prefers the following ordering:
// COPY, LINK then MOVE.
int GetPreferredBookmarkDropOperation(int source_operations, int operations);

// Returns the preferred drop operation on a bookmark menu/bar.
// |parent| is the parent node the drop is to occur on and |index| the index the
// drop is over.
int GetBookmarkDropOperation(Profile* profile,
                             const ui::DropTargetEvent& event,
                             const bookmarks::BookmarkNodeData& data,
                             const bookmarks::BookmarkNode* parent,
                             size_t index);

// Returns true if the bookmark data can be dropped on |drop_parent| at
// |index|. A drop from a separate profile is always allowed, where as
// a drop from the same profile is only allowed if none of the nodes in
// |data| are an ancestor of |drop_parent| and one of the nodes isn't already
// a child of |drop_parent| at |index|.
bool IsValidBookmarkDropLocation(Profile* profile,
                                 const bookmarks::BookmarkNodeData& data,
                                 const bookmarks::BookmarkNode* drop_parent,
                                 size_t index);

#if defined(TOOLKIT_VIEWS)
// |text_color| is the color of associated text and is used to derive the icon's
// color.
gfx::ImageSkia GetBookmarkFolderIcon(SkColor text_color);
gfx::ImageSkia GetBookmarkManagedFolderIcon(SkColor text_color);
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_
