// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/base_node.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"
#include "components/sync/syncable/base_transaction.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/syncable_base_transaction.h"
#include "components/sync/syncable/syncable_id.h"

using sync_pb::AutofillProfileSpecifics;

namespace syncer {

using syncable::SPECIFICS;

// Helper function to look up the int64_t metahandle of an object given the ID
// string.
static int64_t IdToMetahandle(syncable::BaseTransaction* trans,
                              const syncable::Id& id) {
  if (id.IsNull())
    return kInvalidId;
  syncable::Entry entry(trans, syncable::GET_BY_ID, id);
  if (!entry.good())
    return kInvalidId;
  return entry.GetMetahandle();
}

BaseNode::BaseNode()
    : password_data_(new sync_pb::PasswordSpecificsData),
      wifi_configuration_data_(new sync_pb::WifiConfigurationSpecificsData) {}

BaseNode::~BaseNode() {}

bool BaseNode::DecryptIfNecessary() {
  if (GetIsPermanentFolder())
    return true;  // Ignore unique folders.
  const sync_pb::EntitySpecifics& specifics = GetEntry()->GetSpecifics();
  if (specifics.has_password()) {
    // Passwords have their own legacy encryption structure.
    std::unique_ptr<sync_pb::PasswordSpecificsData> data =
        DecryptPasswordSpecifics(specifics,
                                 GetTransaction()->GetCryptographer());
    if (!data) {
      GetTransaction()->GetWrappedTrans()->OnUnrecoverableError(
          FROM_HERE, std::string("Failed to decrypt encrypted node of type ") +
                         ModelTypeToString(GetModelType()));
      return false;
    }
    password_data_.swap(data);
    return true;
  }

  if (specifics.has_wifi_configuration()) {
    // Wifi configs have their own legacy encryption structure.
    std::unique_ptr<sync_pb::WifiConfigurationSpecificsData> data =
        DecryptWifiConfigurationSpecifics(specifics,
                                          GetTransaction()->GetCryptographer());
    if (!data) {
      GetTransaction()->GetWrappedTrans()->OnUnrecoverableError(
          FROM_HERE, std::string("Failed to decrypt encrypted node of type ") +
                         ModelTypeToString(GetModelType()));
      return false;
    }
    wifi_configuration_data_.swap(data);
    return true;
  }

  // We assume any node with the encrypted field set has encrypted data and if
  // not we have no work to do, with the exception of bookmarks. For bookmarks
  // we must make sure the bookmarks data has the title field supplied. If not,
  // we fill the unencrypted_data_ with a copy of the bookmark specifics that
  // follows the new bookmarks format.
  if (!specifics.has_encrypted()) {
    if (GetModelType() == BOOKMARKS && !specifics.bookmark().has_title() &&
        !GetTitle().empty()) {  // Last check ensures this isn't a new node.
      // We need to fill in the title.
      std::string title = GetTitle();
      std::string server_legal_title;
      SyncAPINameToServerName(title, &server_legal_title);
      DVLOG(1) << "Reading from legacy bookmark, manually returning title "
               << title;
      unencrypted_data_.CopyFrom(specifics);
      unencrypted_data_.mutable_bookmark()->set_title(server_legal_title);
    }
    return true;
  }

  const sync_pb::EncryptedData& encrypted = specifics.encrypted();
  std::string plaintext_data;
  if (!GetTransaction()->GetCryptographer()->DecryptToString(encrypted,
                                                             &plaintext_data)) {
    GetTransaction()->GetWrappedTrans()->OnUnrecoverableError(
        FROM_HERE, std::string("Failed to decrypt encrypted node of type ") +
                       ModelTypeToString(GetModelType()));
    return false;
  } else if (!unencrypted_data_.ParseFromString(plaintext_data)) {
    GetTransaction()->GetWrappedTrans()->OnUnrecoverableError(
        FROM_HERE, std::string("Failed to parse encrypted node of type ") +
                       ModelTypeToString(GetModelType()));
    return false;
  }
  DVLOG(2) << "Decrypted specifics of type "
           << ModelTypeToString(GetModelType())
           << " with content: " << plaintext_data;
  return true;
}

const sync_pb::EntitySpecifics& BaseNode::GetUnencryptedSpecifics(
    const syncable::Entry* entry) const {
  const sync_pb::EntitySpecifics& specifics = entry->GetSpecifics();
  if (specifics.has_encrypted()) {
    DCHECK_NE(GetModelTypeFromSpecifics(unencrypted_data_), UNSPECIFIED);
    return unencrypted_data_;
  } else {
    // Due to the change in bookmarks format, we need to check to see if this is
    // a legacy bookmarks (and has no title field in the proto). If it is, we
    // return the unencrypted_data_, which was filled in with the title by
    // DecryptIfNecessary().
    if (GetModelType() == BOOKMARKS) {
      const sync_pb::BookmarkSpecifics& bookmark_specifics =
          specifics.bookmark();
      if (bookmark_specifics.has_title() ||
          GetTitle().empty() ||  // For the empty node case
          GetIsPermanentFolder()) {
        // It's possible we previously had to convert and set
        // |unencrypted_data_| but then wrote our own data, so we allow
        // |unencrypted_data_| to be non-empty.
        return specifics;
      } else {
        DCHECK_EQ(GetModelTypeFromSpecifics(unencrypted_data_), BOOKMARKS);
        return unencrypted_data_;
      }
    } else {
      DCHECK_EQ(GetModelTypeFromSpecifics(unencrypted_data_), UNSPECIFIED);
      return specifics;
    }
  }
}

int64_t BaseNode::GetParentId() const {
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(),
                        GetEntry()->GetParentId());
}

