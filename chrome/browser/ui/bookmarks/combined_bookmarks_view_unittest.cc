// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/combined_bookmarks_view.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/browser_apis/bookmarks/bookmarks_view_observer.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;

class RecordingBookmarksViewObserver
    : public bookmarks_api::BookmarksViewObserver {
 public:
  void OnBookmarksEvents(
      bookmarks_api::BookmarksView* view,
      const std::vector<bookmarks_api::mojom::BookmarksEventPtr>& events)
      override {
    for (const auto& event : events) {
      events_.push_back(event.Clone());
    }
  }

  const std::vector<bookmarks_api::mojom::BookmarksEventPtr>& events() const {
    return events_;
  }
  void Clear() { events_.clear(); }

 private:
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events_;
};

class CombinedBookmarksViewTest : public testing::Test {
 public:
  CombinedBookmarksViewTest() {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    model_ = std::make_unique<bookmarks::BookmarkModel>(std::move(client));
    model_->LoadEmptyForTest();
    view_ = std::make_unique<CombinedBookmarksView>(model_.get(), nullptr);
  }

  bookmarks::BookmarkModel& model() { return *model_; }
  CombinedBookmarksView& view() { return *view_; }

 private:
  base::test::ScopedFeatureList features_{
      switches::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<CombinedBookmarksView> view_;
};

TEST_F(CombinedBookmarksViewTest, BasicTreeQueries) {
  ASSERT_TRUE(view().GetRootNode());
  EXPECT_NE(view().GetRootNode(), model().root_node());
  EXPECT_EQ(view().GetChildren(view().GetRootNode()).size(), 2u);
  EXPECT_TRUE(view().IsPermanentNode(model().bookmark_bar_node()));

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 ");
  auto children = view().GetChildren(model().bookmark_bar_node());
  ASSERT_EQ(children.size(), 2u);
  EXPECT_EQ(children[0]->GetTitle(), u"1");
  EXPECT_EQ(children[1]->GetTitle(), u"2");
}

TEST_F(CombinedBookmarksViewTest, PermanentNodeVisibility) {
  EXPECT_EQ(view().GetChildren(view().GetRootNode()).size(), 2u);
  // Adding a bookmark to the mobile node makes it visible.
  model().AddURL(model().mobile_node(), 0, u"Mobile BM",
                 GURL("http://example.com"));
  EXPECT_EQ(view().GetChildren(view().GetRootNode()).size(), 3u);
}

TEST_F(CombinedBookmarksViewTest, AddAndRemove) {
  const BookmarkNode* bar = model().bookmark_bar_node();
  const BookmarkNode* url_node =
      view().AddURL(bar, 0, u"Title", GURL("http://example.com"));
  ASSERT_TRUE(url_node);
  EXPECT_EQ(view().GetChildren(bar).size(), 1u);

  const BookmarkNode* folder_node = view().AddFolder(bar, 1, u"Folder");
  ASSERT_TRUE(folder_node);
  EXPECT_EQ(view().GetChildren(bar).size(), 2u);

  view().Remove(url_node, bookmarks::metrics::BookmarkEditSource::kUser,
                FROM_HERE);
  EXPECT_EQ(view().GetChildren(bar).size(), 1u);
}

TEST_F(CombinedBookmarksViewTest,
       UniqueUuidMappingForAccountAndLocalPermanentNodes) {
  model().AddURL(model().bookmark_bar_node(), 0, u"Local Bookmark",
                 GURL("http://example.com"));
  model().CreateAccountPermanentFolders();
  auto account_view =
      std::make_unique<CombinedBookmarksView>(&model(), nullptr);

  auto children = account_view->GetChildren(account_view->GetRootNode());
  EXPECT_EQ(children.size(), 4u);

  std::set<base::Uuid> uuids;
  for (const BookmarkNode* child : children) {
    base::Uuid uuid = account_view->GetUuid(child);
    EXPECT_TRUE(uuid.is_valid());
    EXPECT_TRUE(uuids.insert(uuid).second)
        << "Duplicate UUID found for node: " << child->GetTitle();
    auto found = account_view->FindNodeByUuid(uuid);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, child);
  }
}

