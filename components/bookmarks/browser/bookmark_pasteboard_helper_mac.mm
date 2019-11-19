// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_pasteboard_helper_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_util_mac.h"

namespace bookmarks {

NSString* const kUTTypeChromiumBookmarkDictionaryList =
    @"org.chromium.bookmark-dictionary-list";

namespace {

// UTI used to store profile path to determine which profile a set of bookmarks
// came from.
NSString* const kUTTypeChromiumProfilePath = @"org.chromium.profile-path";

// Internal bookmark ID for a bookmark node.  Used only when moving inside of
// one profile.
NSString* const kChromiumBookmarkIdKey = @"ChromiumBookmarkId";

// Internal bookmark meta info dictionary for a bookmark node.
NSString* const kChromiumBookmarkMetaInfoKey = @"ChromiumBookmarkMetaInfo";

// Keys for the type of node in kUTTypeChromiumBookmarkDictionaryList.
NSString* const kWebBookmarkTypeKey = @"WebBookmarkType";
NSString* const kWebBookmarkTypeList = @"WebBookmarkTypeList";
NSString* const kWebBookmarkTypeLeaf = @"WebBookmarkTypeLeaf";

// Property keys.
NSString* const kTitleKey = @"Title";
NSString* const kURLStringKey = @"URLString";
NSString* const kChildrenKey = @"Children";

BookmarkNode::MetaInfoMap MetaInfoMapFromDictionary(NSDictionary* dictionary) {
  __block BookmarkNode::MetaInfoMap meta_info_map;

  [dictionary
      enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop) {
        NSString* key_ns = base::mac::ObjCCast<NSString>(key);
        NSString* value_ns = base::mac::ObjCCast<NSString>(value);
        if (key_ns && value_ns) {
          meta_info_map[base::SysNSStringToUTF8(key_ns)] =
              base::SysNSStringToUTF8(value_ns);
        }
      }];

  return meta_info_map;
}

NSDictionary* DictionaryFromBookmarkMetaInfo(
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];

  for (const auto& item : meta_info_map) {
    dictionary[base::SysUTF8ToNSString(item.first)] =
        base::SysUTF8ToNSString(item.second);
  }

  return dictionary;
}

void ConvertNSArrayToElements(
    NSArray* input,
    std::vector<BookmarkNodeData::Element>* elements) {
  for (NSDictionary* bookmark_dict in input) {
    NSString* type =
        base::mac::ObjCCast<NSString>(bookmark_dict[kWebBookmarkTypeKey]);
    if (!type)
      continue;

    BOOL is_folder = [type isEqualToString:kWebBookmarkTypeList];

    GURL url = GURL();
    if (!is_folder) {
      NSString* url_string =
          base::mac::ObjCCast<NSString>(bookmark_dict[kURLStringKey]);
      if (!url_string)
        continue;
      url = GURL(base::SysNSStringToUTF8(url_string));
    }

    auto new_node =
        std::make_unique<BookmarkNode>(/*id=*/0, base::GenerateGUID(), url);

    NSNumber* node_id =
        base::mac::ObjCCast<NSNumber>(bookmark_dict[kChromiumBookmarkIdKey]);
    if (node_id)
      new_node->set_id([node_id longLongValue]);

    NSDictionary* meta_info = base::mac::ObjCCast<NSDictionary>(
        bookmark_dict[kChromiumBookmarkMetaInfoKey]);
    if (meta_info)
      new_node->SetMetaInfoMap(MetaInfoMapFromDictionary(meta_info));

    NSString* title = base::mac::ObjCCast<NSString>(bookmark_dict[kTitleKey]);
    new_node->SetTitle(base::SysNSStringToUTF16(title));

    BookmarkNodeData::Element e = BookmarkNodeData::Element(new_node.get());
    // BookmarkNodeData::Element::ReadFromPickle explicitly zeroes out the two
    // date fields so do so too. TODO(avi): Refactor this code to be a member
    // function of BookmarkNodeData::Element so that it can write the id_ field
    // directly and avoid the round-trip through BookmarkNode.
    e.date_added = base::Time();
    e.date_folder_modified = base::Time();

    if (is_folder) {
      ConvertNSArrayToElements(bookmark_dict[kChildrenKey], &e.children);
    }

    elements->push_back(e);
  }
}

bool ReadBookmarkDictionaryListType(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>* elements) {
  id bookmarks = [pb propertyListForType:kUTTypeChromiumBookmarkDictionaryList];
  if (!bookmarks)
    return false;
  NSArray* bookmarks_array = base::mac::ObjCCast<NSArray>(bookmarks);
  if (!bookmarks_array)
    return false;

  ConvertNSArrayToElements(bookmarks_array, elements);
  return true;
}

bool ReadWebURLsWithTitlesPboardType(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>* elements) {
  NSArray* urls = nil;
  NSArray* titles = nil;
  if (!ui::ClipboardUtil::URLsAndTitlesFromPasteboard(pb, &urls, &titles))
    return false;

  NSUInteger len = [titles count];
  for (NSUInteger i = 0; i < len; ++i) {
    base::string16 title = base::SysNSStringToUTF16(titles[i]);
    std::string url = base::SysNSStringToUTF8(urls[i]);
    if (!url.empty()) {
      BookmarkNodeData::Element element;
      element.is_url = true;
      element.url = GURL(url);
      element.title = title;
      elements->push_back(element);
    }
  }
  return true;
}

