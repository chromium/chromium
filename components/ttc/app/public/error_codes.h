// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_ERROR_CODES_H_
#define COMPONENTS_TTC_APP_PUBLIC_ERROR_CODES_H_

#include <iosfwd>

namespace ttc {

enum class ErrorCode {
  // The 0-1000 errors map to the ServerErrorNotification frame's ErrorCode in
  // the
  // proto.

  kUnknown = 0,
  kRateLimited = 1,
  kSafetyBlocked = 2,
  kInternalBackendError = 3,
  kSessionExpired = 4,
  kMaxServerErrorCode = kSessionExpired,

  // 1000+ are for client-side errors

  // The RemoteModelExecutor is not available.
  kOptimizationGuideUnavailable = 1001,

  // Encountered a failure when trying to create a RemoteModelExecutionSession.
  kExecutionSessionCreationFailed = 1002,

  // No usable microphone was detected.
  kAudioNoMicrophoneDetected = 1003,

  // The microphone is already in use by another application.
  kAudioMicrophoneInUse = 1004,

  // Audio capture encountered an unknown error.
  kAudioUnknownError = 1005,
};

// Whether an error is fatal to the session. Fatal errors end the session.
bool IsFatal(ErrorCode error);

std::ostream& operator<<(std::ostream& os, ErrorCode error);

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_ERROR_CODES_H_
