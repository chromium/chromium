// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/combined_bookmarks_view.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;

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
  EXPECT_EQ(view().GetChildren(view().GetRootNode()).size(), 3u);
  EXPECT_TRUE(view().IsPermanentNode(model().bookmark_bar_node()));

  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 ");
  auto children = view().GetChildren(model().bookmark_bar_node());
  ASSERT_EQ(children.size(), 2u);
  EXPECT_EQ(children[0]->GetTitle(), u"1");
  EXPECT_EQ(children[1]->GetTitle(), u"2");
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
  model().CreateAccountPermanentFolders();
  auto account_view =
      std::make_unique<CombinedBookmarksView>(&model(), nullptr);

  auto children = account_view->GetChildren(account_view->GetRootNode());
  EXPECT_EQ(children.size(), 6u);

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

}  // namespace
