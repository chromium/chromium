// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/get_calling_process.h"

#include <windows.h>

#include <rpc.h>

#include "base/process/process_handle.h"

base::Process GetCallingProcess() {
  HANDLE calling_process_handle;
  RPC_STATUS status = ::I_RpcOpenClientProcess(
      nullptr, PROCESS_QUERY_LIMITED_INFORMATION, &calling_process_handle);
  if (status == RPC_S_NO_CALL_ACTIVE) {
    return base::Process::Current();  // The caller is in the local process.
  }

  return status == RPC_S_OK ? base::Process(calling_process_handle)
                            : base::Process();
}
