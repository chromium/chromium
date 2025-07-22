// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
#define COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#error On ChromeOS, use DBusThreadManager instead.
#endif

namespace dbus {
class Bus;
}

namespace dbus_thread_linux {

// Obtains a shared session bus for usage on desktop Linux. The task runner is
// the same as the one obtained from GetTaskRunner(). This should be used for
// all session bus operations. Must be called on the UI thread.
COMPONENT_EXPORT(COMPONENTS_DBUS)
scoped_refptr<dbus::Bus> GetSharedSessionBus();

// The same as GetSharedSessionBus(), but for the system bus.
COMPONENT_EXPORT(COMPONENTS_DBUS)
scoped_refptr<dbus::Bus> GetSharedSystemBus();

}  // namespace dbus_thread_linux

#endif  // COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
