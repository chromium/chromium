// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_editor.h"

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

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

}  // namespace
