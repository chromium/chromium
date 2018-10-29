// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_PASTEBOARD_HELPER_MAC_H_

#import <AppKit/AppKit.h>

#include "components/bookmarks/browser/bookmark_node_data.h"

namespace base {
class FilePath;
}

namespace bookmarks {

// Writes a set of bookmark elements from a profile to the specified pasteboard.
void WriteBookmarksToPasteboard(
    NSPasteboard* pb,
    const std::vector<BookmarkNodeData::Element>& elements,
    const base::FilePath& profile_path);

// Reads a set of bookmark elements from the specified pasteboard.
bool ReadBookmarksFromPasteboard(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>* elements,
    base::FilePath* profile_path);

// Returns true if the specified pasteboard contains any sort of bookmark
// elements. It currently does not consider a plaintext url a valid bookmark.
bool PasteboardContainsBookmarks(NSPasteboard* pb);

// UTI for dictionary containing bookmark structure consisting of individual
// bookmark nodes and/or bookmark folders.
extern NSString* const kUTTypeChromiumBookmarkDictionaryList;

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
