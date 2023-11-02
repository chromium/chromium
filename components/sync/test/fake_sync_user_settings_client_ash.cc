// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_user_settings_client_ash.h"

#include <utility>

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace syncer {

FakeSyncUserSettingsClientAsh::FakeSyncUserSettingsClientAsh() = default;
FakeSyncUserSettingsClientAsh::~FakeSyncUserSettingsClientAsh() = default;

void FakeSyncUserSettingsClientAsh::AddObserver(
    mojo::PendingRemote<crosapi::mojom::SyncUserSettingsClientObserver>
        observer) {
  observers_.Add(std::move(observer));
}

void FakeSyncUserSettingsClientAsh::IsAppsSyncEnabled(
    IsAppsSyncEnabledCallback callback) {
  std::move(callback).Run(apps_sync_is_enabled_);
}

void FakeSyncUserSettingsClientAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeSyncUserSettingsClientAsh::SetAppsSyncIsEnabled(bool enabled) {
  apps_sync_is_enabled_ = enabled;
  for (auto& observer : observers_) {
    observer->OnAppsSyncEnabledChanged(enabled);
  }
}

}  // namespace syncer
