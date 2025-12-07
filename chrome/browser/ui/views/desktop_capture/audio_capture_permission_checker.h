// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_AUDIO_CAPTURE_PERMISSION_CHECKER_H_

#include "base/functional/callback.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AudioCapturePermissionCheckerInteractions {
  kDisabled = 0,
  kEnabled = 1,
  kCheckInitiated = 2,
  kPermissionGranted = 3,
  kPermissionDenied = 4,
  kSystemSettingsOpenedAfterDenial = 5,
  kCancelSharingAfterDenial = 6,
  kShareWindowOrScreenWithAudioAfterDenial = 7,
  kShareWindowOrScreenWithoutAudioAfterDenial = 8,
  kShareTabAfterDenial = 9,
  kMaxValue = kShareTabAfterDenial
};

void RecordUmaAudioCapturePermissionCheckerInteractions(
    AudioCapturePermissionCheckerInteractions interaction);
#endif  // BUILDFLAG(IS_MAC)

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
