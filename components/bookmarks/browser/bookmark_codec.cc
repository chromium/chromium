// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_codec.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/strings/grit/components_strings.h"
#include "crypto/hash.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::Time;

namespace bookmarks {

const char BookmarkCodec::kRootsKey[] = "roots";
const char BookmarkCodec::kBookmarkBarFolderNameKey[] = "bookmark_bar";
const char BookmarkCodec::kOtherBookmarkFolderNameKey[] = "other";
// The value is left as 'synced' for historical reasons.
const char BookmarkCodec::kMobileBookmarkFolderNameKey[] = "synced";
const char BookmarkCodec::kVersionKey[] = "version";
const char BookmarkCodec::kChecksumKey[] = "checksum";
const char BookmarkCodec::kChecksumSHA256Key[] = "checksum_sha256";
const char BookmarkCodec::kIdKey[] = "id";
const char BookmarkCodec::kTypeKey[] = "type";
const char BookmarkCodec::kNameKey[] = "name";
const char BookmarkCodec::kGuidKey[] = "guid";
const char BookmarkCodec::kDateAddedKey[] = "date_added";
const char BookmarkCodec::kURLKey[] = "url";
const char BookmarkCodec::kDateModifiedKey[] = "date_modified";
const char BookmarkCodec::kChildrenKey[] = "children";
const char BookmarkCodec::kMetaInfo[] = "meta_info";
const char BookmarkCodec::kTypeURL[] = "url";
const char BookmarkCodec::kTypeFolder[] = "folder";
const char BookmarkCodec::kSyncMetadata[] = "sync_metadata";
const char BookmarkCodec::kDateLastUsed[] = "date_last_used";

// Current version of the file.
static const int kCurrentVersion = 1;

namespace {

// Encodes Sync metadata and cleans up the input string to decrease peak memory
// usage during encoding.
base::Value EncodeSyncMetadata(std::string sync_metadata_str) {
  return base::Value(base::Base64Encode(sync_metadata_str));
}

// Helper function to convert Time to microseconds since Windows epoch.
int64_t ToMicrosecondsSinceWindowsEpoch(Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Helper function to parse date from dictionary, returns nullopt if not found.
std::optional<Time> FindMicrosecondsSinceWindowsEpoch(
    const base::Value::Dict& dict,
    std::string_view key) {
  const std::string* string_value = dict.FindString(key);
  if (!string_value) {
    return std::nullopt;
  }

  int64_t microseconds = 0;
  if (!base::StringToInt64(*string_value, &microseconds)) {
    return std::nullopt;
  }

  return Time::FromDeltaSinceWindowsEpoch(base::Microseconds(microseconds));
}

}  // namespace

BookmarkCodec::BookmarkCodec() = default;

BookmarkCodec::~BookmarkCodec() = default;

base::Value::Dict BookmarkCodec::Encode(
    const BookmarkNode* bookmark_bar_node,
    const BookmarkNode* other_folder_node,
    const BookmarkNode* mobile_folder_node,
    std::string sync_metadata_str) {
  ids_reassigned_ = false;
  uuids_reassigned_ = false;

  base::Value::Dict main;
  main.Set(kVersionKey, kCurrentVersion);

  // Encode Sync metadata before encoding other fields to reduce peak memory
  // usage.
  if (!sync_metadata_str.empty()) {
    main.Set(kSyncMetadata, EncodeSyncMetadata(std::move(sync_metadata_str)));
    sync_metadata_str.clear();
  }

  InitializeChecksum();
  base::Value::Dict roots;

  if (bookmark_bar_node) {
    // If one permanent node is provided, all permanent nodes should have been
    // provided.
    CHECK(other_folder_node);
    CHECK(mobile_folder_node);
    roots.Set(kBookmarkBarFolderNameKey, EncodeNode(bookmark_bar_node));
    roots.Set(kOtherBookmarkFolderNameKey, EncodeNode(other_folder_node));
    roots.Set(kMobileBookmarkFolderNameKey, EncodeNode(mobile_folder_node));
  } else {
    // No permanent node should have been provided.
    CHECK(!other_folder_node);
    CHECK(!mobile_folder_node);
  }

  FinalizeChecksum();
  main.Set(kChecksumKey, computed_checksum_);
  if (base::FeatureList::IsEnabled(kEnableBookmarkCodecSHA256)) {
    main.Set(kChecksumSHA256Key, computed_sha256_checksum_);
  }
  main.Set(kRootsKey, std::move(roots));
  return main;
}

bool BookmarkCodec::Decode(const base::Value::Dict& value,
                           std::set<int64_t> already_assigned_ids,
                           BookmarkNode* bb_node,
                           BookmarkNode* other_folder_node,
                           BookmarkNode* mobile_folder_node,
                           int64_t* max_id,
                           std::string* sync_metadata_str) {
  if (sync_metadata_str) {
    sync_metadata_str->clear();
  }

  ids_ = std::move(already_assigned_ids);
  maximum_id_ = ids_.empty() ? 0 : *ids_.rbegin();
  uuids_ = {base::Uuid::ParseLowercase(kRootNodeUuid),
            base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
            base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
            base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
            base::Uuid::ParseLowercase(kManagedNodeUuid)};
  ids_reassigned_ = false;
  uuids_reassigned_ = false;
  nodes_requiring_id_reassignment_.clear();
  reassigned_ids_per_old_id_.clear();

  bool success = DecodeHelper(bb_node, other_folder_node, mobile_folder_node,
                              value, sync_metadata_str);
  ReassignIDsIfRequired();

  *max_id = maximum_id_ + 1;
  return success;
}

bool BookmarkCodec::required_recovery() const {
  return ids_reassigned_ || uuids_reassigned_;
}

base::Value::Dict BookmarkCodec::EncodeNode(const BookmarkNode* node) {
  base::Value::Dict value;
  std::string id = base::NumberToString(node->id());
  value.Set(kIdKey, id);
  const std::u16string& title = node->GetTitle();
  value.Set(kNameKey, title);
  const std::string& uuid = node->uuid().AsLowercaseString();
  value.Set(kGuidKey, uuid);
  value.Set(kDateAddedKey, base::NumberToString(ToMicrosecondsSinceWindowsEpoch(
                               node->date_added())));
  value.Set(kDateLastUsed, base::NumberToString(ToMicrosecondsSinceWindowsEpoch(
                               node->date_last_used())));
  if (node->is_url()) {
    value.Set(kTypeKey, kTypeURL);
    std::string url = node->url().possibly_invalid_spec();
    value.Set(kURLKey, url);
    UpdateChecksumWithUrlNode(id, title, url);
  } else {
    value.Set(kTypeKey, kTypeFolder);
    value.Set(kDateModifiedKey,
              base::NumberToString(ToMicrosecondsSinceWindowsEpoch(
                  node->date_folder_modified())));
    UpdateChecksumWithFolderNode(id, title);

    base::Value::List child_values;
    for (const auto& child : node->children())
      child_values.Append(EncodeNode(child.get()));
    value.Set(kChildrenKey, base::Value(std::move(child_values)));
  }
  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (meta_info_map)
    value.Set(kMetaInfo, EncodeMetaInfo(*meta_info_map));
  return value;
}

base::Value::Dict BookmarkCodec::EncodeMetaInfo(
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  base::Value::Dict meta_info;
  for (const auto& item : meta_info_map)
    meta_info.Set(item.first, base::Value(item.second));
  return meta_info;
}

bool BookmarkCodec::DecodeHelper(BookmarkNode* bb_node,
                                 BookmarkNode* other_folder_node,
                                 BookmarkNode* mobile_folder_node,
                                 const base::Value::Dict& value,
                                 std::string* sync_metadata_str) {
  std::optional<int> version = value.FindInt(kVersionKey);
  if (!version || *version != kCurrentVersion)
    return false;  // Unknown version.

  if (sync_metadata_str) {
    const std::string* sync_metadata_str_base64 =
        value.FindString(kSyncMetadata);
    if (sync_metadata_str_base64) {
      base::Base64Decode(*sync_metadata_str_base64, sync_metadata_str);
    }
  }

  const base::Value::Dict* roots = value.FindDict(kRootsKey);
  if (!roots)
    return false;  // No roots, or invalid type for roots.
  const base::Value::Dict* bb_value =
      roots->FindDict(kBookmarkBarFolderNameKey);
  const base::Value::Dict* other_folder_value =
      roots->FindDict(kOtherBookmarkFolderNameKey);
  const base::Value::Dict* mobile_folder_value =
      roots->FindDict(kMobileBookmarkFolderNameKey);

  if (!bb_value || !other_folder_value || !mobile_folder_value)
    return false;

  DecodeNode(*bb_value, nullptr, bb_node);
  DecodeNode(*other_folder_value, nullptr, other_folder_node);
  DecodeNode(*mobile_folder_value, nullptr, mobile_folder_node);

  // Need to reset the title as the title is persisted and restored from
  // the file.
  bb_node->SetTitle(l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME));
  other_folder_node->SetTitle(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME));
  mobile_folder_node->SetTitle(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME));

  return true;
}

