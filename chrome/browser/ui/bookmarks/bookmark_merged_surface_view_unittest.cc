// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_merged_surface_view.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
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

class BookmarkMergedSurfaceViewTest : public testing::Test {
 public:
  BookmarkMergedSurfaceViewTest() {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    model_ = std::make_unique<bookmarks::BookmarkModel>(std::move(client));
    service_ =
        std::make_unique<BookmarkMergedSurfaceService>(model_.get(), nullptr);
    model_->LoadEmptyForTest();
    service_->LoadForTesting({});
    view_ = std::make_unique<BookmarkMergedSurfaceView>(service_.get());
  }

  bookmarks::BookmarkModel& model() { return *model_; }
  BookmarkMergedSurfaceView& view() { return *view_; }

 private:
  base::test::ScopedFeatureList features_{
      switches::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkMergedSurfaceService> service_;
  std::unique_ptr<BookmarkMergedSurfaceView> view_;
};

TEST_F(BookmarkMergedSurfaceViewTest, BasicTreeQueries) {
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

TEST_F(BookmarkMergedSurfaceViewTest, AddAndRemove) {
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

}  // namespace
