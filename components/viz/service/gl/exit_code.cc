// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/exit_code.h"

#include "base/logging.h"
#include "components/viz/service/gl/gpu_log_message_manager.h"

namespace viz {

void RestartGpuProcessForContextLoss(std::string_view reason) {
  LOG(ERROR) << "Restarting GPU process due to unrecoverable error. " << reason;

  // Terminate the GPU process on IO thread to ensure previous mojo messages are
  // in message queue.
  GpuLogMessageManager::GetInstance()->TerminateProcess(
      static_cast<int>(ExitCode::RESULT_CODE_GPU_EXIT_ON_CONTEXT_LOST));
}

}  // namespace viz
