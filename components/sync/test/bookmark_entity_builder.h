// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_BOOKMARK_ENTITY_BUILDER_H_
#define COMPONENTS_SYNC_TEST_BOOKMARK_ENTITY_BUILDER_H_

#include <memory>
#include <string>

#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace sync_pb {
class BookmarkSpecifics;
class EntitySpecifics;
}  // namespace sync_pb

namespace fake_server {

// Builder for BookmarkEntity objects.
class BookmarkEntityBuilder {
 public:
  // Represents different generations of bookmarks ordered by time. It doesn't
  // contain all generations and may reflect differences in specifics and
  // SyncEntity.
  enum class BookmarkGeneration {
    // A bookmark which doesn't contain title and GUID in specifics.
    kWithoutTitleInSpecifics,
    // A bookmark entity having legacy title in specifics and uppercase GUID in
    // |originator_client_item_id|
    // without GUID in specifics. For bookmarks created between M45 and M51.
    kLegacyTitleUppercaseOriginatorClientItemId,
    // A bookmark which contains valid GUID in specifics and
    // |originator_client_item_id|. For bookmarks created after M52.
    kLegacyTitleWithoutGuidInSpecifics,
    // Contains legacy title and GUID in specifics which matches to
    // |originator_client_item_id| (see BookmarkSpecifics for details).
    // Introduced in M81.
    kValidGuidAndLegacyTitle,
    // Contains both legacy title and full title in specifics. Introduced in
    // M83.
    kValidGuidAndFullTitle,
    // Contains |unique_position|, |type| and |parent_guid| in specifics.
    // Introduced in M94.
    kHierarchyFieldsInSpecifics,
  };

  BookmarkEntityBuilder(const std::string& title,
                        const std::string& originator_cache_guid,
                        const std::string& originator_client_item_id);

  BookmarkEntityBuilder(const BookmarkEntityBuilder& other);

  ~BookmarkEntityBuilder();

  // Sets the ID for the bookmark to be built. The ID should be in the format
  // returned by LoopbackServerEntity::CreateId. If this is not called, a random
  // ID will be generated.
  void SetId(const std::string& id);

  // Sets the parent ID of the bookmark to be built. If this is not called,
  // the bookmark will be included in the bookmarks bar.
  BookmarkEntityBuilder& SetParentId(const std::string& parent_id);

  // Set parent GUID to populate in specifics for generations above
  // |kHierarchyFieldsInSpecifics|. The GUID must be valid.
  BookmarkEntityBuilder& SetParentGuid(const base::Uuid& parent_guid);

  // Sets the index of the bookmark to be built. If this is not called,
  // the bookmark will be placed at index 0.
  void SetIndex(int index);

  // Update bookmark's generation, will be used to fill in the final entity
  // fields.
  BookmarkEntityBuilder& SetGeneration(BookmarkGeneration generation);

  BookmarkEntityBuilder& SetFavicon(const gfx::Image& favicon,
                                    const GURL& icon_url);

  // Builds and returns a LoopbackServerEntity representing a bookmark. Returns
  // null if the entity could not be built.
  std::unique_ptr<syncer::LoopbackServerEntity> BuildBookmark(const GURL& url);

  // Builds and returns a LoopbackServerEntity representing a bookmark folder.
  // Returns null if the entity could not be built.
  std::unique_ptr<syncer::LoopbackServerEntity> BuildFolder();

 private:
  // Creates an EntitySpecifics and pre-populates its BookmarkSpecifics.
  sync_pb::EntitySpecifics CreateBaseEntitySpecifics(bool is_folder);

  // Builds the parts of a LoopbackServerEntity common to both normal bookmarks
  // and folders.
  std::unique_ptr<syncer::LoopbackServerEntity> Build(
      const sync_pb::EntitySpecifics& entity_specifics,
      bool is_folder);

  // Fill in favicon and icon URL in the specifics. |bookmark_specifics| must
  // not be nullptr.
  void FillWithFaviconIfNeeded(sync_pb::BookmarkSpecifics* bookmark_specifics);

  // Generates unique position based on |index_|, item ID and cache GUID.
  sync_pb::UniquePosition GenerateUniquePosition() const;

  // The bookmark entity's title. This value is also used as the entity's name.
  const std::string title_;

  // Information that associates the bookmark with its original client.
  const std::string originator_cache_guid_;
  const std::string originator_client_item_id_;

  // The ID for the bookmark. This is only non-empty if it was explicitly set
  // via SetId(); otherwise a random ID will be generated on demand.
  std::string id_;

  // The ID of the parent bookmark folder.
  std::string parent_id_;
  base::Uuid parent_guid_;

  // The index of the bookmark folder within its siblings.
  int index_ = 0;
  sync_pb::UniquePosition unique_position_;

  // Information about the favicon of the bookmark.
  gfx::Image favicon_;
  GURL icon_url_;

  BookmarkGeneration bookmark_generation_ =
      BookmarkGeneration::kHierarchyFieldsInSpecifics;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_BOOKMARK_ENTITY_BUILDER_H_
