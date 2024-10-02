// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/bookmark_entity_builder.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/loopback_server/persistent_bookmark_entity.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

using std::string;
using syncer::LoopbackServerEntity;

// A version must be passed when creating a LoopbackServerEntity, but this value
// is overrideen immediately when saving the entity in FakeServer.
const int64_t kUnusedVersion = 0L;

// Default time (creation and last modified) used when creating entities.
const int64_t kDefaultTime = 1234L;

namespace fake_server {

namespace {

syncer::UniquePosition::Suffix GenerateUniquePositionStringForBookmark(
    const base::Uuid& uuid) {
  if (!uuid.is_valid()) {
    return syncer::UniquePosition::RandomSuffix();
  }
  return syncer::UniquePosition::GenerateSuffix(
      syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS,
                                          uuid.AsLowercaseString()));
}

}  // namespace

BookmarkEntityBuilder::BookmarkEntityBuilder(
    const string& title,
    const string& originator_cache_guid,
    const string& originator_client_item_id)
    : title_(title),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id),
      unique_position_(GenerateUniquePosition()) {}

BookmarkEntityBuilder::BookmarkEntityBuilder(
    const BookmarkEntityBuilder& other) = default;

BookmarkEntityBuilder::~BookmarkEntityBuilder() = default;

void BookmarkEntityBuilder::SetId(const std::string& id) {
  id_ = id;
}

BookmarkEntityBuilder& BookmarkEntityBuilder::SetParentId(
    const std::string& parent_id) {
  parent_id_ = parent_id;
  return *this;
}

BookmarkEntityBuilder& BookmarkEntityBuilder::SetParentGuid(
    const base::Uuid& parent_guid) {
  DCHECK(parent_guid.is_valid()) << parent_guid.AsLowercaseString();
  parent_guid_ = parent_guid;
  return *this;
}

void BookmarkEntityBuilder::SetIndex(int index) {
  index_ = index;
  unique_position_ = GenerateUniquePosition();
}

BookmarkEntityBuilder& BookmarkEntityBuilder::SetGeneration(
    BookmarkEntityBuilder::BookmarkGeneration generation) {
  bookmark_generation_ = generation;
  return *this;
}

BookmarkEntityBuilder& BookmarkEntityBuilder::SetFavicon(
    const gfx::Image& favicon,
    const GURL& icon_url) {
  favicon_ = favicon;
  icon_url_ = icon_url;
  return *this;
}

std::unique_ptr<LoopbackServerEntity> BookmarkEntityBuilder::BuildBookmark(
    const GURL& url) {
  if (!url.is_valid()) {
    return base::WrapUnique<LoopbackServerEntity>(nullptr);
  }

  sync_pb::EntitySpecifics entity_specifics =
      CreateBaseEntitySpecifics(/*is_folder=*/false);
  entity_specifics.mutable_bookmark()->set_url(url.spec());
  FillWithFaviconIfNeeded(entity_specifics.mutable_bookmark());
  return Build(entity_specifics, /*is_folder=*/false);
}

std::unique_ptr<LoopbackServerEntity> BookmarkEntityBuilder::BuildFolder() {
  return Build(CreateBaseEntitySpecifics(/*is_folder=*/true),
               /*is_folder=*/true);
}

sync_pb::EntitySpecifics BookmarkEntityBuilder::CreateBaseEntitySpecifics(
    bool is_folder) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      entity_specifics.mutable_bookmark();

  DCHECK_NE(bookmark_generation_,
            BookmarkGeneration::kLegacyTitleUppercaseOriginatorClientItemId)
      << "Bookmark with legacy title and uppercase originator_client_item_id "
         "is not implemented.";
  DCHECK_NE(bookmark_generation_,
            BookmarkGeneration::kLegacyTitleWithoutGuidInSpecifics)
      << "Bookmark with legacy title and without GUID in specifics is not "
         "implemented.";

  if (parent_id_.empty()) {
    parent_id_ =
        LoopbackServerEntity::CreateId(syncer::BOOKMARKS, "bookmark_bar");
    parent_guid_ = base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid);
  }

  if (bookmark_generation_ >= BookmarkGeneration::kValidGuidAndLegacyTitle) {
    bookmark_specifics->set_legacy_canonicalized_title(title_);

    // GUID must be valid for `kValidGuidAndLegacyTitle`.
    base::Uuid uuid =
        base::Uuid::ParseCaseInsensitive(originator_client_item_id_);
    CHECK(uuid.is_valid());
    bookmark_specifics->set_guid(uuid.AsLowercaseString());
  }

  if (bookmark_generation_ >= BookmarkGeneration::kValidGuidAndFullTitle) {
    bookmark_specifics->set_full_title(title_);
  }

  if (bookmark_generation_ >= BookmarkGeneration::kHierarchyFieldsInSpecifics) {
    DCHECK(parent_guid_.is_valid());
    bookmark_specifics->set_parent_guid(parent_guid_.AsLowercaseString());
    bookmark_specifics->set_type(is_folder ? sync_pb::BookmarkSpecifics::FOLDER
                                           : sync_pb::BookmarkSpecifics::URL);
    *bookmark_specifics->mutable_unique_position() = unique_position_;
  }

  return entity_specifics;
}

std::unique_ptr<LoopbackServerEntity> BookmarkEntityBuilder::Build(
    const sync_pb::EntitySpecifics& entity_specifics,
    bool is_folder) {
  if (id_.empty()) {
    id_ = LoopbackServerEntity::CreateId(
        syncer::BOOKMARKS, base::Uuid::GenerateRandomV4().AsLowercaseString());
  }

  return base::WrapUnique<LoopbackServerEntity>(
      new syncer::PersistentBookmarkEntity(
          id_, kUnusedVersion, title_, originator_cache_guid_,
          originator_client_item_id_, /*client_tag_hash=*/"", unique_position_,
          entity_specifics, is_folder, parent_id_, kDefaultTime, kDefaultTime));
}

void BookmarkEntityBuilder::FillWithFaviconIfNeeded(
    sync_pb::BookmarkSpecifics* bookmark_specifics) {
  DCHECK(bookmark_specifics);
  // Both |favicon_| and |icon_url_| must be provided or empty simultaneously.
  DCHECK(favicon_.IsEmpty() == icon_url_.is_empty());
  if (favicon_.IsEmpty()) {
    return;
  }

  scoped_refptr<base::RefCountedMemory> favicon_bytes = favicon_.As1xPNGBytes();
  bookmark_specifics->set_favicon(favicon_bytes->front(),
                                  favicon_bytes->size());
  bookmark_specifics->set_icon_url(icon_url_.spec());
}

sync_pb::UniquePosition BookmarkEntityBuilder::GenerateUniquePosition() const {
  base::Uuid uuid =
      base::Uuid::ParseCaseInsensitive(originator_client_item_id_);
  return syncer::UniquePosition::FromInt64(
             index_, GenerateUniquePositionStringForBookmark(uuid))
      .ToProto();
}

}  // namespace fake_server
