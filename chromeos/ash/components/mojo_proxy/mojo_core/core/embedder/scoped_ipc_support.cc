// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/scoped_ipc_support.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/core.h"

namespace mojo_legacy::core {

namespace {

void ShutdownIPCSupport(base::OnceClosure callback) {
  Core::Get()->RequestShutdown(std::move(callback));
}

}  // namespace

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    ShutdownPolicy shutdown_policy)
    : shutdown_policy_(shutdown_policy) {
  Core::Get()->SetIOTaskRunner(std::move(io_thread_task_runner));
}

ScopedIPCSupport::~ScopedIPCSupport() {
  if (shutdown_policy_ == ShutdownPolicy::FAST) {
    ShutdownIPCSupport(base::DoNothing());
    return;
  }

  base::WaitableEvent shutdown_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  ShutdownIPCSupport(base::BindOnce(&base::WaitableEvent::Signal,
                                    base::Unretained(&shutdown_event)));

  base::ScopedAllowBaseSyncPrimitives allow_io;
  shutdown_event.Wait();
}

}  // namespace mojo_legacy::core
