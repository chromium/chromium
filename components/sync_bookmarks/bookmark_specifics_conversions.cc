// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/favicon/core/favicon_service.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/switches.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

// Maximum number of bytes to allow in a legacy canonicalized title (must match
// sync's internal limits; see write_node.cc).
const int kLegacyCanonicalizedTitleLimitBytes = 255;

// The list of bookmark titles which are reserved for use by the server.
const char* const kForbiddenTitles[] = {"", ".", ".."};

// Maximum size for the favicon URL. This limit should be very generous in most
// cases, the notable exception being data: URLs that encode the content of
// the favicon itself in the URL, and may be arbitrarily large.
const int kMaxFaviconUrlSize = 4096;

// Used in metrics: "Sync.InvalidBookmarkSpecifics". These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
// LINT.IfChange(InvalidBookmarkSpecificsError)
enum class InvalidBookmarkSpecificsError {
  kEmptySpecifics = 0,
  kInvalidURL = 1,
  kIconURLWithoutFavicon = 2,
  kInvalidIconURL = 3,
  kNonUniqueMetaInfoKeys = 4,
  kInvalidGUID = 5,
  kInvalidParentGUID = 6,
  kInvalidUniquePosition = 7,
  kBannedGUID = 8,

  kMaxValue = kBannedGUID,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:InvalidBookmarkSpecificsError)

void LogInvalidSpecifics(InvalidBookmarkSpecificsError error) {
  base::UmaHistogramEnumeration("Sync.InvalidBookmarkSpecifics", error);
}

void LogFaviconContainedInSpecifics(bool contains_favicon) {
  base::UmaHistogramBoolean(
      "Sync.BookmarkSpecificsExcludingFoldersContainFavicon", contains_favicon);
}
void UpdateBookmarkSpecificsMetaInfo(
    const bookmarks::BookmarkNode::MetaInfoMap* metainfo_map,
    sync_pb::BookmarkSpecifics* bm_specifics) {
  for (const auto& [key, value] : *metainfo_map) {
    sync_pb::MetaInfo* meta_info = bm_specifics->add_meta_info();
    meta_info->set_key(key);
    meta_info->set_value(value);
  }
}

// Metainfo entries in |specifics| must have unique keys.
bookmarks::BookmarkNode::MetaInfoMap GetBookmarkMetaInfo(
    const sync_pb::BookmarkSpecifics& specifics) {
  bookmarks::BookmarkNode::MetaInfoMap meta_info_map;
  for (const sync_pb::MetaInfo& meta_info : specifics.meta_info()) {
    meta_info_map[meta_info.key()] = meta_info.value();
  }
  DCHECK_EQ(static_cast<size_t>(specifics.meta_info_size()),
            meta_info_map.size());
  return meta_info_map;
}

// Sets the favicon of the given bookmark node from the given specifics.
void SetBookmarkFaviconFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* bookmark_node,
    favicon::FaviconService* favicon_service) {
  DCHECK(bookmark_node);
  DCHECK(!bookmark_node->is_folder());
  DCHECK(favicon_service);

  favicon_service->AddPageNoVisitForBookmark(bookmark_node->url(),
                                             bookmark_node->GetTitle());

  const std::string& icon_bytes_str = specifics.favicon();
  auto icon_bytes = base::MakeRefCounted<base::RefCountedString>();
  icon_bytes->as_string().assign(icon_bytes_str);

  GURL icon_url(specifics.icon_url());

  if (icon_bytes->size() == 0 && icon_url.is_empty()) {
    // Empty icon URL and no bitmap data means no icon mapping.
    LogFaviconContainedInSpecifics(false);
    favicon_service->DeleteFaviconMappings({bookmark_node->url()},
                                           favicon_base::IconType::kFavicon);
    return;
  }

  LogFaviconContainedInSpecifics(true);

  if (icon_url.is_empty()) {
    // See documentation in BookmarkSpecifics to understand the (rare) scenarios
    // where |icon_url| may be missing despite a favicon image itself (proto
    // field |favicon|) being set. In this case, use the page URL as a fake icon
    // URL as it is guaranteed to be unique.
    icon_url = GURL(bookmark_node->url());
  }

  // The client may have cached the favicon at 2x. Use MergeFavicon() as not to
  // overwrite the cached 2x favicon bitmap. Sync favicons are always
  // gfx::kFaviconSize in width and height. Store the favicon into history
  // as such.
  gfx::Size pixel_size(gfx::kFaviconSize, gfx::kFaviconSize);
  favicon_service->MergeFavicon(bookmark_node->url(), icon_url,
                                favicon_base::IconType::kFavicon, icon_bytes,
                                pixel_size);
}

