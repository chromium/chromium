// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkDragDropTest : public testing::Test {
 public:
  BookmarkDragDropTest() : model_(nullptr) {}

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<bookmarks::BookmarkModel> model_;
};

TEST_F(BookmarkDragDropTest, DropBookmarksWithCopyFromSameProfile) {
  // Adds a url node along with a folder containing another url node.
  const bookmarks::BookmarkNode* bb_node = model_->bookmark_bar_node();
  model_->AddURL(bb_node, 0, u"c", GURL("about:blank"));
  const bookmarks::BookmarkNode* folder =
      model_->AddFolder(bb_node, 1, u"folder");
  const bookmarks::BookmarkNode* folder_child_node =
      model_->AddURL(folder, 0, u"child", GURL("https://foo.com"));
  bookmarks::BookmarkNodeData bookmark_node_data(folder_child_node);
  bookmark_node_data.SetOriginatingProfilePath(profile_->GetPath());
  // Make a copy of `folder_child_node` added to the bookmark bar node.
  chrome::DropBookmarks(profile_.get(), bookmark_node_data, bb_node, 0, true,
                        chrome::BookmarkReorderDropTarget::kBookmarkBarView);
  ASSERT_EQ(3u, bb_node->children().size());
  const bookmarks::BookmarkNode* newly_copied_node =
      bb_node->children()[0].get();
  EXPECT_EQ(folder_child_node->GetTitle(), newly_copied_node->GetTitle());
  EXPECT_EQ(folder_child_node->url(), newly_copied_node->url());
}
