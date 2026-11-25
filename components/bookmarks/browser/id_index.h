// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_ID_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_ID_INDEX_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace bookmarks {

// Used to compare BookmarkNode instances by integer ID.
class NodeIdEquality {
 public:
  using is_transparent = void;

  bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) const {
    return n1->id() == n2->id();
  }

  bool operator()(const BookmarkNode* n1, int64_t id2) const {
    return n1->id() == id2;
  }

  bool operator()(int64_t id1, const BookmarkNode* n2) const {
    return id1 == n2->id();
  }
};

// Used to hash BookmarkNode instances by integer ID. Uses Abseil's default
// hash mixer rather than `std::hash<int64_t>` (which is the identity function
// in libc++ and causes SwissTable H2 tag clustering on sequential IDs).
class NodeIdHash {
 public:
  using is_transparent = void;

  size_t operator()(const BookmarkNode* n) const { return (*this)(n->id()); }

  size_t operator()(int64_t id) const {
    return absl::DefaultHashContainerHash<int64_t>()(id);
  }
};

using IdIndex = absl::
    flat_hash_set<raw_ptr<const BookmarkNode>, NodeIdHash, NodeIdEquality>;

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_ID_INDEX_H_
