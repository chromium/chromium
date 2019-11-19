// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/write_node.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "components/sync/syncable/base_transaction.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/nigori_util.h"
#include "components/sync/syncable/write_transaction.h"

using std::string;
using std::vector;

namespace syncer {

using syncable::kEncryptedString;
using syncable::SPECIFICS;

static const char kDefaultNameForNewNodes[] = " ";

void WriteNode::SetIsFolder(bool folder) {
  if (entry_->GetIsDir() == folder)
    return;  // Skip redundant changes.

  entry_->PutIsDir(folder);
  MarkForSyncing();
}

void WriteNode::SetTitle(const std::string& title) {
  DCHECK_NE(GetModelType(), UNSPECIFIED);
  ModelType type = GetModelType();
  // It's possible the nigori lost the set of encrypted types. If the current
  // specifics are already encrypted, we want to ensure we continue encrypting.
  bool needs_encryption = GetTransaction()->GetEncryptedTypes().Has(type) ||
                          entry_->GetSpecifics().has_encrypted();

  // If this datatype is encrypted and is not a bookmark, we disregard the
  // specified title in favor of kEncryptedString. For encrypted bookmarks the
  // NON_UNIQUE_NAME will still be kEncryptedString, but we store the real title
  // into the specifics. All strings compared are server legal strings.
  std::string new_legal_title;
  if (type != BOOKMARKS && needs_encryption) {
    new_legal_title = kEncryptedString;
  } else {
    DCHECK(base::IsStringUTF8(title));
    SyncAPINameToServerName(title, &new_legal_title);
    base::TruncateUTF8ToByteSize(new_legal_title, 255, &new_legal_title);
  }

  std::string current_legal_title;
  if (BOOKMARKS == type && entry_->GetSpecifics().has_encrypted()) {
    // Encrypted bookmarks only have their title in the unencrypted specifics.
    current_legal_title = GetBookmarkSpecifics().title();
  } else {
    // Non-bookmarks and legacy bookmarks (those with no title in their
    // specifics) store their title in NON_UNIQUE_NAME. Non-legacy bookmarks
    // store their title in specifics as well as NON_UNIQUE_NAME.
    current_legal_title = entry_->GetNonUniqueName();
  }

  bool title_matches = (current_legal_title == new_legal_title);
  bool encrypted_without_overwriting_name =
      (needs_encryption && entry_->GetNonUniqueName() != kEncryptedString);

  // For bookmarks, we also set the title field in the specifics.
  // TODO(zea): refactor bookmarks to not need this functionality.
  sync_pb::EntitySpecifics specifics = GetEntitySpecifics();
  if (GetModelType() == BOOKMARKS &&
      specifics.bookmark().title() != new_legal_title) {
    specifics.mutable_bookmark()->set_title(new_legal_title);
    SetEntitySpecifics(specifics);  // Does it's own encryption checking.
    title_matches = false;
  }

  // If the title matches and the NON_UNIQUE_NAME is properly overwritten as
  // necessary, nothing needs to change.
  if (title_matches && !encrypted_without_overwriting_name) {
    DVLOG(2) << "Title matches, dropping change.";
    return;
  }

  // For bookmarks, this has to happen after we set the title in the specifics,
  // because the presence of a title in the NON_UNIQUE_NAME is what controls
  // the logic deciding whether this is an empty node or a legacy bookmark.
  // See BaseNode::GetUnencryptedSpecific(..).
  if (needs_encryption)
    entry_->PutNonUniqueName(kEncryptedString);
  else
    entry_->PutNonUniqueName(new_legal_title);

  DVLOG(1) << "Overwriting title of type " << ModelTypeToString(type)
           << " and marking for syncing.";
  MarkForSyncing();
}

void WriteNode::SetBookmarkSpecifics(
    const sync_pb::BookmarkSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetNigoriSpecifics(const sync_pb::NigoriSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_nigori()->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetPasswordSpecifics(
    const sync_pb::PasswordSpecificsData& data) {
  DCHECK_EQ(GetModelType(), PASSWORDS);

  const Cryptographer* cryptographer = GetTransaction()->GetCryptographer();

  // We have to do the idempotency check here (vs in UpdateEntryWithEncryption)
  // because Passwords have their encrypted data within the PasswordSpecifics,
  // vs within the EntitySpecifics like all the other types.
  const sync_pb::EntitySpecifics& old_specifics = GetEntitySpecifics();
  sync_pb::EntitySpecifics entity_specifics;
  // Copy over the old specifics if they exist.
  if (GetModelTypeFromSpecifics(old_specifics) == PASSWORDS) {
    entity_specifics.CopyFrom(old_specifics);
  } else {
    AddDefaultFieldValue(PASSWORDS, &entity_specifics);
  }
  sync_pb::PasswordSpecifics* password_specifics =
      entity_specifics.mutable_password();

  const std::string metadata_url(data.signon_realm());
  if (!IsExplicitPassphrase(GetTransaction()->GetPassphraseType()) &&
      password_specifics->unencrypted_metadata().url() != metadata_url) {
    password_specifics->mutable_unencrypted_metadata()->set_url(metadata_url);
  }

  // This will only update password_specifics if the underlying unencrypted blob
  // was different from |data| or was not encrypted with the proper passphrase.
  if (!cryptographer->Encrypt(data, password_specifics->mutable_encrypted())) {
    LOG(ERROR) << "Failed to encrypt password, possibly due to sync node "
               << "corruption";
    return;
  }
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetWifiConfigurationSpecifics(
    const sync_pb::WifiConfigurationSpecificsData& data) {
  DCHECK_EQ(GetModelType(), WIFI_CONFIGURATIONS);

  const Cryptographer* cryptographer = GetTransaction()->GetCryptographer();

  // We have to do the idempotency check here (vs in UpdateEntryWithEncryption)
  // because Passwords have their encrypted data within the PasswordSpecifics,
  // vs within the EntitySpecifics like all the other types.
  const sync_pb::EntitySpecifics& old_specifics = GetEntitySpecifics();
  sync_pb::EntitySpecifics entity_specifics;
  // Copy over the old specifics if they exist.
  if (GetModelTypeFromSpecifics(old_specifics) == WIFI_CONFIGURATIONS) {
    entity_specifics.CopyFrom(old_specifics);
  } else {
    AddDefaultFieldValue(WIFI_CONFIGURATIONS, &entity_specifics);
  }
  sync_pb::WifiConfigurationSpecifics* wifi_configuration_specifics =
      entity_specifics.mutable_wifi_configuration();

  // This will only update wifi_configuration_specifics if the underlying
  // unencrypted blob was different from |data| or was not encrypted with the
  // proper passphrase.
  if (!cryptographer->Encrypt(
          data, wifi_configuration_specifics->mutable_encrypted())) {
    LOG(ERROR) << "Failed to encrypt wifi configuration, possibly due to sync "
               << "node corruption";
    return;
  }
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetEntitySpecifics(const sync_pb::EntitySpecifics& new_value) {
  ModelType new_specifics_type = GetModelTypeFromSpecifics(new_value);

  // Purposefully crash if we have client only data, as this could result in
  // sending password in plain text.
  CHECK(!new_value.password().has_client_only_encrypted_data());

  DCHECK_NE(new_specifics_type, UNSPECIFIED);
  DVLOG(1) << "Writing entity specifics of type "
           << ModelTypeToString(new_specifics_type);
  DCHECK_EQ(new_specifics_type, GetModelType());

  // Preserve unknown fields.
  const sync_pb::EntitySpecifics& old_specifics = entry_->GetSpecifics();
  sync_pb::EntitySpecifics new_specifics;
  new_specifics.CopyFrom(new_value);
  new_specifics.mutable_unknown_fields()->append(
      old_specifics.unknown_fields());

  // Will update the entry if encryption was necessary.
  if (!UpdateEntryWithEncryption(GetTransaction()->GetWrappedTrans(),
                                 new_specifics, entry_)) {
    return;
  }
  if (entry_->GetSpecifics().has_encrypted()) {
    // EncryptIfNecessary already updated the entry for us and marked for
    // syncing if it was needed. Now we just make a copy of the unencrypted
    // specifics so that if this node is updated, we do not have to decrypt the
    // old data. Note that this only modifies the node's local data, not the
    // entry itself.
    SetUnencryptedSpecifics(new_value);
  }

  DCHECK_EQ(new_specifics_type, GetModelType());
}

void WriteNode::ResetFromSpecifics() {
  SetEntitySpecifics(GetEntitySpecifics());
}

void WriteNode::SetTypedUrlSpecifics(
    const sync_pb::TypedUrlSpecifics& new_value) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_typed_url()->CopyFrom(new_value);
  SetEntitySpecifics(entity_specifics);
}

void WriteNode::SetExternalId(int64_t id) {
  if (GetExternalId() != id)
    entry_->PutLocalExternalId(id);
}

WriteNode::WriteNode(WriteTransaction* transaction)
    : entry_(nullptr), transaction_(transaction) {
  DCHECK(transaction);
}

WriteNode::~WriteNode() {
  delete entry_;
}

// Find an existing node matching the ID |id|, and bind this WriteNode to it.
// Return true on success.
BaseNode::InitByLookupResult WriteNode::InitByIdLookup(int64_t id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::GET_BY_HANDLE, id);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

// Find a node by client tag, and bind this WriteNode to it.
// Return true if the write node was found, and was not deleted.
// Undeleting a deleted node is possible by ClientTag.
BaseNode::InitByLookupResult WriteNode::InitByClientTagLookup(
    ModelType model_type,
    const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return INIT_FAILED_PRECONDITION;

  const std::string hash = ClientTagHash::FromUnhashed(model_type, tag).value();

  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::GET_BY_CLIENT_TAG, hash);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

BaseNode::InitByLookupResult WriteNode::InitTypeRoot(ModelType type) {
  DCHECK(!entry_) << "Init called twice";
  if (!IsRealDataType(type))
    return INIT_FAILED_PRECONDITION;
  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::GET_TYPE_ROOT, type);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType model_type = GetModelType();
  DCHECK_EQ(model_type, NIGORI);
  return INIT_OK;
}

// Create a new node with default properties, and bind this WriteNode to it.
// Return true on success.
bool WriteNode::InitBookmarkByCreation(const BaseNode& parent,
                                       const BaseNode* predecessor) {
  DCHECK(!entry_) << "Init called twice";
  // |predecessor| must be a child of |parent| or null.
  if (predecessor && predecessor->GetParentId() != parent.GetId()) {
    DCHECK(false);
    return false;
  }

  syncable::Id parent_id = parent.GetSyncId();
  DCHECK(!parent_id.IsNull());

  // Start out with a dummy name.  We expect
  // the caller to set a meaningful name after creation.
  string dummy(kDefaultNameForNewNodes);

  entry_ =
      new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                 syncable::CREATE, BOOKMARKS, parent_id, dummy);

  if (!entry_->good())
    return false;

  // Entries are untitled folders by default.
  entry_->PutIsDir(true);

  if (!PutPredecessor(predecessor)) {
    return false;
  }

  // Mark this entry as unsynced, to wake up the syncer.
  MarkForSyncing();
  return true;
}

WriteNode::InitUniqueByCreationResult WriteNode::InitUniqueByCreation(
    ModelType model_type,
    const BaseNode& parent,
    const std::string& tag) {
  return InitUniqueByCreationImpl(model_type, parent.GetSyncId(), tag);
}

WriteNode::InitUniqueByCreationResult WriteNode::InitUniqueByCreation(
    ModelType model_type,
    const std::string& tag) {
  return InitUniqueByCreationImpl(model_type, syncable::Id(), tag);
}

// Create a new node with default properties and a client defined unique tag,
// and bind this WriteNode to it.
// Return true on success. If the tag exists in the database, then
// we will attempt to undelete the node.
WriteNode::InitUniqueByCreationResult WriteNode::InitUniqueByCreationImpl(
    ModelType model_type,
    const syncable::Id& parent_id,
    const std::string& tag) {
  // This DCHECK will only fail if init is called twice.
  DCHECK(!entry_);
  if (tag.empty()) {
    LOG(WARNING) << "InitUniqueByCreation failed due to empty tag.";
    return INIT_FAILED_EMPTY_TAG;
  }

  const std::string hash = ClientTagHash::FromUnhashed(model_type, tag).value();

  // Start out with a dummy name.  We expect
  // the caller to set a meaningful name after creation.
  string dummy(kDefaultNameForNewNodes);

  // Check if we have this locally and need to undelete it.
  std::unique_ptr<syncable::MutableEntry> existing_entry(
      new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                 syncable::GET_BY_CLIENT_TAG, hash));

  if (existing_entry->good()) {
    bool entry_undeleted = false;
    if (existing_entry->GetIsDel()) {
      // Rules for undelete:
      // BASE_VERSION: Must keep the same.
      // ID: Essential to keep the same.
      // META_HANDLE: Must be the same, so we can't "split" the entry.
      // IS_DEL: Must be set to false, will cause reindexing.
      //         This one is weird because IS_DEL is true for "update only"
      //         items. It should be OK to undelete an update only.
      // MTIME/CTIME: Seems reasonable to just leave them alone.
      // IS_UNSYNCED: Must set this to true or face database insurrection.
      //              We do this below this block.
      // IS_UNAPPLIED_UPDATE: Either keep it the same or also set BASE_VERSION
      //                      to SERVER_VERSION. We keep it the same here.
      // IS_DIR: We'll leave it the same.
      // SPECIFICS: Reset it.

      // Put specifics to define the entry's model type to handle the case
      // where this is not actually an undeletion, but instead a collision
      // with a newly downloaded, processed, and unapplied server update.
      // This should be done first before inserting the entry into the
      // directory's ParentChildIndex by clearing its "deleted" flag below.
      // This is a fix for http://crbug.com/505761.
      sync_pb::EntitySpecifics specifics;
      AddDefaultFieldValue(model_type, &specifics);
      existing_entry->PutSpecifics(specifics);

      existing_entry->PutIsDel(false);

      // Client tags are immutable and must be paired with the ID.
      // If a server update comes down with an ID and client tag combo,
      // and it already exists, always overwrite it and store only one copy.
      // We have to undelete entries because we can't disassociate IDs from
      // tags and updates.

      existing_entry->PutNonUniqueName(dummy);
      existing_entry->PutParentId(parent_id);
      entry_undeleted = true;
    }  // Else just reuse the existing entry.
    entry_ = existing_entry.release();
    // If entry is undeleted, its specifics are reset to default, unencrypted
    // value, and therefore no decryption is necessary. Moreover trying to
    // decrypt the password entry will fail because passwords are expected to be
    // encrypted.
    if (!entry_undeleted && !DecryptIfNecessary())
      return INIT_FAILED_DECRYPT_EXISTING_ENTRY;
  } else {
    entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                        syncable::CREATE, model_type, parent_id,
                                        dummy);
  }

