// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/dbus/lacros_dbus_thread_manager.h"

#include "base/logging.h"

namespace chromeos {

static LacrosDBusThreadManager* g_lacros_dbus_thread_manager = nullptr;

LacrosDBusThreadManager::LacrosDBusThreadManager() = default;

LacrosDBusThreadManager::~LacrosDBusThreadManager() = default;

// static
void LacrosDBusThreadManager::Initialize() {
  CHECK(!g_lacros_dbus_thread_manager);
  g_lacros_dbus_thread_manager = new LacrosDBusThreadManager();
}

// static
LacrosDBusThreadManager* LacrosDBusThreadManager::Get() {
  CHECK(g_lacros_dbus_thread_manager)
      << "LacrosDBusThreadManager::Get() called before Initialize()";
  return g_lacros_dbus_thread_manager;
}

// static
bool LacrosDBusThreadManager::IsInitialized() {
  return !!g_lacros_dbus_thread_manager;
}

// static
void LacrosDBusThreadManager::Shutdown() {
  // Ensure that we only shutdown DBusThreadManager once.
  CHECK(g_lacros_dbus_thread_manager);

  LacrosDBusThreadManager* lacros_dbus_thread_manager =
      g_lacros_dbus_thread_manager;
  g_lacros_dbus_thread_manager = nullptr;
  delete lacros_dbus_thread_manager;

  VLOG(1) << "LacrosDBusThreadManager Shutdown completed";
}

}  // namespace chromeos
