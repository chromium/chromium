// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
WifiConfigurationSyncServiceProvider* g_instance = nullptr;
}  // namespace

WifiConfigurationSyncServiceProvider::WifiConfigurationSyncServiceProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

WifiConfigurationSyncServiceProvider::~WifiConfigurationSyncServiceProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
WifiConfigurationSyncServiceProvider&
WifiConfigurationSyncServiceProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash
