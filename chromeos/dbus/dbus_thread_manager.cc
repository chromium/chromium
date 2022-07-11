// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "chromeos/dbus/cec_service/cec_service_client.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/dbus/dbus_clients_browser.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock/easy_unlock_client.h"
#include "chromeos/dbus/shill/shill_clients.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = nullptr;
static DBusThreadManagerSetter* g_setter = nullptr;

DBusThreadManager::DBusThreadManager()
    : clients_browser_(
          std::make_unique<DBusClientsBrowser>(use_real_clients_)) {}

DBusThreadManager::~DBusThreadManager() {
  // Delete all D-Bus clients before shutting down the system bus.
  clients_browser_.reset();
}

// Returns a client that is set via DBusThreadManagerSetter when available.
#define RETURN_DBUS_CLIENT(name)      \
  return (g_setter && g_setter->name) \
             ? g_setter->name.get()   \
             : (clients_browser_ ? clients_browser_->name.get() : nullptr)

CecServiceClient* DBusThreadManager::GetCecServiceClient() {
  return clients_browser_ ? clients_browser_->cec_service_client_.get()
                          : nullptr;
}

CrosDisksClient* DBusThreadManager::GetCrosDisksClient() {
  RETURN_DBUS_CLIENT(cros_disks_client_);
}

DebugDaemonClient* DBusThreadManager::GetDebugDaemonClient() {
  RETURN_DBUS_CLIENT(debug_daemon_client_);
}

EasyUnlockClient* DBusThreadManager::GetEasyUnlockClient() {
  return clients_browser_ ? clients_browser_->easy_unlock_client_.get()
                          : nullptr;
}

#undef RETURN_DBUS_CLIENT

void DBusThreadManager::InitializeClients() {
  // Some clients call DBusThreadManager::Get() during initialization.
  DCHECK(g_dbus_thread_manager);

  // TODO(stevenjb): Move these to dbus_helper.cc in src/chrome and any tests
  // that require Shill clients. https://crbug.com/948390.
  shill_clients::Initialize(GetSystemBus());

  if (clients_browser_)
    clients_browser_->Initialize(GetSystemBus());

  if (use_real_clients_)
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
DBusThreadManagerSetter* DBusThreadManager::GetSetterForTesting() {
  if (!g_setter)
    g_setter = new DBusThreadManagerSetter();
  return g_setter;
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

  delete g_setter;
  g_setter = nullptr;

  VLOG(1) << "DBusThreadManager Shutdown completed";
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

DBusThreadManagerSetter::DBusThreadManagerSetter() = default;

DBusThreadManagerSetter::~DBusThreadManagerSetter() = default;

void DBusThreadManagerSetter::SetCrosDisksClient(
    std::unique_ptr<CrosDisksClient> client) {
  cros_disks_client_ = std::move(client);
}

void DBusThreadManagerSetter::SetDebugDaemonClient(
    std::unique_ptr<DebugDaemonClient> client) {
  debug_daemon_client_ = std::move(client);
}

}  // namespace chromeos
