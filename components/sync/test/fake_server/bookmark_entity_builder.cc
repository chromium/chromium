// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/bookmark_entity_builder.h"

#include <stdint.h>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/loopback_server/persistent_bookmark_entity.h"
#include "components/sync/protocol/sync.pb.h"

using std::string;
using syncer::GenerateSyncableBookmarkHash;
using syncer::LoopbackServerEntity;

// A version must be passed when creating a LoopbackServerEntity, but this value
// is overrideen immediately when saving the entity in FakeServer.
const int64_t kUnusedVersion = 0L;

// Default time (creation and last modified) used when creating entities.
const int64_t kDefaultTime = 1234L;

namespace fake_server {

BookmarkEntityBuilder::BookmarkEntityBuilder(
    const string& title,
    const string& originator_cache_guid,
    const string& originator_client_item_id)
    : title_(title),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id) {}

BookmarkEntityBuilder::BookmarkEntityBuilder(
    const BookmarkEntityBuilder& other) = default;

BookmarkEntityBuilder::~BookmarkEntityBuilder() = default;

void BookmarkEntityBuilder::SetId(const std::string& id) {
  id_ = id;
}

void BookmarkEntityBuilder::SetParentId(const std::string& parent_id) {
  parent_id_ = parent_id;
}

void BookmarkEntityBuilder::SetIndex(int index) {
  index_ = index;
}

BookmarkEntityBuilder& BookmarkEntityBuilder::SetFavicon(
    const gfx::Image& favicon,
    const GURL& icon_url) {
  favicon_ = favicon;
  icon_url_ = icon_url;
  return *this;
}

std::unique_ptr<LoopbackServerEntity> BookmarkEntityBuilder::BuildBookmark(
    const GURL& url,
    bool is_legacy) {
  if (!url.is_valid()) {
    return base::WrapUnique<LoopbackServerEntity>(nullptr);
  }

  sync_pb::EntitySpecifics entity_specifics =
      CreateBaseEntitySpecifics(is_legacy);
  entity_specifics.mutable_bookmark()->set_url(url.spec());
  FillWithFaviconIfNeeded(entity_specifics.mutable_bookmark());
  return Build(entity_specifics, /*is_folder=*/false);
}

std::unique_ptr<syncer::LoopbackServerEntity>
BookmarkEntityBuilder::BuildBookmarkWithoutFullTitle(const GURL& url) {
  if (!url.is_valid()) {
    return nullptr;
  }

  sync_pb::EntitySpecifics entity_specifics =
      CreateBaseEntitySpecifics(/*is_legacy=*/false);
  entity_specifics.mutable_bookmark()->set_url(url.spec());
  entity_specifics.mutable_bookmark()->clear_full_title();
  FillWithFaviconIfNeeded(entity_specifics.mutable_bookmark());
  return Build(entity_specifics, /*is_folder=*/false);
}

std::unique_ptr<LoopbackServerEntity> BookmarkEntityBuilder::BuildFolder(
    bool is_legacy) {
  return Build(CreateBaseEntitySpecifics(is_legacy), /*is_folder=*/true);
}

std::unique_ptr<LoopbackServerEntity>
BookmarkEntityBuilder::BuildFolderWithoutFullTitle() {
  sync_pb::EntitySpecifics entity_specifics =
      CreateBaseEntitySpecifics(/*is_legacy=*/false);
  entity_specifics.mutable_bookmark()->clear_full_title();
  return Build(entity_specifics, /*is_folder=*/true);
}

sync_pb::EntitySpecifics BookmarkEntityBuilder::CreateBaseEntitySpecifics(
    bool is_legacy) const {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      entity_specifics.mutable_bookmark();

  if (!is_legacy) {
    bookmark_specifics->set_legacy_canonicalized_title(title_);
    bookmark_specifics->set_full_title(title_);
    bookmark_specifics->set_guid(originator_client_item_id_);
  }

  return entity_specifics;
}

std::unique_ptr<LoopbackServerEntity> BookmarkEntityBuilder::Build(
    const sync_pb::EntitySpecifics& entity_specifics,
    bool is_folder) {
  const string suffix = GenerateSyncableBookmarkHash(
      originator_cache_guid_, originator_client_item_id_);
  sync_pb::UniquePosition unique_position =
      syncer::UniquePosition::FromInt64(index_, suffix).ToProto();

  if (parent_id_.empty()) {
    parent_id_ =
        LoopbackServerEntity::CreateId(syncer::BOOKMARKS, "bookmark_bar");
  }

  if (id_.empty()) {
    id_ =
        LoopbackServerEntity::CreateId(syncer::BOOKMARKS, base::GenerateGUID());
  }

  return base::WrapUnique<LoopbackServerEntity>(
      new syncer::PersistentBookmarkEntity(
          id_, kUnusedVersion, title_, originator_cache_guid_,
          originator_client_item_id_, /*client_tag_hash=*/"", unique_position,
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

}  // namespace fake_server
