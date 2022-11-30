// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/dbus_thread_manager.h"

#include <memory>
#include <utility>

#include "base/logging.h"

namespace ash {

static DBusThreadManager* g_dbus_thread_manager = nullptr;

DBusThreadManager::DBusThreadManager() = default;

DBusThreadManager::~DBusThreadManager() = default;

// static
void DBusThreadManager::Initialize() {
  CHECK(!g_dbus_thread_manager);
  g_dbus_thread_manager = new DBusThreadManager();

  if (!g_dbus_thread_manager->IsUsingFakes())
    VLOG(1) << "ash::DBusThreadManager initialized for ChromeOS";
  else
    VLOG(1) << "ash::DBusThreadManager created for testing";
}

// static
bool DBusThreadManager::IsInitialized() {
  return !!g_dbus_thread_manager;
}

// static
void DBusThreadManager::Shutdown() {
  // Ensure that we only shutdown DBusThreadManager once.
  CHECK(g_dbus_thread_manager);

  DBusThreadManager* dbus_thread_manager = g_dbus_thread_manager;
  g_dbus_thread_manager = nullptr;
  delete dbus_thread_manager;

  VLOG(1) << "DBusThreadManager Shutdown completed";
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

}  // namespace ash