void BookmarkCodec::DecodeChildren(const base::Value::List& child_value_list,
                                   BookmarkNode* parent) {
  for (const base::Value& child_value : child_value_list) {
    if (child_value.is_dict()) {
      DecodeNode(child_value.GetDict(), parent, nullptr);
    }
  }
}

void BookmarkCodec::DecodeNode(const base::Value::Dict& value,
                               BookmarkNode* parent,
                               BookmarkNode* node) {
  // If no `node` is specified, we'll create one and add it to the `parent`.
  // Therefore, in that case, `parent` must be non-NULL.
  CHECK(node || parent);

  // It's not valid to have both a node and a specified parent.
  CHECK(!node || !parent);

  int64_t id = 0;
  bool id_requires_reassignment = true;

  if (const std::string* string = value.FindString(kIdKey);
      string && base::StringToInt64(*string, &id) && id > 0 &&
      ids_.insert(id).second) {
    id_requires_reassignment = false;
    maximum_id_ = std::max(maximum_id_, id);
  }

  std::u16string title;
  if (const std::string* string = value.FindString(kNameKey); string) {
    title = base::UTF8ToUTF16(*string);
  }

  base::Uuid uuid;
  // `node` is only passed in for bookmarks of type BookmarkPermanentNode, in
  // which case we do not need to check for UUID validity as their UUIDs are
  // hard-coded and not read from the persisted file.
  if (!node) {
    // UUIDs can be empty for bookmarks that were created before UUIDs were
    // required. When encountering one such bookmark we thus assign to it a new
    // UUID. The same applies if the stored UUID is invalid or a duplicate.
    const std::string* uuid_str = value.FindString(kGuidKey);
    if (uuid_str && !uuid_str->empty()) {
      uuid = base::Uuid::ParseCaseInsensitive(*uuid_str);
    }

    if (!uuid.is_valid()) {
      uuid = base::Uuid::GenerateRandomV4();
      uuids_reassigned_ = true;
    }

    if (uuid.AsLowercaseString() == kBannedUuidDueToPastSyncBug) {
      uuid = base::Uuid::GenerateRandomV4();
      uuids_reassigned_ = true;
    }

    // Guard against UUID collisions, which would violate BookmarkModel's
    // invariant that each UUID is unique.
    if (base::Contains(uuids_, uuid)) {
      uuid = base::Uuid::GenerateRandomV4();
      uuids_reassigned_ = true;
    }

    uuids_.insert(uuid);
  }

  const std::string* type_string = value.FindString(kTypeKey);
  if (!type_string) {
    return;
  }

  if (*type_string != kTypeURL && *type_string != kTypeFolder) {
    return;  // Unknown type.
  }

  if (*type_string == kTypeURL) {
    const std::string* url_string = value.FindString(kURLKey);
    if (!url_string) {
      return;
    }

    GURL url = GURL(*url_string);
    if (!node && url.is_valid()) {
      DCHECK(uuid.is_valid());
      node = new BookmarkNode(id, uuid, url);
    } else {
      return;  // Node invalid.
    }

    if (parent)
      parent->Add(base::WrapUnique(node));
  } else {
    const base::Value::List* child_values = value.FindList(kChildrenKey);
    if (!child_values)
      return;

    if (!node) {
      DCHECK(uuid.is_valid());
      node = new BookmarkNode(id, uuid, GURL());
    } else {
      // If a new node is not created, explicitly assign ID to the existing one.
      node->set_id(id);
    }

    node->set_date_folder_modified(
        FindMicrosecondsSinceWindowsEpoch(value, kDateModifiedKey)
            .value_or(Time::Now()));

    if (parent) {
      parent->Add(base::WrapUnique(node));
    }

    DecodeChildren(*child_values, node);
  }

  if (id_requires_reassignment) {
    nodes_requiring_id_reassignment_.push_back(node);
  }

  node->SetTitle(title);
  node->set_date_added(FindMicrosecondsSinceWindowsEpoch(value, kDateAddedKey)
                           .value_or(Time::Now()));
  node->set_date_last_used(
      FindMicrosecondsSinceWindowsEpoch(value, kDateLastUsed).value_or(Time()));

  if (BookmarkNode::MetaInfoMap meta_info_map;
      DecodeMetaInfo(value, &meta_info_map)) {
    node->SetMetaInfoMap(meta_info_map);
  }
}

