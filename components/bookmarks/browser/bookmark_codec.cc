// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_codec.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
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
const char BookmarkCodec::kUnsyncedMetaInfo[] = "unsynced_meta_info";
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
  std::string sync_metadata_str_base64;
  base::Base64Encode(sync_metadata_str, &sync_metadata_str_base64);
  return base::Value(std::move(sync_metadata_str_base64));
}

}  // namespace

BookmarkCodec::BookmarkCodec()
    : ids_reassigned_(false),
      guids_reassigned_(false),
      ids_valid_(true),
      maximum_id_(0) {}

BookmarkCodec::~BookmarkCodec() = default;

base::Value BookmarkCodec::Encode(BookmarkModel* model,
                                  std::string sync_metadata_str) {
  return Encode(model->bookmark_bar_node(), model->other_node(),
                model->mobile_node(), model->root_node()->GetMetaInfoMap(),
                model->root_node()->GetUnsyncedMetaInfoMap(),
                std::move(sync_metadata_str));
}

base::Value BookmarkCodec::Encode(
    const BookmarkNode* bookmark_bar_node,
    const BookmarkNode* other_folder_node,
    const BookmarkNode* mobile_folder_node,
    const BookmarkNode::MetaInfoMap* model_meta_info_map,
    const BookmarkNode::MetaInfoMap* model_unsynced_meta_info_map,
    std::string sync_metadata_str) {
  ids_reassigned_ = false;
  guids_reassigned_ = false;

  base::Value main(base::Value::Type::DICTIONARY);
  main.SetIntKey(kVersionKey, kCurrentVersion);

  // Encode Sync metadata before encoding other fields to reduce peak memory
  // usage.
  if (!sync_metadata_str.empty()) {
    main.SetKey(kSyncMetadata,
                EncodeSyncMetadata(std::move(sync_metadata_str)));
    sync_metadata_str.clear();
  }

  InitializeChecksum();
  base::Value roots(base::Value::Type::DICTIONARY);
  roots.SetKey(kBookmarkBarFolderNameKey, EncodeNode(bookmark_bar_node));
  roots.SetKey(kOtherBookmarkFolderNameKey, EncodeNode(other_folder_node));
  roots.SetKey(kMobileBookmarkFolderNameKey, EncodeNode(mobile_folder_node));
  if (model_meta_info_map)
    roots.SetKey(kMetaInfo, EncodeMetaInfo(*model_meta_info_map));
  if (model_unsynced_meta_info_map) {
    roots.SetKey(kUnsyncedMetaInfo,
                 EncodeMetaInfo(*model_unsynced_meta_info_map));
  }

  FinalizeChecksum();
  // We are going to store the computed checksum. So set stored checksum to be
  // the same as computed checksum.
  stored_checksum_ = computed_checksum_;
  main.SetStringKey(kChecksumKey, computed_checksum_);

  main.SetKey(kRootsKey, std::move(roots));
  return main;
}

bool BookmarkCodec::Decode(const base::Value& value,
                           BookmarkNode* bb_node,
                           BookmarkNode* other_folder_node,
                           BookmarkNode* mobile_folder_node,
                           int64_t* max_id,
                           std::string* sync_metadata_str) {
  ids_.clear();
  guids_ = {base::GUID::ParseLowercase(BookmarkNode::kRootNodeGuid),
            base::GUID::ParseLowercase(BookmarkNode::kBookmarkBarNodeGuid),
            base::GUID::ParseLowercase(BookmarkNode::kOtherBookmarksNodeGuid),
            base::GUID::ParseLowercase(BookmarkNode::kMobileBookmarksNodeGuid),
            base::GUID::ParseLowercase(BookmarkNode::kManagedNodeGuid)};
  ids_reassigned_ = false;
  guids_reassigned_ = false;
  ids_valid_ = true;
  maximum_id_ = 0;
  stored_checksum_.clear();
  InitializeChecksum();
  bool success = DecodeHelper(bb_node, other_folder_node, mobile_folder_node,
                              value, sync_metadata_str);
  FinalizeChecksum();
  // If either the checksums differ or some IDs were missing/not unique,
  // reassign IDs.
  if (!ids_valid_ || computed_checksum() != stored_checksum())
    ReassignIDs(bb_node, other_folder_node, mobile_folder_node);
  *max_id = maximum_id_ + 1;
  return success;
}

