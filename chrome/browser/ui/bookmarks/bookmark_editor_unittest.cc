// Copyright 2013 The Chromium Authors. All rights reserved.
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

TEST(BookmarkEditorTest, ApplyEditsWithNoFolderChange) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  const BookmarkNode* bookmarkbar = model->bookmark_bar_node();
  model->AddURL(bookmarkbar, 0, ASCIIToUTF16("url0"), GURL("chrome://newtab"));
  model->AddURL(bookmarkbar, 1, ASCIIToUTF16("url1"), GURL("chrome://newtab"));

  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 1));
    BookmarkEditor::ApplyEditsWithNoFolderChange(model.get(),
                                                 bookmarkbar,
                                                 detail,
                                                 ASCIIToUTF16("folder0"),
                                                 GURL(std::string()));
    EXPECT_EQ(ASCIIToUTF16("folder0"), bookmarkbar->children()[1]->GetTitle());
  }
  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, size_t{-1}));
    BookmarkEditor::ApplyEditsWithNoFolderChange(model.get(),
                                                 bookmarkbar,
                                                 detail,
                                                 ASCIIToUTF16("folder1"),
                                                 GURL(std::string()));
    EXPECT_EQ(ASCIIToUTF16("folder1"), bookmarkbar->children()[3]->GetTitle());
  }
  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 10));
    BookmarkEditor::ApplyEditsWithNoFolderChange(model.get(),
                                                 bookmarkbar,
                                                 detail,
                                                 ASCIIToUTF16("folder2"),
                                                 GURL(std::string()));
    EXPECT_EQ(ASCIIToUTF16("folder2"), bookmarkbar->children()[4]->GetTitle());
  }
}

}  // namespace
