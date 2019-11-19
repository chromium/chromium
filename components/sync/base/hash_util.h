// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_HASH_UTIL_H_
#define COMPONENTS_SYNC_BASE_HASH_UTIL_H_

#include <string>

namespace syncer {

// A helper for generating the bookmark type's tag.  This is required in more
// than one place, so we define the algorithm here to make sure the
// implementation is consistent.
std::string GenerateSyncableBookmarkHash(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_HASH_UTIL_H_