base::Value BookmarkCodec::EncodeNode(const BookmarkNode* node) {
  base::Value value(base::Value::Type::DICTIONARY);
  std::string id = base::NumberToString(node->id());
  value.SetStringKey(kIdKey, id);
  const std::u16string& title = node->GetTitle();
  value.SetStringKey(kNameKey, title);
  const std::string& guid = node->guid().AsLowercaseString();
  value.SetStringKey(kGuidKey, guid);
  // TODO(crbug.com/634507): Avoid ToInternalValue().
  value.SetStringKey(kDateAddedKey, base::NumberToString(
                                        node->date_added().ToInternalValue()));
  value.SetStringKey(
      kDateLastUsed,
      base::NumberToString(node->date_last_used().ToInternalValue()));
  if (node->is_url()) {
    value.SetStringKey(kTypeKey, kTypeURL);
    std::string url = node->url().possibly_invalid_spec();
    value.SetStringKey(kURLKey, url);
    UpdateChecksumWithUrlNode(id, title, url);
  } else {
    value.SetStringKey(kTypeKey, kTypeFolder);
    value.SetStringKey(
        kDateModifiedKey,
        base::NumberToString(node->date_folder_modified().ToInternalValue()));
    UpdateChecksumWithFolderNode(id, title);

    base::Value::List child_values;
    for (const auto& child : node->children())
      child_values.Append(EncodeNode(child.get()));
    value.SetKey(kChildrenKey, base::Value(std::move(child_values)));
  }
  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (meta_info_map)
    value.SetKey(kMetaInfo, EncodeMetaInfo(*meta_info_map));
  const BookmarkNode::MetaInfoMap* unsynced_meta_info_map =
      node->GetUnsyncedMetaInfoMap();
  if (unsynced_meta_info_map)
    value.SetKey(kUnsyncedMetaInfo, EncodeMetaInfo(*unsynced_meta_info_map));
  return value;
}

base::Value BookmarkCodec::EncodeMetaInfo(
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  base::Value meta_info(base::Value::Type::DICTIONARY);
  for (const auto& item : meta_info_map)
    meta_info.SetKey(item.first, base::Value(item.second));
  return meta_info;
}

bool BookmarkCodec::DecodeHelper(BookmarkNode* bb_node,
                                 BookmarkNode* other_folder_node,
                                 BookmarkNode* mobile_folder_node,
                                 const base::Value& value,
                                 std::string* sync_metadata_str) {
  if (!value.is_dict())
    return false;  // Unexpected type.

  absl::optional<int> version = value.FindIntKey(kVersionKey);
  if (!version || *version != kCurrentVersion)
    return false;  // Unknown version.

  const base::Value* checksum_value = value.FindKey(kChecksumKey);
  if (checksum_value) {
    const std::string* checksum = checksum_value->GetIfString();
    if (checksum)
      stored_checksum_ = *checksum;
    else
      return false;
  }

  const base::Value* roots = value.FindDictKey(kRootsKey);
  if (!roots)
    return false;  // No roots, or invalid type for roots.
  const base::Value* bb_value = roots->FindDictKey(kBookmarkBarFolderNameKey);
  const base::Value* other_folder_value =
      roots->FindDictKey(kOtherBookmarkFolderNameKey);
  const base::Value* mobile_folder_value =
      roots->FindDictKey(kMobileBookmarkFolderNameKey);

  if (!bb_value || !other_folder_value || !mobile_folder_value)
    return false;

  DecodeNode(*bb_value, nullptr, bb_node);
  DecodeNode(*other_folder_value, nullptr, other_folder_node);
  DecodeNode(*mobile_folder_value, nullptr, mobile_folder_node);

  if (!DecodeMetaInfo(*roots, &model_meta_info_map_))
    return false;
  if (!DecodeUnsyncedMetaInfo(*roots, &model_unsynced_meta_info_map_))
    return false;

  if (sync_metadata_str) {
    const std::string* sync_metadata_str_base64 =
        value.FindStringKey(kSyncMetadata);
    if (sync_metadata_str_base64)
      base::Base64Decode(*sync_metadata_str_base64, sync_metadata_str);
  }

  // Need to reset the title as the title is persisted and restored from
  // the file.
  bb_node->SetTitle(l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME));
  other_folder_node->SetTitle(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME));
  mobile_folder_node->SetTitle(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME));

  return true;
}

