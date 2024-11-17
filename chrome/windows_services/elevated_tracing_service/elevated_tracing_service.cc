// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"
#include "chrome/windows_services/elevated_tracing_service/elevated_tracing_service_delegate.h"
#include "chrome/windows_services/service_program/service_program_main.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

extern "C" int WINAPI wWinMain(HINSTANCE /*instance*/,
                               HINSTANCE /*prev_instance*/,
                               wchar_t* /*command_line*/,
                               int /*show_command*/) {
  mojo::core::Init();
  base::Thread ipc_thread("IPC");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  elevated_tracing_service::Delegate delegate;

  return ServiceProgramMain(delegate);
}
