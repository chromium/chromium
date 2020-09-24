// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/favicon/core/favicon_service.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_bookmarks/switches.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

// Maximum number of bytes to allow in a legacy canonicalized title (must match
// sync's internal limits; see write_node.cc).
const int kLegacyCanonicalizedTitleLimitBytes = 255;

// Used in metrics: "Sync.InvalidBookmarkSpecifics". These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class InvalidBookmarkSpecificsError {
  kEmptySpecifics = 0,
  kInvalidURL = 1,
  kIconURLWithoutFavicon = 2,
  kInvalidIconURL = 3,
  kNonUniqueMetaInfoKeys = 4,
  kInvalidGUID = 5,

  kMaxValue = kInvalidGUID,
};

void LogInvalidSpecifics(InvalidBookmarkSpecificsError error) {
  base::UmaHistogramEnumeration("Sync.InvalidBookmarkSpecifics", error);
}

void UpdateBookmarkSpecificsMetaInfo(
    const bookmarks::BookmarkNode::MetaInfoMap* metainfo_map,
    sync_pb::BookmarkSpecifics* bm_specifics) {
  for (const std::pair<const std::string, std::string>& pair : *metainfo_map) {
    sync_pb::MetaInfo* meta_info = bm_specifics->add_meta_info();
    meta_info->set_key(pair.first);
    meta_info->set_value(pair.second);
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
  DCHECK(favicon_service);

  favicon_service->AddPageNoVisitForBookmark(bookmark_node->url(),
                                             bookmark_node->GetTitle());

  const std::string& icon_bytes_str = specifics.favicon();
  scoped_refptr<base::RefCountedString> icon_bytes(
      new base::RefCountedString());
  icon_bytes->data().assign(icon_bytes_str);

  GURL icon_url(specifics.icon_url());

  if (icon_bytes->size() == 0 && icon_url.is_empty()) {
    // Empty icon URL and no bitmap data means no icon mapping.
    favicon_service->DeleteFaviconMappings({bookmark_node->url()},
                                           favicon_base::IconType::kFavicon);
    return;
  }

  if (icon_url.is_empty()) {
    // WebUI pages such as "chrome://bookmarks/" are missing a favicon URL but
    // they have a favicon. In addition, ancient clients (prior to M25) may not
    // be syncing the favicon URL. If the icon URL is not synced, use the page
    // URL as a fake icon URL as it is guaranteed to be unique.
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
// TODO(crbug.com/1032052): Remove when client tags are adopted in
// ModelTypeWorker.
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

// This is an exact copy of the same code in bookmark_update_preprocessing.cc.
// TODO(crbug.com/1032052): Remove when client tags are adopted in
// ModelTypeWorker.
std::string InferGuidForLegacyBookmark(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  DCHECK(!base::IsValidGUID(originator_client_item_id));

  const std::string unique_tag =
      base::StrCat({originator_cache_guid, originator_client_item_id});
  const base::SHA1Digest hash =
      base::SHA1HashSpan(base::as_bytes(base::make_span(unique_tag)));

  static_assert(base::kSHA1Length >= 16, "16 bytes needed to infer GUID");

  const std::string guid = ComputeGuidFromBytes(base::make_span(hash));
  DCHECK(base::IsValidGUIDOutputString(guid));
  return guid;
}

base::string16 NodeTitleFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics) {
  if (specifics.has_full_title()) {
    return base::UTF8ToUTF16(specifics.full_title());
  }
  std::string node_title;
  syncer::ServerNameToSyncAPIName(specifics.legacy_canonicalized_title(),
                                  &node_title);
  return base::UTF8ToUTF16(node_title);
}

}  // namespace

std::string FullTitleToLegacyCanonicalizedTitle(const std::string& node_title) {
  // Adjust the title for backward compatibility with legacy clients.
  std::string specifics_title;
  syncer::SyncAPINameToServerName(node_title, &specifics_title);
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
  if (remote_entity_data.specifics.bookmark().has_full_title() &&
      !remote_entity_data.is_bookmark_guid_in_specifics_preprocessed) {
    return false;
  }
  return base::FeatureList::IsEnabled(
      switches::kSyncReuploadBookmarkFullTitles);
}

sync_pb::EntitySpecifics CreateSpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    bool force_favicon_load,
    bool include_guid) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  if (!node->is_folder()) {
    bm_specifics->set_url(node->url().spec());
  }

  DCHECK(!node->guid().empty());
  DCHECK(base::IsValidGUIDOutputString(node->guid()))
      << "Actual: " << node->guid();

  if (include_guid) {
    bm_specifics->set_guid(node->guid());
  }

  const std::string node_title = base::UTF16ToUTF8(node->GetTitle());
  bm_specifics->set_legacy_canonicalized_title(
      FullTitleToLegacyCanonicalizedTitle(node_title));
  bm_specifics->set_full_title(node_title);
  bm_specifics->set_creation_time_us(
      node->date_added().ToDeltaSinceWindowsEpoch().InMicroseconds());

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
    bm_specifics->set_favicon(favicon_bytes->front(), favicon_bytes->size());
    bm_specifics->set_icon_url(node->icon_url() ? node->icon_url()->spec()
                                                : std::string());
  } else {
    bm_specifics->clear_favicon();
    bm_specifics->clear_icon_url();
  }

  return specifics;
}

