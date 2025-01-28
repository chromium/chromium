// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_SYSTEM_TRACING_UTIL_H_
#define CHROME_INSTALLER_UTIL_SYSTEM_TRACING_UTIL_H_

namespace installer {

// Returns true if the Windows system tracing service is supported for the
// currently running process. This is true for per-machine installs of Chrome.
bool IsSystemTracingServiceSupported();

// Returns true if the Windows system tracing service is registered for the
// currently running process.
bool IsSystemTracingServiceRegistered();

// Presents a UAC prompt to the user to register the Windows system tracing
// service if supported for the currently running process.
bool ElevateAndRegisterSystemTracingService();

// Presents a UAC prompt to the user to deregister the Windows system tracing
// service if supported for the currently running process.
bool ElevateAndDeregisterSystemTracingService();

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_SYSTEM_TRACING_UTIL_H_
