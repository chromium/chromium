// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/exit_code.h"

#include "base/logging.h"
#include "base/process/process.h"

namespace viz {

void RestartGpuProcessForContextLoss(std::string_view reason) {
  LOG(ERROR) << "Restarting GPU process due to unrecoverable error. " << reason;
  base::Process::TerminateCurrentProcessImmediately(
      static_cast<int>(ExitCode::RESULT_CODE_GPU_EXIT_ON_CONTEXT_LOST));
}

}  // namespace viz
