// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/nigori_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/nigori_handler.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/syncable/syncable_write_transaction.h"

namespace syncer {
namespace syncable {
namespace {

bool CanDecryptUsingDefaultKey(const Cryptographer& cryptographer,
                               const sync_pb::EncryptedData& encrypted) {
  return !encrypted.key_name().empty() &&
         encrypted.key_name() == cryptographer.GetDefaultEncryptionKeyName();
}

}  // namespace

bool ProcessUnsyncedChangesForEncryption(WriteTransaction* const trans) {
  NigoriHandler* nigori_handler = trans->directory()->GetNigoriHandler();
  ModelTypeSet encrypted_types = nigori_handler->GetEncryptedTypes(trans);
  const Cryptographer* cryptographer =
      trans->directory()->GetCryptographer(trans);
  DCHECK(cryptographer->CanEncrypt());

  // Get list of all datatypes with unsynced changes. It's possible that our
  // local changes need to be encrypted if encryption for that datatype was
  // just turned on (and vice versa).
  // Note: we do not attempt to re-encrypt data with a new key here as key
  // changes in this code path are likely due to consistency issues (we have
  // to be updated to a key we already have, e.g. an old key).
  std::vector<int64_t> handles;
  GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    MutableEntry entry(trans, GET_BY_HANDLE, handles[i]);
    const sync_pb::EntitySpecifics& specifics = entry.GetSpecifics();
    // Ignore types that don't need encryption or entries that are already
    // encrypted.
    if (!SpecificsNeedsEncryption(encrypted_types, specifics))
      continue;
    if (!UpdateEntryWithEncryption(trans, specifics, &entry))
      return false;
  }
  return true;
}

bool VerifyUnsyncedChangesAreEncrypted(BaseTransaction* const trans,
                                       ModelTypeSet encrypted_types) {
  std::vector<int64_t> handles;
  GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    Entry entry(trans, GET_BY_HANDLE, handles[i]);
    if (!entry.good()) {
      NOTREACHED();
      return false;
    }
    if (EntryNeedsEncryption(encrypted_types, entry))
      return false;
  }
  return true;
}

bool EntryNeedsEncryption(ModelTypeSet encrypted_types, const Entry& entry) {
  if (!entry.GetUniqueServerTag().empty())
    return false;  // We don't encrypt unique server nodes.
  ModelType type = entry.GetModelType();
  if (type == PASSWORDS || type == WIFI_CONFIGURATIONS || IsControlType(type))
    return false;
  // Checking NON_UNIQUE_NAME is not necessary for the correctness of encrypting
  // the data, nor for determining if data is encrypted. We simply ensure it has
  // been overwritten to avoid any possible leaks of sensitive data.
  return SpecificsNeedsEncryption(encrypted_types, entry.GetSpecifics()) ||
         (encrypted_types.Has(type) &&
          entry.GetNonUniqueName() != kEncryptedString);
}

bool SpecificsNeedsEncryption(ModelTypeSet encrypted_types,
                              const sync_pb::EntitySpecifics& specifics) {
  const ModelType type = GetModelTypeFromSpecifics(specifics);
  if (type == PASSWORDS || type == WIFI_CONFIGURATIONS || IsControlType(type))
    return false;  // These types have their own encryption schemes.
  if (!encrypted_types.Has(type))
    return false;  // This type does not require encryption
  return !specifics.has_encrypted();
}

// Mainly for testing.
bool VerifyDataTypeEncryptionForTest(BaseTransaction* const trans,
                                     ModelType type,
                                     bool is_encrypted) {
  const Cryptographer* cryptographer =
      trans->directory()->GetCryptographer(trans);
  if (type == PASSWORDS || type == WIFI_CONFIGURATIONS || IsControlType(type)) {
    NOTREACHED();
    return true;
  }
  Entry type_root(trans, GET_TYPE_ROOT, type);
  if (!type_root.good()) {
    NOTREACHED();
    return false;
  }

  base::queue<Id> to_visit;
  Id id_string = type_root.GetFirstChildId();
  to_visit.push(id_string);
  while (!to_visit.empty()) {
    id_string = to_visit.front();
    to_visit.pop();
    if (id_string.IsNull())
      continue;

    Entry child(trans, GET_BY_ID, id_string);
    if (!child.good()) {
      NOTREACHED();
      return false;
    }
    if (child.GetIsDir()) {
      Id child_id_string = child.GetFirstChildId();
      // Traverse the children.
      to_visit.push(child_id_string);
    }
    const sync_pb::EntitySpecifics& specifics = child.GetSpecifics();
    DCHECK_EQ(type, child.GetModelType());
    DCHECK_EQ(type, GetModelTypeFromSpecifics(specifics));
    // We don't encrypt the server's permanent items.
    if (child.GetUniqueServerTag().empty()) {
      if (specifics.has_encrypted() != is_encrypted)
        return false;
      if (specifics.has_encrypted()) {
        if (child.GetNonUniqueName() != kEncryptedString)
          return false;
        if (!CanDecryptUsingDefaultKey(*cryptographer, specifics.encrypted()))
          return false;
      }
    }
    // Push the successor.
    to_visit.push(child.GetSuccessorId());
  }
  return true;
}

