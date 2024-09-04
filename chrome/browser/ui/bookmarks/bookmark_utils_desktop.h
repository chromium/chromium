// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_DESKTOP_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_DESKTOP_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
struct NavigateParams;

namespace bookmarks {
class BookmarkNode;
}

namespace content {
class BrowserContext;
class NavigationHandle;
}

namespace chrome {

// Wraps bookmark navigations to support view testing.
class BookmarkNavigationWrapper {
 public:
  virtual ~BookmarkNavigationWrapper() = default;

  // Wraps browser_navigator::Navigate.
  virtual base::WeakPtr<content::NavigationHandle> NavigateTo(
      NavigateParams* params);

  // Provide an instance for use in testing.
  static void SetInstanceForTesting(BookmarkNavigationWrapper* instance);
};

using TabGroupData =
    std::pair<std::optional<tab_groups::TabGroupId>, std::u16string>;

// Number of bookmarks we'll open before prompting the user to see if they
// really want to open all.
//
// NOTE: treat this as a const. It is not const so unit tests can change the
// value.
extern size_t kNumBookmarkUrlsBeforePrompting;

// Tries to open all bookmarks in `nodes`. If there are many, prompts
// the user first. Returns immediately, opening the bookmarks
// asynchronously if prompting the user. `browser` is the browser from
// which the bookmarks were opened. Its window is used as the anchor for
// the dialog (if shown).
// `launch_action` represents the location and time of the bookmark launch
// action for callsites that support it.
// TODO(crbug.com/40914589): This should be made non-optional once all callsites
// have all the information needed to correctly construct the `launch_action`.
void OpenAllIfAllowed(
    Browser* browser,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
    WindowOpenDisposition initial_disposition,
    bool add_to_group,
    page_load_metrics::NavigationHandleUserData::InitiatorLocation
        navigation_type = page_load_metrics::NavigationHandleUserData::
            InitiatorLocation::kOther,
    std::optional<BookmarkLaunchAction> launch_action = std::nullopt);

// Returns the count of bookmarks that would be opened by OpenAll. If
// |incognito_context| is set, the function will use it to check if the URLs
// can be opened in incognito mode, which may affect the count.
int OpenCount(gfx::NativeWindow parent,
              const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                        VectorExperimental>>& nodes,
              content::BrowserContext* incognito_context = nullptr);

// Convenience for OpenCount() with a single BookmarkNode.
int OpenCount(gfx::NativeWindow parent,
              const bookmarks::BookmarkNode* node,
              content::BrowserContext* incognito_context = nullptr);

// Asks the user before deleting a non-empty bookmark folder.
bool ConfirmDeleteBookmarkNode(gfx::NativeWindow window,
                               const bookmarks::BookmarkNode* node);

// Shows the bookmark all tabs dialog.
void ShowBookmarkAllTabsDialog(Browser* browser);

// Returns true if OpenAll() can open at least one bookmark of type url
// in |selection|.
bool HasBookmarkURLs(const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                               VectorExperimental>>& selection);

// Returns true if OpenAll() can open at least one bookmark of type url
// in |selection| with incognito mode.
bool HasBookmarkURLsAllowedInIncognitoMode(
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& selection,
    content::BrowserContext* browser_context);

// Populates |folder_data| with all tab items and sub-folders for any open tab
// groups.
void GetURLsAndFoldersForTabEntries(
    std::vector<BookmarkEditor::EditDetails::BookmarkData>* folder_data,
    std::vector<std::pair<GURL, std::u16string>> tab_entries,
    base::flat_map<int, TabGroupData> groups_by_index);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_DESKTOP_H_
