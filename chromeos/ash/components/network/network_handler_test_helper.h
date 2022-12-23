// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_TEST_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_TEST_HELPER_H_

#include "chromeos/ash/components/network/network_test_helper_base.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Helper class for tests that uses network handler classes. This class
// handles initialization and shutdown of Shill and Hermes DBus clients and
// NetworkHandler instance.
//
// NOTE: This class is intended for use in tests that use NetworkHandler::Get()
// and thus require NetworkHandler and related DBus clients to be initialized.
// For tests that only need NetworkStateHandler and or NetworkDeviceHandler to
// be initialized use NetworkStateTestHelper.
class NetworkHandlerTestHelper : public NetworkTestHelperBase {
 public:
  explicit NetworkHandlerTestHelper();
  ~NetworkHandlerTestHelper();

  // Registers any prefs required by NetworkHandler.
  void RegisterPrefs(PrefRegistrySimple* user_registry,
                     PrefRegistrySimple* device_registry);

  // Calls NetworkHandler::InitializePrefServices.
  void InitializePrefs(PrefService* user_prefs, PrefService* device_prefs);

 private:
  bool network_handler_initialized_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_TEST_HELPER_H_
