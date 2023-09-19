// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "chromeos/ash/services/orca/public/cpp/orca_entry.h"

extern "C" {

OrcaBindServiceStatus __attribute__((visibility("default")))
OrcaBindService(const MojoSystemThunks2* /*mojo_thunks*/,
                const MojoSystemThunks* /*mojo_thunks_legacy*/,
                uint32_t /*receiver_handle*/,
                OrcaLogger* /*logger*/) {
  return OrcaBindServiceStatus::ORCA_BIND_SERVICE_STATUS_UNKNOWN_ERROR;
}
}
