// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/fake_account_status_change_delegate.h"

namespace ash {

namespace multidevice_setup {

FakeAccountStatusChangeDelegate::FakeAccountStatusChangeDelegate() = default;

FakeAccountStatusChangeDelegate::~FakeAccountStatusChangeDelegate() = default;

mojo::PendingRemote<mojom::AccountStatusChangeDelegate>
FakeAccountStatusChangeDelegate::GenerateRemote() {
  mojo::PendingRemote<mojom::AccountStatusChangeDelegate> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeAccountStatusChangeDelegate::OnPotentialHostExistsForNewUser() {
  ++num_new_user_potential_host_events_handled_;
}

void FakeAccountStatusChangeDelegate::OnNoLongerNewUser() {
  ++num_no_longer_new_user_events_handled_;
}

void FakeAccountStatusChangeDelegate::OnConnectedHostSwitchedForExistingUser(
    const std::string& new_host_device_name) {
  ++num_existing_user_host_switched_events_handled_;
}

void FakeAccountStatusChangeDelegate::OnNewChromebookAddedForExistingUser(
    const std::string& new_host_device_name) {
  ++num_existing_user_chromebook_added_events_handled_;
}

void FakeAccountStatusChangeDelegate::OnBecameEligibleForWifiSync() {
  ++num_eligible_for_wifi_sync_events_handled_;
}

}  // namespace multidevice_setup

}  // namespace ash
