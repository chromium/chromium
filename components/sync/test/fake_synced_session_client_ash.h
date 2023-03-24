// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNCED_SESSION_CLIENT_ASH_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNCED_SESSION_CLIENT_ASH_H_

#include <vector>

#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace syncer {

// Mimics the behavior of SyncedSessionClient.
class FakeSyncedSessionClientAsh : public crosapi::mojom::SyncedSessionClient {
 public:
  FakeSyncedSessionClientAsh();
  ~FakeSyncedSessionClientAsh() override;

  // crosapi::mojom::SyncedSessionClient:
  void OnForeignSyncedPhoneSessionsUpdated(
      std::vector<crosapi::mojom::SyncedSessionPtr> sessions) override;
  void OnSessionSyncEnabledChanged(bool enabled) override;
  void SetFaviconDelegate(
      mojo::PendingRemote<crosapi::mojom::SyncedSessionClientFaviconDelegate>
          delegate) override;

  void SetOnForeignSyncedPhoneSessionsUpdatedCallback(
      base::RepeatingClosure callback);

  // Returns a reference to foreign synced phone sessions received from
  // a remote crosapi::mojom::SyncedSessionClient call to
  // `OnForeignSyncedPhoneSessionsUpdated()`.
  const std::vector<crosapi::mojom::SyncedSessionPtr>&
  LookupForeignSyncedPhoneSessions();

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver);
  mojo::PendingRemote<crosapi::mojom::SyncedSessionClient> CreateRemote();

  bool is_session_sync_enabled() { return is_session_sync_enabled_; }

  void FlushMojoForTesting();

 private:
  mojo::ReceiverSet<crosapi::mojom::SyncedSessionClient> receivers_;
  std::vector<crosapi::mojom::SyncedSessionPtr>
      last_foreign_synced_phone_sessions_;
  base::RepeatingClosure
      on_foreign_synced_phone_sessions_updated_complete_callback_;
  bool is_session_sync_enabled_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNCED_SESSION_CLIENT_ASH_H_
