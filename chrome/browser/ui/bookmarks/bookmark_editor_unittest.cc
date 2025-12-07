// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_editor.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using testing::ElementsAre;
using testing::Pointer;

namespace {

const BookmarkNode* InitBookmarkBar(BookmarkModel* model) {
  const BookmarkNode* bookmarkbar = model->bookmark_bar_node();
  model->AddURL(bookmarkbar, 0, u"url0", GURL("chrome://newtab"));
  model->AddURL(bookmarkbar, 1, u"url1", GURL("chrome://newtab"));
  return bookmarkbar;
}

TEST(BookmarkEditorTest, ApplyEditsWithNoFolderChange) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  const BookmarkNode* bookmarkbar = InitBookmarkBar(model.get());
  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 1));
    BookmarkEditor::ApplyEdits(model.get(), bookmarkbar, detail, u"folder0",
                               GURL(std::string()));
    EXPECT_EQ(u"folder0", bookmarkbar->children()[1]->GetTitle());
  }
  {
    BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::AddFolder(
        bookmarkbar, static_cast<size_t>(-1)));
    BookmarkEditor::ApplyEdits(model.get(), bookmarkbar, detail, u"folder1",
                               GURL(std::string()));
    EXPECT_EQ(u"folder1", bookmarkbar->children()[3]->GetTitle());
  }
  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 10));
    BookmarkEditor::ApplyEdits(model.get(), bookmarkbar, detail, u"folder2",
                               GURL(std::string()));
    EXPECT_EQ(u"folder2", bookmarkbar->children()[4]->GetTitle());
  }
}

TEST(BookmarkEditorTest, ApplyEditsWithMultipleURLs) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  const BookmarkNode* bookmarkbar = InitBookmarkBar(model.get());
  BookmarkEditor::EditDetails detail(
      BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 2));
  const std::u16string url_title_0 = u"url_0";
  const std::u16string url_title_1 = u"url_1";
  BookmarkEditor::EditDetails::BookmarkData url_data_0;
  url_data_0.url = std::make_optional(GURL("chrome://newtab"));
  url_data_0.title = url_title_0;
  BookmarkEditor::EditDetails::BookmarkData url_data_1;
  url_data_1.url = std::make_optional(GURL("chrome://newtab"));
  url_data_1.title = url_title_1;
  detail.bookmark_data.children.push_back(url_data_0);
  detail.bookmark_data.children.push_back(url_data_1);
  BookmarkEditor::ApplyEdits(model.get(), bookmarkbar, detail, u"folder",
                             GURL(std::string()));
  EXPECT_EQ(u"folder", bookmarkbar->children()[2]->GetTitle());
  EXPECT_EQ(url_title_0, bookmarkbar->children()[2]->children()[0]->GetTitle());
  EXPECT_EQ(url_title_1, bookmarkbar->children()[2]->children()[1]->GetTitle());
}

TEST(BookmarkEditorTest, ApplyEditsWithNestedFolder) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  const BookmarkNode* bookmarkbar = InitBookmarkBar(model.get());
  BookmarkEditor::EditDetails detail(
      BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 2));
  const std::u16string nested_folder_title = u"nested_folder";
  const std::u16string nested_url_title = u"nested_url";
  BookmarkEditor::EditDetails::BookmarkData url_data;
  url_data.url = std::make_optional(GURL("chrome://newtab"));
  url_data.title = nested_url_title;
  BookmarkEditor::EditDetails::BookmarkData folder_data;
  folder_data.title = nested_folder_title;
  folder_data.children.push_back(url_data);
  detail.bookmark_data.children.push_back(folder_data);

  BookmarkEditor::ApplyEdits(model.get(), bookmarkbar, detail, u"folder",
                             GURL(std::string()));
  EXPECT_EQ(u"folder", bookmarkbar->children()[2]->GetTitle());
  EXPECT_EQ(nested_folder_title,
            bookmarkbar->children()[2]->children()[0]->GetTitle());
  EXPECT_EQ(
      nested_url_title,
      bookmarkbar->children()[2]->children()[0]->children()[0]->GetTitle());
}