  if (!entry_->good())
    return INIT_FAILED_COULD_NOT_CREATE_ENTRY;

  // Has no impact if the client tag is already set.
  entry_->PutUniqueClientTag(hash);

  // We don't support directory and tag combinations.
  entry_->PutIsDir(false);

  if (entry_->ShouldMaintainPosition()) {
    if (!entry_->PutPredecessor(syncable::Id()))
      return INIT_FAILED_SET_PREDECESSOR;
  }

  // Mark this entry as unsynced, to wake up the syncer.
  MarkForSyncing();

  return INIT_SUCCESS;
}

bool WriteNode::SetPosition(const BaseNode& new_parent,
                            const BaseNode* predecessor) {
  // |predecessor| must be a child of |new_parent| or null.
  if (predecessor && predecessor->GetParentId() != new_parent.GetId()) {
    DCHECK(false);
    return false;
  }

  syncable::Id new_parent_id = new_parent.GetSyncId();
  DCHECK(!new_parent_id.IsNull());

  // Filter out redundant changes if both the parent and the predecessor match.
  if (new_parent_id == entry_->GetParentId()) {
    const syncable::Id& old = entry_->GetPredecessorId();
    if ((!predecessor && old.IsNull()) ||
        (predecessor && (old == predecessor->GetSyncId()))) {
      return true;
    }
  }

  entry_->PutParentId(new_parent_id);

  if (!PutPredecessor(predecessor)) {
    return false;
  }

  // Mark this entry as unsynced, to wake up the syncer.
  MarkForSyncing();
  return true;
}

const syncable::Entry* WriteNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* WriteNode::GetTransaction() const {
  return transaction_;
}

syncable::MutableEntry* WriteNode::GetMutableEntryForTest() {
  return entry_;
}

void WriteNode::Tombstone() {
  // These lines must be in this order.  The call to Put(IS_DEL) might choose to
  // unset the IS_UNSYNCED bit if the item was not known to the server at the
  // time of deletion.  It's important that the bit not be reset in that case.
  MarkForSyncing();
  entry_->PutIsDel(true);
}

void WriteNode::Drop() {
  if (entry_->GetId().ServerKnows()) {
    entry_->PutIsDel(true);
  }
}

bool WriteNode::PutPredecessor(const BaseNode* predecessor) {
  DCHECK(!entry_->GetParentId().IsNull());
  syncable::Id predecessor_id =
      predecessor ? predecessor->GetSyncId() : syncable::Id();
  return entry_->PutPredecessor(predecessor_id);
}

void WriteNode::MarkForSyncing() {
  syncable::MarkForSyncing(entry_);
}

}  // namespace syncer
