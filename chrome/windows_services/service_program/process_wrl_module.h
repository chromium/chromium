// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_PROCESS_WRL_MODULE_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_PROCESS_WRL_MODULE_H_

#include "base/functional/callback.h"

// Creates the process-wide WRL module for an out-of-process server.
void CreateWrlModule();

// Sets a callback to be run when the last reference to the process's WRL module
// is released. This can be used, for example, to trigger process shutdown.
void SetModuleReleasedCallback(base::OnceClosure callback);

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_PROCESS_WRL_MODULE_H_
