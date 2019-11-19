// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node.h"

#include <map>
#include <string>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace bookmarks {

namespace {

// Whitespace characters to strip from bookmark titles.
const base::char16 kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

std::string PermanentNodeTypeToGuid(BookmarkNode::Type type) {
  switch (type) {
    case BookmarkNode::BOOKMARK_BAR:
      return BookmarkNode::kBookmarkBarNodeGuid;
    case BookmarkNode::OTHER_NODE:
      return BookmarkNode::kOtherBookmarksNodeGuid;
    case BookmarkNode::MOBILE:
      return BookmarkNode::kMobileBookmarksNodeGuid;
    case BookmarkNode::FOLDER:
      return BookmarkNode::kManagedNodeGuid;
    case BookmarkNode::URL:
      NOTREACHED();
      return std::string();
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

// BookmarkNode ---------------------------------------------------------------

// static
const int64_t BookmarkNode::kInvalidSyncTransactionVersion = -1;
const char BookmarkNode::kRootNodeGuid[] =
    "00000000-0000-4000-A000-000000000001";
const char BookmarkNode::kBookmarkBarNodeGuid[] =
    "00000000-0000-4000-A000-000000000002";
const char BookmarkNode::kOtherBookmarksNodeGuid[] =
    "00000000-0000-4000-A000-000000000003";
const char BookmarkNode::kMobileBookmarksNodeGuid[] =
    "00000000-0000-4000-A000-000000000004";
const char BookmarkNode::kManagedNodeGuid[] =
    "00000000-0000-4000-A000-000000000005";

std::string BookmarkNode::RootNodeGuid() {
  return BookmarkNode::kRootNodeGuid;
}

BookmarkNode::BookmarkNode(int64_t id, const std::string& guid, const GURL& url)
    : BookmarkNode(id, guid, url, url.is_empty() ? FOLDER : URL, false) {}

BookmarkNode::~BookmarkNode() = default;

void BookmarkNode::SetTitle(const base::string16& title) {
  // Replace newlines and other problematic whitespace characters in
  // folder/bookmark names with spaces.
  base::string16 trimmed_title;
  base::ReplaceChars(title, kInvalidChars, base::ASCIIToUTF16(" "),
                     &trimmed_title);
  ui::TreeNode<BookmarkNode>::SetTitle(trimmed_title);
}

bool BookmarkNode::IsVisible() const {
  return true;
}

bool BookmarkNode::GetMetaInfo(const std::string& key,
                               std::string* value) const {
  if (!meta_info_map_)
    return false;

  MetaInfoMap::const_iterator it = meta_info_map_->find(key);
  if (it == meta_info_map_->end())
    return false;

  *value = it->second;
  return true;
}

bool BookmarkNode::SetMetaInfo(const std::string& key,
                               const std::string& value) {
  if (!meta_info_map_)
    meta_info_map_.reset(new MetaInfoMap);

  auto it = meta_info_map_->find(key);
  if (it == meta_info_map_->end()) {
    (*meta_info_map_)[key] = value;
    return true;
  }
  // Key already in map, check if the value has changed.
  if (it->second == value)
    return false;
  it->second = value;
  return true;
}

bool BookmarkNode::DeleteMetaInfo(const std::string& key) {
  if (!meta_info_map_)
    return false;
  bool erased = meta_info_map_->erase(key) != 0;
  if (meta_info_map_->empty())
    meta_info_map_.reset();
  return erased;
}

void BookmarkNode::SetMetaInfoMap(const MetaInfoMap& meta_info_map) {
  if (meta_info_map.empty())
    meta_info_map_.reset();
  else
    meta_info_map_.reset(new MetaInfoMap(meta_info_map));
}

const BookmarkNode::MetaInfoMap* BookmarkNode::GetMetaInfoMap() const {
  return meta_info_map_.get();
}

const base::string16& BookmarkNode::GetTitledUrlNodeTitle() const {
  return GetTitle();
}

const GURL& BookmarkNode::GetTitledUrlNodeUrl() const {
  return url_;
}

BookmarkNode::BookmarkNode(int64_t id,
                           const std::string& guid,
                           const GURL& url,
                           Type type,
                           bool is_permanent_node)
    : id_(id),
      guid_(guid),
      url_(url),
      type_(type),
      date_added_(base::Time::Now()),
      favicon_type_(favicon_base::IconType::kInvalid),
      is_permanent_node_(is_permanent_node) {
  DCHECK((type == URL) != url.is_empty());
  DCHECK(base::IsValidGUID(guid));
}

void BookmarkNode::InvalidateFavicon() {
  icon_url_.reset();
  favicon_ = gfx::Image();
  favicon_type_ = favicon_base::IconType::kInvalid;
  favicon_state_ = INVALID_FAVICON;
}

// BookmarkPermanentNode -------------------------------------------------------

BookmarkPermanentNode::BookmarkPermanentNode(int64_t id, Type type)
    : BookmarkNode(id, PermanentNodeTypeToGuid(type), GURL(), type, true) {
  DCHECK(type != URL);
}

BookmarkPermanentNode::~BookmarkPermanentNode() = default;

bool BookmarkPermanentNode::IsVisible() const {
  return visible_ || !children().empty();
}

}  // namespace bookmarks
