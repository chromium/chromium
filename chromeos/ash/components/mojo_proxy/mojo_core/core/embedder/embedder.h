// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_EMBEDDER_EMBEDDER_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_EMBEDDER_EMBEDDER_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/configuration.h"

namespace mojo_legacy::core {

// Basic configuration/initialization ------------------------------------------

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system state. There is no corresponding
// shutdown operation: once the embedder is initialized, public Mojo C API calls
// remain available for the remainder of the process's lifetime.
COMPONENT_EXPORT(MOJO_LEGACY_CORE_EMBEDDER)
void Init(const Configuration& configuration);

// Like above but uses a default Configuration.
COMPONENT_EXPORT(MOJO_LEGACY_CORE_EMBEDDER) void Init();

// Always returns false: this frozen clone is legacy Mojo Core only and never
// runs ipcz. Retained because the cloned, implementation-agnostic Mojo tests
// still consult it to select their legacy code paths.
COMPONENT_EXPORT(MOJO_LEGACY_CORE_EMBEDDER) bool IsMojoIpczEnabled();

// Explicitly shuts down Mojo stopping any IO thread work and destroying any
// global state initialized by Init().
COMPONENT_EXPORT(MOJO_LEGACY_CORE_EMBEDDER) void ShutDown();

// Initialialization/shutdown for interprocess communication (IPC) -------------

// Retrieves the SequencedTaskRunner used for IPC I/O, as set by
// ScopedIPCSupport.
COMPONENT_EXPORT(MOJO_LEGACY_CORE_EMBEDDER)
scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner();

}  // namespace mojo_legacy::core

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_EMBEDDER_EMBEDDER_H_