// This is an exact copy of the same code in bookmark_update_preprocessing.cc.
std::string ComputeGuidFromBytes(base::span<const uint8_t> bytes) {
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

// This is an exact copy of the same code in bookmark_update_preprocessing.cc,
// which could be removed if eventually client tags are adapted/inferred in
// DataTypeWorker. The reason why this is non-trivial today is that some users
// are known to contain corrupt data in the sense that several different
// entities (identified by their server-provided ID) use the same client tag
// (and UUID). Currently BookmarkModelMerger has logic to prefer folders over
// regular URLs and reassign UUIDs.
std::string InferGuidForLegacyBookmark(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  DCHECK(
      !base::Uuid::ParseCaseInsensitive(originator_client_item_id).is_valid());

  const std::string unique_tag =
      base::StrCat({originator_cache_guid, originator_client_item_id});
  const base::SHA1Digest hash = base::SHA1Hash(base::as_byte_span(unique_tag));

  static_assert(base::kSHA1Length >= 16, "16 bytes needed to infer UUID");

  const std::string guid = ComputeGuidFromBytes(base::make_span(hash));
  DCHECK(base::Uuid::ParseLowercase(guid).is_valid());
  return guid;
}

bool IsForbiddenTitleWithMaybeTrailingSpaces(const std::string& title) {
  return base::Contains(
      kForbiddenTitles,
      base::TrimWhitespaceASCII(title, base::TrimPositions::TRIM_TRAILING));
}

std::u16string NodeTitleFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics) {
  if (specifics.has_full_title()) {
    return base::UTF8ToUTF16(specifics.full_title());
  }

  std::string node_title = specifics.legacy_canonicalized_title();
  if (base::EndsWith(node_title, " ") &&
      IsForbiddenTitleWithMaybeTrailingSpaces(node_title)) {
    // Legacy clients added an extra space to the real title, so remove it here.
    // See also FullTitleToLegacyCanonicalizedTitle().
    node_title.pop_back();
  }
  return base::UTF8ToUTF16(node_title);
}

void MoveAllChildren(BookmarkModelView* model,
                     const bookmarks::BookmarkNode* old_parent,
                     const bookmarks::BookmarkNode* new_parent) {
  DCHECK(old_parent && old_parent->is_folder());
  DCHECK(new_parent && new_parent->is_folder());
  DCHECK(old_parent != new_parent);
  DCHECK(new_parent->children().empty());

  if (old_parent->children().empty()) {
    return;
  }

  // This code relies on the underlying type to store children in the
  // BookmarkModel which is vector. It moves the last child from |old_parent| to
  // the end of |new_parent| step by step (which reverses the order of
  // children). After that all children must be reordered to keep the original
  // order in |new_parent|.
  // This algorithm is used because of performance reasons.
  std::vector<const bookmarks::BookmarkNode*> children_order(
      old_parent->children().size(), nullptr);
  for (size_t i = old_parent->children().size(); i > 0; --i) {
    const size_t old_index = i - 1;
    const bookmarks::BookmarkNode* child_to_move =
        old_parent->children()[old_index].get();
    children_order[old_index] = child_to_move;
    model->Move(child_to_move, new_parent, new_parent->children().size());
  }
  model->ReorderChildren(new_parent, children_order);
}

}  // namespace

std::string FullTitleToLegacyCanonicalizedTitle(const std::string& node_title) {
  // Add an extra space for backward compatibility with legacy clients.
  std::string specifics_title =
      IsForbiddenTitleWithMaybeTrailingSpaces(node_title) ? node_title + " "
                                                          : node_title;
  base::TruncateUTF8ToByteSize(
      specifics_title, kLegacyCanonicalizedTitleLimitBytes, &specifics_title);
  return specifics_title;
}

