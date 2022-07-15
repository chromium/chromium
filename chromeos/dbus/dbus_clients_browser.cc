// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_clients_browser.h"

#include "base/check.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock/easy_unlock_client.h"
#include "chromeos/dbus/easy_unlock/fake_easy_unlock_client.h"
#include "chromeos/dbus/fwupd/fake_fwupd_client.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"

namespace chromeos {

// CREATE_DBUS_CLIENT creates the appropriate version of D-Bus client.
#if defined(USE_REAL_DBUS_CLIENTS)
// Create the real D-Bus client. use_real_clients is ignored.
#define CREATE_DBUS_CLIENT(type, use_real_clients) type::Create()
#else
// Create a fake if use_real_clients == false.
// TODO(hashimoto): Always use fakes after adding
// use_real_dbus_clients=true to where needed. crbug.com/952745
#define CREATE_DBUS_CLIENT(type, use_real_clients) \
  (use_real_clients ? type::Create() : std::make_unique<Fake##type>())
#endif  // USE_REAL_DBUS_CLIENTS

DBusClientsBrowser::DBusClientsBrowser(bool use_real_clients) {
  debug_daemon_client_ =
      CREATE_DBUS_CLIENT(DebugDaemonClient, use_real_clients);
  easy_unlock_client_ = CREATE_DBUS_CLIENT(EasyUnlockClient, use_real_clients);
  fwupd_client_ = CREATE_DBUS_CLIENT(FwupdClient, use_real_clients);
}

DBusClientsBrowser::~DBusClientsBrowser() = default;

void DBusClientsBrowser::Initialize(dbus::Bus* system_bus) {
  DCHECK(DBusThreadManager::IsInitialized());

  debug_daemon_client_->Init(system_bus);
  easy_unlock_client_->Init(system_bus);
  fwupd_client_->Init(system_bus);
}

}  // namespace chromeos
