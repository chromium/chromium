// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/common/bookmark_features.h"
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

BookmarkNode::BookmarkNode(int64_t id, const base::Uuid& uuid, const GURL& url)
    : BookmarkNode(id, uuid, url, url.is_empty() ? FOLDER : URL, false) {}

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

const std::u16string& BookmarkNode::GetTitledUrlNodeTitle() const {
  return GetTitle();
}

const GURL& BookmarkNode::GetTitledUrlNodeUrl() const {
  return url_;
}

std::vector<std::u16string_view> BookmarkNode::GetTitledUrlNodeAncestorTitles()
    const {
  std::vector<std::u16string_view> paths;
  for (const BookmarkNode* n = this; n->parent(); n = n->parent())
    paths.push_back(n->parent()->GetTitle());
  return paths;
}

BookmarkNode::BookmarkNode(int64_t id,
                           const base::Uuid& uuid,
                           const GURL& url,
                           Type type,
                           bool is_permanent_node)
    : id_(id),
      uuid_(uuid),
      url_(url),
      type_(type),
      date_added_(base::Time::Now()),
      is_permanent_node_(is_permanent_node) {
  DCHECK_NE(type == URL, url.is_empty());
  DCHECK(uuid.is_valid());
  DCHECK_NE(uuid.AsLowercaseString(), std::string(kBannedUuidDueToPastSyncBug));
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
      id, FOLDER, base::Uuid::ParseLowercase(kManagedNodeUuid),
      std::u16string()));
}

// static
bool BookmarkPermanentNode::IsTypeVisibleWhenEmpty(Type type) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  bool is_desktop = false;
#else
  bool is_desktop = true;
#endif

  switch (type) {
    case BookmarkNode::URL:
      NOTREACHED();
    case BookmarkNode::FOLDER:
      // Managed node.
      return false;
    case BookmarkNode::BOOKMARK_BAR:
      return is_desktop;
    case BookmarkNode::OTHER_NODE:
      return is_desktop || base::FeatureList::IsEnabled(
                               kAllBookmarksBaselineFolderVisibility);
    case BookmarkNode::MOBILE:
      // Either MOBILE or OTHER_NODE is visible when empty, but never both.
      return !IsTypeVisibleWhenEmpty(BookmarkNode::OTHER_NODE);
  }
  NOTREACHED();
}

BookmarkPermanentNode::~BookmarkPermanentNode() = default;

bool BookmarkPermanentNode::IsVisible() const {
  return visible_when_empty_ || !children().empty();
}

// static
std::unique_ptr<BookmarkPermanentNode> BookmarkPermanentNode::CreateBookmarkBar(
    int64_t id) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, BOOKMARK_BAR, base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME)));
}

// static
std::unique_ptr<BookmarkPermanentNode>
BookmarkPermanentNode::CreateOtherBookmarks(int64_t id) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, OTHER_NODE, base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME)));
}

// static
std::unique_ptr<BookmarkPermanentNode>
BookmarkPermanentNode::CreateMobileBookmarks(int64_t id) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, MOBILE, base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME)));
}

BookmarkPermanentNode::BookmarkPermanentNode(int64_t id,
                                             Type type,
                                             const base::Uuid& uuid,
                                             const std::u16string& title)
    : BookmarkNode(id, uuid, GURL(), type, /*is_permanent_node=*/true),
      visible_when_empty_(IsTypeVisibleWhenEmpty(type)) {
  CHECK(type != URL);
  SetTitle(title);
}

}  // namespace bookmarks
