// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node.h"

#include <map>
#include <string>

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

}  // namespace

// BookmarkNode ---------------------------------------------------------------

// static
const int64_t BookmarkNode::kInvalidSyncTransactionVersion = -1;

BookmarkNode::BookmarkNode(const GURL& url) : BookmarkNode(0, url, false) {}

BookmarkNode::BookmarkNode(int64_t id, const GURL& url)
    : BookmarkNode(id, url, false) {}

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

BookmarkNode::BookmarkNode(int64_t id, const GURL& url, bool is_permanent_node)
    : id_(id),
      url_(url),
      type_(url_.is_empty() ? FOLDER : URL),
      date_added_(base::Time::Now()),
      favicon_type_(favicon_base::IconType::kInvalid),
      is_permanent_node_(is_permanent_node) {}

void BookmarkNode::InvalidateFavicon() {
  icon_url_.reset();
  favicon_ = gfx::Image();
  favicon_type_ = favicon_base::IconType::kInvalid;
  favicon_state_ = INVALID_FAVICON;
}

// BookmarkPermanentNode -------------------------------------------------------

BookmarkPermanentNode::BookmarkPermanentNode(int64_t id)
    : BookmarkNode(id, GURL(), true) {}

BookmarkPermanentNode::~BookmarkPermanentNode() = default;

bool BookmarkPermanentNode::IsVisible() const {
  return visible_ || !empty();
}

}  // namespace bookmarks