TEST_F(CombinedBookmarksViewTest,
       AccountPermanentFoldersSignInAndSignOutEvents) {
  model().AddURL(model().bookmark_bar_node(), 0, u"Local Bookmark",
                 GURL("http://example.com"));

  RecordingBookmarksViewObserver observer;
  view().AddObserver(&observer);

  model().CreateAccountPermanentFolders();

  const base::Uuid root_uuid = view().GetUuid(view().GetRootNode());
  const base::Uuid account_bb_uuid =
      view().GetUuid(model().account_bookmark_bar_node());
  const base::Uuid account_other_uuid =
      view().GetUuid(model().account_other_node());
  ASSERT_EQ(observer.events().size(), 2u);
  ASSERT_TRUE(observer.events()[0]->is_added());
  EXPECT_EQ(observer.events()[0]->get_added()->parent_id, root_uuid);
  EXPECT_EQ(observer.events()[0]->get_added()->index, 2);
  EXPECT_EQ(observer.events()[0]->get_added()->node->get_folder()->id,
            account_bb_uuid);

  ASSERT_TRUE(observer.events()[1]->is_added());
  EXPECT_EQ(observer.events()[1]->get_added()->parent_id, root_uuid);
  EXPECT_EQ(observer.events()[1]->get_added()->index, 3);
  EXPECT_EQ(observer.events()[1]->get_added()->node->get_folder()->id,
            account_other_uuid);

  // Adding a bookmark to the initially invisible account mobile folder makes it
  // visible and should emit a single Added event for the mobile folder followed
  // by an Added event for the bookmark itself (rather than adding the mobile
  // folder twice).
  observer.Clear();
  const BookmarkNode* mobile_bm =
      model().AddURL(model().account_mobile_node(), 0, u"Account Mobile BM",
                     GURL("http://mobile.example.com"));

  const base::Uuid account_mobile_uuid =
      view().GetUuid(model().account_mobile_node());
  ASSERT_EQ(observer.events().size(), 2u);
  ASSERT_TRUE(observer.events()[0]->is_added());
  EXPECT_EQ(observer.events()[0]->get_added()->parent_id, root_uuid);
  EXPECT_EQ(observer.events()[0]->get_added()->index, 4);
  EXPECT_EQ(observer.events()[0]->get_added()->node->get_folder()->id,
            account_mobile_uuid);

  ASSERT_TRUE(observer.events()[1]->is_added());
  EXPECT_EQ(observer.events()[1]->get_added()->parent_id, account_mobile_uuid);
  EXPECT_EQ(observer.events()[1]->get_added()->index, 0);
  EXPECT_EQ(observer.events()[1]->get_added()->node->get_url()->id,
            view().GetUuid(mobile_bm));

  observer.Clear();
  model().RemoveAccountPermanentFolders();

  ASSERT_EQ(observer.events().size(), 3u);
  ASSERT_TRUE(observer.events()[0]->is_removed());
  EXPECT_EQ(observer.events()[0]->get_removed()->id, account_mobile_uuid);
  ASSERT_TRUE(observer.events()[1]->is_removed());
  EXPECT_EQ(observer.events()[1]->get_removed()->id, account_other_uuid);
  ASSERT_TRUE(observer.events()[2]->is_removed());
  EXPECT_EQ(observer.events()[2]->get_removed()->id, account_bb_uuid);
  EXPECT_FALSE(view().FindNodeByUuid(account_mobile_uuid).has_value());

  // Sign in and sign out again without adding any account mobile bookmarks:
  // the invisible account mobile folder should emit neither Added nor Removed,
  // and deterministic UUIDs must be preserved even if GetUuid() is queried
  // while the mobile folder is invisible.
  observer.Clear();
  model().CreateAccountPermanentFolders();
  ASSERT_EQ(observer.events().size(), 2u);
  EXPECT_EQ(view().GetUuid(model().account_bookmark_bar_node()),
            account_bb_uuid);
  EXPECT_EQ(view().GetUuid(model().account_other_node()), account_other_uuid);
  EXPECT_EQ(view().GetUuid(model().account_mobile_node()), account_mobile_uuid);

  observer.Clear();
  model().RemoveAccountPermanentFolders();
  ASSERT_EQ(observer.events().size(), 2u);
  ASSERT_TRUE(observer.events()[0]->is_removed());
  EXPECT_EQ(observer.events()[0]->get_removed()->id, account_other_uuid);
  ASSERT_TRUE(observer.events()[1]->is_removed());
  EXPECT_EQ(observer.events()[1]->get_removed()->id, account_bb_uuid);

  // Recreating account folders again must still yield the same deterministic
  // UUID for the mobile folder after it was evicted while invisible.
  model().CreateAccountPermanentFolders();
  EXPECT_EQ(view().GetUuid(model().account_mobile_node()), account_mobile_uuid);
  EXPECT_EQ(view().FindNodeByUuid(account_mobile_uuid),
            model().account_mobile_node());

  view().RemoveObserver(&observer);
}

TEST_F(CombinedBookmarksViewTest, ChangedInvisiblePermanentNodeIgnored) {
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkPermanentNode* managed_node = client->EnableManagedNode();
  auto managed_model =
      std::make_unique<bookmarks::BookmarkModel>(std::move(client));
  managed_model->LoadEmptyForTest();
  CombinedBookmarksView managed_view(managed_model.get(), nullptr);

  RecordingBookmarksViewObserver observer;
  managed_view.AddObserver(&observer);

  ASSERT_FALSE(managed_node->IsVisible());
  managed_model->SetTitle(managed_node, u"Managed Bookmarks",
                          bookmarks::metrics::BookmarkEditSource::kOther);
  EXPECT_TRUE(observer.events().empty());

  managed_view.RemoveObserver(&observer);
}

}  // namespace