TEST(BookmarkEditorTest, ApplyEditsWithURLsAndNestedFolders) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  const BookmarkNode* bookmarkbar = InitBookmarkBar(model.get());
  BookmarkEditor::EditDetails detail(
      BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 10));
  const std::u16string nested_folder_title_0 = u"nested_folder_0";
  const std::u16string nested_folder_title_1 = u"nested_folder_1";
  const std::u16string nested_folder_title_2 = u"nested_folder_2";
  const std::u16string url_title_0 = u"url_0";
  const std::u16string url_title_1 = u"url_1";
  const std::u16string nested_url_title = u"nested_url";
  BookmarkEditor::EditDetails::BookmarkData url_data_0;
  url_data_0.url = std::make_optional(GURL("chrome://newtab"));
  url_data_0.title = url_title_0;
  BookmarkEditor::EditDetails::BookmarkData url_data_1;
  url_data_1.url = std::make_optional(GURL("chrome://newtab"));
  url_data_1.title = url_title_1;
  BookmarkEditor::EditDetails::BookmarkData nested_url_data;
  nested_url_data.url = std::make_optional(GURL("chrome://newtab"));
  nested_url_data.title = nested_url_title;
  BookmarkEditor::EditDetails::BookmarkData folder_data_0;
  folder_data_0.title = nested_folder_title_0;
  folder_data_0.children.push_back(nested_url_data);
  BookmarkEditor::EditDetails::BookmarkData folder_data_1;
  folder_data_1.title = nested_folder_title_1;
  folder_data_1.children.push_back(nested_url_data);
  BookmarkEditor::EditDetails::BookmarkData folder_data_2;
  folder_data_2.title = nested_folder_title_2;
  folder_data_2.children.push_back(nested_url_data);

  detail.bookmark_data.children.push_back(folder_data_0);
  detail.bookmark_data.children.push_back(url_data_0);
  detail.bookmark_data.children.push_back(folder_data_1);
  detail.bookmark_data.children.push_back(folder_data_2);
  detail.bookmark_data.children.push_back(url_data_1);

  BookmarkEditor::ApplyEdits(model.get(), bookmarkbar, detail, u"folder",
                             GURL(std::string()));
  EXPECT_EQ(u"folder", bookmarkbar->children()[2]->GetTitle());
  EXPECT_EQ(nested_folder_title_0,
            bookmarkbar->children()[2]->children()[0]->GetTitle());
  EXPECT_EQ(url_title_0, bookmarkbar->children()[2]->children()[1]->GetTitle());
  EXPECT_EQ(nested_folder_title_1,
            bookmarkbar->children()[2]->children()[2]->GetTitle());
  EXPECT_EQ(nested_folder_title_2,
            bookmarkbar->children()[2]->children()[3]->GetTitle());
  EXPECT_EQ(url_title_1, bookmarkbar->children()[2]->children()[4]->GetTitle());
}

TEST(BookmarkEditorTest, ApplyEditsMoveBookmarks) {
  base::test::ScopedFeatureList override_features{
      switches::kSyncEnableBookmarksInTransportMode};

  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  model->CreateAccountPermanentFolders();

  const BookmarkNode* local_node = model->AddURL(
      model->bookmark_bar_node(), 0, u"title", GURL("chrome://newtab"));
  const BookmarkNode* account_node = model->AddURL(
      model->account_bookmark_bar_node(), 0, u"title", GURL("chrome://newtab"));

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(Pointer(local_node)));
  ASSERT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(Pointer(account_node)));

  BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::MoveNodes(
      model.get(), {local_node, account_node}));
  BookmarkEditor::ApplyEdits(model.get(), model->account_other_node(), detail,
                             std::u16string(), GURL());

  EXPECT_TRUE(model->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model->account_bookmark_bar_node()->children().empty());
  EXPECT_THAT(model->account_other_node()->children(),
              ElementsAre(Pointer(local_node), Pointer(account_node)));
}

TEST(BookmarkEditorTest, ApplyEditsMoveAccountBookmarks) {
  base::test::ScopedFeatureList override_features{
      switches::kSyncEnableBookmarksInTransportMode};

  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  model->CreateAccountPermanentFolders();

  const BookmarkNode* folder =
      model->AddFolder(model->account_bookmark_bar_node(), 0, u"folder");
  const BookmarkNode* node = model->AddURL(
      model->account_bookmark_bar_node(), 1, u"title", GURL("chrome://newtab"));

  ASSERT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(Pointer(folder), Pointer(node)));

  BookmarkEditor::EditDetails detail(
      BookmarkEditor::EditDetails::MoveNodes(model.get(), {folder, node}));
  BookmarkEditor::ApplyEdits(model.get(), model->other_node(), detail,
                             std::u16string(), GURL());

  EXPECT_TRUE(model->account_bookmark_bar_node()->children().empty());
  EXPECT_THAT(model->other_node()->children(),
              ElementsAre(Pointer(folder), Pointer(node)));
}

TEST(BookmarkEditorTest, ApplyEditsMoveLocalBookmarks) {
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"folder");
  const BookmarkNode* node = model->AddURL(model->bookmark_bar_node(), 1,
                                           u"title", GURL("chrome://newtab"));

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(Pointer(folder), Pointer(node)));

  BookmarkEditor::EditDetails detail(
      BookmarkEditor::EditDetails::MoveNodes(model.get(), {folder, node}));
  BookmarkEditor::ApplyEdits(model.get(), model->other_node(), detail,
                             std::u16string(), GURL());

  EXPECT_TRUE(model->bookmark_bar_node()->children().empty());
  EXPECT_THAT(model->other_node()->children(),
              ElementsAre(Pointer(folder), Pointer(node)));
}

