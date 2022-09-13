// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node.h"

#include <map>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace bookmarks {

namespace {

// Whitespace characters to strip from bookmark titles.
const char16_t kInvalidChars[] = {'\n',   '\r', '\t',
                                  0x2028,  // Line separator
                                  0x2029,  // Paragraph separator
                                  0};

}  // namespace

// BookmarkNode ---------------------------------------------------------------

// Below predefined GUIDs for permanent bookmark folders, determined via named
// GUIDs/UUIDs. Do NOT modify them as they may be exposed via Sync. For
// reference, here's the python script to produce them:
// > import uuid
// > chromium_namespace = uuid.uuid5(uuid.NAMESPACE_DNS, "chromium.org")
// > bookmarks_namespace = uuid.uuid5(chromium_namespace, "bookmarks")
// > root_guid = uuid.uuid5(bookmarks_namespace, "root")
// > bookmark_bar = uuid.uuid5(bookmarks_namespace, "bookmark_bar")
// > mobile_bookmarks = uuid.uuid5(bookmarks_namespace, "mobile_bookmarks")
// > other_bookmarks = uuid.uuid5(bookmarks_namespace, "other_bookmarks")
// > managed_bookmarks = uuid.uuid5(bookmarks_namespace, "managed_bookmarks")

// static
const char BookmarkNode::kRootNodeGuid[] =
    "2509a7dc-215d-52f7-a429-8d80431c6c75";

// static
const char BookmarkNode::kBookmarkBarNodeGuid[] =
    "0bc5d13f-2cba-5d74-951f-3f233fe6c908";

// static
const char BookmarkNode::kOtherBookmarksNodeGuid[] =
    "82b081ec-3dd3-529c-8475-ab6c344590dd";

// static
const char BookmarkNode::kMobileBookmarksNodeGuid[] =
    "4cf2e351-0e85-532b-bb37-df045d8f8d0f";

// static
const char BookmarkNode::kManagedNodeGuid[] =
    "323123f4-9381-5aee-80e6-ea5fca2f7672";

// This value is the result of exercising sync's function
// syncer::InferGuidForLegacyBookmark() with an empty input.
const char BookmarkNode::kBannedGuidDueToPastSyncBug[] =
    "da39a3ee-5e6b-fb0d-b255-bfef95601890";

BookmarkNode::BookmarkNode(int64_t id, const base::GUID& guid, const GURL& url)
    : BookmarkNode(id, guid, url, url.is_empty() ? FOLDER : URL, false) {}

BookmarkNode::~BookmarkNode() = default;

void BookmarkNode::SetTitle(const std::u16string& title) {
  // Replace newlines and other problematic whitespace characters in
  // folder/bookmark names with spaces.
  std::u16string trimmed_title;
  base::ReplaceChars(title, kInvalidChars, u" ", &trimmed_title);
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
    meta_info_map_ = std::make_unique<MetaInfoMap>();

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
    meta_info_map_ = std::make_unique<MetaInfoMap>(meta_info_map);
}

const BookmarkNode::MetaInfoMap* BookmarkNode::GetMetaInfoMap() const {
  return meta_info_map_.get();
}

bool BookmarkNode::GetUnsyncedMetaInfo(const std::string& key,
                                       std::string* value) const {
  if (!unsynced_meta_info_map_)
    return false;

  MetaInfoMap::const_iterator it = unsynced_meta_info_map_->find(key);
  if (it == unsynced_meta_info_map_->end())
    return false;

  *value = it->second;
  return true;
}

bool BookmarkNode::SetUnsyncedMetaInfo(const std::string& key,
                                       const std::string& value) {
  if (!unsynced_meta_info_map_)
    unsynced_meta_info_map_ = std::make_unique<MetaInfoMap>();

  auto it = unsynced_meta_info_map_->find(key);
  if (it == unsynced_meta_info_map_->end()) {
    (*unsynced_meta_info_map_)[key] = value;
    return true;
  }
  // Key already in map, check if the value has changed.
  if (it->second == value)
    return false;
  it->second = value;
  return true;
}

