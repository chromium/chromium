// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_handler_test_helper.h"

#include "chromeos/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/network/managed_cellular_pref_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_test_helper_base.h"
#include "components/onc/onc_pref_names.h"

namespace chromeos {

NetworkHandlerTestHelper::NetworkHandlerTestHelper() {
  if (!NetworkHandler::IsInitialized()) {
    NetworkHandler::Initialize();
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
}

void NetworkHandlerTestHelper::InitializePrefs(PrefService* user_prefs,
                                               PrefService* device_prefs) {
  NetworkHandler::Get()->InitializePrefServices(user_prefs, device_prefs);
}

}  // namespace chromeos