TEST(BookmarkEditorTest, MoveNodesDefaultLocation) {
  base::test::ScopedFeatureList features(
      switches::kSyncEnableBookmarksInTransportMode);

  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();

  // Bookmark tree will have the following structure:
  //  LocalBookmarkBar
  //    local_folder
  //      local_nested_node
  //    local_node
  //  AccountBookmarkBar
  //    account_folder
  //      account_nested_node
  //    account_node
  const BookmarkNode* local_folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"folder");
  const BookmarkNode* local_node = model->AddURL(
      model->bookmark_bar_node(), 0, u"title", GURL(u"chrome://newtab"));
  const BookmarkNode* local_nested_node =
      model->AddURL(local_folder, 0, u"title", GURL(u"chrome://newtab"));

  const BookmarkNode* account_folder =
      model->AddFolder(model->account_bookmark_bar_node(), 0, u"folder");
  const BookmarkNode* account_node =
      model->AddURL(model->account_bookmark_bar_node(), 0, u"title",
                    GURL(u"chrome://newtab"));
  const BookmarkNode* account_nested_node =
      model->AddURL(account_folder, 0, u"title", GURL(u"chrome://newtab"));

  // Move nodes with the same local parent.
  {
    BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::MoveNodes(
        model.get(), {local_folder, local_node}));
    EXPECT_EQ(model->bookmark_bar_node(), detail.parent_node);
  }

  // Move nodes with the same account parent.
  {
    BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::MoveNodes(
        model.get(), {account_folder, account_node}));
    EXPECT_EQ(model->account_bookmark_bar_node(), detail.parent_node);
  }

  // Move only local nodes.
  {
    BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::MoveNodes(
        model.get(), {local_node, local_nested_node}));
    EXPECT_EQ(model->other_node(), detail.parent_node);
  }

  // Move only account nodes.
  {
    BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::MoveNodes(
        model.get(), {account_node, account_nested_node}));
    EXPECT_EQ(model->account_other_node(), detail.parent_node);
  }

  // Move both local and account nodes.
  {
    BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::MoveNodes(
        model.get(), {local_node, account_nested_node}));
    EXPECT_EQ(model->account_other_node(), detail.parent_node);
  }
}

TEST(BookmarkEditorTest, ApplyEditsPersistOrderAfterMove) {
  base::test::ScopedFeatureList override_features{
      switches::kSyncEnableBookmarksInTransportMode};

  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  model->CreateAccountPermanentFolders();

  const BookmarkNode* local_node1 = model->AddURL(
      model->bookmark_bar_node(), 0, u"title", GURL("chrome://newtab"));
  const BookmarkNode* local_node2 = model->AddURL(
      model->bookmark_bar_node(), 1, u"title", GURL("chrome://newtab"));
  const BookmarkNode* local_node3 = model->AddURL(
      model->bookmark_bar_node(), 2, u"title", GURL("chrome://newtab"));

  const BookmarkNode* account_node1 = model->AddURL(
      model->account_other_node(), 0, u"title", GURL("chrome://newtab"));
  const BookmarkNode* account_node2 = model->AddURL(
      model->account_other_node(), 1, u"title", GURL("chrome://newtab"));
  const BookmarkNode* account_node3 = model->AddURL(
      model->account_other_node(), 2, u"title", GURL("chrome://newtab"));

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(Pointer(local_node1), Pointer(local_node2),
                          Pointer(local_node3)));
  ASSERT_THAT(model->account_other_node()->children(),
              ElementsAre(Pointer(account_node1), Pointer(account_node2),
                          Pointer(account_node3)));

  // Make the selection in a random order. The order after moving should however
  // remain the same as in the initial parent node.
  BookmarkEditor::EditDetails detail(BookmarkEditor::EditDetails::MoveNodes(
      model.get(), {account_node2, local_node1, local_node3, account_node3,
                    account_node1, local_node2}));
  BookmarkEditor::ApplyEdits(model.get(), model->other_node(), detail,
                             std::u16string(), GURL());

  // Since the account nodes were added more recently, they should be rowed
  // last.
  EXPECT_THAT(model->other_node()->children(),
              ElementsAre(Pointer(local_node1), Pointer(local_node2),
                          Pointer(local_node3), Pointer(account_node1),
                          Pointer(account_node2), Pointer(account_node3)));
}

// Make sure tab group to folder creates a bookmark folder.
TEST(BookmarkEditorTest, TabGroupToFolder) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  const BookmarkNode* bookmarkbar = InitBookmarkBar(model.get());

  std::u16string title = u"tab group";
  BookmarkEditor::EditDetails detail(
      BookmarkEditor::EditDetails::TabGroupToFolder(bookmarkbar, 1, title));
  BookmarkEditor::ApplyEdits(model.get(), bookmarkbar, detail, title,
                             GURL(std::string()));
  EXPECT_EQ(title, bookmarkbar->children()[1]->GetTitle());
}

}  // namespace
