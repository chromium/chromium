// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_GET_CALLING_PROCESS_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_GET_CALLING_PROCESS_H_

#include <windows.h>

#include "base/process/process.h"

// Returns a base::Process of the process making the inbound RPC call, with the
// `desired_access` (defaults to PROCESS_QUERY_LIMITED_INFORMATION), or an
// invalid base::Process if such could not be determined.
base::Process GetCallingProcess(
    DWORD desired_access = PROCESS_QUERY_LIMITED_INFORMATION);

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_GET_CALLING_PROCESS_H_
