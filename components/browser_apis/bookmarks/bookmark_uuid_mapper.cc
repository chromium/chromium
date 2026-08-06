// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_uuid_mapper.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks_api {

BookmarkIdTuple::BookmarkIdTuple(const bookmarks::BookmarkNode* node) {
  const bookmarks::BookmarkNode& node_ref = CHECK_DEREF(node);
  uuid_ = node_ref.uuid();
  id_ = node_ref.id();
}

BookmarkUuidMapper::BookmarkUuidMapper() = default;
BookmarkUuidMapper::~BookmarkUuidMapper() = default;

void BookmarkUuidMapper::SetUuidOverride(const bookmarks::BookmarkNode* node,
                                         const base::Uuid& api_uuid) {
  SetUuidOverride(BookmarkIdTuple(node), api_uuid);
}

void BookmarkUuidMapper::SetUuidOverride(const BookmarkIdTuple& tuple,
                                         const base::Uuid& api_uuid) {
  CHECK(api_uuid.is_valid());
  CHECK(!tuple_to_uuid_.contains(tuple));
  CHECK(!uuid_to_tuple_.contains(api_uuid));
  tuple_to_uuid_[tuple] = api_uuid;
  uuid_to_tuple_[api_uuid] = tuple;
}

bool BookmarkUuidMapper::HasOverrideFor(
    const bookmarks::BookmarkNode* node) const {
  return HasOverrideFor(BookmarkIdTuple(node));
}

bool BookmarkUuidMapper::HasOverrideFor(const BookmarkIdTuple& tuple) const {
  return tuple_to_uuid_.contains(tuple);
}

base::Uuid BookmarkUuidMapper::GetUuidFor(const bookmarks::BookmarkNode* node) {
  return GetUuidFor(BookmarkIdTuple(node));
}

base::Uuid BookmarkUuidMapper::GetUuidFor(const BookmarkIdTuple& tuple) {
  CHECK(tuple.uuid().is_valid());
  auto it = tuple_to_uuid_.find(tuple);
  if (it != tuple_to_uuid_.end()) {
    return it->second;
  }

  auto mapped_uuid = base::Uuid::GenerateRandomV4();
  tuple_to_uuid_[tuple] = mapped_uuid;
  uuid_to_tuple_[mapped_uuid] = tuple;
  return mapped_uuid;
}

std::optional<BookmarkIdTuple> BookmarkUuidMapper::MaybeGetModelId(
    const base::Uuid& api_uuid) const {
  auto it = uuid_to_tuple_.find(api_uuid);
  if (it != uuid_to_tuple_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<int64_t> BookmarkUuidMapper::MaybeGetIdFromUuidOverride(
    const base::Uuid& api_uuid) const {
  auto it = uuid_to_tuple_.find(api_uuid);
  if (it != uuid_to_tuple_.end()) {
    return it->second.id();
  }
  return std::nullopt;
}

void BookmarkUuidMapper::RemoveNode(const BookmarkIdTuple& tuple) {
  auto it = tuple_to_uuid_.find(tuple);
  if (it != tuple_to_uuid_.end()) {
    uuid_to_tuple_.erase(it->second);
    tuple_to_uuid_.erase(it);
  }
}

void BookmarkUuidMapper::RemoveNode(const bookmarks::BookmarkNode* node) {
  if (!node) {
    return;
  }
  for (const auto& child : node->children()) {
    RemoveNode(child.get());
  }
  RemoveNode(BookmarkIdTuple(node));
}

void BookmarkUuidMapper::ClearAllExcept(
    const std::vector<const bookmarks::BookmarkNode*>& nodes_to_retain) {
  std::unordered_map<BookmarkIdTuple, base::Uuid, BookmarkIdTupleHash>
      new_tuple_to_uuid;
  std::unordered_map<base::Uuid, BookmarkIdTuple, base::UuidHash>
      new_uuid_to_tuple;
  for (const auto* node : nodes_to_retain) {
    if (!node) {
      continue;
    }
    BookmarkIdTuple tuple(node);
    auto it = tuple_to_uuid_.find(tuple);
    if (it != tuple_to_uuid_.end()) {
      new_tuple_to_uuid[tuple] = it->second;
      new_uuid_to_tuple[it->second] = tuple;
    }
  }
  tuple_to_uuid_ = std::move(new_tuple_to_uuid);
  uuid_to_tuple_ = std::move(new_uuid_to_tuple);
}

void BookmarkUuidMapper::Clear() {
  tuple_to_uuid_.clear();
  uuid_to_tuple_.clear();
}

}  // namespace bookmarks_api
