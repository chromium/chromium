// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/public/cpp/esim_manager_test_observer.h"

namespace ash::cellular_setup {

ESimManagerTestObserver::ESimManagerTestObserver() = default;
ESimManagerTestObserver::~ESimManagerTestObserver() = default;

void ESimManagerTestObserver::OnAvailableEuiccListChanged() {
  available_euicc_list_change_count_++;
}

void ESimManagerTestObserver::OnProfileListChanged(
    mojo::PendingRemote<mojom::Euicc> euicc) {
  profile_list_change_calls_.push_back(std::move(euicc));
}

void ESimManagerTestObserver::OnEuiccChanged(
    mojo::PendingRemote<mojom::Euicc> euicc) {
  euicc_change_calls_.push_back(std::move(euicc));
}

void ESimManagerTestObserver::OnProfileChanged(
    mojo::PendingRemote<mojom::ESimProfile> esim_profile) {
  profile_change_calls_.push_back(std::move(esim_profile));
}

mojo::PendingRemote<mojom::ESimManagerObserver>
ESimManagerTestObserver::GenerateRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void ESimManagerTestObserver::Reset() {
  available_euicc_list_change_count_ = 0;
  profile_list_change_calls_.clear();
  euicc_change_calls_.clear();
  profile_change_calls_.clear();
}

mojo::PendingRemote<mojom::Euicc>
ESimManagerTestObserver::PopLastChangedEuicc() {
  mojo::PendingRemote<mojom::Euicc> euicc =
      std::move(euicc_change_calls_.front());
  euicc_change_calls_.erase(euicc_change_calls_.begin());
  return euicc;
}

mojo::PendingRemote<mojom::ESimProfile>
ESimManagerTestObserver::PopLastChangedESimProfile() {
  mojo::PendingRemote<mojom::ESimProfile> esim_profile =
      std::move(profile_change_calls_.front());
  profile_change_calls_.erase(profile_change_calls_.begin());
  return esim_profile;
}

}  // namespace ash::cellular_setup
