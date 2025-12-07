// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {
namespace {
SettingsAppManager* g_instance = nullptr;
}  // namespace

// static
SettingsAppManager* SettingsAppManager::Get() {
  return g_instance;
}

SettingsAppManager::SettingsAppManager() {
  CHECK(!g_instance);
  g_instance = this;
}

SettingsAppManager::~SettingsAppManager() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
