// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_GET_CALLING_PROCESS_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_GET_CALLING_PROCESS_H_

#include "base/process/process.h"

// Returns a base::Process of the process making the inbound RPC call, or an
// invalid base::Process if such could not be determined.
base::Process GetCallingProcess();

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_GET_CALLING_PROCESS_H_
