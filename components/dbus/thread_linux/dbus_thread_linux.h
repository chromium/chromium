// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
#define COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#error On ChromeOS, use DBusThreadManager instead.
#endif

namespace dbus {
class Bus;
}

namespace dbus_thread_linux {

// Starts the shared D-Bus thread and creates the session and system buses on
// it, if a getter below has not already done so, and sets the callback that
// runs when either bus loses its connection, e.g. because the desktop session
// is ending. Call early in process startup, on the thread that will use the
// buses and later call Shutdown() (the UI thread in the browser process); the
// callback runs on that thread. The process cannot keep using the bus after
// a disconnect, so the callback should exit; without one, losing the
// connection is fatal.
COMPONENT_EXPORT(COMPONENTS_DBUS)
void Initialize(base::RepeatingClosure disconnected_callback = {});

// The shared session bus. Must be called on the thread that calls
// Initialize(); creates the thread and buses on first use if Initialize() has
// not run yet.
// TODO(shelley.vohr): Require Initialize() to have run and CHECK here instead
// of creating the thread lazily, once every embedder and test harness that
// reaches these getters has an explicit Initialize() call.
COMPONENT_EXPORT(COMPONENTS_DBUS)
scoped_refptr<dbus::Bus> GetSharedSessionBus();

// The same as GetSharedSessionBus(), but for the system bus.
COMPONENT_EXPORT(COMPONENTS_DBUS)
scoped_refptr<dbus::Bus> GetSharedSystemBus();

// Shuts down both buses (blocking) and stops the D-Bus thread. Must be called
// on the thread that called Initialize(), after the last user of the buses is
// gone.
// TODO(shelley.vohr): Call this from the browser process during shutdown as
// well (see ChromeBrowserMainPartsLinux::PostDestroyThreads()); today only
// tests call it and the browser releases the buses at process exit.
COMPONENT_EXPORT(COMPONENTS_DBUS)
void Shutdown();

}  // namespace dbus_thread_linux

#endif  // COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
