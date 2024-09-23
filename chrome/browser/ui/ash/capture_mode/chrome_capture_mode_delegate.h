// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPTURE_MODE_CHROME_CAPTURE_MODE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CAPTURE_MODE_CHROME_CAPTURE_MODE_DELEGATE_H_

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/drive/file_errors.h"

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
  // Returns true if a video recording was interrupted, and false otherwise.
  bool InterruptVideoRecordingIfAny();

  // ash::CaptureModeDelegate:
  base::FilePath GetUserDefaultDownloadsFolder() const override;
  void OpenScreenCaptureItem(const base::FilePath& file_path) override;
  void OpenScreenshotInImageEditor(const base::FilePath& file_path) override;
  bool Uses24HourFormat() const override;
  void CheckCaptureModeInitRestrictionByDlp(
      ash::OnCaptureModeDlpRestrictionChecked callback) override;
  void CheckCaptureOperationRestrictionByDlp(
      const aura::Window* window,
      const gfx::Rect& bounds,
      ash::OnCaptureModeDlpRestrictionChecked callback) override;
  bool IsCaptureAllowedByPolicy() const override;
  void StartObservingRestrictedContent(
      const aura::Window* window,
      const gfx::Rect& bounds,
      base::OnceClosure stop_callback) override;
  void StopObservingRestrictedContent(
      ash::OnCaptureModeDlpRestrictionChecked callback) override;
  void OnCaptureImageAttempted(const aura::Window* window,
                               const gfx::Rect& bounds) override;
  mojo::Remote<recording::mojom::RecordingService> LaunchRecordingService()
      override;
  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override;
  void OnSessionStateChanged(bool started) override;
  void OnServiceRemoteReset() override;
  bool GetDriveFsMountPointPath(base::FilePath* path) const override;
  base::FilePath GetAndroidFilesPath() const override;
  base::FilePath GetLinuxFilesPath() const override;
  base::FilePath GetOneDriveMountPointPath() const override;
  PolicyCapturePath GetPolicyCapturePath() const override;
  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override;
  void GetDriveFsFreeSpaceBytes(ash::OnGotDriveFsFreeSpace callback) override;
  bool IsCameraDisabledByPolicy() const override;
  bool IsAudioCaptureDisabledByPolicy() const override;
  void RegisterVideoConferenceManagerClient(
      crosapi::mojom::VideoConferenceManagerClient* client,
      const base::UnguessableToken& client_id) override;
  void UnregisterVideoConferenceManagerClient(
      const base::UnguessableToken& client_id) override;
  void UpdateVideoConferenceManager(
      crosapi::mojom::VideoConferenceMediaUsageStatusPtr status) override;
  void NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device) override;
  void FinalizeSavedFile(
      base::OnceCallback<void(bool, const base::FilePath&)> callback,
      const base::FilePath& path,
      const gfx::Image& thumbnail) override;
  base::FilePath RedirectFilePath(const base::FilePath& path) override;
  std::unique_ptr<ash::AshWebView> CreateSearchResultsView() const override;

 private:
  // Called back by the Drive integration service when the quota usage is
  // retrieved.
  void OnGetDriveQuotaUsage(ash::OnGotDriveFsFreeSpace callback,
                            drive::FileError error,
                            drivefs::mojom::QuotaUsagePtr usage);

  // Called back once temporary directory for OneDrive is created.
  void SetOdfsTempDir(base::ScopedTempDir temp_dir);

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

  // Temporary directory to which files will be redirected before being uploaded
  // to OneDrive cloud. Created and destructed asynchronously.
  base::ScopedTempDir odfs_temp_dir_;

  base::WeakPtrFactory<ChromeCaptureModeDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_CAPTURE_MODE_CHROME_CAPTURE_MODE_DELEGATE_H_
