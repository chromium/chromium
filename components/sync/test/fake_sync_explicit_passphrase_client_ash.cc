// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_explicit_passphrase_client_ash.h"

#include <utility>

#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace syncer {

FakeSyncExplicitPassphraseClientAsh::FakeSyncExplicitPassphraseClientAsh() =
    default;

FakeSyncExplicitPassphraseClientAsh::~FakeSyncExplicitPassphraseClientAsh() =
    default;

void FakeSyncExplicitPassphraseClientAsh::AddObserver(
    mojo::PendingRemote<crosapi::mojom::SyncExplicitPassphraseClientObserver>
        observer) {
  observers_.Add(std::move(observer));
}

void FakeSyncExplicitPassphraseClientAsh::GetDecryptionNigoriKey(
    crosapi::mojom::AccountKeyPtr account_key,
    GetDecryptionNigoriKeyCallback callback) {
  get_decryption_nigori_key_called_ = true;
  if (!expected_account_key_ || !expected_account_key_.Equals(account_key)) {
    std::move(callback).Run(nullptr);
    return;
  }

  // `stored_nigori_key_` can be null, but this matches the expected behavior of
  // this fake.
  std::move(callback).Run(stored_nigori_key_.Clone());
}

void FakeSyncExplicitPassphraseClientAsh::SetDecryptionNigoriKey(
    crosapi::mojom::AccountKeyPtr account_key,
    crosapi::mojom::NigoriKeyPtr nigori_key) {
  set_decryption_nigori_key_called_ = true;
  if (!expected_account_key_ || !expected_account_key_.Equals(account_key)) {
    return;
  }
  if (!expected_nigori_key_ || !expected_nigori_key_.Equals(nigori_key)) {
    return;
  }
  stored_nigori_key_ = std::move(nigori_key);
  expected_nigori_key_ = nullptr;
  std::move(passphrase_provided_callback_).Run();
}

void FakeSyncExplicitPassphraseClientAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeSyncExplicitPassphraseClientAsh::MimicPassphraseAvailable(
    crosapi::mojom::NigoriKeyPtr nigori_key) {
  stored_nigori_key_ = std::move(nigori_key);
  for (auto& observer : observers_) {
    observer->OnPassphraseAvailable();
  }
  observers_.FlushForTesting();
}

void FakeSyncExplicitPassphraseClientAsh::MimicPassphraseRequired(
    crosapi::mojom::NigoriKeyPtr expected_nigori_key,
    base::OnceClosure passphrase_provided_callback) {
  expected_nigori_key_ = std::move(expected_nigori_key);
  passphrase_provided_callback_ = std::move(passphrase_provided_callback);
  for (auto& observer : observers_) {
    observer->OnPassphraseRequired();
  }
  observers_.FlushForTesting();
}

void FakeSyncExplicitPassphraseClientAsh::SetExpectedAccountKey(
    crosapi::mojom::AccountKeyPtr account_key) {
  expected_account_key_ = std::move(account_key);
}

bool FakeSyncExplicitPassphraseClientAsh::IsPassphraseRequired() const {
  return !expected_nigori_key_.is_null();
}

bool FakeSyncExplicitPassphraseClientAsh::IsGetDecryptionNigoriKeyCalled()
    const {
  return get_decryption_nigori_key_called_;
}

bool FakeSyncExplicitPassphraseClientAsh::IsSetDecryptionNigoriKeyCalled()
    const {
  return set_decryption_nigori_key_called_;
}

}  // namespace syncer
