// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/skyvault_capture_upload_notification.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/experiences/screenshot_area/screenshot_area.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "components/drive/file_errors.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/video_capture_service.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

namespace {

ChromeCaptureModeDelegate* g_instance = nullptr;

ScreenshotArea ConvertToScreenshotArea(const aura::Window* window,
                                       const gfx::Rect& bounds) {
  return window->IsRootWindow()
             ? ScreenshotArea::CreateForPartialWindow(window, bounds)
             : ScreenshotArea::CreateForWindow(window);
}

bool IsScreenCaptureDisabledByPolicy() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kDisableScreenshots);
}

void CaptureFileFinalized(
    const base::FilePath& original_path,
    base::OnceCallback<void(bool, const base::FilePath&)> callback,
    std::unique_ptr<policy::skyvault::SkyvaultCaptureUploadNotification>
        upload_notification,
    bool success,
    storage::FileSystemURL file_url) {
  std::move(callback).Run(success, file_url.path());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), original_path));
}

base::ScopedTempDir CreateTempDir() {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  return temp_dir;
}

}  // namespace

ChromeCaptureModeDelegate::ChromeCaptureModeDelegate() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateTempDir),
      base::BindOnce(&ChromeCaptureModeDelegate::SetOdfsTempDir,
                     weak_ptr_factory_.GetWeakPtr()));
}

ChromeCaptureModeDelegate::~ChromeCaptureModeDelegate() {
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(
                                 [](base::ScopedTempDir) {
                                   // No-op other than running
                                   // the base::ScopedTempDir
                                   // destructor.
                                 },
                                 std::move(odfs_temp_dir_)));
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ChromeCaptureModeDelegate* ChromeCaptureModeDelegate::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void ChromeCaptureModeDelegate::SetIsScreenCaptureLocked(bool locked) {
  is_screen_capture_locked_ = locked;
  if (is_screen_capture_locked_)
    InterruptVideoRecordingIfAny();
}

bool ChromeCaptureModeDelegate::InterruptVideoRecordingIfAny() {
  if (interrupt_video_recording_callback_) {
    std::move(interrupt_video_recording_callback_).Run();
    return true;
  }
  return false;
}

base::FilePath ChromeCaptureModeDelegate::GetUserDefaultDownloadsFolder()
    const {
  DCHECK(ash::LoginState::Get()->IsUserLoggedIn());

  auto* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  if (!profile->GetDownloadManager()->GetBrowserContext()) {
    // Some browser tests use a |content::MockDownloadManager| which doesn't
    // have a browser context. In this case, just return an empty path.
    return base::FilePath();
  }

  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(ProfileManager::GetActiveUserProfile());
  // We use the default downloads directory instead of the one that can be
  // configured from the browser's settings, since it can point to an invalid
  // location, which the browser handles by prompting the user to select
  // another one when accessed, but Capture Mode doesn't have this capability.
  // We also decided that this browser setting should not affect where the OS
  // saves the captured files. https://crbug.com/1192406.
  return download_prefs->GetDefaultDownloadDirectoryForProfile();
}

void ChromeCaptureModeDelegate::OpenScreenCaptureItem(
    const base::FilePath& file_path) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    return;
  }

  platform_util::OpenItem(profile, file_path,
                          platform_util::OpenItemType::OPEN_FILE,
                          platform_util::OpenOperationCallback());
}

void ChromeCaptureModeDelegate::OpenScreenshotInImageEditor(
    const base::FilePath& file_path) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  ash::SystemAppLaunchParams params;
  params.launch_paths = {file_path};
  params.launch_source = apps::LaunchSource::kFromFileManager;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::MEDIA, params);
}

bool ChromeCaptureModeDelegate::Uses24HourFormat() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // TODO(afakhry): Consider moving |prefs::kUse24HourClock| to ash/public so
  // we can do this entirely in ash.
  if (profile)
    return profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  return base::GetHourClockType() == base::k24HourClock;
}

