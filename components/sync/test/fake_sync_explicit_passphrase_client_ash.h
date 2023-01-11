// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_EXPLICIT_PASSPHRASE_CLIENT_ASH_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_EXPLICIT_PASSPHRASE_CLIENT_ASH_H_

#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace syncer {

// Mimics the behavior of SyncExplicitPassphraseClientAsh, exposes helper
// methods to achieve the specific state and to verify callers behavior.
class FakeSyncExplicitPassphraseClientAsh
    : public crosapi::mojom::SyncExplicitPassphraseClient {
 public:
  FakeSyncExplicitPassphraseClientAsh();
  ~FakeSyncExplicitPassphraseClientAsh() override;

  // crosapi::mojom::SyncExplicitPassphraseClient implementation.
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::SyncExplicitPassphraseClientObserver>
          observer) override;
  void GetDecryptionNigoriKey(crosapi::mojom::AccountKeyPtr account_key,
                              GetDecryptionNigoriKeyCallback callback) override;
  void SetDecryptionNigoriKey(crosapi::mojom::AccountKeyPtr account_key,
                              crosapi::mojom::NigoriKeyPtr nigori_key) override;

  // Own methods.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          receiver);
  void MimicPassphraseAvailable(crosapi::mojom::NigoriKeyPtr nigori_key);
  void MimicPassphraseRequired(
      crosapi::mojom::NigoriKeyPtr expected_nigori_key,
      base::OnceClosure passphrase_provided_callback = base::DoNothing());
  void SetExpectedAccountKey(crosapi::mojom::AccountKeyPtr account_key);

  bool IsPassphraseRequired() const;
  bool IsGetDecryptionNigoriKeyCalled() const;
  bool IsSetDecryptionNigoriKeyCalled() const;

 private:
  mojo::RemoteSet<crosapi::mojom::SyncExplicitPassphraseClientObserver>
      observers_;
  mojo::ReceiverSet<crosapi::mojom::SyncExplicitPassphraseClient> receivers_;

  crosapi::mojom::NigoriKeyPtr stored_nigori_key_;
  crosapi::mojom::NigoriKeyPtr expected_nigori_key_;
  crosapi::mojom::AccountKeyPtr expected_account_key_;

  base::OnceClosure passphrase_provided_callback_;

  bool get_decryption_nigori_key_called_ = false;
  bool set_decryption_nigori_key_called_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_EXPLICIT_PASSPHRASE_CLIENT_ASH_H_
