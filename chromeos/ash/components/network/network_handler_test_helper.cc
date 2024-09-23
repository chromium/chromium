// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_handler_test_helper.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_test_helper_base.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

NetworkHandlerTestHelper::NetworkHandlerTestHelper() {
  if (!NetworkHandler::IsInitialized()) {
    NetworkHandler::InitializeFake();
    network_handler_initialized_ = true;
  }
}

NetworkHandlerTestHelper::~NetworkHandlerTestHelper() {
  if (network_handler_initialized_)
    NetworkHandler::Shutdown();
}

void NetworkHandlerTestHelper::RegisterPrefs(
    PrefRegistrySimple* user_registry,
    PrefRegistrySimple* device_registry) {
  DCHECK(device_registry);
  ::onc::RegisterPrefs(device_registry);
  NetworkMetadataStore::RegisterPrefs(device_registry);
  ManagedCellularPrefHandler::RegisterLocalStatePrefs(device_registry);
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(device_registry);
  if (user_registry) {
    NetworkMetadataStore::RegisterPrefs(user_registry);
    ::onc::RegisterProfilePrefs(user_registry);
  }
  device_registry->RegisterBooleanPref(
      prefs::kDeviceEphemeralNetworkPoliciesEnabled, false);
}

void NetworkHandlerTestHelper::InitializePrefs(PrefService* user_prefs,
                                               PrefService* device_prefs) {
  NetworkHandler::Get()->InitializePrefServices(user_prefs, device_prefs);
}

}  // namespace ash