int64_t BaseNode::GetId() const {
  return GetEntry()->GetMetahandle();
}

bool BaseNode::GetIsFolder() const {
  return GetEntry()->GetIsDir();
}

bool BaseNode::GetIsPermanentFolder() const {
  bool is_permanent_folder = !GetEntry()->GetUniqueServerTag().empty();
  if (is_permanent_folder) {
    // If the node is a permanent folder it must also have IS_DIR bit set,
    // except some nigori nodes on old accounts.
    DCHECK(GetIsFolder() || GetModelType() == NIGORI);
  }
  return is_permanent_folder;
}

std::string BaseNode::GetTitle() const {
  std::string result;
  // TODO(zea): refactor bookmarks to not need this functionality.
  if (BOOKMARKS == GetModelType() &&
      GetEntry()->GetSpecifics().has_encrypted()) {
    // Special case for legacy bookmarks dealing with encryption.
    ServerNameToSyncAPIName(GetBookmarkSpecifics().title(), &result);
  } else {
    ServerNameToSyncAPIName(GetEntry()->GetNonUniqueName(), &result);
  }
  return result;
}

bool BaseNode::HasChildren() const {
  syncable::Directory* dir = GetTransaction()->GetDirectory();
  syncable::BaseTransaction* trans = GetTransaction()->GetWrappedTrans();
  return dir->HasChildren(trans, GetEntry()->GetId());
}

int64_t BaseNode::GetPredecessorId() const {
  syncable::Id id_string = GetEntry()->GetPredecessorId();
  if (id_string.IsNull())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64_t BaseNode::GetSuccessorId() const {
  syncable::Id id_string = GetEntry()->GetSuccessorId();
  if (id_string.IsNull())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64_t BaseNode::GetFirstChildId() const {
  syncable::Id id_string = GetEntry()->GetFirstChildId();
  if (id_string.IsNull())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

void BaseNode::GetChildIds(std::vector<int64_t>* result) const {
  GetEntry()->GetChildHandles(result);
}

int BaseNode::GetTotalNodeCount() const {
  return GetEntry()->GetTotalNodeCount();
}

int BaseNode::GetPositionIndex() const {
  return GetEntry()->GetPositionIndex();
}

std::unique_ptr<base::DictionaryValue> BaseNode::ToValue() const {
  return GetEntry()->ToValue(GetTransaction()->GetCryptographer());
}

int64_t BaseNode::GetExternalId() const {
  return GetEntry()->GetLocalExternalId();
}

const syncable::Id& BaseNode::GetSyncId() const {
  return GetEntry()->GetId();
}

const sync_pb::BookmarkSpecifics& BaseNode::GetBookmarkSpecifics() const {
  DCHECK_EQ(GetModelType(), BOOKMARKS);
  return GetEntitySpecifics().bookmark();
}

const sync_pb::NigoriSpecifics& BaseNode::GetNigoriSpecifics() const {
  DCHECK_EQ(GetModelType(), NIGORI);
  return GetEntitySpecifics().nigori();
}

const sync_pb::PasswordSpecificsData& BaseNode::GetPasswordSpecifics() const {
  DCHECK_EQ(GetModelType(), PASSWORDS);
  return *password_data_;
}

const sync_pb::WifiConfigurationSpecificsData&
BaseNode::GetWifiConfigurationSpecifics() const {
  DCHECK_EQ(GetModelType(), WIFI_CONFIGURATIONS);
  return *wifi_configuration_data_;
}

const sync_pb::EntitySpecifics& BaseNode::GetEntitySpecifics() const {
  return GetUnencryptedSpecifics(GetEntry());
}

ModelType BaseNode::GetModelType() const {
  return GetEntry()->GetModelType();
}

void BaseNode::SetUnencryptedSpecifics(
    const sync_pb::EntitySpecifics& specifics) {
  ModelType type = GetModelTypeFromSpecifics(specifics);
  DCHECK_NE(UNSPECIFIED, type);
  if (GetModelType() != UNSPECIFIED) {
    DCHECK_EQ(GetModelType(), type);
  }
  unencrypted_data_.CopyFrom(specifics);
}

}  // namespace syncer
