// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

namespace {

using bookmarks::BookmarkNode;

constexpr std::string_view kParentTitle = "Parent";
constexpr std::string_view kChildTitle = "Child";
constexpr std::string_view kSavedFolderTitle = "Saved folder";
constexpr std::string_view kShortcutTitle = "Shortcut to saved folder";

GURL BookmarksUrlWithId(int64_t id) {
  return GURL(
      base::StrCat({"chrome://bookmarks/?id=", base::NumberToString(id)}));
}

// These tests treat chrome://bookmarks as a black box. They set up bookmarks
// through the BookmarkModel and look at the page through its accessibility
// tree, the way a screen reader would.
class BookmarksUIBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    accessibility_mode_.emplace(ui::kAXModeComplete);
    bookmarks::test::WaitForBookmarkModelToLoad(model());
  }

  void TearDownOnMainThread() override {
    accessibility_mode_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  bookmarks::BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  }

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  const BookmarkNode* AddFolder(const BookmarkNode* parent,
                                std::string_view title) {
    return model()->AddFolder(parent, parent->children().size(),
                              base::UTF8ToUTF16(title));
  }

  const BookmarkNode* FindChildByTitle(const BookmarkNode* parent,
                                       std::string_view title) {
    for (const auto& child : parent->children()) {
      if (child->GetTitle() == base::UTF8ToUTF16(title)) {
        return child.get();
      }
    }
    return nullptr;
  }

  // Opens `url` and waits until the page has selected a folder.
  void OpenBookmarksPage(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return !GetSelectedFolderTitle().empty(); }));
  }

  // Waits until the page has replaced the URL it was opened with, and returns
  // the new one.
  GURL WaitForUrlToChangeFrom(const GURL& url) {
    EXPECT_TRUE(base::test::RunUntil(
        [&] { return web_contents()->GetLastCommittedURL() != url; }));
    return web_contents()->GetLastCommittedURL();
  }

  std::vector<ui::AXNodeData> GetAccessibleNodes(ax::mojom::Role role) {
    std::vector<ui::AXNodeData> nodes;
    for (const ui::AXNodeData& node :
         content::GetAccessibilityTreeSnapshot(web_contents()).nodes) {
      if (node.role == role) {
        nodes.push_back(node);
      }
    }
    return nodes;
  }

  // Returns the name of the folder selected in the folder tree, or an empty
  // string if there is none.
  std::string GetSelectedFolderTitle() {
    for (const ui::AXNodeData& folder :
         GetAccessibleNodes(ax::mojom::Role::kTreeItem)) {
      if (folder.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
        return folder.GetStringAttribute(ax::mojom::StringAttribute::kName);
      }
    }
    return std::string();
  }

  // Returns "expanded", "collapsed" or "not shown" for the folder with
  // `title` in the folder tree.
  std::string GetFolderState(std::string_view title) {
    for (const ui::AXNodeData& folder :
         GetAccessibleNodes(ax::mojom::Role::kTreeItem)) {
      if (folder.GetStringAttribute(ax::mojom::StringAttribute::kName) ==
          title) {
        return folder.HasState(ax::mojom::State::kExpanded) ? "expanded"
                                                            : "collapsed";
      }
    }
    return "";
  }

  // Returns the names of the rows in the bookmark list, once there is at
  // least one.
  std::vector<std::string> WaitForListedBookmarks() {
    std::vector<std::string> rows;
    EXPECT_TRUE(base::test::RunUntil([&] {
      rows.clear();
      for (const ui::AXNodeData& row :
           GetAccessibleNodes(ax::mojom::Role::kRow)) {
        rows.push_back(
            row.GetStringAttribute(ax::mojom::StringAttribute::kName));
      }
      return !rows.empty();
    }));
    return rows;
  }

 private:
  // Turns on accessibility while it's alive; without it the page has no
  // accessibility tree and the snapshots above are empty. It's optional
  // because it can only be created once the browser is up, which is after
  // this fixture is constructed.
  std::optional<content::ScopedAccessibilityModeOverride> accessibility_mode_;
};

// chrome://bookmarks/?id=<integer> URLs predate the UUID-based ids and are
// still generated by the browser (e.g. the bookmark bar context menu) and
// saved by users, so they must keep selecting the right folder.
IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest, LegacyIdSelectsFolder) {
  const BookmarkNode* folder = AddFolder(model()->other_node(), "Folder");

  OpenBookmarksPage(BookmarksUrlWithId(folder->id()));

  EXPECT_EQ(GetSelectedFolderTitle(), "Folder");
}

IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest, LegacyIdSelectsNestedFolder) {
  const BookmarkNode* parent =
      AddFolder(model()->bookmark_bar_node(), kParentTitle);
  const BookmarkNode* child = AddFolder(parent, kChildTitle);

  OpenBookmarksPage(BookmarksUrlWithId(child->id()));

  EXPECT_EQ(GetSelectedFolderTitle(), kChildTitle);
}

IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest,
                       UnknownLegacyIdSelectsBookmarkBar) {
  OpenBookmarksPage(BookmarksUrlWithId(9001));

  EXPECT_EQ(GetSelectedFolderTitle(),
            base::UTF16ToUTF8(model()->bookmark_bar_node()->GetTitle()));
}

// Opening a folder puts its numeric id in the URL. This is a public
// interface: users bookmark and share that URL, and automation reads the id
// from it and passes it to the chrome.bookmarks extension API, which uses the
// same ids as BookmarkNode::id(). Do not change the format. The URL must also
// keep working, both in the same session and after a restart.
IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest, FolderUrlUsesNumericId) {
  const BookmarkNode* folder =
      AddFolder(model()->bookmark_bar_node(), kSavedFolderTitle);

  // We want to check the URL the page writes for the folder, but opening
  // ?id=<id> directly gives us nothing to wait for if the page (correctly)
  // keeps it. The page drops query parameters it doesn't use, so add one to
  // force a rewrite we can wait for.
  const GURL opened_url(base::StrCat(
      {BookmarksUrlWithId(folder->id()).spec(), "&ignored-by-the-page=1"}));
  OpenBookmarksPage(opened_url);
  const GURL folder_url = WaitForUrlToChangeFrom(opened_url);
  EXPECT_EQ(folder_url, BookmarksUrlWithId(folder->id()));

  OpenBookmarksPage(GURL("chrome://bookmarks"));
  ASSERT_NE(GetSelectedFolderTitle(), kSavedFolderTitle);

  OpenBookmarksPage(folder_url);
  EXPECT_EQ(GetSelectedFolderTitle(), kSavedFolderTitle);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), folder_url);
}

IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest, UnknownUuidSelectsBookmarkBar) {
  AddFolder(model()->other_node(), "Folder");

  OpenBookmarksPage(
      GURL(base::StrCat({"chrome://bookmarks/?id=",
                         base::Uuid::GenerateRandomV4().AsLowercaseString()})));

  EXPECT_EQ(GetSelectedFolderTitle(),
            base::UTF16ToUTF8(model()->bookmark_bar_node()->GetTitle()));
}

IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest,
                       PRE_FolderShortcutWorksAfterRestart) {
  const BookmarkNode* folder =
      AddFolder(model()->bookmark_bar_node(), kSavedFolderTitle);

  model()->AddURL(model()->bookmark_bar_node(), 1,
                  base::UTF8ToUTF16(kShortcutTitle),
                  BookmarksUrlWithId(folder->id()));
}

IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest,
                       FolderShortcutWorksAfterRestart) {
  const BookmarkNode* shortcut =
      FindChildByTitle(model()->bookmark_bar_node(), kShortcutTitle);
  ASSERT_TRUE(shortcut);

  OpenBookmarksPage(shortcut->url());

  EXPECT_EQ(GetSelectedFolderTitle(), kSavedFolderTitle);
}

// The page remembers which folders are expanded in the tree, keyed by the ids
// it gets for them. This only works if a folder keeps its id across restarts.
IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest,
                       PRE_ExpandedFoldersStayExpandedAfterRestart) {
  const BookmarkNode* parent =
      AddFolder(model()->bookmark_bar_node(), kParentTitle);
  const BookmarkNode* child = AddFolder(parent, kChildTitle);

  // Selecting the child expands everything above it.
  OpenBookmarksPage(BookmarksUrlWithId(child->id()));

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetFolderState(kParentTitle) == "expanded"; }));
}

IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest,
                       ExpandedFoldersStayExpandedAfterRestart) {
  ASSERT_TRUE(FindChildByTitle(model()->bookmark_bar_node(), kParentTitle));

  OpenBookmarksPage(GURL("chrome://bookmarks"));

  EXPECT_EQ(GetFolderState(kParentTitle), "expanded");
}

// A search can be reloaded, bookmarked or shared through its URL.
IN_PROC_BROWSER_TEST_F(BookmarksUIBrowserTest, SearchUrlShowsMatches) {
  model()->AddURL(model()->bookmark_bar_node(), 0, u"Needle",
                  GURL("https://a.test/"));
  const BookmarkNode* folder = AddFolder(model()->other_node(), "Folder");
  model()->AddURL(folder, 0, u"Another needle", GURL("https://b.test/"));
  model()->AddURL(folder, 1, u"Haystack", GURL("https://c.test/"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://bookmarks/?q=needle")));

  EXPECT_THAT(
      WaitForListedBookmarks(),
      testing::UnorderedElementsAre(testing::HasSubstr("Needle"),
                                    testing::HasSubstr("Another needle")));
}

}  // namespace
