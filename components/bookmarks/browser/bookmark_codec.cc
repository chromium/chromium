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
#include "components/strings/grit/components_strings.h"
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
  // We are going to store the computed checksum. So set stored checksum to be
  // the same as computed checksum.
  stored_checksum_ = computed_checksum_;
  main.Set(kChecksumKey, computed_checksum_);

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
  const int64_t max_already_assigned_id =
      already_assigned_ids.empty() ? 0 : *already_assigned_ids.rbegin();

  if (sync_metadata_str) {
    sync_metadata_str->clear();
  }

  ids_ = std::move(already_assigned_ids);
  uuids_ = {base::Uuid::ParseLowercase(kRootNodeUuid),
            base::Uuid::ParseLowercase(kBookmarkBarNodeUuid),
            base::Uuid::ParseLowercase(kOtherBookmarksNodeUuid),
            base::Uuid::ParseLowercase(kMobileBookmarksNodeUuid),
            base::Uuid::ParseLowercase(kManagedNodeUuid)};
  ids_reassigned_ = false;
  uuids_reassigned_ = false;
  ids_valid_ = true;
  maximum_id_ = 0;
  stored_checksum_.clear();
  InitializeChecksum();
  bool success = DecodeHelper(bb_node, other_folder_node, mobile_folder_node,
                              value, sync_metadata_str);
  FinalizeChecksum();
  // If either the checksums differ or some IDs were missing/not unique,
  // reassign IDs.
  if (!ids_valid_ || computed_checksum_ != stored_checksum_) {
    maximum_id_ = max_already_assigned_id;
    ReassignIDs(bb_node, other_folder_node, mobile_folder_node);
  }
  *max_id = maximum_id_ + 1;
  return success;
}

bool BookmarkCodec::required_recovery() const {
  return ids_reassigned_ || uuids_reassigned_ ||
         computed_checksum_ != stored_checksum_;
}

