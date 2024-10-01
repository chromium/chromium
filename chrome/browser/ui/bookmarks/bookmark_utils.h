// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_

#include <string>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;

namespace bookmarks {
class BookmarkNode;
class ManagedBookmarkService;
struct BookmarkNodeData;
}

namespace content {
class BrowserContext;
class WebContents;
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

// Fills in the URL and title for a bookmark of |web_contents|. If this function
// returns false, there was no valid URL and neither |url| nor |title| have been
// modified.
bool GetURLAndTitleToBookmark(content::WebContents* web_contents,
                              GURL* url,
                              std::u16string* title);

// Toggles whether the bookmark bar is shown only on the new tab page or on
// all tabs. This is a preference modifier, not a visual modifier.
void ToggleBookmarkBarWhenVisible(content::BrowserContext* browser_context);

// Returns a formatted version of |url| appropriate to display to a user.
// When re-parsing this URL, clients should call url_formatter::FixupURL().
std::u16string FormatBookmarkURLForDisplay(const GURL& url);

// Returns whether the Apps shortcut is enabled. If true, then the visibility
// of the Apps shortcut should be controllable via an item in the bookmark
// context menu.
bool IsAppsShortcutEnabled(Profile* profile);

// Returns true if the Apps shortcut should be displayed in the bookmark bar.
bool ShouldShowAppsShortcutInBookmarkBar(Profile* profile);

// Returns true if the tab groups should be displayed in the bookmark bar.
bool ShouldShowTabGroupsInBookmarkBar(Profile* profile);

// Returns true if the reading list should be displayed in the bookmark bar.
bool ShouldShowReadingListInBookmarkBar(Profile* profile);

// Returns the drag operations for the specified node.
int GetBookmarkDragOperation(content::BrowserContext* browser_context,
                             const bookmarks::BookmarkNode* node);

// Calculates the drop operation given |source_operations| and the ideal
// set of drop operations (|operations|). This prefers the following ordering:
// `DragOperation::kCopy`, `DragOperation::kLink` then `DragOperation::kMove`.
ui::mojom::DragOperation GetPreferredBookmarkDropOperation(
    int source_operations,
    int operations);

// Returns the preferred drop operation on a bookmark menu/bar.
// |parent| is the parent node the drop is to occur on and |index| the index the
// drop is over.
ui::mojom::DragOperation GetBookmarkDropOperation(
    Profile* profile,
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

// Returns true if all the |nodes| can be edited by the user, which means they
// aren't enterprise-managed, as per `ManagedBookmarkService::IsNodeManaged()`.
bool CanAllBeEditedByUser(
    bookmarks::ManagedBookmarkService* managed_bookmark_service,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes);

#if defined(TOOLKIT_VIEWS)
enum class BookmarkFolderIconType {
  kNormal,
  kManaged,
};
ui::ImageModel GetBookmarkFolderIcon(BookmarkFolderIconType icon_type,
                                     absl::variant<ui::ColorId, SkColor> color);

// returns the vector image used for bookmarks folder.
gfx::ImageSkia GetBookmarkFolderImageFromVectorIcon(
    BookmarkFolderIconType icon_type,
    absl::variant<ui::ColorId, SkColor> color,
    ui::ColorProvider* color_provider);
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_
