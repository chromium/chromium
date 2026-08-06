// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_UUID_MAPPER_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_UUID_MAPPER_H_

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/uuid.h"

namespace bookmarks {
class BookmarkNode;
}

namespace bookmarks_api {

// Represents the underlying bookmark model identity, consisting of the node's
// native storage UUID and its unique numeric int64_t model ID.
class BookmarkIdTuple {
 public:
  BookmarkIdTuple() = default;
  BookmarkIdTuple(const base::Uuid& uuid, int64_t id) : uuid_(uuid), id_(id) {}
  explicit BookmarkIdTuple(const bookmarks::BookmarkNode* node);

  const base::Uuid& uuid() const { return uuid_; }
  int64_t id() const { return id_; }

  bool operator==(const BookmarkIdTuple& other) const {
    return id_ == other.id_ && uuid_ == other.uuid_;
  }
  bool operator!=(const BookmarkIdTuple& other) const {
    return !(*this == other);
  }
  bool operator<(const BookmarkIdTuple& other) const {
    if (id_ != other.id_) {
      return id_ < other.id_;
    }
    return uuid_ < other.uuid_;
  }

 private:
  base::Uuid uuid_;
  int64_t id_ = 0;
};

struct BookmarkIdTupleHash {
  size_t operator()(const BookmarkIdTuple& tuple) const {
    return base::UuidHash()(tuple.uuid()) ^
           (std::hash<int64_t>()(tuple.id()) << 1);
  }
};

// Manages the indirection layer between external API UUIDs and underlying
// bookmark model identities (BookmarkIdTuple).
//
// In Chromium's multi-tree / dual-storage BookmarkModel (where local and
// account partitions may share duplicate storage GUIDs), this class guarantees
// that every bookmark node exposed across the Mojo / WebUI / Extension API
// surface has a strictly unique, collision-free API UUID.
//
// Key Invariants:
// 1. 1:1 Bi-directional Mapping: Every BookmarkIdTuple maps to exactly one API
//    UUID, and every API UUID maps to at most one BookmarkIdTuple.
// 2. UUIDs are never recycled from the underlying BookmarkNode. A new UUID is
//    generated for each BookmarkIdTuple.
// 3. Local Disambiguation: If a local node's native GUID collides with an
//    account node or an existing API UUID, a unique random V4 API UUID is
//    assigned to the local node without modifying the underlying storage model.
class BookmarkUuidMapper {
 public:
  BookmarkUuidMapper();
  ~BookmarkUuidMapper();

  // Explicitly sets an API UUID override for `node`. `node` must not be null.
  void SetUuidOverride(const bookmarks::BookmarkNode* node,
                       const base::Uuid& api_uuid);

  // Returns true if an explicit API UUID override has been set.
  bool HasOverrideFor(const bookmarks::BookmarkNode* node) const;

  // Returns the API UUID for `node`. If an API UUID or explicit override has
  // already been assigned, returns it. Otherwise assigns and returns a unique
  // API UUID (generating a unique random V4 UUID).
  base::Uuid GetUuidFor(const bookmarks::BookmarkNode* node);

  // Returns the underlying BookmarkIdTuple associated with `api_uuid` if
  // mapped.
  std::optional<BookmarkIdTuple> MaybeGetModelId(
      const base::Uuid& api_uuid) const;

  // Convenience helper returning the numeric int64_t ID associated with
  // `api_uuid`.
  std::optional<int64_t> MaybeGetIdFromUuidOverride(
      const base::Uuid& api_uuid) const;

  // Removes mapping for `node` when a node is deleted.
  void RemoveNode(const bookmarks::BookmarkNode* node);

  // Clears all mappings except for the specified `nodes_to_retain`.
  void ClearAllExcept(
      const std::vector<const bookmarks::BookmarkNode*>& nodes_to_retain);

  // Clears all mappings.
  void Clear();

 private:
  void SetUuidOverride(const BookmarkIdTuple& tuple,
                       const base::Uuid& api_uuid);
  bool HasOverrideFor(const BookmarkIdTuple& tuple) const;
  base::Uuid GetUuidFor(const BookmarkIdTuple& tuple);
  void RemoveNode(const BookmarkIdTuple& tuple);

  std::unordered_map<BookmarkIdTuple, base::Uuid, BookmarkIdTupleHash>
      tuple_to_uuid_;
  std::unordered_map<base::Uuid, BookmarkIdTuple, base::UuidHash>
      uuid_to_tuple_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_UUID_MAPPER_H_
