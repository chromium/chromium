// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/public/error_codes.h"

#include <ostream>

#include "base/notreached.h"

namespace ttc {

std::ostream& operator<<(std::ostream& os, ErrorCode error) {
  switch (error) {
    case ErrorCode::kUnknown:
      return os << "Unknown";
    case ErrorCode::kRateLimited:
      return os << "RateLimited";
    case ErrorCode::kSafetyBlocked:
      return os << "SafetyBlocked";
    case ErrorCode::kInternalBackendError:
      return os << "InternalBackendError";
    case ErrorCode::kSessionExpired:
      return os << "SessionExpired";
    case ErrorCode::kOptimizationGuideUnavailable:
      return os << "OptimizationGuideUnavailable";
    case ErrorCode::kExecutionSessionCreationFailed:
      return os << "ExecutionSessionCreationFailed";
    case ErrorCode::kAudioNoMicrophoneDetected:
      return os << "AudioNoMicrophoneDetected";
    case ErrorCode::kAudioMicrophoneInUse:
      return os << "AudioMicrophoneInUse";
    case ErrorCode::kAudioUnknownError:
      return os << "AudioUnknownError";
  }
  NOTREACHED();
}

bool IsFatal(ErrorCode error) {
  switch (error) {
    case ErrorCode::kUnknown:
    case ErrorCode::kRateLimited:
    case ErrorCode::kSafetyBlocked:
    case ErrorCode::kInternalBackendError:
    case ErrorCode::kSessionExpired:
    case ErrorCode::kOptimizationGuideUnavailable:
    case ErrorCode::kExecutionSessionCreationFailed:
    case ErrorCode::kAudioNoMicrophoneDetected:
    case ErrorCode::kAudioMicrophoneInUse:
    case ErrorCode::kAudioUnknownError:
      return true;
  }
  NOTREACHED();
}

}  // namespace ttc
