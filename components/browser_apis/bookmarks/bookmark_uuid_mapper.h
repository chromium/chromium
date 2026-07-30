// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_UUID_MAPPER_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_UUID_MAPPER_H_

#include <cstdint>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/uuid.h"

namespace bookmarks {
class BookmarkNode;
}

namespace bookmarks_api {

// Utility class to manage UUID overrides for specific bookmark nodes.
class BookmarkUuidMapper {
 public:
  BookmarkUuidMapper();
  ~BookmarkUuidMapper();

  // Sets a UUID override for `node`. `node` must not be null.
  void SetUuidOverride(const bookmarks::BookmarkNode* node,
                       const base::Uuid& uuid);

  // Returns true if a UUID override has been set for `node`. `node` must not be
  // null.
  bool HasOverrideFor(const bookmarks::BookmarkNode* node) const;

  // Returns the UUID for `node`. Returns the override UUID if set for
  // `node->id()`, or `node->uuid()` (which must be valid). `node` must not be
  // null.
  base::Uuid GetUuidFor(const bookmarks::BookmarkNode* node) const;

  // Returns the node ID associated with `uuid` if an override was set for it.
  std::optional<int64_t> MaybeGetIdFromUuidOverride(
      const base::Uuid& uuid) const;

 private:
  base::flat_map<int64_t, base::Uuid> node_id_to_uuid_;
  base::flat_map<base::Uuid, int64_t> uuid_to_node_id_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_UUID_MAPPER_H_
