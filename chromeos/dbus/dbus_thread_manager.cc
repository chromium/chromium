// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/shill/shill_clients.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = nullptr;

DBusThreadManager::DBusThreadManager() = default;

DBusThreadManager::~DBusThreadManager() = default;

void DBusThreadManager::InitializeClients() {
  // Some clients call DBusThreadManager::Get() during initialization.
  DCHECK(g_dbus_thread_manager);

  // TODO(stevenjb): Move these to dbus_helper.cc in src/chrome and any tests
  // that require Shill clients. https://crbug.com/948390.
  shill_clients::Initialize(GetSystemBus());

  if (!IsUsingFakes())
    VLOG(1) << "DBusThreadManager initialized for ChromeOS";
  else
    VLOG(1) << "DBusThreadManager created for testing";
}

// static
void DBusThreadManager::Initialize() {
  CHECK(!g_dbus_thread_manager);
  g_dbus_thread_manager = new DBusThreadManager();
  g_dbus_thread_manager->InitializeClients();
}

// static
bool DBusThreadManager::IsInitialized() {
  return !!g_dbus_thread_manager;
}

// static
void DBusThreadManager::Shutdown() {
  // Ensure that we only shutdown DBusThreadManager once.
  CHECK(g_dbus_thread_manager);

  // TODO(stevenjb): Remove. https://crbug.com/948390.
  shill_clients::Shutdown();

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

}  // namespace chromeos