const bookmarks::BookmarkNode* CreateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool is_folder,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service) {
  DCHECK(parent);
  DCHECK(model);
  DCHECK(favicon_service);
  DCHECK(base::IsValidGUIDOutputString(specifics.guid()));

  bookmarks::BookmarkNode::MetaInfoMap metainfo =
      GetBookmarkMetaInfo(specifics);
  const bookmarks::BookmarkNode* node;
  if (is_folder) {
    node = model->AddFolder(parent, index, NodeTitleFromSpecifics(specifics),
                            &metainfo, specifics.guid());
  } else {
    const int64_t create_time_us = specifics.creation_time_us();
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        // Use FromDeltaSinceWindowsEpoch because create_time_us has
        // always used the Windows epoch.
        base::TimeDelta::FromMicroseconds(create_time_us));
    node = model->AddURL(parent, index, NodeTitleFromSpecifics(specifics),
                         GURL(specifics.url()), &metainfo, create_time,
                         specifics.guid());
  }
  SetBookmarkFaviconFromSpecifics(specifics, node, favicon_service);
  return node;
}

void UpdateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service) {
  DCHECK(node);
  DCHECK(model);
  DCHECK(favicon_service);
  // We shouldn't try to update the properties of the BookmarkNode before
  // resolving any conflict in GUID. Either GUIDs are the same, or the GUID in
  // specifics is invalid, and hence we can ignore it.
  DCHECK(specifics.guid() == node->guid() ||
         !base::IsValidGUIDOutputString(specifics.guid()));

  if (!node->is_folder()) {
    model->SetURL(node, GURL(specifics.url()));
  }

  model->SetTitle(node, NodeTitleFromSpecifics(specifics));
  model->SetNodeMetaInfoMap(node, GetBookmarkMetaInfo(specifics));
  SetBookmarkFaviconFromSpecifics(specifics, node, favicon_service);
}

// TODO(crbug.com/1005219): Replace this function to move children between
// parent nodes more efficiently.
const bookmarks::BookmarkNode* ReplaceBookmarkNodeGUID(
    const bookmarks::BookmarkNode* node,
    const std::string& guid,
    bookmarks::BookmarkModel* model) {
  DCHECK(base::IsValidGUIDOutputString(guid));

  if (node->guid() == guid) {
    // Nothing to do.
    return node;
  }

  const bookmarks::BookmarkNode* new_node = nullptr;
  if (node->is_folder()) {
    new_node =
        model->AddFolder(node->parent(), node->parent()->GetIndexOf(node),
                         node->GetTitle(), node->GetMetaInfoMap(), guid);
  } else {
    new_node = model->AddURL(node->parent(), node->parent()->GetIndexOf(node),
                             node->GetTitle(), node->url(),
                             node->GetMetaInfoMap(), node->date_added(), guid);
  }
  for (size_t i = node->children().size(); i > 0; --i) {
    model->Move(node->children()[i - 1].get(), new_node, 0);
  }
  model->Remove(node);

  return new_node;
}

bool IsValidBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics,
                              bool is_folder) {
  bool is_valid = true;
  if (specifics.ByteSize() == 0) {
    DLOG(ERROR) << "Invalid bookmark: empty specifics.";
    LogInvalidSpecifics(InvalidBookmarkSpecificsError::kEmptySpecifics);
    is_valid = false;
  }
  if (!base::IsValidGUIDOutputString(specifics.guid())) {
    DLOG(ERROR) << "Invalid bookmark: invalid GUID in the specifics.";
    LogInvalidSpecifics(InvalidBookmarkSpecificsError::kInvalidGUID);
    is_valid = false;
  }
  if (!is_folder) {
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
  }

  // Verify all keys in meta_info are unique.
  std::unordered_set<base::StringPiece, base::StringPieceHash> keys;
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

bool HasExpectedBookmarkGuid(const sync_pb::BookmarkSpecifics& specifics,
                             const std::string& originator_cache_guid,
                             const std::string& originator_client_item_id) {
  DCHECK(base::IsValidGUIDOutputString(specifics.guid()));

  if (originator_client_item_id.empty()) {
    // This could be a future bookmark with a client tag instead of an
    // originator client item ID.
    NOTIMPLEMENTED();
    return true;
  }

  if (base::IsValidGUID(originator_client_item_id)) {
    // Bookmarks created around 2016, between [M44..M52) use an uppercase GUID
    // as originator client item ID, so it needs to be lowercased to adhere to
    // the invariant that GUIDs in specifics are canonicalized.
    return specifics.guid() == base::ToLowerASCII(originator_client_item_id);
  }

  return specifics.guid() ==
         InferGuidForLegacyBookmark(originator_cache_guid,
                                    originator_client_item_id);
}

}  // namespace sync_bookmarks