bool IsBookmarkEntityReuploadNeeded(
    const syncer::EntityData& remote_entity_data) {
  DCHECK(remote_entity_data.server_defined_unique_tag.empty());
  // Do not initiate a reupload for a remote deletion.
  if (remote_entity_data.is_deleted()) {
    return false;
  }

  DCHECK(remote_entity_data.specifics.has_bookmark());
  if (!remote_entity_data
           .is_bookmark_unique_position_in_specifics_preprocessed) {
    return false;
  }

  return base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks);
}

sync_pb::EntitySpecifics CreateSpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node,
    BookmarkModelView* model,
    const sync_pb::UniquePosition& unique_position,
    bool force_favicon_load) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();

  bm_specifics->set_type(GetProtoTypeFromBookmarkNode(node));
  if (!node->is_folder()) {
    bm_specifics->set_url(node->url().spec());
  }

  DCHECK(node->uuid().is_valid()) << "Actual: " << node->uuid();
  bm_specifics->set_guid(node->uuid().AsLowercaseString());

  DCHECK(node->parent()->uuid().is_valid())
      << "Actual: " << node->parent()->uuid();
  bm_specifics->set_parent_guid(node->parent()->uuid().AsLowercaseString());

  const std::string node_title = base::UTF16ToUTF8(node->GetTitle());
  bm_specifics->set_legacy_canonicalized_title(
      FullTitleToLegacyCanonicalizedTitle(node_title));
  bm_specifics->set_full_title(node_title);
  bm_specifics->set_creation_time_us(
      node->date_added().ToDeltaSinceWindowsEpoch().InMicroseconds());
  *bm_specifics->mutable_unique_position() = unique_position;
  if (!node->is_folder() && node->date_last_used() != base::Time()) {
    bm_specifics->set_last_used_time_us(
        node->date_last_used().ToDeltaSinceWindowsEpoch().InMicroseconds());
  }

  if (node->GetMetaInfoMap()) {
    UpdateBookmarkSpecificsMetaInfo(node->GetMetaInfoMap(), bm_specifics);
  }

  if (!force_favicon_load && !node->is_favicon_loaded()) {
    return specifics;
  }

  // Encodes a bookmark's favicon into raw PNG data.
  scoped_refptr<base::RefCountedMemory> favicon_bytes(nullptr);
  const gfx::Image& favicon = model->GetFavicon(node);
  // Check for empty images. This can happen if the favicon is still being
  // loaded.
  if (!favicon.IsEmpty()) {
    // Re-encode the BookmarkNode's favicon as a PNG.
    favicon_bytes = favicon.As1xPNGBytes();
  }

  if (favicon_bytes.get() && favicon_bytes->size() != 0) {
    bm_specifics->set_favicon(favicon_bytes->data(), favicon_bytes->size());
    // Avoid sync-ing favicon URLs that are unreasonably large, as determined by
    // |kMaxFaviconUrlSize|. Most notably, URLs prefixed with the data: scheme
    // to embed the content of the image itself in the URL may be arbitrarily
    // large and run into the server-side enforced limit per sync entity.
    if (node->icon_url() &&
        node->icon_url()->spec().size() <= kMaxFaviconUrlSize) {
      bm_specifics->set_icon_url(node->icon_url()->spec());
    } else {
      bm_specifics->set_icon_url(std::string());
    }
  }

  return specifics;
}

