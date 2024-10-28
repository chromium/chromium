// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPTURE_MODE_CHROME_CAPTURE_MODE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CAPTURE_MODE_CHROME_CAPTURE_MODE_DELEGATE_H_

#include <utility>

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/drive/file_errors.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace screen_ai {
class OpticalCharacterRecognizer;
}  // namespace screen_ai

namespace lens {
class LensOverlayQueryController;
}  // namespace lens

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
  void DetectTextInImage(const SkBitmap& image,
                         ash::OnTextDetectionComplete callback) override;
  void SendRegionSearch(const SkBitmap& image,
                        const gfx::Rect& region,
                        ash::OnSearchUrlFetchedCallback callback) override;
  void SendMultimodalSearch(const SkBitmap& image,
                            const gfx::Rect& region,
                            const std::string& text,
                            ash::OnSearchUrlFetchedCallback callback) override;

  void set_optical_character_recognizer_for_testing(
      scoped_refptr<screen_ai::OpticalCharacterRecognizer>
          optical_character_recognizer) {
    optical_character_recognizer_ = std::move(optical_character_recognizer);
  }

 private:
  void HandleStartQueryResponse(
      std::vector<lens::mojom::OverlayObjectPtr> objects,
      lens::mojom::TextPtr text,
      bool is_error);
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);
  void HandleSuggestInputsResponse(
      lens::proto::LensOverlaySuggestInputs suggest_inputs);
  void HandleThumbnailCreated(const std::string& thumbnail_bytes);

  // Called back by the Drive integration service when the quota usage is
  // retrieved.
  void OnGetDriveQuotaUsage(ash::OnGotDriveFsFreeSpace callback,
                            drive::FileError error,
                            drivefs::mojom::QuotaUsagePtr usage);

  // Called back once temporary directory for OneDrive is created.
  void SetOdfsTempDir(base::ScopedTempDir temp_dir);

  // Called back by the OCR service after it is initialized.
  void OnOcrServiceInitialized(bool is_successful);

  // Performs OCR on `pending_ocr_request_image_`. This is used to fulfill the
  // latest OCR request that occurs while the OCR service is being initialized.
  void PerformOcrOnPendingRequest();

  // Performs OCR to detect text in `image`.
  void PerformOcr(const SkBitmap& image, ash::OnTextDetectionComplete callback);

  // Called back by the OCR service once OCR has been performed.
  void OnOcrPerformed(ash::OnTextDetectionComplete callback,
                      screen_ai::mojom::VisualAnnotationPtr visual_annotation);

  // Releases the OCR handle and resets pending OCR requests.
  void ResetOcr();

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

  // A callback that will be invoked when the search URL is fetched.
  ash::OnSearchUrlFetchedCallback on_search_url_fetched_callback_;

  // True when a capture mode session is currently active.
  bool is_session_active_ = false;

  // Temporary directory to which files will be redirected before being uploaded
  // to OneDrive cloud. Created and destructed asynchronously.
  base::ScopedTempDir odfs_temp_dir_;

  // OCR used to detect text in a selected capture region.
  scoped_refptr<screen_ai::OpticalCharacterRecognizer>
      optical_character_recognizer_;

  // Stores the image and callback for the latest OCR request in the case that
  // the OCR service is not ready yet. These will be used to perform OCR after
  // the service indicates that it is ready.
  SkBitmap pending_ocr_request_image_ GUARDED_BY_CONTEXT(sequence_checker_);
  ash::OnTextDetectionComplete pending_ocr_request_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  // TODO(crbug.com/375491451): Remove this.
  std::unique_ptr<lens::LensOverlayQueryController>
      lens_overlay_query_controller_;

  std::unique_ptr<lens::LensOverlayGen204Controller> gen204_controller_;

  base::WeakPtrFactory<ChromeCaptureModeDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_CAPTURE_MODE_CHROME_CAPTURE_MODE_DELEGATE_H_
