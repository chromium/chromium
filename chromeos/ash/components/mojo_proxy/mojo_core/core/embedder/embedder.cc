// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/embedder.h"

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/configuration.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/core.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/entrypoints.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/node_controller.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/thunks.h"

namespace mojo_legacy::core {

void Init(const Configuration& configuration) {
  internal::g_configuration = configuration;

  InitializeCore();
  MojoEmbedderSetSystemThunks(&GetSystemThunks());
}

void Init() {
  Init(Configuration());
}

bool IsMojoIpczEnabled() {
  // This clone is legacy Mojo Core only; ipcz is never enabled here.
  return false;
}

void ShutDown() {
  ShutDownCore();
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() {
  return Core::Get()->GetNodeController()->io_task_runner();
}

}  // namespace mojo_legacy::core
