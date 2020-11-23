// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/bookmark_update_preprocessing.h"

#include <array>

#include "base/containers/span.h"
#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

namespace {

// Used in metric "Sync.BookmarkGUIDSource2". These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class BookmarkGuidSource {
  // GUID came from specifics.
  kSpecifics = 0,
  // GUID came from originator_client_item_id and is valid.
  kValidOCII = 1,
  // GUID not found in the specifics and originator_client_item_id is invalid,
  // so field left empty (currently unused).
  kDeprecatedLeftEmpty = 2,
  // GUID not found in the specifics and originator_client_item_id is invalid,
  // so the GUID is inferred from combining originator_client_item_id and
  // originator_cache_guid.
  kInferred = 3,

  kMaxValue = kInferred,
};

inline void LogGuidSource(BookmarkGuidSource source) {
  base::UmaHistogramEnumeration("Sync.BookmarkGUIDSource2", source);
}

std::string ComputeGuidFromBytes(base::span<const uint8_t> bytes) {
  DCHECK_GE(bytes.size(), 16U);

  // This implementation is based on the equivalent logic in base/guid.cc.

  // Set the GUID to version 4 as described in RFC 4122, section 4.4.
  // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
  // where y is one of [8, 9, A, B].

  // Clear the version bits and set the version to 4:
  const uint8_t byte6 = (bytes[6] & 0x0fU) | 0xf0U;

  // Set the two most significant bits (bits 6 and 7) of the
  // clock_seq_hi_and_reserved to zero and one, respectively:
  const uint8_t byte8 = (bytes[8] & 0x3fU) | 0x80U;

  return base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], byte6,
      bytes[7], byte8, bytes[9], bytes[10], bytes[11], bytes[12], bytes[13],
      bytes[14], bytes[15]);
}

// Bookmarks created before 2015 (https://codereview.chromium.org/1136953013)
// have an originator client item ID that is NOT a GUID. Hence, an alternative
// method must be used to infer a GUID deterministically from a combination of
// sync fields that is known to be a) immutable and b) unique per synced
// bookmark.
std::string InferGuidForLegacyBookmark(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  DCHECK(
      !base::GUID::ParseCaseInsensitive(originator_client_item_id).is_valid());

  const std::string unique_tag =
      base::StrCat({originator_cache_guid, originator_client_item_id});
  const base::SHA1Digest hash =
      base::SHA1HashSpan(base::as_bytes(base::make_span(unique_tag)));

  static_assert(base::kSHA1Length >= 16, "16 bytes needed to infer GUID");

  const std::string guid = ComputeGuidFromBytes(base::make_span(hash));
  DCHECK(base::GUID::ParseLowercase(guid).is_valid());
  return guid;
}

}  // namespace

void AdaptUniquePositionForBookmark(const sync_pb::SyncEntity& update_entity,
                                    EntityData* data) {
  DCHECK(data);

  // Tombstones don't need positioning information.
  if (update_entity.deleted()) {
    return;
  }

  // Permanent folders don't need positioning information.
  if (update_entity.folder() &&
      !update_entity.server_defined_unique_tag().empty()) {
    return;
  }

  if (update_entity.has_unique_position()) {
    data->unique_position = update_entity.unique_position();
  } else if (update_entity.has_position_in_parent() ||
             update_entity.has_insert_after_item_id()) {
    bool missing_originator_fields = false;
    if (!update_entity.has_originator_cache_guid() ||
        !update_entity.has_originator_client_item_id()) {
      DLOG(ERROR) << "Update is missing requirements for bookmark position.";
      missing_originator_fields = true;
    }

    std::string suffix = missing_originator_fields
                             ? UniquePosition::RandomSuffix()
                             : GenerateSyncableBookmarkHash(
                                   update_entity.originator_cache_guid(),
                                   update_entity.originator_client_item_id());

    if (update_entity.has_position_in_parent()) {
      data->unique_position =
          UniquePosition::FromInt64(update_entity.position_in_parent(), suffix)
              .ToProto();
    } else {
      // If update_entity has insert_after_item_id, use 0 index.
      DCHECK(update_entity.has_insert_after_item_id());
      data->unique_position = UniquePosition::FromInt64(0, suffix).ToProto();
    }
  } else {
    DLOG(ERROR) << "Missing required position information in update: "
                << update_entity.id_string();
  }
}

void AdaptTitleForBookmark(const sync_pb::SyncEntity& update_entity,
                           sync_pb::EntitySpecifics* specifics,
                           bool specifics_were_encrypted) {
  DCHECK(specifics);
  if (specifics_were_encrypted || update_entity.deleted()) {
    // If encrypted, the name field is never populated (unencrypted) for privacy
    // reasons. Encryption was also introduced after moving the name out of
    // SyncEntity so this hack is not needed at all.
    return;
  }
  // Legacy clients populate the name field in the SyncEntity instead of the
  // title field in the BookmarkSpecifics.
  if (!specifics->bookmark().has_legacy_canonicalized_title() &&
      !update_entity.name().empty()) {
    specifics->mutable_bookmark()->set_legacy_canonicalized_title(
        update_entity.name());
  }
}

bool AdaptGuidForBookmark(const sync_pb::SyncEntity& update_entity,
                          sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Tombstones and permanent entities don't have a GUID.
  if (update_entity.deleted() ||
      !update_entity.server_defined_unique_tag().empty()) {
    return false;
  }
  // Legacy clients don't populate the guid field in the BookmarkSpecifics, so
  // we use the originator_client_item_id instead, if it is a valid GUID.
  // Otherwise, we leave the field empty.
  if (specifics->bookmark().has_guid()) {
    LogGuidSource(BookmarkGuidSource::kSpecifics);
    return false;
  }
  if (base::IsValidGUID(update_entity.originator_client_item_id())) {
    // Bookmarks created around 2016, between [M44..M52) use an uppercase GUID
    // as originator client item ID, so it needs to be lowercased to adhere to
    // the invariant that GUIDs in specifics are canonicalized.
    specifics->mutable_bookmark()->set_guid(
        base::ToLowerASCII(update_entity.originator_client_item_id()));
    DCHECK(base::IsValidGUIDOutputString(specifics->bookmark().guid()));
    LogGuidSource(BookmarkGuidSource::kValidOCII);
  } else {
    specifics->mutable_bookmark()->set_guid(
        InferGuidForLegacyBookmark(update_entity.originator_cache_guid(),
                                   update_entity.originator_client_item_id()));
    DCHECK(base::IsValidGUIDOutputString(specifics->bookmark().guid()));
    LogGuidSource(BookmarkGuidSource::kInferred);
  }
  return true;
}

std::string InferGuidForLegacyBookmarkForTesting(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  return InferGuidForLegacyBookmark(originator_cache_guid,
                                    originator_client_item_id);
}

}  // namespace syncer
