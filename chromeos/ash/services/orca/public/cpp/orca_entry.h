// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ORCA_PUBLIC_CPP_ORCA_ENTRY_H_
#define CHROMEOS_ASH_SERVICES_ORCA_PUBLIC_CPP_ORCA_ENTRY_H_

#include <stdint.h>

struct MojoSystemThunks;
struct MojoSystemThunks2;

extern "C" {

enum OrcaLogSeverity : int32_t {
  ORCA_LOG_SEVERITY_WARNING = 1,
  ORCA_LOG_SEVERITY_ERROR = 2,
};

enum OrcaBindServiceStatus : int32_t {
  ORCA_BIND_SERVICE_STATUS_UNKNOWN_ERROR = -1,
  ORCA_BIND_SERVICE_STATUS_OK = 0,
};

// Logger specified by clients to control where logs go.
struct __attribute__((visibility("default"))) OrcaLogger {
  void* user_data;
  void (*log)(OrcaLogger* /*self*/,
              OrcaLogSeverity /*severity*/,
              const char* /*message*/);
};

// Returns whether the receiver was successfully bound.
// The parameters include two MojoSystemThunks for backwards compatibility.
// The legacy thunks can be deleted once the shared library migrates to
// MojoSystemThunks2.
OrcaBindServiceStatus __attribute__((visibility("default")))
OrcaBindService(const MojoSystemThunks2* mojo_thunks,
                const MojoSystemThunks* mojo_thunks_legacy,
                uint32_t receiver_handle,
                OrcaLogger* logger);

// Resets the OrcaService.
// If `OrcaBindService` was called, this function must be called to clean up
// resources before calling OrcaBindService again or unloading the shared
// library.
void __attribute__((visibility("default"))) OrcaResetService();

}  // extern "C"

#endif  // CHROMEOS_ASH_SERVICES_ORCA_PUBLIC_CPP_ORCA_ENTRY_H_
