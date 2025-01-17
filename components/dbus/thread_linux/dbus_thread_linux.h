// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
#define COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

// Many APIs in ::dbus are required to be called from the same thread
// (https://crbug.com/130984). Therefore, a SingleThreadedTaskRunner is
// maintained and accessible through GetTaskRunner(), from which all calls
// to dbus on Linux have to be made.

#if BUILDFLAG(IS_CHROMEOS)
#error On ChromeOS, use DBusThreadManager instead.
#endif

namespace dbus {
class Bus;
}

namespace dbus_thread_linux {

// Obtains a task runner to handle DBus IO for usage on desktop Linux.
COMPONENT_EXPORT(COMPONENTS_DBUS)
scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

// Obtains a shared session bus for usage on desktop Linux. The task runner is
// the same as the one obtained from GetTaskRunner(). This should be used for
// all session bus operations except for those that require a named connection
// (currently MPRIS is the only one). Must be called on the UI thread.
COMPONENT_EXPORT(COMPONENTS_DBUS)
scoped_refptr<dbus::Bus> GetSharedSessionBus();

// The same as GetSharedSessionBus(), but for the system bus.
COMPONENT_EXPORT(COMPONENTS_DBUS)
scoped_refptr<dbus::Bus> GetSharedSystemBus();

}  // namespace dbus_thread_linux

#endif  // COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
