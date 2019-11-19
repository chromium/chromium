// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_codec.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::Time;

namespace bookmarks {

const char BookmarkCodec::kRootsKey[] = "roots";
const char BookmarkCodec::kRootFolderNameKey[] = "bookmark_bar";
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
const char BookmarkCodec::kSyncTransactionVersion[] =
    "sync_transaction_version";
const char BookmarkCodec::kTypeURL[] = "url";
const char BookmarkCodec::kTypeFolder[] = "folder";
const char BookmarkCodec::kSyncMetadata[] = "sync_metadata";

// Current version of the file.
static const int kCurrentVersion = 1;

BookmarkCodec::BookmarkCodec()
    : ids_reassigned_(false),
      guids_reassigned_(false),
      ids_valid_(true),
      maximum_id_(0),
      model_sync_transaction_version_(
          BookmarkNode::kInvalidSyncTransactionVersion) {}

BookmarkCodec::~BookmarkCodec() = default;

std::unique_ptr<base::Value> BookmarkCodec::Encode(
    BookmarkModel* model,
    const std::string& sync_metadata_str) {
  return Encode(model->bookmark_bar_node(), model->other_node(),
                model->mobile_node(), model->root_node()->GetMetaInfoMap(),
                model->root_node()->sync_transaction_version(),
                sync_metadata_str);
}

std::unique_ptr<base::Value> BookmarkCodec::Encode(
    const BookmarkNode* bookmark_bar_node,
    const BookmarkNode* other_folder_node,
    const BookmarkNode* mobile_folder_node,
    const BookmarkNode::MetaInfoMap* model_meta_info_map,
    int64_t sync_transaction_version,
    const std::string& sync_metadata_str) {
  ids_reassigned_ = false;
  guids_reassigned_ = false;
  InitializeChecksum();
  auto roots = std::make_unique<base::DictionaryValue>();
  roots->Set(kRootFolderNameKey, EncodeNode(bookmark_bar_node));
  roots->Set(kOtherBookmarkFolderNameKey, EncodeNode(other_folder_node));
  roots->Set(kMobileBookmarkFolderNameKey, EncodeNode(mobile_folder_node));
  if (model_meta_info_map)
    roots->Set(kMetaInfo, EncodeMetaInfo(*model_meta_info_map));
  if (sync_transaction_version !=
      BookmarkNode::kInvalidSyncTransactionVersion) {
    roots->SetString(kSyncTransactionVersion,
                     base::NumberToString(sync_transaction_version));
  }
  auto main = std::make_unique<base::DictionaryValue>();
  main->SetInteger(kVersionKey, kCurrentVersion);
  FinalizeChecksum();
  // We are going to store the computed checksum. So set stored checksum to be
  // the same as computed checksum.
  stored_checksum_ = computed_checksum_;
  main->SetString(kChecksumKey, computed_checksum_);
  main->Set(kRootsKey, std::move(roots));
  if (!sync_metadata_str.empty()) {
    std::string sync_metadata_str_base64;
    base::Base64Encode(sync_metadata_str, &sync_metadata_str_base64);
    main->SetKey(kSyncMetadata,
                 base::Value(std::move(sync_metadata_str_base64)));
  }
  return std::move(main);
}

bool BookmarkCodec::Decode(const base::Value& value,
                           BookmarkNode* bb_node,
                           BookmarkNode* other_folder_node,
                           BookmarkNode* mobile_folder_node,
                           int64_t* max_id,
                           std::string* sync_metadata_str) {
  ids_.clear();
  guids_ = {BookmarkNode::kRootNodeGuid, BookmarkNode::kBookmarkBarNodeGuid,
            BookmarkNode::kOtherBookmarksNodeGuid,
            BookmarkNode::kMobileBookmarksNodeGuid,
            BookmarkNode::kManagedNodeGuid};
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

std::unique_ptr<base::Value> BookmarkCodec::EncodeNode(
    const BookmarkNode* node) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  std::string id = base::NumberToString(node->id());
  value->SetString(kIdKey, id);
  const base::string16& title = node->GetTitle();
  value->SetString(kNameKey, title);
  const std::string& guid = node->guid();
  value->SetString(kGuidKey, guid);
  value->SetString(kDateAddedKey,
                   base::NumberToString(node->date_added().ToInternalValue()));
  if (node->is_url()) {
    value->SetString(kTypeKey, kTypeURL);
    std::string url = node->url().possibly_invalid_spec();
    value->SetString(kURLKey, url);
    UpdateChecksumWithUrlNode(id, title, url);
  } else {
    value->SetString(kTypeKey, kTypeFolder);
    value->SetString(
        kDateModifiedKey,
        base::NumberToString(node->date_folder_modified().ToInternalValue()));
    UpdateChecksumWithFolderNode(id, title);

    auto child_values = std::make_unique<base::ListValue>();
    for (const auto& child : node->children())
      child_values->Append(EncodeNode(child.get()));
    value->Set(kChildrenKey, std::move(child_values));
  }
  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (meta_info_map)
    value->Set(kMetaInfo, EncodeMetaInfo(*meta_info_map));
  if (node->sync_transaction_version() !=
      BookmarkNode::kInvalidSyncTransactionVersion) {
    value->SetString(kSyncTransactionVersion,
                     base::NumberToString(node->sync_transaction_version()));
  }
  return std::move(value);
}

std::unique_ptr<base::Value> BookmarkCodec::EncodeMetaInfo(
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  auto meta_info = std::make_unique<base::DictionaryValue>();
  for (const auto& item : meta_info_map) {
    meta_info->SetKey(item.first, base::Value(item.second));
  }
  return std::move(meta_info);
}

bool BookmarkCodec::DecodeHelper(BookmarkNode* bb_node,
                                 BookmarkNode* other_folder_node,
                                 BookmarkNode* mobile_folder_node,
                                 const base::Value& value,
                                 std::string* sync_metadata_str) {
  const base::DictionaryValue* d_value = nullptr;
  if (!value.GetAsDictionary(&d_value))
    return false;  // Unexpected type.

  int version;
  if (!d_value->GetInteger(kVersionKey, &version) || version != kCurrentVersion)
    return false;  // Unknown version.

  const base::Value* checksum_value;
  if (d_value->Get(kChecksumKey, &checksum_value)) {
    if (!checksum_value->is_string())
      return false;
    if (!checksum_value->GetAsString(&stored_checksum_))
      return false;
  }

  const base::Value* roots;
  if (!d_value->Get(kRootsKey, &roots))
    return false;  // No roots.

  const base::DictionaryValue* roots_d_value = nullptr;
  if (!roots->GetAsDictionary(&roots_d_value))
    return false;  // Invalid type for roots.
  const base::Value* root_folder_value;
  const base::Value* other_folder_value = nullptr;
  const base::DictionaryValue* root_folder_d_value = nullptr;
  const base::DictionaryValue* other_folder_d_value = nullptr;
  if (!roots_d_value->Get(kRootFolderNameKey, &root_folder_value) ||
      !root_folder_value->GetAsDictionary(&root_folder_d_value) ||
      !roots_d_value->Get(kOtherBookmarkFolderNameKey, &other_folder_value) ||
      !other_folder_value->GetAsDictionary(&other_folder_d_value)) {
    return false;  // Invalid type for root folder and/or other
                   // folder.
  }
  DecodeNode(*root_folder_d_value, nullptr, bb_node);
  DecodeNode(*other_folder_d_value, nullptr, other_folder_node);

  // Fail silently if we can't deserialize mobile bookmarks. We can't require
  // them to exist in order to be backwards-compatible with older versions of
  // chrome.
  const base::Value* mobile_folder_value;
  const base::DictionaryValue* mobile_folder_d_value = nullptr;
  if (roots_d_value->Get(kMobileBookmarkFolderNameKey, &mobile_folder_value) &&
      mobile_folder_value->GetAsDictionary(&mobile_folder_d_value)) {
    DecodeNode(*mobile_folder_d_value, nullptr, mobile_folder_node);
  } else {
    // If we didn't find the mobile folder, we're almost guaranteed to have a
    // duplicate id when we add the mobile folder. Consequently, if we don't
    // intend to reassign ids in the future (ids_valid_ is still true), then at
    // least reassign the mobile bookmarks to avoid it colliding with anything
    // else.
    if (ids_valid_)
      ReassignIDsHelper(mobile_folder_node);
  }

  if (!DecodeMetaInfo(*roots_d_value, &model_meta_info_map_,
                      &model_sync_transaction_version_))
    return false;

  std::string sync_transaction_version_str;
  if (roots_d_value->GetString(kSyncTransactionVersion,
                               &sync_transaction_version_str) &&
      !base::StringToInt64(sync_transaction_version_str,
                           &model_sync_transaction_version_))
    return false;

  std::string sync_metadata_str_base64;
  if (sync_metadata_str &&
      d_value->GetString(kSyncMetadata, &sync_metadata_str_base64)) {
    base::Base64Decode(sync_metadata_str_base64, sync_metadata_str);
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

bool BookmarkCodec::DecodeChildren(const base::ListValue& child_value_list,
                                   BookmarkNode* parent) {
  for (size_t i = 0; i < child_value_list.GetSize(); ++i) {
    const base::Value* child_value;
    if (!child_value_list.Get(i, &child_value))
      return false;

    const base::DictionaryValue* child_d_value = nullptr;
    if (!child_value->GetAsDictionary(&child_d_value))
      return false;
    DecodeNode(*child_d_value, parent, nullptr);
  }
  return true;
}

bool BookmarkCodec::DecodeNode(const base::DictionaryValue& value,
                               BookmarkNode* parent,
                               BookmarkNode* node) {
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
    if (!value.GetString(kIdKey, &id_string) ||
        !base::StringToInt64(id_string, &id) ||
        ids_.count(id) != 0) {
      ids_valid_ = false;
    } else {
      ids_.insert(id);
    }
  }

  maximum_id_ = std::max(maximum_id_, id);

  base::string16 title;
  value.GetString(kNameKey, &title);

  std::string guid;
  // |node| is only passed in for bookmarks of type BookmarkPermanentNode, in
  // which case we do not need to check for GUID validity as their GUIDs are
  // hard-coded and not read from the persisted file.
  if (!node) {
    // GUIDs can be empty for bookmarks that were created before GUIDs were
    // required. When encountering one such bookmark we thus assign to it a new
    // GUID. The same applies if the stored GUID is invalid or a duplicate.
    if (!value.GetString(kGuidKey, &guid) || guid.empty() ||
        !base::IsValidGUID(guid) || guids_.count(guid) != 0) {
      guid = base::GenerateGUID();
      guids_reassigned_ = true;
    }
    guids_.insert(guid);
  }

  std::string date_added_string;
  if (!value.GetString(kDateAddedKey, &date_added_string))
    date_added_string = base::NumberToString(Time::Now().ToInternalValue());
  int64_t internal_time;
  base::StringToInt64(date_added_string, &internal_time);

  std::string type_string;
  if (!value.GetString(kTypeKey, &type_string))
    return false;

  if (type_string != kTypeURL && type_string != kTypeFolder)
    return false;  // Unknown type.

  if (type_string == kTypeURL) {
    std::string url_string;
    if (!value.GetString(kURLKey, &url_string))
      return false;

    GURL url = GURL(url_string);
    if (!node && url.is_valid())
      node = new BookmarkNode(id, guid, url);
    else
      return false;  // Node invalid.

    if (parent)
      parent->Add(base::WrapUnique(node));
    UpdateChecksumWithUrlNode(id_string, title, url_string);
  } else {
    std::string last_modified_date;
    if (!value.GetString(kDateModifiedKey, &last_modified_date))
      last_modified_date = base::NumberToString(Time::Now().ToInternalValue());

    const base::Value* child_values;
    if (!value.Get(kChildrenKey, &child_values))
      return false;

    if (child_values->type() != base::Value::Type::LIST)
      return false;

    if (!node) {
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

    const base::ListValue* child_l_values = nullptr;
    if (!child_values->GetAsList(&child_l_values))
      return false;
    if (!DecodeChildren(*child_l_values, node))
      return false;
  }

  node->SetTitle(title);
  node->set_date_added(Time::FromInternalValue(internal_time));

  int64_t sync_transaction_version = node->sync_transaction_version();
  BookmarkNode::MetaInfoMap meta_info_map;
  if (!DecodeMetaInfo(value, &meta_info_map, &sync_transaction_version))
    return false;
  node->SetMetaInfoMap(meta_info_map);

  std::string sync_transaction_version_str;
  if (value.GetString(kSyncTransactionVersion, &sync_transaction_version_str) &&
      !base::StringToInt64(sync_transaction_version_str,
                           &sync_transaction_version))
    return false;

  node->set_sync_transaction_version(sync_transaction_version);

  return true;
}

bool BookmarkCodec::DecodeMetaInfo(const base::DictionaryValue& value,
                                   BookmarkNode::MetaInfoMap* meta_info_map,
                                   int64_t* sync_transaction_version) {
  DCHECK(meta_info_map);
  DCHECK(sync_transaction_version);
  meta_info_map->clear();

  const base::Value* meta_info;
  if (!value.Get(kMetaInfo, &meta_info))
    return true;

  std::unique_ptr<base::Value> deserialized_holder;

  // Meta info used to be stored as a serialized dictionary, so attempt to
  // parse the value as one.
  if (meta_info->is_string()) {
    std::string meta_info_str;
    meta_info->GetAsString(&meta_info_str);
    JSONStringValueDeserializer deserializer(meta_info_str);
    deserialized_holder = deserializer.Deserialize(nullptr, nullptr);
    if (!deserialized_holder)
      return false;
    meta_info = deserialized_holder.get();
  }
  // meta_info is now either the kMetaInfo node, or the deserialized node if it
  // was stored as a string. Either way it should now be a (possibly nested)
  // dictionary of meta info values.
  const base::DictionaryValue* meta_info_dict;
  if (!meta_info->GetAsDictionary(&meta_info_dict))
    return false;
  DecodeMetaInfoHelper(*meta_info_dict, std::string(), meta_info_map);

  // Previously sync transaction version was stored in the meta info field
  // using this key. If the key is present when decoding, set the sync
  // transaction version to its value, then delete the field.
  if (deserialized_holder) {
    const char kBookmarkTransactionVersionKey[] = "sync.transaction_version";
    auto it = meta_info_map->find(kBookmarkTransactionVersionKey);
    if (it != meta_info_map->end()) {
      base::StringToInt64(it->second, sync_transaction_version);
      meta_info_map->erase(it);
    }
  }

  return true;
}

void BookmarkCodec::DecodeMetaInfoHelper(
    const base::DictionaryValue& dict,
    const std::string& prefix,
    BookmarkNode::MetaInfoMap* meta_info_map) {
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    // Deprecated keys should be excluded after removing enhanced bookmarks
    // feature crrev.com/1638413003.
    if (base::StartsWith(it.key(), "stars.", base::CompareCase::SENSITIVE))
      continue;

    if (it.value().is_dict()) {
      const base::DictionaryValue* subdict;
      it.value().GetAsDictionary(&subdict);
      DecodeMetaInfoHelper(*subdict, prefix + it.key() + ".", meta_info_map);
    } else if (it.value().is_string()) {
      it.value().GetAsString(&(*meta_info_map)[prefix + it.key()]);
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

void BookmarkCodec::UpdateChecksum(const base::string16& str) {
  base::MD5Update(&md5_context_,
                  base::StringPiece(
                      reinterpret_cast<const char*>(str.data()),
                      str.length() * sizeof(str[0])));
}

void BookmarkCodec::UpdateChecksumWithUrlNode(const std::string& id,
                                              const base::string16& title,
                                              const std::string& url) {
  DCHECK(base::IsStringUTF8(url));
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeURL);
  UpdateChecksum(url);
}

void BookmarkCodec::UpdateChecksumWithFolderNode(const std::string& id,
                                                 const base::string16& title) {
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