void ChromeCaptureModeDelegate::CheckCaptureModeInitRestrictionByDlp(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  policy::DlpContentManagerAsh::Get()->CheckCaptureModeInitRestriction(
      std::move(callback));
}

void ChromeCaptureModeDelegate::CheckCaptureOperationRestrictionByDlp(
    const aura::Window* window,
    const gfx::Rect& bounds,
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  const ScreenshotArea area = ConvertToScreenshotArea(window, bounds);
  policy::DlpContentManagerAsh::Get()->CheckScreenshotRestriction(
      area, std::move(callback));
}

bool ChromeCaptureModeDelegate::IsCaptureAllowedByPolicy() const {
  return !is_screen_capture_locked_ && !IsScreenCaptureDisabledByPolicy();
}

void ChromeCaptureModeDelegate::StartObservingRestrictedContent(
    const aura::Window* window,
    const gfx::Rect& bounds,
    base::OnceClosure stop_callback) {
  // The order here matters, since DlpContentManagerAsh::OnVideoCaptureStarted()
  // may call InterruptVideoRecordingIfAny() right away, so the callback must be
  // set first.
  interrupt_video_recording_callback_ = std::move(stop_callback);
  policy::DlpContentManagerAsh::Get()->OnVideoCaptureStarted(
      ConvertToScreenshotArea(window, bounds));
}

void ChromeCaptureModeDelegate::StopObservingRestrictedContent(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  interrupt_video_recording_callback_.Reset();
  policy::DlpContentManagerAsh::Get()->CheckStoppedVideoCapture(
      std::move(callback));
}

void ChromeCaptureModeDelegate::OnCaptureImageAttempted(
    const aura::Window* window,
    const gfx::Rect& bounds) {
  policy::DlpContentManagerAsh::Get()->OnImageCapture(
      ConvertToScreenshotArea(window, bounds));
}

mojo::Remote<recording::mojom::RecordingService>
ChromeCaptureModeDelegate::LaunchRecordingService() {
  return content::ServiceProcessHost::Launch<
      recording::mojom::RecordingService>(
      content::ServiceProcessHost::Options()
          .WithDisplayName("Recording Service")
          .Pass());
}

void ChromeCaptureModeDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  content::GetAudioService().BindStreamFactory(std::move(receiver));
}

void ChromeCaptureModeDelegate::OnSessionStateChanged(bool started) {
  is_session_active_ = started;

  if (!is_session_active_) {
    // Release the OCR handle to save memory.
    ResetOcr();
  }
}

void ChromeCaptureModeDelegate::OnServiceRemoteReset() {}

bool ChromeCaptureModeDelegate::GetDriveFsMountPointPath(
    base::FilePath* result) const {
  if (!ash::LoginState::Get()->IsUserLoggedIn())
    return false;

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          ProfileManager::GetActiveUserProfile());
  if (!integration_service || !integration_service->IsMounted())
    return false;

  *result = integration_service->GetMountPointPath();
  return true;
}

base::FilePath ChromeCaptureModeDelegate::GetAndroidFilesPath() const {
  return file_manager::util::GetAndroidFilesPath();
}

base::FilePath ChromeCaptureModeDelegate::GetLinuxFilesPath() const {
  return file_manager::util::GetCrostiniMountDirectory(
      ProfileManager::GetActiveUserProfile());
}

base::FilePath ChromeCaptureModeDelegate::GetOneDriveMountPointPath() const {
  return policy::local_user_files::GetODFSVirtualPath();
}

ChromeCaptureModeDelegate::PolicyCapturePath
ChromeCaptureModeDelegate::GetPolicyCapturePath() const {
  if (auto* profile = ProfileManager::GetActiveUserProfile()) {
    auto* pref = profile->GetPrefs()->FindPreference(
        ash::prefs::kCaptureModePolicySavePath);
    if (pref->IsManaged()) {
      const base::FilePath resolved_path =
          policy::local_user_files::ResolvePath(pref->GetValue()->GetString());
      if (!resolved_path.empty()) {
        return {resolved_path, CapturePathEnforcement::kManaged};
      }
    }
    if (pref->IsRecommended()) {
      const base::FilePath resolved_path =
          policy::local_user_files::ResolvePath(
              pref->GetRecommendedValue()->GetString());
      if (!resolved_path.empty()) {
        return {resolved_path, CapturePathEnforcement::kRecommended};
      }
    }
  }
  return {base::FilePath(), CapturePathEnforcement::kNone};
}

