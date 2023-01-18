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
#include "mojo/public/cpp/bindings/remote_set.h"

namespace syncer {

// Mimics the behavior of SyncedSessionClient.
class FakeSyncedSessionClientAsh : public crosapi::mojom::SyncedSessionClient {
 public:
  FakeSyncedSessionClientAsh();
  ~FakeSyncedSessionClientAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver);
  void OnForeignSyncedPhoneSessionsUpdated(
      std::vector<crosapi::mojom::SyncedSessionPtr> sessions) override;

 private:
  mojo::ReceiverSet<crosapi::mojom::SyncedSessionClient> receivers_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNCED_SESSION_CLIENT_ASH_H_