NSArray* GetNSArrayForBookmarkList(
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSMutableArray* array = [NSMutableArray array];
  for (const auto& element : elements) {
    NSDictionary* meta_info =
        DictionaryFromBookmarkMetaInfo(element.meta_info_map);
    NSString* title = base::SysUTF16ToNSString(element.title);
    NSNumber* element_id = @(element.id());

    NSDictionary* object;
    if (element.is_url) {
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      object = @{
        kTitleKey : title,
        kURLStringKey : url,
        kWebBookmarkTypeKey : kWebBookmarkTypeLeaf,
        kChromiumBookmarkIdKey : element_id,
        kChromiumBookmarkMetaInfoKey : meta_info
      };
    } else {
      NSArray* children = GetNSArrayForBookmarkList(element.children);
      object = @{
        kTitleKey : title,
        kChildrenKey : children,
        kWebBookmarkTypeKey : kWebBookmarkTypeList,
        kChromiumBookmarkIdKey : element_id,
        kChromiumBookmarkMetaInfoKey : meta_info
      };
    }
    [array addObject:object];
  }
  return array;
}

void WriteBookmarkDictionaryListType(
    NSPasteboardItem* item,
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSArray* array = GetNSArrayForBookmarkList(elements);
  [item setPropertyList:array forType:kUTTypeChromiumBookmarkDictionaryList];
}

void FillFlattenedArraysForBookmarks(
    const std::vector<BookmarkNodeData::Element>& elements,
    NSMutableArray* url_titles,
    NSMutableArray* urls,
    NSMutableArray* toplevel_string_data) {
  for (const auto& element : elements) {
    NSString* title = base::SysUTF16ToNSString(element.title);
    if (element.is_url) {
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      [url_titles addObject:title];
      [urls addObject:url];
      if (toplevel_string_data)
        [toplevel_string_data addObject:url];
    } else {
      if (toplevel_string_data)
        [toplevel_string_data addObject:title];
      FillFlattenedArraysForBookmarks(element.children, url_titles, urls, nil);
    }
  }
}

base::scoped_nsobject<NSPasteboardItem> WriteSimplifiedBookmarkTypes(
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSMutableArray* url_titles = [NSMutableArray array];
  NSMutableArray* urls = [NSMutableArray array];
  NSMutableArray* toplevel_string_data = [NSMutableArray array];
  FillFlattenedArraysForBookmarks(
      elements, url_titles, urls, toplevel_string_data);

  base::scoped_nsobject<NSPasteboardItem> item;
  if ([urls count] > 0) {
    if ([urls count] == 1) {
      item = ui::ClipboardUtil::PasteboardItemFromUrl([urls firstObject],
                                                      [url_titles firstObject]);
    } else {
      item = ui::ClipboardUtil::PasteboardItemFromUrls(urls, url_titles);
    }
  }

  if (!item) {
    item.reset([[NSPasteboardItem alloc] init]);
  }

  [item setString:[toplevel_string_data componentsJoinedByString:@"\n"]
          forType:base::mac::CFToNSCast(kUTTypeUTF8PlainText)];
  return item;
}

NSPasteboardItem* PasteboardItemFromBookmarks(
    const std::vector<BookmarkNodeData::Element>& elements,
    const base::FilePath& profile_path) {
  base::scoped_nsobject<NSPasteboardItem> item =
      WriteSimplifiedBookmarkTypes(elements);

  WriteBookmarkDictionaryListType(item, elements);

  [item setString:base::SysUTF8ToNSString(profile_path.value())
          forType:kUTTypeChromiumProfilePath];
  return item.autorelease();
}

}  // namespace

void WriteBookmarksToPasteboard(
    NSPasteboard* pb,
    const std::vector<BookmarkNodeData::Element>& elements,
    const base::FilePath& profile_path) {
  if (elements.empty())
    return;

  NSPasteboardItem* item = PasteboardItemFromBookmarks(elements, profile_path);
  [pb clearContents];
  [pb writeObjects:@[ item ]];
}

bool ReadBookmarksFromPasteboard(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>* elements,
    base::FilePath* profile_path) {
  elements->clear();
  NSString* profile = [pb stringForType:kUTTypeChromiumProfilePath];
  *profile_path = base::FilePath(base::SysNSStringToUTF8(profile));
  return ReadBookmarkDictionaryListType(pb, elements) ||
         ReadWebURLsWithTitlesPboardType(pb, elements);
}

bool PasteboardContainsBookmarks(NSPasteboard* pb) {
  NSArray* availableTypes = @[
    ui::ClipboardUtil::UTIForWebURLsAndTitles(),
    kUTTypeChromiumBookmarkDictionaryList
  ];
  return [pb availableTypeFromArray:availableTypes] != nil;
}

}  // namespace bookmarks