void ChromeCaptureModeDelegate::ConnectToVideoSourceProvider(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver) {
  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      std::move(receiver));
}

void ChromeCaptureModeDelegate::GetDriveFsFreeSpaceBytes(
    ash::OnGotDriveFsFreeSpace callback) {
  DCHECK(ash::LoginState::Get()->IsUserLoggedIn());

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          ProfileManager::GetActiveUserProfile());
  if (!integration_service) {
    std::move(callback).Run(std::numeric_limits<int64_t>::max());
    return;
  }

  integration_service->GetQuotaUsage(
      base::BindOnce(&ChromeCaptureModeDelegate::OnGetDriveQuotaUsage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ChromeCaptureModeDelegate::IsCameraDisabledByPolicy() const {
  return policy::SystemFeaturesDisableListPolicyHandler::
      IsSystemFeatureDisabled(policy::SystemFeature::kCamera,
                              g_browser_process->local_state());
}

bool ChromeCaptureModeDelegate::IsAudioCaptureDisabledByPolicy() const {
  return !ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kAudioCaptureAllowed);
}

void ChromeCaptureModeDelegate::RegisterVideoConferenceManagerClient(
    crosapi::mojom::VideoConferenceManagerClient* client,
    const base::UnguessableToken& client_id) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->RegisterCppClient(client, client_id);
}

void ChromeCaptureModeDelegate::UnregisterVideoConferenceManagerClient(
    const base::UnguessableToken& client_id) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->UnregisterClient(client_id);
}

void ChromeCaptureModeDelegate::UpdateVideoConferenceManager(
    crosapi::mojom::VideoConferenceMediaUsageStatusPtr status) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->NotifyMediaUsageUpdate(std::move(status), base::DoNothing());
}

void ChromeCaptureModeDelegate::NotifyDeviceUsedWhileDisabled(
    crosapi::mojom::VideoConferenceMediaDevice device) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->NotifyDeviceUsedWhileDisabled(
          device,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE),
          base::DoNothing());
}

void ChromeCaptureModeDelegate::FinalizeSavedFile(
    base::OnceCallback<void(bool, const base::FilePath&)> callback,
    const base::FilePath& path,
    const gfx::Image& thumbnail) {
  auto* profile = ProfileManager::GetActiveUserProfile();
  if (!odfs_temp_dir_.GetPath().empty() &&
      odfs_temp_dir_.GetPath().IsParent(path) && profile) {
    // Passing the notification to the callback so that it's destructed once
    // file upload finishes.
    auto notification =
        std::make_unique<policy::skyvault::SkyvaultCaptureUploadNotification>(
            path);
    auto notification_ptr = notification.get();
    auto uploader = ash::cloud_upload::OdfsSkyvaultUploader::Upload(
        profile, path, policy::local_user_files::UploadTrigger::kScreenCapture,
        base::BindRepeating(
            &policy::skyvault::SkyvaultCaptureUploadNotification::
                UpdateProgress,
            notification->GetWeakPtr()),
        base::BindOnce(&CaptureFileFinalized, path, std::move(callback),
                       std::move(notification)),
        thumbnail);
    notification_ptr->SetCancelClosure(base::BindOnce(
        &ash::cloud_upload::OdfsSkyvaultUploader::Cancel, uploader));
    return;
  }
  std::move(callback).Run(/*success=*/true, path);
}

base::FilePath ChromeCaptureModeDelegate::RedirectFilePath(
    const base::FilePath& path) {
  if (odfs_temp_dir_.GetPath().empty()) {
    return path;
  }
  base::FilePath odfs_path = GetOneDriveMountPointPath();
  if (!odfs_path.empty() && path.DirName() == odfs_path) {
    return odfs_temp_dir_.GetPath().Append(path.BaseName());
  }
  if (!odfs_path.empty() && odfs_path.IsParent(path)) {
    base::FilePath ret = path;
    if (odfs_path.AppendRelativePath(odfs_temp_dir_.GetPath(), &ret)) {
      return ret;
    }
  }
  return path;
}

