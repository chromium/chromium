// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_UUID_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_UUID_INDEX_H_

#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"

namespace bookmarks {

// Used to compare BookmarkNode instances by UUID.
class NodeUuidEquality {
 public:
  using is_transparent = void;

  bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) const {
    return n1->uuid() == n2->uuid();
  }

  bool operator()(const BookmarkNode* n1, const base::Uuid& uuid2) const {
    return n1->uuid() == uuid2;
  }

  bool operator()(const base::Uuid& uuid1, const BookmarkNode* n2) const {
    return uuid1 == n2->uuid();
  }
};

// Used to hash BookmarkNode instances by UUID.
class NodeUuidHash {
 public:
  using is_transparent = void;

  size_t operator()(const BookmarkNode* n) const {
    return base::UuidHash()(n->uuid());
  }

  size_t operator()(const base::Uuid& uuid) const {
    return base::UuidHash()(uuid);
  }
};

using UuidIndex =
    std::unordered_set<raw_ptr<const BookmarkNode, CtnExperimental>,
                       NodeUuidHash,
                       NodeUuidEquality>;

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_UUID_INDEX_H_
