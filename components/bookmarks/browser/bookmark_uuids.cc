// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_uuids.h"

namespace bookmarks {

// Below predefined UUIDs for permanent bookmark folders, determined via named
// UUIDs/UUIDs. Do NOT modify them as they may be exposed via Sync. Once a
// constant is added, make sure to add it to the set at the bottom of this file.
// For reference, here's the python script to produce them:
// > import uuid
// > chromium_namespace = uuid.uuid5(uuid.NAMESPACE_DNS, "chromium.org")
// > bookmarks_namespace = uuid.uuid5(chromium_namespace, "bookmarks")
// > my_bookmark_id = uuid.uuid5(bookmarks_namespace, "my_bookmark_id")

// > uuid.uuid5(bookmarks_namespace, "root")
const char kRootNodeUuid[] = "2509a7dc-215d-52f7-a429-8d80431c6c75";

// > uuid.uuid5(bookmarks_namespace, "bookmark_bar")
const char kBookmarkBarNodeUuid[] = "0bc5d13f-2cba-5d74-951f-3f233fe6c908";

// > uuid.uuid5(bookmarks_namespace, "other_bookmarks")
const char kOtherBookmarksNodeUuid[] = "82b081ec-3dd3-529c-8475-ab6c344590dd";

// > uuid.uuid5(bookmarks_namespace, "mobile_bookmarks")
const char kMobileBookmarksNodeUuid[] = "4cf2e351-0e85-532b-bb37-df045d8f8d0f";

// > uuid.uuid5(bookmarks_namespace, "managed_bookmarks")
const char kManagedNodeUuid[] = "323123f4-9381-5aee-80e6-ea5fca2f7672";

// > uuid.uuid5(bookmarks_namespace, "shopping_collection_m118")
// "shopping_collection" is not used due to a bug involving that ID on M-117,
//  see https://crbug.com/1484372 for details.
const char kShoppingCollectionUuid[] = "89fc5b66-beb6-56c1-a99b-70635d7df201";

// This value is the result of exercising sync's function
// syncer::InferGuidForLegacyBookmark() with an empty input.
const char kBannedUuidDueToPastSyncBug[] =
    "da39a3ee-5e6b-fb0d-b255-bfef95601890";

}  // namespace bookmarks
