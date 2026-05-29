// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/sync_encryption_handler_observer_list.h"

#include <algorithm>
#include <utility>

#include "components/sync/base/custom_passphrase_bootstrap_token.h"
#include "components/sync/engine/cryptographer.h"
#include "components/sync/engine/required_passphrase_verifier.h"

namespace syncer {

SyncEncryptionHandlerObserverList::SyncEncryptionHandlerObserverList() =
    default;

SyncEncryptionHandlerObserverList::~SyncEncryptionHandlerObserverList() =
    default;

void SyncEncryptionHandlerObserverList::AddObserver(
    SyncEncryptionHandler::Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncEncryptionHandlerObserverList::RemoveObserver(
    SyncEncryptionHandler::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SyncEncryptionHandlerObserverList::NotifyPassphraseRequired(
    const RequiredPassphraseVerifier& verifier) const {
  for (SyncEncryptionHandler::Observer& observer : observers_) {
    observer.OnPassphraseRequired(verifier.Clone());
  }
}

void SyncEncryptionHandlerObserverList::NotifyPassphraseAccepted(
    const CustomPassphraseBootstrapToken& bootstrap_token) const {
  for (SyncEncryptionHandler::Observer& observer : observers_) {
    observer.OnPassphraseAccepted(bootstrap_token);
  }
}

void SyncEncryptionHandlerObserverList::NotifyTrustedVaultKeyRequired() const {
  std::ranges::for_each(
      observers_, &SyncEncryptionHandler::Observer::OnTrustedVaultKeyRequired);
}

void SyncEncryptionHandlerObserverList::NotifyTrustedVaultKeyAccepted() const {
  std::ranges::for_each(
      observers_, &SyncEncryptionHandler::Observer::OnTrustedVaultKeyAccepted);
}

void SyncEncryptionHandlerObserverList::NotifyKeystoreKeysRequired() const {
  std::ranges::for_each(
      observers_, &SyncEncryptionHandler::Observer::OnKeystoreKeysRequired);
}

void SyncEncryptionHandlerObserverList::NotifyKeystoreKeysAccepted() const {
  std::ranges::for_each(
      observers_, &SyncEncryptionHandler::Observer::OnKeystoreKeysAccepted);
}

void SyncEncryptionHandlerObserverList::NotifyEncryptedTypesChanged(
    DataTypeSet encrypted_types,
    bool encrypt_everything) const {
  for (SyncEncryptionHandler::Observer& observer : observers_) {
    observer.OnEncryptedTypesChanged(encrypted_types, encrypt_everything);
  }
}

void SyncEncryptionHandlerObserverList::NotifyCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) const {
  for (SyncEncryptionHandler::Observer& observer : observers_) {
    observer.OnCryptographerStateChanged(cryptographer, has_pending_keys);
  }
}

void SyncEncryptionHandlerObserverList::NotifyPassphraseTypeChanged(
    PassphraseType type,
    base::Time passphrase_time) const {
  for (SyncEncryptionHandler::Observer& observer : observers_) {
    observer.OnPassphraseTypeChanged(type, passphrase_time);
  }
}

}  // namespace syncer
