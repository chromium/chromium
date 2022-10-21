// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_H_

#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

namespace multidevice_setup {

// Fake mojom::AccountStatusChangeDelegate implementation for tests.
class FakeAccountStatusChangeDelegate
    : public mojom::AccountStatusChangeDelegate {
 public:
  FakeAccountStatusChangeDelegate();

  FakeAccountStatusChangeDelegate(const FakeAccountStatusChangeDelegate&) =
      delete;
  FakeAccountStatusChangeDelegate& operator=(
      const FakeAccountStatusChangeDelegate&) = delete;

  ~FakeAccountStatusChangeDelegate() override;

  mojo::PendingRemote<mojom::AccountStatusChangeDelegate> GenerateRemote();

  size_t num_new_user_potential_host_events_handled() const {
    return num_new_user_potential_host_events_handled_;
  }

  size_t num_no_longer_new_user_events_handled() const {
    return num_no_longer_new_user_events_handled_;
  }

  size_t num_existing_user_host_switched_events_handled() const {
    return num_existing_user_host_switched_events_handled_;
  }

  size_t num_existing_user_chromebook_added_events_handled() const {
    return num_existing_user_chromebook_added_events_handled_;
  }

  size_t num_eligible_for_wifi_sync_events_handled() const {
    return num_eligible_for_wifi_sync_events_handled_;
  }

  // mojom::AccountStatusChangeDelegate:
  void OnPotentialHostExistsForNewUser() override;
  void OnNoLongerNewUser() override;
  void OnConnectedHostSwitchedForExistingUser(
      const std::string& new_host_device_name) override;
  void OnNewChromebookAddedForExistingUser(
      const std::string& new_host_device_name) override;
  void OnBecameEligibleForWifiSync() override;

 private:
  size_t num_new_user_potential_host_events_handled_ = 0u;
  size_t num_no_longer_new_user_events_handled_ = 0u;
  size_t num_existing_user_host_switched_events_handled_ = 0u;
  size_t num_existing_user_chromebook_added_events_handled_ = 0u;
  size_t num_eligible_for_wifi_sync_events_handled_ = 0u;

  mojo::ReceiverSet<mojom::AccountStatusChangeDelegate> receivers_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_H_