base::Value::Dict BookmarkCodec::EncodeNode(const BookmarkNode* node) {
  base::Value::Dict value;
  std::string id = base::NumberToString(node->id());
  value.Set(kIdKey, id);
  const std::u16string& title = node->GetTitle();
  value.Set(kNameKey, title);
  const std::string& uuid = node->uuid().AsLowercaseString();
  value.Set(kGuidKey, uuid);
  // TODO(crbug.com/40479288): Avoid ToInternalValue().
  value.Set(kDateAddedKey,
            base::NumberToString(node->date_added().ToInternalValue()));
  value.Set(kDateLastUsed,
            base::NumberToString(node->date_last_used().ToInternalValue()));
  if (node->is_url()) {
    value.Set(kTypeKey, kTypeURL);
    std::string url = node->url().possibly_invalid_spec();
    value.Set(kURLKey, url);
    UpdateChecksumWithUrlNode(id, title, url);
  } else {
    value.Set(kTypeKey, kTypeFolder);
    value.Set(
        kDateModifiedKey,
        base::NumberToString(node->date_folder_modified().ToInternalValue()));
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

  const base::Value* checksum_value = value.Find(kChecksumKey);
  if (checksum_value) {
    const std::string* checksum = checksum_value->GetIfString();
    if (checksum)
      stored_checksum_ = *checksum;
    else
      return false;
  }

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

bool BookmarkCodec::DecodeChildren(const base::Value::List& child_value_list,
                                   BookmarkNode* parent) {
  for (const base::Value& child_value : child_value_list) {
    if (!child_value.is_dict())
      return false;
    DecodeNode(child_value.GetDict(), parent, nullptr);
  }
  return true;
}

bool BookmarkCodec::DecodeNode(const base::Value::Dict& value,
                               BookmarkNode* parent,
                               BookmarkNode* node) {
  // If no |node| is specified, we'll create one and add it to the |parent|.
  // Therefore, in that case, |parent| must be non-NULL.
  if (!node && !parent) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // It's not valid to have both a node and a specified parent.
  if (node && parent) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  std::string id_string;
  int64_t id = 0;

  {
    const std::string* string = value.FindString(kIdKey);
    if (!string || !base::StringToInt64(*string, &id) || id <= 0 ||
        ids_.count(id) != 0) {
      ids_valid_ = false;
    } else {
      ids_.insert(id);
      id_string = *string;
    }
  }

  maximum_id_ = std::max(maximum_id_, id);

  std::u16string title;
  const std::string* string_value = value.FindString(kNameKey);
  if (string_value)
    title = base::UTF8ToUTF16(*string_value);

  base::Uuid uuid;
  // |node| is only passed in for bookmarks of type BookmarkPermanentNode, in
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

  std::string date_added_string;
  string_value = value.FindString(kDateAddedKey);
  if (string_value)
    date_added_string = *string_value;
  else
    date_added_string = base::NumberToString(Time::Now().ToInternalValue());
  int64_t date_added_time;
  base::StringToInt64(date_added_string, &date_added_time);

  std::string date_last_used_string;
  string_value = value.FindString(kDateLastUsed);
  if (string_value)
    date_last_used_string = *string_value;
  else
    date_last_used_string = base::NumberToString(0);
  int64_t date_last_used;
  base::StringToInt64(date_last_used_string, &date_last_used);

  const std::string* type_string = value.FindString(kTypeKey);
  if (!type_string)
    return false;

  if (*type_string != kTypeURL && *type_string != kTypeFolder)
    return false;  // Unknown type.

  if (*type_string == kTypeURL) {
    const std::string* url_string = value.FindString(kURLKey);
    if (!url_string)
      return false;

    GURL url = GURL(*url_string);
    if (!node && url.is_valid()) {
      DCHECK(uuid.is_valid());
      node = new BookmarkNode(id, uuid, url);
    } else {
      return false;  // Node invalid.
    }

    if (parent)
      parent->Add(base::WrapUnique(node));
    UpdateChecksumWithUrlNode(id_string, title, *url_string);
  } else {
    std::string last_modified_date;
    string_value = value.FindString(kDateModifiedKey);
    if (string_value)
      last_modified_date = *string_value;
    else
      last_modified_date = base::NumberToString(Time::Now().ToInternalValue());

    const base::Value::List* child_values = value.FindList(kChildrenKey);
    if (!child_values)
      return false;

    if (!node) {
      DCHECK(uuid.is_valid());
      node = new BookmarkNode(id, uuid, GURL());
    } else {
      // If a new node is not created, explicitly assign ID to the existing one.
      node->set_id(id);
    }

    int64_t internal_time;
    base::StringToInt64(last_modified_date, &internal_time);
    node->set_date_folder_modified(Time::FromInternalValue(internal_time));

    if (parent)
      parent->Add(base::WrapUnique(node));

    UpdateChecksumWithFolderNode(id_string, title);

    if (!DecodeChildren(*child_values, node))
      return false;
  }

  node->SetTitle(title);
  node->set_date_added(Time::FromInternalValue(date_added_time));
  node->set_date_last_used(Time::FromInternalValue(date_last_used));

  BookmarkNode::MetaInfoMap meta_info_map;
  if (!DecodeMetaInfo(value, &meta_info_map))
    return false;
  node->SetMetaInfoMap(meta_info_map);

  return true;
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

void BookmarkCodec::ReassignIDs(BookmarkNode* bb_node,
                                BookmarkNode* other_node,
                                BookmarkNode* mobile_node) {
  ids_.clear();
  reassigned_ids_per_old_id_.clear();
  ReassignIDsHelper(bb_node);
  ReassignIDsHelper(other_node);
  ReassignIDsHelper(mobile_node);
  ids_reassigned_ = true;
}

void BookmarkCodec::ReassignIDsHelper(BookmarkNode* node) {
  DCHECK(node);
  const int64_t old_id = node->id();
  node->set_id(++maximum_id_);
  reassigned_ids_per_old_id_.emplace(old_id, node->id());
  ids_.insert(node->id());
  for (const auto& child : node->children())
    ReassignIDsHelper(child.get());
}

void BookmarkCodec::UpdateChecksum(const std::string& str) {
  base::MD5Update(&md5_context_, str);
}

void BookmarkCodec::UpdateChecksum(const std::u16string& str) {
  base::MD5Update(&md5_context_,
                  std::string_view(reinterpret_cast<const char*>(str.data()),
                                   str.length() * sizeof(str[0])));
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
  base::MD5Init(&md5_context_);
}

void BookmarkCodec::FinalizeChecksum() {
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  computed_checksum_ = base::MD5DigestToBase16(digest);
}

}  // namespace bookmarks