std::unique_ptr<ash::AshWebView>
ChromeCaptureModeDelegate::CreateSearchResultsView() const {
  return std::make_unique<ash::SearchResultsView>();
}

void ChromeCaptureModeDelegate::DetectTextInImage(
    const SkBitmap& image,
    ash::OnTextDetectionComplete callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ash::features::IsScannerEnabled());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run("");
    return;
  }

  if (optical_character_recognizer_ &&
      optical_character_recognizer_->is_ready()) {
    // Request `PerformOcr` asynchronously, so that it can be handled similarly
    // to the case where the OCR is not ready yet.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ChromeCaptureModeDelegate::PerformOcr,
                                  weak_ptr_factory_.GetWeakPtr(), image,
                                  std::move(callback)));
    return;
  }

  // Set a pending request to be fulfilled after the OCR service is ready. We
  // only need to fulfill the latest request when the OCR service becomes ready,
  // so if there is a previous request then respond to it with an empty string
  // and create a new request with the new `image` and `callback`.
  if (!pending_ocr_request_callback_.is_null()) {
    std::move(pending_ocr_request_callback_).Run("");
  }
  pending_ocr_request_image_ = image;
  pending_ocr_request_callback_ = std::move(callback);

  if (!optical_character_recognizer_) {
    optical_character_recognizer_ =
        screen_ai::OpticalCharacterRecognizer::CreateWithStatusCallback(
            profile, screen_ai::mojom::OcrClientType::kScreenshotTextDetection,
            base::BindOnce(&ChromeCaptureModeDelegate::OnOcrServiceInitialized,
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void ChromeCaptureModeDelegate::SendRegionSearch(
    const SkBitmap& image,
    const gfx::Rect& region,
    ash::OnSearchUrlFetchedCallback callback) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile || image.empty() || region.IsEmpty()) {
    return;
  }
  DCHECK(ash::IsSunfishFeatureEnabledWithFeatureKey());
  if (!gen204_controller_) {
    gen204_controller_ = std::make_unique<lens::LensOverlayGen204Controller>();
  }
  if (!lens_overlay_query_controller_) {
    lens_overlay_query_controller_ =
        std::make_unique<lens::LensOverlayQueryController>(
            base::BindRepeating(
                &ChromeCaptureModeDelegate::HandleStartQueryResponse,
                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(
                &ChromeCaptureModeDelegate::HandleInteractionURLResponse,
                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(
                &ChromeCaptureModeDelegate::HandleSuggestInputsResponse,
                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(
                &ChromeCaptureModeDelegate::HandleThumbnailCreated,
                weak_ptr_factory_.GetWeakPtr()),
            profile->GetVariationsClient(), /*identity_manager=*/nullptr,
            profile, lens::LensOverlayInvocationSource(),
            /*use_dark_mode=*/false,
            /*gen204_controller=*/gen204_controller_.get());
  }
  on_search_url_fetched_callback_ = std::move(callback);
  lens_overlay_query_controller_->StartQueryFlow(
      /*screenshot=*/image,
      /*page_url=*/GURL(),
      /*page_title=*/std::nullopt, /*significant_region_boxes=*/
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/base::span<const uint8_t>(),
      /*underlying_content_type=*/lens::MimeType(),
      /*ui_scale_factor=*/1.f, /*invocation_time=*/base::TimeTicks::Now());
  lens_overlay_query_controller_->SendRegionSearch(
      lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
          /*tab_bounds=*/region, /*view_bounds=*/region,
          /*image_bounds=*/region),
      lens::LensOverlaySelectionType::REGION_SEARCH,
      /*additional_search_query_params=*/std::map<std::string, std::string>(),
      /*region_bytes=*/image);
}

void ChromeCaptureModeDelegate::SendMultimodalSearch(
    const SkBitmap& image,
    const gfx::Rect& region,
    const std::string& text,
    ash::OnSearchUrlFetchedCallback callback) {
  // TODO(crbug.com/375670205): Investigate edge cases when the region is
  // adjusted or `SendMultimodalSearch()` is called before `StartQueryFlow()`.
  if (!lens_overlay_query_controller_ || image.empty() || region.IsEmpty() ||
      text.empty()) {
    return;
  }
  on_search_url_fetched_callback_ = std::move(callback);
  lens_overlay_query_controller_->SendMultimodalRequest(
      lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
          /*tab_bounds=*/region, /*view_bounds=*/region,
          /*image_bounds=*/region),
      text,
      lens::LensOverlaySelectionType::
          MULTIMODAL_SEARCH, /*additional_search_query_params=*/
      std::map<std::string, std::string>(),
      /*region_bytes=*/image);
}

void ChromeCaptureModeDelegate::HandleStartQueryResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    bool is_error) {}

void ChromeCaptureModeDelegate::HandleInteractionURLResponse(
    lens::proto::LensOverlayUrlResponse response) {
  if (on_search_url_fetched_callback_ && response.IsInitialized() &&
      response.has_url()) {
    std::move(on_search_url_fetched_callback_).Run(GURL(response.url()));
  }
}

void ChromeCaptureModeDelegate::HandleSuggestInputsResponse(
    lens::proto::LensOverlaySuggestInputs suggest_inputs) {}

void ChromeCaptureModeDelegate::HandleThumbnailCreated(
    const std::string& thumbnail_bytes) {}

void ChromeCaptureModeDelegate::OnGetDriveQuotaUsage(
    ash::OnGotDriveFsFreeSpace callback,
    drive::FileError error,
    drivefs::mojom::QuotaUsagePtr usage) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    std::move(callback).Run(-1);
    return;
  }

  std::move(callback).Run(usage->free_cloud_bytes);
}

