// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/context_lost_reason.h"

#include "base/notreached.h"

namespace viz {

ContextLostReason GetContextLostReason(gpu::error::Error error,
                                       gpu::error::ContextLostReason reason) {
  if (error == gpu::error::kLostContext) {
    switch (reason) {
      case gpu::error::kGuilty:
        return CONTEXT_LOST_GUILTY;
      case gpu::error::kInnocent:
        return CONTEXT_LOST_INNOCENT;
      case gpu::error::kUnknown:
        return CONTEXT_LOST_UNKNOWN;
      case gpu::error::kOutOfMemory:
        return CONTEXT_LOST_OUT_OF_MEMORY;
      case gpu::error::kMakeCurrentFailed:
        return CONTEXT_LOST_MAKECURRENT_FAILED;
      case gpu::error::kGpuChannelLost:
        return CONTEXT_LOST_GPU_CHANNEL_ERROR;
      case gpu::error::kInvalidGpuMessage:
        return CONTEXT_LOST_INVALID_GPU_MESSAGE;
    }
  }
  switch (error) {
    case gpu::error::kInvalidSize:
      return CONTEXT_PARSE_ERROR_INVALID_SIZE;
    case gpu::error::kOutOfBounds:
      return CONTEXT_PARSE_ERROR_OUT_OF_BOUNDS;
    case gpu::error::kUnknownCommand:
      return CONTEXT_PARSE_ERROR_UNKNOWN_COMMAND;
    case gpu::error::kInvalidArguments:
      return CONTEXT_PARSE_ERROR_INVALID_ARGS;
    case gpu::error::kGenericError:
      return CONTEXT_PARSE_ERROR_GENERIC_ERROR;
    case gpu::error::kDeferCommandUntilLater:
    case gpu::error::kDeferLaterCommands:
    case gpu::error::kNoError:
    case gpu::error::kLostContext:
      NOTREACHED();
      return CONTEXT_LOST_UNKNOWN;
  }
  NOTREACHED();
  return CONTEXT_LOST_UNKNOWN;
}

}  // namespace viz
