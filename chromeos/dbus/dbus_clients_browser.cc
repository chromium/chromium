// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_clients_browser.h"

#include "base/check.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

DBusClientsBrowser::DBusClientsBrowser(bool use_real_clients) {}

DBusClientsBrowser::~DBusClientsBrowser() = default;

void DBusClientsBrowser::Initialize(dbus::Bus* system_bus) {
  DCHECK(DBusThreadManager::IsInitialized());
}

}  // namespace chromeos
