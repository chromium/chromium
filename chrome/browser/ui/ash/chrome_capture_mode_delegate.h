// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_CAPTURE_MODE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_CAPTURE_MODE_DELEGATE_H_

#include "ash/public/cpp/capture_mode_delegate.h"
#include "base/callback.h"

// Implements the interface needed for the delegate of the Capture Mode feature
// in Chrome.
class ChromeCaptureModeDelegate : public ash::CaptureModeDelegate {
 public:
  ChromeCaptureModeDelegate();
  ChromeCaptureModeDelegate(const ChromeCaptureModeDelegate&) = delete;
  ChromeCaptureModeDelegate& operator=(const ChromeCaptureModeDelegate&) =
      delete;
  ~ChromeCaptureModeDelegate() override;

  static ChromeCaptureModeDelegate* Get();

  bool is_session_active() const { return is_session_active_; }

  // Sets |is_screen_capture_locked_| to the given |locked|, and interrupts any
  // on going video capture.
  void SetIsScreenCaptureLocked(bool locked);

  // Interrupts an on going video recording if any, due to some restricted
  // content showing up on the screen, or if screen capture becomes locked.
  void InterruptVideoRecordingIfAny();

  // ash::CaptureModeDelegate:
  base::FilePath GetScreenCaptureDir() const override;
  void ShowScreenCaptureItemInFolder(const base::FilePath& file_path) override;
  void OpenScreenshotInImageEditor(const base::FilePath& file_path) override;
  bool Uses24HourFormat() const override;
  bool IsCaptureModeInitRestrictedByDlp() const override;
  bool IsCaptureAllowedByDlp(const aura::Window* window,
                             const gfx::Rect& bounds,
                             bool for_video) const override;
  bool IsCaptureAllowedByPolicy() const override;
  void StartObservingRestrictedContent(
      const aura::Window* window,
      const gfx::Rect& bounds,
      base::OnceClosure stop_callback) override;
  void StopObservingRestrictedContent() override;
  mojo::Remote<recording::mojom::RecordingService> LaunchRecordingService()
      override;
  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override;
  void OnSessionStateChanged(bool started) override;

 private:
  // Used to temporarily disable capture mode in certain cases for which neither
  // a device policy, nor DLP will be triggered. For example, Some extension
  // APIs can request that a tab operate in a locked fullscreen mode, and in
  // that, capturing the screen is disabled.
  bool is_screen_capture_locked_ = false;

  // A callback to terminate an on going video recording on ash side due to a
  // restricted content showing up on the screen, or screen capture becoming
  // locked.
  // This is only non-null during recording.
  base::OnceClosure interrupt_video_recording_callback_;

  // True when a capture mode session is currently active.
  bool is_session_active_ = false;
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_CAPTURE_MODE_DELEGATE_H_
