// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_uuid_mapper.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks_api {

BookmarkUuidMapper::BookmarkUuidMapper() = default;
BookmarkUuidMapper::~BookmarkUuidMapper() = default;

void BookmarkUuidMapper::SetUuidOverride(const bookmarks::BookmarkNode* node,
                                         const base::Uuid& uuid) {
  const bookmarks::BookmarkNode& node_ref = CHECK_DEREF(node);
  CHECK(uuid.is_valid());
  node_id_to_uuid_[node_ref.id()] = uuid;
  uuid_to_node_id_[uuid] = node_ref.id();
}

bool BookmarkUuidMapper::HasOverrideFor(
    const bookmarks::BookmarkNode* node) const {
  const bookmarks::BookmarkNode& node_ref = CHECK_DEREF(node);
  return node_id_to_uuid_.contains(node_ref.id());
}

base::Uuid BookmarkUuidMapper::GetUuidFor(
    const bookmarks::BookmarkNode* node) const {
  const bookmarks::BookmarkNode& node_ref = CHECK_DEREF(node);
  auto it = node_id_to_uuid_.find(node_ref.id());
  if (it != node_id_to_uuid_.end()) {
    return it->second;
  }
  CHECK(node_ref.uuid().is_valid());
  return node_ref.uuid();
}

std::optional<int64_t> BookmarkUuidMapper::MaybeGetIdFromUuidOverride(
    const base::Uuid& uuid) const {
  auto it = uuid_to_node_id_.find(uuid);
  if (it != uuid_to_node_id_.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace bookmarks_api