bool UpdateEntryWithEncryption(BaseTransaction* const trans,
                               const sync_pb::EntitySpecifics& new_specifics,
                               syncable::MutableEntry* entry) {
  NigoriHandler* nigori_handler = trans->directory()->GetNigoriHandler();
  const Cryptographer* cryptographer =
      trans->directory()->GetCryptographer(trans);
  ModelType type = GetModelTypeFromSpecifics(new_specifics);
  DCHECK_GE(type, FIRST_REAL_MODEL_TYPE);
  const sync_pb::EntitySpecifics& old_specifics = entry->GetSpecifics();
  const ModelTypeSet encrypted_types =
      nigori_handler ? nigori_handler->GetEncryptedTypes(trans)
                     : ModelTypeSet();
  // It's possible the nigori lost the set of encrypted types. If the current
  // specifics are already encrypted, we want to ensure we continue encrypting.
  bool was_encrypted = old_specifics.has_encrypted();
  sync_pb::EntitySpecifics generated_specifics;
  if (new_specifics.has_encrypted()) {
    NOTREACHED() << "New specifics already has an encrypted blob.";
    return false;
  }
  if (!SpecificsNeedsEncryption(encrypted_types, new_specifics) &&
      !was_encrypted) {
    // No encryption required.
    generated_specifics.CopyFrom(new_specifics);
  } else if (!cryptographer || !cryptographer->CanEncrypt()) {
    // We are currently unable to encrypt, so store unencrypted. The data will
    // be reencrypted when the encryption key becomes available, via
    // SyncEncryptionHandlerImpl::ReEncryptEverything().
    generated_specifics.CopyFrom(new_specifics);
  } else {
    // Encrypt new_specifics into generated_specifics.
    if (VLOG_IS_ON(2)) {
      std::unique_ptr<base::DictionaryValue> value(entry->ToValue(nullptr));
      std::string info;
      base::JSONWriter::WriteWithOptions(
          *value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &info);
      DVLOG(2) << "Encrypting specifics of type " << ModelTypeToString(type)
               << " with content: " << info;
    }
    // Only copy over the old specifics if it is of the right type and already
    // encrypted. The first time we encrypt a node we start from scratch, hence
    // removing all the unencrypted data, but from then on we only want to
    // update the node if the data changes or the encryption key changes.
    if (GetModelTypeFromSpecifics(old_specifics) == type && was_encrypted) {
      generated_specifics.CopyFrom(old_specifics);
    } else {
      AddDefaultFieldValue(type, &generated_specifics);
    }
    // Does not change anything if underlying encrypted blob was already up
    // to date and encrypted with the default key.
    if (!cryptographer->Encrypt(new_specifics,
                                generated_specifics.mutable_encrypted())) {
      NOTREACHED() << "Could not encrypt data for node of type "
                   << ModelTypeToString(type);
      return false;
    }
  }

  // It's possible this entry was encrypted but didn't properly overwrite the
  // non_unique_name (see crbug.com/96314).
  bool encrypted_without_overwriting_name =
      (was_encrypted && entry->GetNonUniqueName() != kEncryptedString);

  // If we're encrypted but the name wasn't overwritten properly we still want
  // to rewrite the entry, irrespective of whether the specifics match.
  if (!encrypted_without_overwriting_name &&
      old_specifics.SerializeAsString() ==
          generated_specifics.SerializeAsString()) {
    DVLOG(2) << "Specifics of type " << ModelTypeToString(type)
             << " already match, dropping change.";
    UMA_HISTOGRAM_ENUMERATION("Sync.ModelTypeRedundantPut",
                              ModelTypeHistogramValue(type));
    return true;
  }

  if (generated_specifics.has_encrypted()) {
    // Overwrite the possibly sensitive non-specifics data.
    entry->PutNonUniqueName(kEncryptedString);
    // For bookmarks we actually put bogus data into the unencrypted specifics,
    // else the server will try to do it for us.
    if (type == BOOKMARKS) {
      sync_pb::BookmarkSpecifics* bookmark_specifics =
          generated_specifics.mutable_bookmark();
      if (!entry->GetIsDir())
        bookmark_specifics->set_url(kEncryptedString);
      bookmark_specifics->set_title(kEncryptedString);
    }
  }

  if (type == PASSWORDS &&
      IsExplicitPassphrase(nigori_handler->GetPassphraseType(trans))) {
    sync_pb::PasswordSpecifics* password_specifics =
        generated_specifics.mutable_password();
    password_specifics->clear_unencrypted_metadata();
  }

  entry->PutSpecifics(generated_specifics);
  DVLOG(1) << "Overwriting specifics of type " << ModelTypeToString(type)
           << " and marking for syncing.";
  syncable::MarkForSyncing(entry);
  return true;
}

