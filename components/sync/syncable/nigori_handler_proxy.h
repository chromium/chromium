// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_NIGORI_HANDLER_PROXY_H_
#define COMPONENTS_SYNC_SYNCABLE_NIGORI_HANDLER_PROXY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/syncable/nigori_handler.h"

namespace syncer {

struct UserShare;

namespace syncable {

// This class makes Directory working, while USS implementation of Nigori is
// enabled. The problem is that Directory needs access to Cryptographer,
// encrypted types and passphrase type from different sequences, while USS
// implementation of Nigori assumes we access it only from sync sequence. To
// achieve this we store copies of interesting fields in this class, refresh
// them on SyncEncryptionHandler events (and protect updates by transactions)
// and provide minimal part of NigoriHandler interface.
class NigoriHandlerProxy : public SyncEncryptionHandler::Observer,
                           public NigoriHandler {
 public:
  // |user_share| must be not null and must outlive this object.
  explicit NigoriHandlerProxy(UserShare* user_share);
  ~NigoriHandlerProxy() override;

  // SyncEncryptionHandler::Observer implementation. These methods must be
  // called only from sync sequence.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootrstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override;

  // NigoriHandler implementation. ApplyNigoriUpdate() and
  // UpdateNigoriFromEncryptedTypes() should never be called. Other methods can
  // be called from any sequence.
  bool ApplyNigoriUpdate(const sync_pb::NigoriSpecifics& nigori,
                         syncable::BaseTransaction* const trans) override;
  void UpdateNigoriFromEncryptedTypes(
      sync_pb::NigoriSpecifics* nigori,
      const syncable::BaseTransaction* const trans) const override;
  const Cryptographer* GetCryptographer(
      const syncable::BaseTransaction* const trans) const override;
  const DirectoryCryptographer* GetDirectoryCryptographer(
      const syncable::BaseTransaction* const trans) const override;
  ModelTypeSet GetEncryptedTypes(
      const syncable::BaseTransaction* const trans) const override;
  PassphraseType GetPassphraseType(
      const syncable::BaseTransaction* const trans) const override;

 private:
  UserShare* user_share_;

  std::unique_ptr<Cryptographer> cryptographer_;
  ModelTypeSet encrypted_types_;
  PassphraseType passphrase_type_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NigoriHandlerProxy);
};

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_NIGORI_HANDLER_PROXY_H_
