// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_synced_session_client_ash.h"

#include <utility>

#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace syncer {

FakeSyncedSessionClientAsh::FakeSyncedSessionClientAsh() = default;
FakeSyncedSessionClientAsh::~FakeSyncedSessionClientAsh() = default;

void FakeSyncedSessionClientAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver) {
  receivers_.Add(this, std::move(receiver));
}

mojo::PendingRemote<crosapi::mojom::SyncedSessionClient>
FakeSyncedSessionClientAsh::CreateRemote() {
  mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> pending_receiver;
  mojo::PendingRemote<crosapi::mojom::SyncedSessionClient> pending_remote =
      pending_receiver.InitWithNewPipeAndPassRemote();
  receivers_.Add(this, std::move(pending_receiver));
  return {pending_remote.PassPipe(),
          crosapi::mojom::SyncedSessionClient::Version_};
}

void FakeSyncedSessionClientAsh::OnForeignSyncedPhoneSessionsUpdated(
    std::vector<crosapi::mojom::SyncedSessionPtr> sessions) {
  last_foreign_synced_phone_sessions_ = std::move(sessions);
  if (on_foreign_synced_phone_sessions_updated_complete_callback_) {
    std::move(on_foreign_synced_phone_sessions_updated_complete_callback_)
        .Run();
  }
}

void FakeSyncedSessionClientAsh::SetOnForeignSyncedPhoneSessionsUpdatedCallback(
    base::RepeatingClosure callback) {
  on_foreign_synced_phone_sessions_updated_complete_callback_ =
      std::move(callback);
}

const std::vector<crosapi::mojom::SyncedSessionPtr>&
FakeSyncedSessionClientAsh::LookupForeignSyncedPhoneSessions() {
  return last_foreign_synced_phone_sessions_;
}

void FakeSyncedSessionClientAsh::OnSessionSyncEnabledChanged(bool enabled) {
  is_session_sync_enabled_ = enabled;
}

void FakeSyncedSessionClientAsh::SetFaviconDelegate(
    mojo::PendingRemote<crosapi::mojom::SyncedSessionClientFaviconDelegate>
        delegate) {}

void FakeSyncedSessionClientAsh::FlushMojoForTesting() {
  receivers_.FlushForTesting();
}

}  // namespace syncer
