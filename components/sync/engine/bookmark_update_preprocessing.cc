// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/bookmark_update_preprocessing.h"

#include <array>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

namespace {

// Used in metric "Sync.BookmarkGUIDSource2". These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
// LINT.IfChange(BookmarkGUIDSource)
enum class BookmarkGuidSource {
  // UUID came from specifics.
  kSpecifics = 0,
  // UUID came from originator_client_item_id and is valid.
  kValidOCII = 1,
  // UUID not found in the specifics and originator_client_item_id is invalid,
  // so field left empty (currently unused).
  kDeprecatedLeftEmpty = 2,
  // UUID not found in the specifics and originator_client_item_id is invalid,
  // so the UUID is inferred from combining originator_client_item_id and
  // originator_cache_guid.
  kInferred = 3,
  // UUID not found in the specifics and the update doesn't have enough
  // information to infer it. This is likely because the update contains a
  // client tag instead of originator information.
  kLeftEmptyPossiblyForClientTag = 4,
  kMaxValue = kLeftEmptyPossiblyForClientTag,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:BookmarkGUIDSource)

inline void LogGuidSource(BookmarkGuidSource source) {
  base::UmaHistogramEnumeration("Sync.BookmarkGUIDSource2", source);
}

std::string ComputeUuidFromBytes(base::span<const uint8_t> bytes) {
  DCHECK_GE(bytes.size(), 16U);

  // This implementation is based on the equivalent logic in base/guid.cc.

  // Set the UUID to version 4 as described in RFC 4122, section 4.4.
  // The format of UUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
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
// have an originator client item ID that is NOT a UUID. Hence, an alternative
// method must be used to infer a UUID deterministically from a combination of
// sync fields that is known to be a) immutable and b) unique per synced
// bookmark.
std::string InferGuidForLegacyBookmark(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  DCHECK(
      !base::Uuid::ParseCaseInsensitive(originator_client_item_id).is_valid());

  const std::string unique_tag =
      base::StrCat({originator_cache_guid, originator_client_item_id});
  const base::SHA1Digest hash = base::SHA1Hash(base::as_byte_span(unique_tag));

  static_assert(base::kSHA1Length >= 16, "16 bytes needed to infer UUID");

  const std::string guid = ComputeUuidFromBytes(base::make_span(hash));
  DCHECK(base::Uuid::ParseLowercase(guid).is_valid());
  return guid;
}

// Legacy method to calculate unique position suffix for the bookmarks which did
// not have client tag hash.
UniquePosition::Suffix GenerateUniquePositionSuffixForBookmark(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  // Blank PB with just the field in it has termination symbol,
  // handy for delimiter.
  sync_pb::EntitySpecifics serialized_type;
  AddDefaultFieldValue(BOOKMARKS, &serialized_type);
  std::string hash_input;
  serialized_type.AppendToString(&hash_input);
  hash_input.append(originator_cache_guid + originator_client_item_id);
  UniquePosition::Suffix suffix;
  std::string suffix_str =
      base::Base64Encode(base::SHA1Hash(base::as_byte_span(hash_input)));
  CHECK_EQ(suffix.size(), suffix_str.size());
  base::ranges::copy(suffix_str, suffix.begin());
  return suffix;
}

sync_pb::UniquePosition GetUniquePositionFromSyncEntity(
    const sync_pb::SyncEntity& update_entity) {
  if (update_entity.has_unique_position()) {
    return update_entity.unique_position();
  }

  UniquePosition::Suffix suffix;
  if (update_entity.has_originator_cache_guid() &&
      update_entity.has_originator_client_item_id()) {
    suffix = GenerateUniquePositionSuffixForBookmark(
        update_entity.originator_cache_guid(),
        update_entity.originator_client_item_id());
  } else {
    suffix = UniquePosition::RandomSuffix();
  }

  if (update_entity.has_position_in_parent()) {
    return UniquePosition::FromInt64(update_entity.position_in_parent(), suffix)
        .ToProto();
  }

  if (update_entity.has_insert_after_item_id()) {
    return UniquePosition::FromInt64(0, suffix).ToProto();
  }

  // No positioning information whatsoever, which should be unreachable today.
  // For future-compatibility in case the fields in SyncEntity get removed,
  // let's use a random position, which is better than dropping the whole
  // update.
  return UniquePosition::InitialPosition(suffix).ToProto();
}

}  // namespace

bool AdaptUniquePositionForBookmark(const sync_pb::SyncEntity& update_entity,
                                    sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Nothing to do if the field is set or if it's a deletion.
  if (specifics->bookmark().has_unique_position() || update_entity.deleted()) {
    return false;
  }

  // Permanent folders don't need positioning information.
  if (update_entity.folder() &&
      !update_entity.server_defined_unique_tag().empty()) {
    return false;
  }

  *specifics->mutable_bookmark()->mutable_unique_position() =
      GetUniquePositionFromSyncEntity(update_entity);
  return true;
}

void AdaptTypeForBookmark(const sync_pb::SyncEntity& update_entity,
                          sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Nothing to do if the field is set or if it's a deletion.
  if (specifics->bookmark().has_type() || update_entity.deleted()) {
    return;
  }
  DCHECK(specifics->has_bookmark());
  // For legacy data, SyncEntity.folder is always populated.
  if (update_entity.has_folder()) {
    specifics->mutable_bookmark()->set_type(
        update_entity.folder() ? sync_pb::BookmarkSpecifics::FOLDER
                               : sync_pb::BookmarkSpecifics::URL);
    return;
  }
  // Remaining cases should be unreachable today. In case SyncEntity.folder gets
  // removed in the future, with legacy data still being around prior to M94,
  // infer folderness based on the present of field |url| (only populated for
  // URL bookmarks).
  specifics->mutable_bookmark()->set_type(
      specifics->bookmark().has_url() ? sync_pb::BookmarkSpecifics::URL
                                      : sync_pb::BookmarkSpecifics::FOLDER);
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
  DCHECK(specifics->has_bookmark());
  // Legacy clients populate the name field in the SyncEntity instead of the
  // title field in the BookmarkSpecifics.
  if (!specifics->bookmark().has_legacy_canonicalized_title() &&
      !update_entity.name().empty()) {
    specifics->mutable_bookmark()->set_legacy_canonicalized_title(
        update_entity.name());
  }
}

void AdaptGuidForBookmark(const sync_pb::SyncEntity& update_entity,
                          sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Tombstones and permanent entities don't have a UUID.
  if (update_entity.deleted() ||
      !update_entity.server_defined_unique_tag().empty()) {
    return;
  }
  DCHECK(specifics->has_bookmark());
  // Legacy clients don't populate the guid field in the BookmarkSpecifics, so
  // we use the originator_client_item_id instead, if it is a valid UUID.
  // Otherwise, we leave the field empty.
  if (specifics->bookmark().has_guid()) {
    LogGuidSource(BookmarkGuidSource::kSpecifics);
    return;
  }
  if (base::Uuid::ParseCaseInsensitive(
          update_entity.originator_client_item_id())
          .is_valid()) {
    // Bookmarks created around 2016, between [M44..M52) use an uppercase UUID
    // as originator client item ID, so it needs to be lowercased to adhere to
    // the invariant that UUIDs in specifics are canonicalized.
    specifics->mutable_bookmark()->set_guid(
        base::ToLowerASCII(update_entity.originator_client_item_id()));
    DCHECK(base::Uuid::ParseLowercase(specifics->bookmark().guid()).is_valid());
    LogGuidSource(BookmarkGuidSource::kValidOCII);
  } else if (update_entity.originator_cache_guid().empty() &&
             update_entity.originator_client_item_id().empty()) {
    // There's no UUID that could be inferred from empty originator
    // information.
    LogGuidSource(BookmarkGuidSource::kLeftEmptyPossiblyForClientTag);
  } else {
    specifics->mutable_bookmark()->set_guid(
        InferGuidForLegacyBookmark(update_entity.originator_cache_guid(),
                                   update_entity.originator_client_item_id()));
    DCHECK(base::Uuid::ParseLowercase(specifics->bookmark().guid()).is_valid());
    LogGuidSource(BookmarkGuidSource::kInferred);
  }
}

std::string InferGuidForLegacyBookmarkForTesting(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  return InferGuidForLegacyBookmark(originator_cache_guid,
                                    originator_client_item_id);
}

}  // namespace syncer
