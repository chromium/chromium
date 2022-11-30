// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_ESIM_MANAGER_TEST_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_ESIM_MANAGER_TEST_OBSERVER_H_

#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::cellular_setup {

// Fake observer for testing ESimManager.
class ESimManagerTestObserver : public mojom::ESimManagerObserver {
 public:
  ESimManagerTestObserver();
  ESimManagerTestObserver(const ESimManagerTestObserver&) = delete;
  ESimManagerTestObserver& operator=(const ESimManagerTestObserver&) = delete;
  ~ESimManagerTestObserver() override;

  // mojom::ESimManagerObserver:
  void OnAvailableEuiccListChanged() override;
  void OnProfileListChanged(mojo::PendingRemote<mojom::Euicc> euicc) override;
  void OnEuiccChanged(mojo::PendingRemote<mojom::Euicc> euicc) override;
  void OnProfileChanged(
      mojo::PendingRemote<mojom::ESimProfile> esim_profile) override;

  // Generates pending remote bound to this observer.
  mojo::PendingRemote<mojom::ESimManagerObserver> GenerateRemote();

  // Resets all counters and call lists.
  void Reset();

  // Pops the last Euicc from change calls.
  mojo::PendingRemote<mojom::Euicc> PopLastChangedEuicc();

  // Pops the last ESimProfile from change calls.
  mojo::PendingRemote<mojom::ESimProfile> PopLastChangedESimProfile();

  int available_euicc_list_change_count() {
    return available_euicc_list_change_count_;
  }
  const std::vector<mojo::PendingRemote<mojom::Euicc>>&
  profile_list_change_calls() {
    return profile_list_change_calls_;
  }
  const std::vector<mojo::PendingRemote<mojom::Euicc>>& euicc_change_calls() {
    return euicc_change_calls_;
  }
  const std::vector<mojo::PendingRemote<mojom::ESimProfile>>&
  profile_change_calls() {
    return profile_change_calls_;
  }

 private:
  int available_euicc_list_change_count_ = 0;
  std::vector<mojo::PendingRemote<mojom::Euicc>> profile_list_change_calls_;
  std::vector<mojo::PendingRemote<mojom::Euicc>> euicc_change_calls_;
  std::vector<mojo::PendingRemote<mojom::ESimProfile>> profile_change_calls_;
  mojo::Receiver<mojom::ESimManagerObserver> receiver_{this};
};

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_ESIM_MANAGER_TEST_OBSERVER_H_
