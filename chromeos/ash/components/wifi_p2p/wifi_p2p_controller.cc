// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {
WifiP2PController* g_wifi_p2p_controller = nullptr;
}  // namespace

WifiP2PController::WifiP2PController() {}

WifiP2PController::~WifiP2PController() {}

void WifiP2PController::Init() {
  ShillManagerClient::Get()->SetProperty(
      shill::kP2PAllowedProperty,
      base::Value(ash::features::IsWifiDirectEnabled()), base::DoNothing(),
      base::BindOnce(&WifiP2PController::OnSetManagerPropertyFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     shill::kP2PAllowedProperty));
}

void WifiP2PController::OnSetManagerPropertyFailure(
    const std::string& property_name,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Error setting Shill manager properties: " << property_name
                 << ", error: " << error_name << ", message: " << error_message;
}

// static
void WifiP2PController::Initialize() {
  CHECK(!g_wifi_p2p_controller);
  g_wifi_p2p_controller = new WifiP2PController();
  g_wifi_p2p_controller->Init();
}

// static
void WifiP2PController::Shutdown() {
  CHECK(g_wifi_p2p_controller);
  delete g_wifi_p2p_controller;
  g_wifi_p2p_controller = nullptr;
}

// static
WifiP2PController* WifiP2PController::Get() {
  CHECK(g_wifi_p2p_controller)
      << "WifiP2PController::Get() called before Initialize()";
  return g_wifi_p2p_controller;
}

// static
bool WifiP2PController::IsInitialized() {
  return g_wifi_p2p_controller;
}

}  // namespace ash
