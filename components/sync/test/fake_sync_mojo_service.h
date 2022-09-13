// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_MOJO_SERVICE_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_MOJO_SERVICE_H_

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/test/fake_sync_explicit_passphrase_client_ash.h"
#include "components/sync/test/fake_sync_user_settings_client_ash.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace syncer {

// Fake implementation of crosapi::mojom::SyncService (normally implemented in
// Ash-chrome and plumbed through crosapi).
class FakeSyncMojoService : public crosapi::mojom::SyncService {
 public:
  FakeSyncMojoService();
  FakeSyncMojoService(const FakeSyncMojoService& other) = delete;
  FakeSyncMojoService& operator=(const FakeSyncMojoService& other) = delete;
  ~FakeSyncMojoService() override;

  // crosapi::mojom::SyncService implementation.
  void BindExplicitPassphraseClient(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          receiver) override;
  void BindUserSettingsClient(
      mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver)
      override;

  // Own methods.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncService> receiver);
  FakeSyncExplicitPassphraseClientAsh& GetFakeSyncExplicitPassphraseClientAsh();
  FakeSyncUserSettingsClientAsh& GetFakeSyncUserSettingsClientAsh();

 private:
  FakeSyncExplicitPassphraseClientAsh fake_sync_explicit_passphrase_client_ash_;
  FakeSyncUserSettingsClientAsh fake_sync_user_settings_client_ash_;

  mojo::ReceiverSet<crosapi::mojom::SyncService> receivers_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_MOJO_SERVICE_H_