bool BookmarkNode::DeleteUnsyncedMetaInfo(const std::string& key) {
  if (!unsynced_meta_info_map_)
    return false;
  bool erased = unsynced_meta_info_map_->erase(key) != 0;
  if (unsynced_meta_info_map_->empty())
    unsynced_meta_info_map_.reset();
  return erased;
}

void BookmarkNode::SetUnsyncedMetaInfoMap(const MetaInfoMap& meta_info_map) {
  if (meta_info_map.empty())
    unsynced_meta_info_map_.reset();
  else
    unsynced_meta_info_map_ = std::make_unique<MetaInfoMap>(meta_info_map);
}

const BookmarkNode::MetaInfoMap* BookmarkNode::GetUnsyncedMetaInfoMap() const {
  return unsynced_meta_info_map_.get();
}

const std::u16string& BookmarkNode::GetTitledUrlNodeTitle() const {
  return GetTitle();
}

const GURL& BookmarkNode::GetTitledUrlNodeUrl() const {
  return url_;
}

std::vector<base::StringPiece16> BookmarkNode::GetTitledUrlNodeAncestorTitles()
    const {
  std::vector<base::StringPiece16> paths;
  for (const BookmarkNode* n = this; n->parent(); n = n->parent())
    paths.push_back(n->parent()->GetTitle());
  return paths;
}

BookmarkNode::BookmarkNode(int64_t id,
                           const base::GUID& guid,
                           const GURL& url,
                           Type type,
                           bool is_permanent_node)
    : id_(id),
      guid_(guid),
      url_(url),
      type_(type),
      date_added_(base::Time::Now()),
      is_permanent_node_(is_permanent_node) {
  DCHECK_NE(type == URL, url.is_empty());
  DCHECK(guid.is_valid());
  DCHECK_NE(guid.AsLowercaseString(), std::string(kBannedGuidDueToPastSyncBug));
}

void BookmarkNode::InvalidateFavicon() {
  icon_url_.reset();
  favicon_ = gfx::Image();
  favicon_state_ = INVALID_FAVICON;
}

// BookmarkPermanentNode -------------------------------------------------------

// static
std::unique_ptr<BookmarkPermanentNode>
BookmarkPermanentNode::CreateManagedBookmarks(int64_t id) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, FOLDER, base::GUID::ParseLowercase(kManagedNodeGuid),
      std::u16string(),
      /*visible_when_empty=*/false));
}

BookmarkPermanentNode::~BookmarkPermanentNode() = default;

bool BookmarkPermanentNode::IsVisible() const {
  return visible_when_empty_ || !children().empty();
}

// static
std::unique_ptr<BookmarkPermanentNode> BookmarkPermanentNode::CreateBookmarkBar(
    int64_t id,
    bool visible_when_empty) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, BOOKMARK_BAR, base::GUID::ParseLowercase(kBookmarkBarNodeGuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME),
      visible_when_empty));
}

// static
std::unique_ptr<BookmarkPermanentNode>
BookmarkPermanentNode::CreateOtherBookmarks(int64_t id,
                                            bool visible_when_empty) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, OTHER_NODE, base::GUID::ParseLowercase(kOtherBookmarksNodeGuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME),
      visible_when_empty));
}

// static
std::unique_ptr<BookmarkPermanentNode>
BookmarkPermanentNode::CreateMobileBookmarks(int64_t id,
                                             bool visible_when_empty) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, MOBILE, base::GUID::ParseLowercase(kMobileBookmarksNodeGuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME),
      visible_when_empty));
}

BookmarkPermanentNode::BookmarkPermanentNode(int64_t id,
                                             Type type,
                                             const base::GUID& guid,
                                             const std::u16string& title,
                                             bool visible_when_empty)
    : BookmarkNode(id, guid, GURL(), type, /*is_permanent_node=*/true),
      visible_when_empty_(visible_when_empty) {
  DCHECK(type != URL);
  SetTitle(title);
}

}  // namespace bookmarks
