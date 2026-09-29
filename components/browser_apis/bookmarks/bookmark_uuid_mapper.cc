// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_uuid_mapper.h"

#include <array>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks_api {

namespace {

// Formats `input` as a name-based (version 5) UUID, e.g.
// "xxxxxxxx-xxxx-5xxx-yxxx-xxxxxxxxxxxx".
base::Uuid FormatAsV5Uuid(base::span<const uint8_t, 16> input) {
  std::array<uint8_t, 16> bytes;
  base::span(bytes).copy_from(input);
  bytes[6] = (bytes[6] & 0x0f) | 0x50;  // Version 5.
  bytes[8] = (bytes[8] & 0x3f) | 0x80;  // RFC 9562 variant.

  std::string uuid = base::HexEncodeLower(bytes);
  // Insert dashes back to front so earlier offsets stay valid (8-4-4-4-12).
  for (size_t pos : {20, 16, 12, 8}) {
    uuid.insert(pos, "-");
  }
  return base::Uuid::ParseLowercase(uuid);
}

// Prefixes that identify each BookmarkStorage in the derivation below.
//
// WARNING: Changing these values could cause issues with clients that
// assume the stability of the UUIDs.
constexpr std::string_view kLocalStoragePrefix = "local";
constexpr std::string_view kAccountStoragePrefix = "account";

constexpr std::string_view GetStoragePrefix(BookmarkStorage storage) {
  switch (storage) {
    case BookmarkStorage::kLocal:
      return kLocalStoragePrefix;
    case BookmarkStorage::kAccount:
      return kAccountStoragePrefix;
  }
}

// API UUIDs are derived from each node's storage and native UUID:
//
//   api_uuid = UUIDv5-style SHA-1(storage_prefix + ":" + native_uuid)
//
// where storage_prefix comes from GetStoragePrefix(), e.g. "local".
//
// Why not something simpler:
// - Random UUIDs (the previous approach) change every session. Clients persist
//   API UUIDs (e.g. chrome://bookmarks stores folder open state and puts the
//   selected folder in the URL), so random ids broke both after a restart.
// - The native UUID alone isn't unique: local and account storage may contain
//   nodes with the same UUID. In particular, the local and account permanent
//   folders share hard-coded UUIDs. The storage prefix keeps them apart.
//   Within a single storage, native UUIDs are unique (duplicates are replaced
//   when the bookmarks file is loaded).
// - The int64 model id is persisted and normally stable, but it isn't used
//   because it changes when account storage is rebuilt (sign-out then sign-in
//   re-downloads account bookmarks with their UUIDs but fresh ids), and when
//   the model reassigns colliding ids on load.
// - Hashing means API UUIDs don't look like native UUIDs, so clients can't
//   mistake one for the other.
base::Uuid DeriveApiUuid(const bookmarks::BookmarkNode* node,
                         BookmarkStorage storage) {
  const std::string name = base::StrCat(
      {GetStoragePrefix(storage), ":", node->uuid().AsLowercaseString()});
  const base::SHA1Digest digest = base::SHA1Hash(base::as_byte_span(name));
  return FormatAsV5Uuid(base::span(digest).first<16>());
}

}  // namespace

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

base::Uuid BookmarkUuidMapper::GetUuidFor(const bookmarks::BookmarkNode* node,
                                          BookmarkStorage storage) {
  const BookmarkIdTuple tuple(node);
  CHECK(tuple.uuid().is_valid());
  auto it = tuple_to_uuid_.find(tuple);
  if (it != tuple_to_uuid_.end()) {
    return it->second;
  }

  base::Uuid api_uuid = DeriveApiUuid(node, storage);
  // Only possible if the model violates UUID uniqueness within a storage, or a
  // node that was moved across storages is still cached. Stay unique at the
  // cost of stability for this node.
  if (uuid_to_tuple_.contains(api_uuid)) {
    api_uuid = base::Uuid::GenerateRandomV4();
  }
  tuple_to_uuid_[tuple] = api_uuid;
  uuid_to_tuple_[api_uuid] = tuple;
  return api_uuid;
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
