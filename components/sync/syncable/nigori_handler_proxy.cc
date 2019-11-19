// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/nigori_handler_proxy.h"

#include "components/sync/syncable/directory_cryptographer.h"
#include "components/sync/syncable/syncable_base_transaction.h"
#include "components/sync/syncable/user_share.h"
#include "components/sync/syncable/write_transaction.h"

namespace syncer {

namespace syncable {

NigoriHandlerProxy::NigoriHandlerProxy(UserShare* user_share)
    : user_share_(user_share),
      cryptographer_(std::make_unique<DirectoryCryptographer>()),
      encrypted_types_(SyncEncryptionHandler::SensitiveTypes()),
      passphrase_type_(SyncEncryptionHandler::kInitialPassphraseType) {
  DCHECK(user_share);
}

NigoriHandlerProxy::~NigoriHandlerProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriHandlerProxy::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriHandlerProxy::OnPassphraseAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriHandlerProxy::OnTrustedVaultKeyRequired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriHandlerProxy::OnTrustedVaultKeyAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriHandlerProxy::OnBootstrapTokenUpdated(
    const std::string& bootrstrap_token,
    BootstrapTokenType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriHandlerProxy::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                                 bool encrypt_everything) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::WriteTransaction trans(FROM_HERE, user_share_);
  encrypted_types_ = encrypted_types;
}

void NigoriHandlerProxy::OnEncryptionComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriHandlerProxy::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::WriteTransaction trans(FROM_HERE, user_share_);
  cryptographer_ = cryptographer->Clone();
}

void NigoriHandlerProxy::OnPassphraseTypeChanged(PassphraseType type,
                                                 base::Time passphrase_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::WriteTransaction trans(FROM_HERE, user_share_);
  passphrase_type_ = type;
}

bool NigoriHandlerProxy::ApplyNigoriUpdate(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  NOTREACHED();
  return false;
}

void NigoriHandlerProxy::UpdateNigoriFromEncryptedTypes(
    sync_pb::NigoriSpecifics* nigori,
    const syncable::BaseTransaction* const trans) const {
  NOTREACHED();
}

const Cryptographer* NigoriHandlerProxy::GetCryptographer(
    const syncable::BaseTransaction* const trans) const {
  DCHECK_EQ(user_share_->directory.get(), trans->directory());
  DCHECK(cryptographer_);
  return cryptographer_.get();
}

const DirectoryCryptographer* NigoriHandlerProxy::GetDirectoryCryptographer(
    const syncable::BaseTransaction* const trans) const {
  return nullptr;
}

ModelTypeSet NigoriHandlerProxy::GetEncryptedTypes(
    const syncable::BaseTransaction* const trans) const {
  DCHECK_EQ(user_share_->directory.get(), trans->directory());
  return encrypted_types_;
}

PassphraseType NigoriHandlerProxy::GetPassphraseType(
    const syncable::BaseTransaction* const trans) const {
  DCHECK_EQ(user_share_->directory.get(), trans->directory());
  return passphrase_type_;
}

}  // namespace syncable
}  // namespace syncer