bool BookmarkCodec::DecodeMetaInfo(const base::Value::Dict& value,
                                   BookmarkNode::MetaInfoMap* meta_info_map) {
  DCHECK(meta_info_map);
  meta_info_map->clear();

  const base::Value* meta_info = value.Find(kMetaInfo);
  if (!meta_info)
    return true;

  std::unique_ptr<base::Value> deserialized_holder;

  // Meta info used to be stored as a serialized dictionary, so attempt to
  // parse the value as one.
  const std::string* meta_info_str = meta_info->GetIfString();
  if (meta_info_str) {
    JSONStringValueDeserializer deserializer(*meta_info_str);
    deserialized_holder = deserializer.Deserialize(nullptr, nullptr);
    if (!deserialized_holder)
      return false;
    meta_info = deserialized_holder.get();
  }
  // meta_info is now either the kMetaInfo node, or the deserialized node if it
  // was stored as a string. Either way it should now be a (possibly nested)
  // dictionary of meta info values.
  if (!meta_info->is_dict())
    return false;
  DecodeMetaInfoHelper(meta_info->GetDict(), std::string(), meta_info_map);

  return true;
}

void BookmarkCodec::DecodeMetaInfoHelper(
    const base::Value::Dict& dict,
    const std::string& prefix,
    BookmarkNode::MetaInfoMap* meta_info_map) {
  for (const auto it : dict) {
    // Deprecated keys should be excluded after removing enhanced bookmarks
    // feature crrev.com/1638413003.
    if (base::StartsWith(it.first, "stars.", base::CompareCase::SENSITIVE)) {
      continue;
    }

    if (it.second.is_dict()) {
      DecodeMetaInfoHelper(it.second.GetDict(), prefix + it.first + ".",
                           meta_info_map);
    } else {
      const std::string* str = it.second.GetIfString();
      if (str)
        (*meta_info_map)[prefix + it.first] = *str;
    }
  }
}

