// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_

#include <stddef.h>

#include <string>

#include "base/uuid.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace sync_pb {
class EntitySpecifics;
class UniquePosition;
}  // namespace sync_pb

namespace syncer {
class ClientTagHash;
struct EntityData;
}  // namespace syncer

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace sync_bookmarks {

class BookmarkModelView;

// Canonicalize |node_title| similar to legacy client's implementation by
// truncating and the appending ' ' in some cases.
std::string FullTitleToLegacyCanonicalizedTitle(const std::string& node_title);

// Used to decide if entity needs to be reuploaded for each remote change.
bool IsBookmarkEntityReuploadNeeded(
    const syncer::EntityData& remote_entity_data);

sync_pb::EntitySpecifics CreateSpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node,
    BookmarkModelView* model,
    const sync_pb::UniquePosition& unique_position,
    bool force_favicon_load);

// Creates a bookmark node under the given parent node from the given specifics.
// Returns the newly created node. Callers must verify that
// |specifics| passes the IsValidBookmarkSpecifics().
const bookmarks::BookmarkNode* CreateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    BookmarkModelView* model,
    favicon::FaviconService* favicon_service);

// Updates the bookmark node |node| with the data in |specifics|. Callers must
// verify that |specifics| passes the IsValidBookmarkSpecifics().
void UpdateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* node,
    BookmarkModelView* model,
    favicon::FaviconService* favicon_service);

// Convnience function that returns BookmarkSpecifics::URL or
// BookmarkSpecifics::FOLDER based on whether the input node is a folder. |node|
// must not be null.
sync_pb::BookmarkSpecifics::Type GetProtoTypeFromBookmarkNode(
    const bookmarks::BookmarkNode* node);

// Replaces |node| with a BookmarkNode of equal properties and original node
// creation timestamp but a different UUID, set to |guid|, which must be a
// valid version 4 UUID. Intended to be used in cases where the UUID must be
// modified despite being immutable within the BookmarkNode itself. Returns
// the newly created node, and the original node gets deleted.
const bookmarks::BookmarkNode* ReplaceBookmarkNodeUuid(
    const bookmarks::BookmarkNode* node,
    const base::Uuid& guid,
    BookmarkModelView* model);

// Checks if a bookmark specifics represents a valid bookmark. Valid specifics
// must not be empty, non-folders must contains a valid url, and all keys in the
// meta_info must be unique.
bool IsValidBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics);

// Returns the inferred UUID for given remote update's originator information.
base::Uuid InferGuidFromLegacyOriginatorId(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id);

// Checks if bookmark specifics contain a UUID that matches the value that would
// be inferred from other redundant fields. |specifics| must be valid as per
// IsValidBookmarkSpecifics().
bool HasExpectedBookmarkGuid(const sync_pb::BookmarkSpecifics& specifics,
                             const syncer::ClientTagHash& client_tag_hash,
                             const std::string& originator_cache_guid,
                             const std::string& originator_client_item_id);

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_