bool BookmarkCodec::DecodeChildren(const base::Value& child_value_list,
                                   BookmarkNode* parent) {
  DCHECK(child_value_list.is_list());
  for (const base::Value& child_value : child_value_list.GetList()) {
    if (!child_value.is_dict())
      return false;
    DecodeNode(child_value, parent, nullptr);
  }
  return true;
}

bool BookmarkCodec::DecodeNode(const base::Value& value,
                               BookmarkNode* parent,
                               BookmarkNode* node) {
  DCHECK(value.is_dict());
  // If no |node| is specified, we'll create one and add it to the |parent|.
  // Therefore, in that case, |parent| must be non-NULL.
  if (!node && !parent) {
    NOTREACHED();
    return false;
  }

  // It's not valid to have both a node and a specified parent.
  if (node && parent) {
    NOTREACHED();
    return false;
  }

  std::string id_string;
  int64_t id = 0;
  if (ids_valid_) {
    const std::string* string = value.FindStringKey(kIdKey);
    if (!string || !base::StringToInt64(*string, &id) || ids_.count(id) != 0) {
      ids_valid_ = false;
    } else {
      ids_.insert(id);
      id_string = *string;
    }
  }

  maximum_id_ = std::max(maximum_id_, id);

  std::u16string title;
  const std::string* string_value = value.FindStringKey(kNameKey);
  if (string_value)
    title = base::UTF8ToUTF16(*string_value);

  base::GUID guid;
  // |node| is only passed in for bookmarks of type BookmarkPermanentNode, in
  // which case we do not need to check for GUID validity as their GUIDs are
  // hard-coded and not read from the persisted file.
  if (!node) {
    // GUIDs can be empty for bookmarks that were created before GUIDs were
    // required. When encountering one such bookmark we thus assign to it a new
    // GUID. The same applies if the stored GUID is invalid or a duplicate.
    const std::string* guid_str = value.FindStringKey(kGuidKey);
    if (guid_str && !guid_str->empty()) {
      guid = base::GUID::ParseCaseInsensitive(*guid_str);
    }

    if (!guid.is_valid()) {
      guid = base::GUID::GenerateRandomV4();
      guids_reassigned_ = true;
    }

    if (guid.AsLowercaseString() == BookmarkNode::kBannedGuidDueToPastSyncBug) {
      guid = base::GUID::GenerateRandomV4();
      guids_reassigned_ = true;
    }

    // Guard against GUID collisions, which would violate BookmarkModel's
    // invariant that each GUID is unique.
    if (base::Contains(guids_, guid)) {
      guid = base::GUID::GenerateRandomV4();
      guids_reassigned_ = true;
    }

    guids_.insert(guid);
  }

  std::string date_added_string;
  string_value = value.FindStringKey(kDateAddedKey);
  if (string_value)
    date_added_string = *string_value;
  else
    date_added_string = base::NumberToString(Time::Now().ToInternalValue());
  int64_t date_added_time;
  base::StringToInt64(date_added_string, &date_added_time);

  std::string date_last_used_string;
  string_value = value.FindStringKey(kDateLastUsed);
  if (string_value)
    date_last_used_string = *string_value;
  else
    date_last_used_string = base::NumberToString(0);
  int64_t date_last_used;
  base::StringToInt64(date_last_used_string, &date_last_used);

  const std::string* type_string = value.FindStringKey(kTypeKey);
  if (!type_string)
    return false;

  if (*type_string != kTypeURL && *type_string != kTypeFolder)
    return false;  // Unknown type.

  if (*type_string == kTypeURL) {
    const std::string* url_string = value.FindStringKey(kURLKey);
    if (!url_string)
      return false;

    GURL url = GURL(*url_string);
    if (!node && url.is_valid()) {
      DCHECK(guid.is_valid());
      node = new BookmarkNode(id, guid, url);
    } else {
      return false;  // Node invalid.
    }

    if (parent)
      parent->Add(base::WrapUnique(node));
    UpdateChecksumWithUrlNode(id_string, title, *url_string);
  } else {
    std::string last_modified_date;
    string_value = value.FindStringKey(kDateModifiedKey);
    if (string_value)
      last_modified_date = *string_value;
    else
      last_modified_date = base::NumberToString(Time::Now().ToInternalValue());

    const base::Value* child_values = value.FindListKey(kChildrenKey);
    if (!child_values)
      return false;

    if (!node) {
      DCHECK(guid.is_valid());
      node = new BookmarkNode(id, guid, GURL());
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

  BookmarkNode::MetaInfoMap unsynced_meta_info_map;
  if (!DecodeUnsyncedMetaInfo(value, &unsynced_meta_info_map))
    return false;
  node->SetUnsyncedMetaInfoMap(unsynced_meta_info_map);

  return true;
}

bool BookmarkCodec::DecodeMetaInfo(const base::Value& value,
                                   BookmarkNode::MetaInfoMap* meta_info_map) {
  DCHECK(value.is_dict());
  DCHECK(meta_info_map);
  meta_info_map->clear();

  const base::Value* meta_info = value.FindKey(kMetaInfo);
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
  DecodeMetaInfoHelper(*meta_info, std::string(), meta_info_map);

  return true;
}

bool BookmarkCodec::DecodeUnsyncedMetaInfo(
    const base::Value& value,
    BookmarkNode::MetaInfoMap* meta_info_map) {
  DCHECK(value.is_dict());
  DCHECK(meta_info_map);
  meta_info_map->clear();

  const base::Value* meta_info = value.FindKey(kUnsyncedMetaInfo);
  if (!meta_info)
    return true;
  if (!meta_info->is_dict())
    return false;

  DecodeMetaInfoHelper(*meta_info, std::string(), meta_info_map);

  return true;
}

void BookmarkCodec::DecodeMetaInfoHelper(
    const base::Value& dict,
    const std::string& prefix,
    BookmarkNode::MetaInfoMap* meta_info_map) {
  DCHECK(dict.is_dict());
  for (const auto it : dict.DictItems()) {
    // Deprecated keys should be excluded after removing enhanced bookmarks
    // feature crrev.com/1638413003.
    if (base::StartsWith(it.first, "stars.", base::CompareCase::SENSITIVE))
      continue;

    if (it.second.is_dict()) {
      DecodeMetaInfoHelper(it.second, prefix + it.first + ".", meta_info_map);
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
  maximum_id_ = 0;
  ReassignIDsHelper(bb_node);
  ReassignIDsHelper(other_node);
  ReassignIDsHelper(mobile_node);
  ids_reassigned_ = true;
}

void BookmarkCodec::ReassignIDsHelper(BookmarkNode* node) {
  DCHECK(node);
  node->set_id(++maximum_id_);
  for (const auto& child : node->children())
    ReassignIDsHelper(child.get());
}

void BookmarkCodec::UpdateChecksum(const std::string& str) {
  base::MD5Update(&md5_context_, str);
}

void BookmarkCodec::UpdateChecksum(const std::u16string& str) {
  base::MD5Update(&md5_context_,
                  base::StringPiece(
                      reinterpret_cast<const char*>(str.data()),
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