void ChromeCaptureModeDelegate::SetOdfsTempDir(base::ScopedTempDir temp_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  odfs_temp_dir_ = std::move(temp_dir);
}

void ChromeCaptureModeDelegate::OnOcrServiceInitialized(bool is_successful) {
  if (is_successful) {
    PerformOcrOnPendingRequest();
  } else {
    ResetOcr();
  }
}

void ChromeCaptureModeDelegate::PerformOcrOnPendingRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!pending_ocr_request_callback_.is_null());
  PerformOcr(pending_ocr_request_image_,
             std::move(pending_ocr_request_callback_));
}

void ChromeCaptureModeDelegate::PerformOcr(
    const SkBitmap& image,
    ash::OnTextDetectionComplete callback) {
  // Since `PerformOcr` is called asynchronously, it's possible that OCR becomes
  // unavailable before this point, e.g. if the capture mode session is closed
  // before OCR finishes initialization or if the OCR service is disconnected.
  if (!optical_character_recognizer_ ||
      !optical_character_recognizer_->is_ready()) {
    std::move(callback).Run("");
    ResetOcr();
    return;
  }

  optical_character_recognizer_->PerformOCR(
      image,
      base::BindOnce(&ChromeCaptureModeDelegate::OnOcrPerformed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeCaptureModeDelegate::OnOcrPerformed(
    ash::OnTextDetectionComplete callback,
    screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reset the pending request image to save memory.
  pending_ocr_request_image_.reset();

  std::vector<std::string> text_lines;
  text_lines.reserve(visual_annotation->lines.size());
  for (screen_ai::mojom::LineBoxPtr& line : visual_annotation->lines) {
    text_lines.push_back(std::move(line->text_line));
  }
  std::move(callback).Run(base::JoinString(text_lines, "\n"));
}

void ChromeCaptureModeDelegate::ResetOcr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  optical_character_recognizer_ = nullptr;
  pending_ocr_request_image_.reset();
  if (!pending_ocr_request_callback_.is_null()) {
    std::move(pending_ocr_request_callback_).Run("");
  }
}
