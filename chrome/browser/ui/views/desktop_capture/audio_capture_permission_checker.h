// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_H_

#include "base/functional/callback.h"

class AudioCapturePermissionChecker {
 public:
  // This enum is used to track the audio permission state.
  enum class State { kUnknown, kChecking, kGranted, kDenied };

  // Create an AudioCapturePermissionChecker if there is one available and
  // enabled for this platform.
  static std::unique_ptr<AudioCapturePermissionChecker> MaybeCreate(
      base::RepeatingCallback<void(void)> callback);

  virtual ~AudioCapturePermissionChecker() = default;

  virtual State GetState() const = 0;

  virtual void RunCheck() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_H_
