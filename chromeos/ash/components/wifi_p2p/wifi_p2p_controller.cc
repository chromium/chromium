// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

namespace ash {

namespace {
WifiP2PController* g_wifi_p2p_controller = nullptr;
}  // namespace

WifiP2PController::WifiP2PController() {}

WifiP2PController::~WifiP2PController() {}

void WifiP2PController::Init() {}

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