const bookmarks::BookmarkNode* CreateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    BookmarkModelView* model,
    favicon::FaviconService* favicon_service) {
  DCHECK(parent);
  DCHECK(model);
  DCHECK(favicon_service);
  DCHECK(IsValidBookmarkSpecifics(specifics));
  TRACE_EVENT0("sync", "CreateBookmarkNodeFromSpecifics");

  const base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());
  DCHECK(guid.is_valid());

  const base::Uuid parent_guid =
      base::Uuid::ParseLowercase(specifics.parent_guid());
  DCHECK(parent_guid.is_valid());
  DCHECK_EQ(parent_guid, parent->uuid());

  bookmarks::BookmarkNode::MetaInfoMap metainfo =
      GetBookmarkMetaInfo(specifics);

  const int64_t creation_time_us = specifics.creation_time_us();
  const base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      // Use FromDeltaSinceWindowsEpoch because creation_time_us has
      // always used the Windows epoch.
      base::Microseconds(creation_time_us));

  switch (specifics.type()) {
    case sync_pb::BookmarkSpecifics::UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
      break;
    case sync_pb::BookmarkSpecifics::URL: {
      const bookmarks::BookmarkNode* node =
          model->AddURL(parent, index, NodeTitleFromSpecifics(specifics),
                        GURL(specifics.url()), &metainfo, creation_time, guid);
      if (specifics.has_last_used_time_us()) {
        const int64_t last_used_time_us = specifics.last_used_time_us();
        const base::Time last_used_time =
            base::Time::FromDeltaSinceWindowsEpoch(
                // Use FromDeltaSinceWindowsEpoch because last_used_time_us has
                // always used the Windows epoch.
                base::Microseconds(last_used_time_us));
        model->UpdateLastUsedTime(node, last_used_time, /*just_opened=*/false);
      }
      SetBookmarkFaviconFromSpecifics(specifics, node, favicon_service);
      return node;
    }
    case sync_pb::BookmarkSpecifics::FOLDER:
      return model->AddFolder(parent, index, NodeTitleFromSpecifics(specifics),
                              &metainfo, creation_time, guid);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void UpdateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* node,
    BookmarkModelView* model,
    favicon::FaviconService* favicon_service) {
  DCHECK(node);
  DCHECK(model);
  DCHECK(favicon_service);
  // We shouldn't try to update the properties of the BookmarkNode before
  // resolving any conflict in UUID. Either UUIDs are the same, or the UUID in
  // specifics is invalid, and hence we can ignore it.
  base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());
  DCHECK(!guid.is_valid() || guid == node->uuid());

  model->SetTitle(node, NodeTitleFromSpecifics(specifics));
  model->SetNodeMetaInfoMap(node, GetBookmarkMetaInfo(specifics));

  if (!node->is_folder()) {
    model->SetURL(node, GURL(specifics.url()));
    SetBookmarkFaviconFromSpecifics(specifics, node, favicon_service);

    if (specifics.has_last_used_time_us()) {
      const int64_t last_used_time_us = specifics.last_used_time_us();
      const base::Time last_used_time = base::Time::FromDeltaSinceWindowsEpoch(
          // Use FromDeltaSinceWindowsEpoch because last_used_time_us has
          // always used the Windows epoch.
          base::Microseconds(last_used_time_us));
      model->UpdateLastUsedTime(node, last_used_time, /*just_opened=*/false);
    }
  }
}

sync_pb::BookmarkSpecifics::Type GetProtoTypeFromBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  DCHECK(node);

  switch (node->type()) {
    case bookmarks::BookmarkNode::URL:
      DCHECK(!node->is_folder());
      return sync_pb::BookmarkSpecifics::URL;
    case bookmarks::BookmarkNode::FOLDER:
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
    case bookmarks::BookmarkNode::MOBILE:
      DCHECK(node->is_folder());
      return sync_pb::BookmarkSpecifics::FOLDER;
  }
}

const bookmarks::BookmarkNode* ReplaceBookmarkNodeUuid(
    const bookmarks::BookmarkNode* node,
    const base::Uuid& guid,
    BookmarkModelView* model) {
  DCHECK(guid.is_valid());

  if (node->uuid() == guid) {
    // Nothing to do.
    return node;
  }

  const bookmarks::BookmarkNode* new_node = nullptr;
  if (node->is_folder()) {
    new_node = model->AddFolder(
        node->parent(), node->parent()->GetIndexOf(node).value(),
        node->GetTitle(), node->GetMetaInfoMap(), node->date_added(), guid);
    MoveAllChildren(model, node, new_node);
  } else {
    new_node =
        model->AddURL(node->parent(), node->parent()->GetIndexOf(node).value(),
                      node->GetTitle(), node->url(), node->GetMetaInfoMap(),
                      node->date_added(), guid);
  }

  model->Remove(node, FROM_HERE);

  return new_node;
}

