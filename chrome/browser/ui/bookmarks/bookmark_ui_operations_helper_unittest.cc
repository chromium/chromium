// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <type_traits>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace {

using bookmarks::BookmarkNode;

BookmarkParentFolder NodeToBookmarkParentFolder(const BookmarkNode* node) {
  CHECK(node);
  CHECK(node->is_folder());
  if (!node->is_permanent_node()) {
    return BookmarkParentFolder::FromNonPermanentNode(node);
  }

  switch (node->type()) {
    case bookmarks::BookmarkNode::URL:
      NOTREACHED();
    case bookmarks::BookmarkNode::FOLDER:
      return BookmarkParentFolder::ManagedFolder();
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
      return BookmarkParentFolder::BookmarkBarFolder();
    case bookmarks::BookmarkNode::OTHER_NODE:
      return BookmarkParentFolder::OtherFolder();
    case bookmarks::BookmarkNode::MOBILE:
      return BookmarkParentFolder::MobileFolder();
  }
  NOTREACHED();
}

template <class T>
class BookmarkUIOperationsHelperTest : public testing::Test {
 public:
  BookmarkUIOperationsHelperTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        BookmarkMergedSurfaceServiceFactory::GetInstance(),
        BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmark_merged_surface_service_ =
        BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_.get());
    model_->LoadEmptyForTest();
  }

  ~BookmarkUIOperationsHelperTest() override = default;

  TestingProfile* profile() { return profile_.get(); }
  bookmarks::BookmarkModel* model() { return model_; }

  internal::BookmarkUIOperationsHelper* CreateHelper(
      const BookmarkNode* parent) {
    if (std::is_same<T, BookmarkUIOperationsHelperNonMergedSurfaces>::value) {
      CHECK(parent->is_folder());
      helper_ = std::make_unique<BookmarkUIOperationsHelperNonMergedSurfaces>(
          model_.get(), parent);
    } else if (std::is_same<T,
                            BookmarkUIOperationsHelperMergedSurfaces>::value) {
      parent_folder_ = NodeToBookmarkParentFolder(parent);
      helper_ = std::make_unique<BookmarkUIOperationsHelperMergedSurfaces>(
          bookmark_merged_surface_service_.get(), &parent_folder_.value());
    }
    return helper_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_merged_surface_service_ =
      nullptr;
  std::optional<BookmarkParentFolder> parent_folder_;
  std::unique_ptr<internal::BookmarkUIOperationsHelper> helper_;
};

using testing::Types;

typedef Types<BookmarkUIOperationsHelperNonMergedSurfaces,
              BookmarkUIOperationsHelperMergedSurfaces>
    Implementations;

TYPED_TEST_SUITE(BookmarkUIOperationsHelperTest, Implementations);

TYPED_TEST(BookmarkUIOperationsHelperTest,
           DropBookmarksWithCopyFromSameProfile) {
  // Adds a url node along with a folder containing another url node.
  const bookmarks::BookmarkNode* bb_node = this->model()->bookmark_bar_node();
  this->model()->AddURL(bb_node, 0, u"c", GURL("about:blank"));
  const bookmarks::BookmarkNode* folder =
      this->model()->AddFolder(bb_node, 1, u"folder");
  const bookmarks::BookmarkNode* folder_child_node =
      this->model()->AddURL(folder, 0, u"child", GURL("https://foo.com"));
  bookmarks::BookmarkNodeData bookmark_node_data(folder_child_node);
  bookmark_node_data.SetOriginatingProfilePath(this->profile()->GetPath());

  internal::BookmarkUIOperationsHelper* helper = this->CreateHelper(bb_node);
  // Make a copy of `folder_child_node` added to the bookmark bar node.
  EXPECT_EQ(
      helper->DropBookmarks(
          this->profile(), bookmark_node_data, /*index=*/0,
          /*copy=*/true, chrome::BookmarkReorderDropTarget::kBookmarkBarView),
      ui::mojom::DragOperation::kCopy);
  ASSERT_EQ(3u, bb_node->children().size());
  const bookmarks::BookmarkNode* newly_copied_node =
      bb_node->children()[0].get();
  EXPECT_EQ(folder_child_node->GetTitle(), newly_copied_node->GetTitle());
  EXPECT_EQ(folder_child_node->url(), newly_copied_node->url());
}

TYPED_TEST(BookmarkUIOperationsHelperTest,
           DropBookmarksWithMoveFromSameProfile) {
  // Adds a url node along with a folder containing another url node.
  const bookmarks::BookmarkNode* bb_node = this->model()->bookmark_bar_node();
  this->model()->AddURL(bb_node, 0, u"c", GURL("about:blank"));
  const bookmarks::BookmarkNode* folder =
      this->model()->AddFolder(bb_node, 1, u"folder");
  const bookmarks::BookmarkNode* folder_child_node =
      this->model()->AddURL(folder, 0, u"child", GURL("https://foo.com"));
  bookmarks::BookmarkNodeData bookmark_node_data(folder_child_node);
  bookmark_node_data.SetOriginatingProfilePath(this->profile()->GetPath());

  internal::BookmarkUIOperationsHelper* helper =
      this->CreateHelper(this->model()->other_node());
  // Make a move of `folder_child_node` from bookmark bar node to other node.
  EXPECT_EQ(
      helper->DropBookmarks(
          this->profile(), bookmark_node_data, /*index=*/0,
          /*copy=*/false, chrome::BookmarkReorderDropTarget::kBookmarkBarView),
      ui::mojom::DragOperation::kMove);
  EXPECT_EQ(2u, bb_node->children().size());
  EXPECT_EQ(this->model()->other_node()->children()[0].get(),
            folder_child_node);
}

TYPED_TEST(BookmarkUIOperationsHelperTest, DropBookmarksFromAnotherProfile) {
  // Adds a url node along with a folder containing another url node.
  const bookmarks::BookmarkNode* bb_node = this->model()->bookmark_bar_node();
  this->model()->AddURL(bb_node, 0, u"c", GURL("about:blank"));
  const bookmarks::BookmarkNode* folder =
      this->model()->AddFolder(bb_node, 1, u"folder");
  const bookmarks::BookmarkNode* folder_child_node =
      this->model()->AddURL(folder, 0, u"child", GURL("https://foo.com"));
  bookmarks::BookmarkNodeData bookmark_node_data(folder_child_node);
  // Profile path is empty for `bookmark_node_data`.
  EXPECT_FALSE(
      bookmark_node_data.IsFromProfilePath(this->profile()->GetPath()));

  internal::BookmarkUIOperationsHelper* helper = this->CreateHelper(folder);

  // Use `copy` false as it shouldn't matter, as long as the data is not from
  // the same profile, data should be copied.
  EXPECT_EQ(
      helper->DropBookmarks(
          this->profile(), bookmark_node_data, /*index=*/0,
          /*copy=*/false, chrome::BookmarkReorderDropTarget::kBookmarkBarView),
      ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(folder->children().size(), 2u);
  const bookmarks::BookmarkNode* newly_copied_node =
      folder->children()[0].get();
  EXPECT_EQ(newly_copied_node->url(), folder_child_node->url());
  EXPECT_EQ(folder->children()[1].get(), folder_child_node);
}

}  // namespace
