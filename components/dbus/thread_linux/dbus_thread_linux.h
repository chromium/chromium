// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
#define COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

// Many APIs in ::dbus are required to be called from the same thread
// (https://crbug.com/130984). Therefore, a SingleThreadedTaskRunner is
// maintained and accessible through GetTaskRunner(), from which all calls
// to dbus on Linux have to be made.

#if BUILDFLAG(IS_CHROMEOS_ASH)
#error On ChromeOS, use DBusThreadManager instead.
#endif

namespace dbus_thread_linux {

// Obtains a task runner to handle DBus IO for usage on desktop Linux.
COMPONENT_EXPORT(DBUS)
scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

}  // namespace dbus_thread_linux

#endif  // COMPONENTS_DBUS_THREAD_LINUX_DBUS_THREAD_LINUX_H_