bool IsValidBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics) {
  bool is_valid = true;
  if (specifics.ByteSize() == 0) {
    DLOG(ERROR) << "Invalid bookmark: empty specifics.";
    LogInvalidSpecifics(InvalidBookmarkSpecificsError::kEmptySpecifics);
    is_valid = false;
  }
  const base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());

  if (!guid.is_valid()) {
    DLOG(ERROR) << "Invalid bookmark: invalid UUID in specifics.";
    LogInvalidSpecifics(InvalidBookmarkSpecificsError::kInvalidGUID);
    is_valid = false;
  } else if (guid.AsLowercaseString() ==
             bookmarks::kBannedUuidDueToPastSyncBug) {
    DLOG(ERROR) << "Invalid bookmark: banned UUID in specifics.";
    LogInvalidSpecifics(InvalidBookmarkSpecificsError::kBannedGUID);
    is_valid = false;
  }

  const base::Uuid parent_guid =
      base::Uuid::ParseLowercase(specifics.parent_guid());
  if (!parent_guid.is_valid()) {
    DLOG(ERROR) << "Invalid bookmark: invalid parent UUID in specifics.";
    LogInvalidSpecifics(InvalidBookmarkSpecificsError::kInvalidParentGUID);
    is_valid = false;
  }

  switch (specifics.type()) {
    case sync_pb::BookmarkSpecifics::UNSPECIFIED:
      // Note that old data doesn't run into this because DataTypeWorker takes
      // care of backfilling the field.
      DLOG(ERROR) << "Invalid bookmark: invalid type in specifics.";
      is_valid = false;
      break;
    case sync_pb::BookmarkSpecifics::URL:
      if (!GURL(specifics.url()).is_valid()) {
        DLOG(ERROR) << "Invalid bookmark: invalid url in the specifics.";
        LogInvalidSpecifics(InvalidBookmarkSpecificsError::kInvalidURL);
        is_valid = false;
      }
      if (specifics.favicon().empty() && !specifics.icon_url().empty()) {
        DLOG(ERROR) << "Invalid bookmark: specifics cannot have an icon_url "
                       "without having a favicon.";
        LogInvalidSpecifics(
            InvalidBookmarkSpecificsError::kIconURLWithoutFavicon);
        is_valid = false;
      }
      if (!specifics.icon_url().empty() &&
          !GURL(specifics.icon_url()).is_valid()) {
        DLOG(ERROR) << "Invalid bookmark: invalid icon_url in specifics.";
        LogInvalidSpecifics(InvalidBookmarkSpecificsError::kInvalidIconURL);
        is_valid = false;
      }
      break;
    case sync_pb::BookmarkSpecifics::FOLDER:
      break;
  }

  if (!syncer::UniquePosition::FromProto(specifics.unique_position())
           .IsValid()) {
    // Ignore updates with invalid positions.
    DLOG(ERROR) << "Invalid bookmark: invalid unique position.";
    LogInvalidSpecifics(InvalidBookmarkSpecificsError::kInvalidUniquePosition);
    is_valid = false;
  }

  // Verify all keys in meta_info are unique.
  std::unordered_set<std::string_view> keys;
  for (const sync_pb::MetaInfo& meta_info : specifics.meta_info()) {
    if (!keys.insert(meta_info.key()).second) {
      DLOG(ERROR) << "Invalid bookmark: keys in meta_info aren't unique.";
      LogInvalidSpecifics(
          InvalidBookmarkSpecificsError::kNonUniqueMetaInfoKeys);
      is_valid = false;
    }
  }
  return is_valid;
}

base::Uuid InferGuidFromLegacyOriginatorId(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  // Bookmarks created around 2016, between [M44..M52) use an uppercase UUID
  // as originator client item ID, so it requires case-insensitive parsing.
  base::Uuid guid = base::Uuid::ParseCaseInsensitive(originator_client_item_id);
  if (guid.is_valid()) {
    return guid;
  }

  return base::Uuid::ParseLowercase(InferGuidForLegacyBookmark(
      originator_cache_guid, originator_client_item_id));
}

bool HasExpectedBookmarkGuid(const sync_pb::BookmarkSpecifics& specifics,
                             const syncer::ClientTagHash& client_tag_hash,
                             const std::string& originator_cache_guid,
                             const std::string& originator_client_item_id) {
  DCHECK(base::Uuid::ParseLowercase(specifics.guid()).is_valid());

  if (!client_tag_hash.value().empty()) {
    return syncer::ClientTagHash::FromUnhashed(
               syncer::BOOKMARKS, specifics.guid()) == client_tag_hash;
  }

  // Guard against returning true for cases where the UUID cannot be inferred.
  if (originator_cache_guid.empty() && originator_client_item_id.empty()) {
    return false;
  }

  return base::Uuid::ParseLowercase(specifics.guid()) ==
         InferGuidFromLegacyOriginatorId(originator_cache_guid,
                                         originator_client_item_id);
}

}  // namespace sync_bookmarks
