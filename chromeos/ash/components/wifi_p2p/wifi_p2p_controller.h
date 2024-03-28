// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_CONTROLLER_H_

#include "base/check.h"
#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

// Class for handling initialization and access to chromeos wifi_p2p controller.
// Exposes functions for following operations:
// 1. Create a p2p group
// 2. Destroy a p2p group
// 3. Connect to a p2p group
// 4. Disconnect from a p2p group
// 5. Fetch p2p group/client properties
// 6. Tag socket to a WiFi direct group network rules.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_WIFI_P2P) WifiP2PController {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static WifiP2PController* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

 private:
  WifiP2PController();
  WifiP2PController(const WifiP2PController&) = delete;
  WifiP2PController& operator=(const WifiP2PController&) = delete;
  ~WifiP2PController();

  void Init();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_CONTROLLER_H_
