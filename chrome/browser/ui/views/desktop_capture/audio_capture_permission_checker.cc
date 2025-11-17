// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/audio_capture_permission_checker.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/desktop_capture/audio_capture_permission_checker_mac.h"

void RecordUmaAudioCapturePermissionCheckerInteractions(
    AudioCapturePermissionCheckerInteractions interaction) {
  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.AudioCapturePermissionChecker.Interactions",
      interaction);
}
#endif  // BUILDFLAG(IS_MAC)

std::unique_ptr<AudioCapturePermissionChecker>
AudioCapturePermissionChecker::MaybeCreate(
    base::RepeatingCallback<void(void)> callback) {
#if BUILDFLAG(IS_MAC)
  return AudioCapturePermissionCheckerMac::MaybeCreate(callback);
#else
  return nullptr;
#endif
}
