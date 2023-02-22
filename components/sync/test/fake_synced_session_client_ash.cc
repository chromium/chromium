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

void FakeSyncedSessionClientAsh::OnForeignSyncedPhoneSessionsUpdated(
    std::vector<crosapi::mojom::SyncedSessionPtr> sessions) {
  NOTIMPLEMENTED();
}

void FakeSyncedSessionClientAsh::OnSessionSyncEnabledChanged(bool enabled) {
  NOTIMPLEMENTED();
}

}  // namespace syncer
