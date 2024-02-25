// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_mojo_service.h"

#include <utility>

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace syncer {

FakeSyncMojoService::FakeSyncMojoService() = default;
FakeSyncMojoService::~FakeSyncMojoService() = default;

void FakeSyncMojoService::BindExplicitPassphraseClient(
    mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
        receiver) {
  fake_sync_explicit_passphrase_client_ash_.BindReceiver(std::move(receiver));
}

void FakeSyncMojoService::BindUserSettingsClient(
    mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver) {
  if (fake_sync_user_settings_client_ash_available_) {
    fake_sync_user_settings_client_ash_.BindReceiver(std::move(receiver));
  }
}

void FakeSyncMojoService::DEPRECATED_BindSyncedSessionClient(
    mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver) {
  fake_synced_session_client_ash_.BindReceiver(std::move(receiver));
}

void FakeSyncMojoService::CreateSyncedSessionClient(
    CreateSyncedSessionClientCallback callback) {
  std::move(callback).Run(fake_synced_session_client_ash_.CreateRemote());
}

mojo::PendingRemote<crosapi::mojom::SyncService>
FakeSyncMojoService::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

FakeSyncExplicitPassphraseClientAsh&
FakeSyncMojoService::GetFakeSyncExplicitPassphraseClientAsh() {
  return fake_sync_explicit_passphrase_client_ash_;
}

FakeSyncUserSettingsClientAsh&
FakeSyncMojoService::GetFakeSyncUserSettingsClientAsh() {
  return fake_sync_user_settings_client_ash_;
}

}  // namespace syncer