void UpdateNigoriFromEncryptedTypes(ModelTypeSet encrypted_types,
                                    bool encrypt_everything,
                                    sync_pb::NigoriSpecifics* nigori) {
  nigori->set_encrypt_everything(encrypt_everything);
  static_assert(40 == ModelType::NUM_ENTRIES,
                "If adding an encryptable type, update handling below.");
  nigori->set_encrypt_bookmarks(encrypted_types.Has(BOOKMARKS));
  nigori->set_encrypt_preferences(encrypted_types.Has(PREFERENCES));
  nigori->set_encrypt_autofill_profile(encrypted_types.Has(AUTOFILL_PROFILE));
  nigori->set_encrypt_autofill(encrypted_types.Has(AUTOFILL));
  nigori->set_encrypt_autofill_wallet_metadata(
      encrypted_types.Has(AUTOFILL_WALLET_METADATA));
  nigori->set_encrypt_themes(encrypted_types.Has(THEMES));
  nigori->set_encrypt_typed_urls(encrypted_types.Has(TYPED_URLS));
  nigori->set_encrypt_extensions(encrypted_types.Has(EXTENSIONS));
  nigori->set_encrypt_search_engines(encrypted_types.Has(SEARCH_ENGINES));
  nigori->set_encrypt_sessions(encrypted_types.Has(SESSIONS));
  nigori->set_encrypt_apps(encrypted_types.Has(APPS));
  nigori->set_encrypt_app_settings(encrypted_types.Has(APP_SETTINGS));
  nigori->set_encrypt_extension_settings(
      encrypted_types.Has(EXTENSION_SETTINGS));
  nigori->set_encrypt_dictionary(encrypted_types.Has(DICTIONARY));
  nigori->set_encrypt_favicon_images(encrypted_types.Has(FAVICON_IMAGES));
  nigori->set_encrypt_favicon_tracking(encrypted_types.Has(FAVICON_TRACKING));
  nigori->set_encrypt_app_list(encrypted_types.Has(APP_LIST));
  nigori->set_encrypt_arc_package(encrypted_types.Has(ARC_PACKAGE));
  nigori->set_encrypt_printers(encrypted_types.Has(PRINTERS));
  nigori->set_encrypt_reading_list(encrypted_types.Has(READING_LIST));
  nigori->set_encrypt_send_tab_to_self(encrypted_types.Has(SEND_TAB_TO_SELF));
  nigori->set_encrypt_web_apps(encrypted_types.Has(WEB_APPS));
  nigori->set_encrypt_os_preferences(encrypted_types.Has(OS_PREFERENCES));
}

ModelTypeSet GetEncryptedTypesFromNigori(
    const sync_pb::NigoriSpecifics& nigori) {
  if (nigori.encrypt_everything())
    return ModelTypeSet::All();

  ModelTypeSet encrypted_types;
  static_assert(40 == ModelType::NUM_ENTRIES,
                "If adding an encryptable type, update handling below.");
  if (nigori.encrypt_bookmarks())
    encrypted_types.Put(BOOKMARKS);
  if (nigori.encrypt_preferences())
    encrypted_types.Put(PREFERENCES);
  if (nigori.encrypt_autofill_profile())
    encrypted_types.Put(AUTOFILL_PROFILE);
  if (nigori.encrypt_autofill())
    encrypted_types.Put(AUTOFILL);
  if (nigori.encrypt_autofill_wallet_metadata())
    encrypted_types.Put(AUTOFILL_WALLET_METADATA);
  if (nigori.encrypt_themes())
    encrypted_types.Put(THEMES);
  if (nigori.encrypt_typed_urls())
    encrypted_types.Put(TYPED_URLS);
  if (nigori.encrypt_extensions())
    encrypted_types.Put(EXTENSIONS);
  if (nigori.encrypt_search_engines())
    encrypted_types.Put(SEARCH_ENGINES);
  if (nigori.encrypt_sessions())
    encrypted_types.Put(SESSIONS);
  if (nigori.encrypt_apps())
    encrypted_types.Put(APPS);
  if (nigori.encrypt_app_settings())
    encrypted_types.Put(APP_SETTINGS);
  if (nigori.encrypt_extension_settings())
    encrypted_types.Put(EXTENSION_SETTINGS);
  if (nigori.encrypt_dictionary())
    encrypted_types.Put(DICTIONARY);
  if (nigori.encrypt_favicon_images())
    encrypted_types.Put(FAVICON_IMAGES);
  if (nigori.encrypt_favicon_tracking())
    encrypted_types.Put(FAVICON_TRACKING);
  if (nigori.encrypt_app_list())
    encrypted_types.Put(APP_LIST);
  if (nigori.encrypt_arc_package())
    encrypted_types.Put(ARC_PACKAGE);
  if (nigori.encrypt_printers())
    encrypted_types.Put(PRINTERS);
  if (nigori.encrypt_reading_list())
    encrypted_types.Put(READING_LIST);
  if (nigori.encrypt_send_tab_to_self())
    encrypted_types.Put(SEND_TAB_TO_SELF);
  if (nigori.encrypt_web_apps())
    encrypted_types.Put(WEB_APPS);
  if (nigori.encrypt_os_preferences())
    encrypted_types.Put(OS_PREFERENCES);
  return encrypted_types;
}

}  // namespace syncable
}  // namespace syncer