void BookmarkCodec::ReassignIDsIfRequired() {
  if (nodes_requiring_id_reassignment_.empty()) {
    // Nothing to do.
    return;
  }

  for (BookmarkNode* node : nodes_requiring_id_reassignment_) {
    const int64_t old_id = node->id();
    node->set_id(++maximum_id_);
    reassigned_ids_per_old_id_.emplace(old_id, node->id());
    ids_.insert(node->id());
  }

  nodes_requiring_id_reassignment_.clear();
  ids_reassigned_ = true;
}

void BookmarkCodec::UpdateChecksum(const std::string& str) {
  md5_hasher_.Update(str);
  sha256_hasher_.Update(str);
}

void BookmarkCodec::UpdateChecksum(const std::u16string& str) {
  auto bytes = base::as_byte_span(str);
  md5_hasher_.Update(bytes);
  sha256_hasher_.Update(bytes);
}

void BookmarkCodec::UpdateChecksumWithUrlNode(const std::string& id,
                                              const std::u16string& title,
                                              const std::string& url) {
  DCHECK(base::IsStringUTF8(url));
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeURL);
  UpdateChecksum(url);
}

void BookmarkCodec::UpdateChecksumWithFolderNode(const std::string& id,
                                                 const std::u16string& title) {
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeFolder);
}

void BookmarkCodec::InitializeChecksum() {
  md5_hasher_ = crypto::obsolete::Md5();
  sha256_hasher_ = crypto::hash::Hasher(crypto::hash::kSha256);
}

void BookmarkCodec::FinalizeChecksum() {
  computed_checksum_ = base::HexEncodeLower(md5_hasher_.Finish());
  std::string result(crypto::hash::kSha256Size, 0);
  sha256_hasher_.Finish(base::as_writable_byte_span(result));
  computed_sha256_checksum_ = base::HexEncodeLower(result);
}

}  // namespace bookmarks
