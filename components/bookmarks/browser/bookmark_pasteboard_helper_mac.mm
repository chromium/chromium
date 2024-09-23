// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_pasteboard_helper_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
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
        NSString* key_ns = base::apple::ObjCCast<NSString>(key);
        NSString* value_ns = base::apple::ObjCCast<NSString>(value);
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
        base::apple::ObjCCast<NSString>(bookmark_dict[kWebBookmarkTypeKey]);
    if (!type)
      continue;

    BOOL is_folder = [type isEqualToString:kWebBookmarkTypeList];

    GURL url = GURL();
    if (!is_folder) {
      NSString* url_string =
          base::apple::ObjCCast<NSString>(bookmark_dict[kURLStringKey]);
      if (!url_string)
        continue;
      url = GURL(base::SysNSStringToUTF8(url_string));
    }

    auto new_node = std::make_unique<BookmarkNode>(
        /*id=*/0, base::Uuid::GenerateRandomV4(), url);

    NSNumber* node_id =
        base::apple::ObjCCast<NSNumber>(bookmark_dict[kChromiumBookmarkIdKey]);
    if (node_id)
      new_node->set_id(node_id.longLongValue);

    NSDictionary* meta_info = base::apple::ObjCCast<NSDictionary>(
        bookmark_dict[kChromiumBookmarkMetaInfoKey]);
    if (meta_info)
      new_node->SetMetaInfoMap(MetaInfoMapFromDictionary(meta_info));

    NSString* title = base::apple::ObjCCast<NSString>(bookmark_dict[kTitleKey]);
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

bool ReadChromiumBookmarks(NSPasteboard* pb,
                           std::vector<BookmarkNodeData::Element>* elements) {
  id bookmarks = [pb propertyListForType:kUTTypeChromiumBookmarkDictionaryList];
  if (!bookmarks)
    return false;

  NSArray* bookmarks_array = base::apple::ObjCCast<NSArray>(bookmarks);
  if (!bookmarks_array)
    return false;

  ConvertNSArrayToElements(bookmarks_array, elements);
  return true;
}

bool ReadStandardBookmarks(NSPasteboard* pb,
                           std::vector<BookmarkNodeData::Element>* elements) {
  NSArray<URLAndTitle*>* urls_and_titles =
      ui::clipboard_util::URLsAndTitlesFromPasteboard(pb,
                                                      /*include_files=*/false);

  if (!urls_and_titles.count) {
    return false;
  }

  for (URLAndTitle* url_and_title in urls_and_titles) {
    std::string url = base::SysNSStringToUTF8(url_and_title.URL);
    std::u16string title = base::SysNSStringToUTF16(url_and_title.title);
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

// Transforms a list of bookmark nodes into an `NSArray` of `NSDictionaries`
// encoding them.
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

void CollectUrlsAndTitlesOfBookmarks(
    const std::vector<BookmarkNodeData::Element>& elements,
    NSMutableArray* url_titles,
    NSMutableArray* urls) {
  for (const auto& element : elements) {
    NSString* title = base::SysUTF16ToNSString(element.title);
    if (element.is_url) {
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      [url_titles addObject:title];
      [urls addObject:url];
    } else {
      CollectUrlsAndTitlesOfBookmarks(element.children, url_titles, urls);
    }
  }
}

// Generates a list of pasteboard items representing bookmarks. Note that the
// special items are included only on the first of the items.
NSArray<NSPasteboardItem*>* PasteboardItemsFromBookmarks(
    const std::vector<BookmarkNodeData::Element>& elements,
    const base::FilePath& profile_path) {
  // Bookmarks are encoded into pasteboard items in two ways:
  //
  // 1. As a flat array of pasteboard items, one for each of the bookmarks. This
  //    loses the hierarchical information, but this makes the bookmark drags
  //    interoperable with other applications on the system.
  // 2. As a plist and path containing full information about everything.
  //
  // The OS pasteboard provides support for multiple items, so the array of
  // items created as part of step 1 is set to be the items on the pasteboard.
  // Blobs of data that are only useful to Chromium are added to the first item
  // to go along for the ride.

  // 1. The flat array of URLs for interoperability.

  NSMutableArray* url_titles = [NSMutableArray array];
  NSMutableArray* urls = [NSMutableArray array];
  CollectUrlsAndTitlesOfBookmarks(elements, url_titles, urls);

  NSArray<NSPasteboardItem*>* items =
      ui::clipboard_util::PasteboardItemsFromUrls(urls, url_titles);

  // 2. The plist and path for Chromium use.

  if (!items.count) {
    // There were no bookmark URLs encoded, therefore the elements being encoded
    // consist of bookmark folders. The data for those folders will be contained
    // in the Chromium-specific data, so make a single pasteboard item to hold
    // it.
    items = @[ [[NSPasteboardItem alloc] init] ];
  }

  [items.firstObject setPropertyList:GetNSArrayForBookmarkList(elements)
                             forType:kUTTypeChromiumBookmarkDictionaryList];

  [items.firstObject setString:base::SysUTF8ToNSString(profile_path.value())
                       forType:kUTTypeChromiumProfilePath];

  return items;
}

}  // namespace

void WriteBookmarksToPasteboard(
    NSPasteboard* pb,
    const std::vector<BookmarkNodeData::Element>& elements,
    const base::FilePath& profile_path,
    bool is_off_the_record) {
  if (elements.empty()) {
    return;
  }

  NSArray<NSPasteboardItem*>* items =
      PasteboardItemsFromBookmarks(elements, profile_path);
  [pb clearContents];
  if (is_off_the_record) {
    // Make the pasteboard content current host only.
    [pb prepareForNewContentsWithOptions:NSPasteboardContentsCurrentHostOnly];
  }
  [pb writeObjects:items];
}

bool ReadBookmarksFromPasteboard(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>* elements,
    base::FilePath* profile_path) {
  elements->clear();
  NSString* profile = [pb stringForType:kUTTypeChromiumProfilePath];
  *profile_path = base::FilePath(base::SysNSStringToUTF8(profile));

  // Corresponding to the two types of data written above in
  // `PasteboardItemsFromBookmarks()`, first attempt to read the Chromium-only
  // data that has more fidelity, and then fall back to reading standard URL
  // types.

  return ReadChromiumBookmarks(pb, elements) ||
         ReadStandardBookmarks(pb, elements);
}

bool PasteboardContainsBookmarks(NSPasteboard* pb) {
  NSArray* availableTypes = @[
    ui::kUTTypeWebKitWebURLsWithTitles,
    kUTTypeChromiumBookmarkDictionaryList,
    NSPasteboardTypeURL,
  ];
  return [pb availableTypeFromArray:availableTypes] != nil;
}

}  // namespace bookmarks
