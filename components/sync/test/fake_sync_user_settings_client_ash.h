// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_USER_SETTINGS_CLIENT_ASH_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_USER_SETTINGS_CLIENT_ASH_H_

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace syncer {

// Mimics the behavior of SyncUserSettingsClientAsh.
class FakeSyncUserSettingsClientAsh
    : public crosapi::mojom::SyncUserSettingsClient {
 public:
  constexpr static bool kDefaultAppsSyncIsEnabled = true;

  FakeSyncUserSettingsClientAsh();
  ~FakeSyncUserSettingsClientAsh() override;

  // crosapi::mojom::SyncUserSettingsClient implementation
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::SyncUserSettingsClientObserver>
          observer) override;
  void IsAppsSyncEnabled(IsAppsSyncEnabledCallback callback) override;

  // Own methods.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver);
  void SetAppsSyncIsEnabled(bool enabled);

 private:
  bool apps_sync_is_enabled_ = kDefaultAppsSyncIsEnabled;

  mojo::ReceiverSet<crosapi::mojom::SyncUserSettingsClient> receivers_;
  mojo::RemoteSet<crosapi::mojom::SyncUserSettingsClientObserver> observers_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_USER_SETTINGS_CLIENT_ASH_H_
