// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_pasteboard_helper_mac.h"

#import <AppKit/AppKit.h>

#include <vector>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "url/gurl.h"

namespace bookmarks {
namespace {

constexpr size_t kSafeDepth = 10;
constexpr size_t kTooDeepDepth = 600;

BookmarkNodeData::Element MakeNestedElement(size_t depth) {
  BookmarkNodeData::Element element;
  element.is_url = false;
  element.title = u"Folder";

  if (depth == 0) {
    BookmarkNodeData::Element url;
    url.is_url = true;
    url.title = u"Example";
    url.url = GURL("https://example.com/");
    element.children.push_back(url);
    return element;
  }

  element.children.push_back(MakeNestedElement(depth - 1));
  return element;
}

NSDictionary* MakeNestedBookmarkDictionary(size_t depth) {
  NSDictionary* current = @{
    @"Title" : @"Example",
    @"URLString" : @"https://example.com/",
    @"WebBookmarkType" : @"WebBookmarkTypeLeaf",
    @"ChromiumBookmarkId" : @0,
    @"ChromiumBookmarkMetaInfo" : @{}
  };

  for (size_t i = 0; i < depth; ++i) {
    current = @{
      @"Title" : @"Folder",
      @"Children" : @[ current ],
      @"WebBookmarkType" : @"WebBookmarkTypeList",
      @"ChromiumBookmarkId" : @0,
      @"ChromiumBookmarkMetaInfo" : @{}
    };
  }

  return current;
}

TEST(BookmarkPasteboardHelperMacTest,
     WriteBookmarksToPasteboardAllowsModerateHierarchy) {
  NSPasteboard* pb = [NSPasteboard pasteboardWithUniqueName];

  std::vector<BookmarkNodeData::Element> elements;
  elements.push_back(MakeNestedElement(kSafeDepth));

  WriteBookmarksToPasteboard(pb, elements, base::FilePath(),
                             /*is_off_the_record=*/false);

  EXPECT_TRUE(PasteboardContainsBookmarks(pb));
}

TEST(BookmarkPasteboardHelperMacTest,
     WriteBookmarksToPasteboardRejectsOverlyDeepHierarchy) {
  NSPasteboard* pb = [NSPasteboard pasteboardWithUniqueName];

  std::vector<BookmarkNodeData::Element> elements;
  elements.push_back(MakeNestedElement(kTooDeepDepth));

  WriteBookmarksToPasteboard(pb, elements, base::FilePath(),
                             /*is_off_the_record=*/false);

  EXPECT_FALSE(PasteboardContainsBookmarks(pb));
}

TEST(BookmarkPasteboardHelperMacTest,
     ReadBookmarksFromPasteboardRejectsOverlyDeepHierarchy) {
  NSPasteboard* pb = [NSPasteboard pasteboardWithUniqueName];
  NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
  [item setPropertyList:@[ MakeNestedBookmarkDictionary(kTooDeepDepth) ]
                forType:ui::kUTTypeChromiumBookmarkDictionaryList];
  [item setString:@"" forType:@"org.chromium.profile-path"];

  [pb clearContents];
  ASSERT_TRUE([pb writeObjects:@[ item ]]);

  std::vector<BookmarkNodeData::Element> elements;
  base::FilePath profile_path;
  EXPECT_FALSE(ReadBookmarksFromPasteboard(pb, &elements, &profile_path));
  EXPECT_TRUE(elements.empty());
}

}  // namespace
}  // namespace bookmarks
