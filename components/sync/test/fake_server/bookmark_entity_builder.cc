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
#include "components/sync/engine_impl/loopback_server/persistent_bookmark_entity.h"
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

BookmarkEntityBuilder::~BookmarkEntityBuilder() {}

void BookmarkEntityBuilder::SetId(const std::string& id) {
  id_ = id;
}

void BookmarkEntityBuilder::SetParentId(const std::string& parent_id) {
  parent_id_ = parent_id;
}

void BookmarkEntityBuilder::SetIndex(int index) {
  index_ = index;
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
  const bool kIsNotFolder = false;
  return Build(entity_specifics, kIsNotFolder);
}

std::unique_ptr<LoopbackServerEntity> BookmarkEntityBuilder::BuildFolder(
    bool is_legacy) {
  const bool kIsFolder = true;
  return Build(CreateBaseEntitySpecifics(is_legacy), kIsFolder);
}

sync_pb::EntitySpecifics BookmarkEntityBuilder::CreateBaseEntitySpecifics(
    bool is_legacy) const {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      entity_specifics.mutable_bookmark();
  if (!is_legacy) {
    bookmark_specifics->set_title(title_);
    // TODO(crbug.com/516866): Use originator_client_item_id here instead of a
    // new GUID, ensuring the value is of valid GUID format.
    bookmark_specifics->set_guid(base::GenerateGUID());
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
          originator_client_item_id_, unique_position, entity_specifics,
          is_folder, parent_id_, kDefaultTime, kDefaultTime));
}

}  // namespace fake_server
