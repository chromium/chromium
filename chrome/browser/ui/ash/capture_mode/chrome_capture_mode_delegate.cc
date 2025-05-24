// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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
#include "chrome/browser/ash/policy/skyvault/odfs_file_deleter.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/skyvault_capture_upload_notification.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/experiences/screenshot_area/screenshot_area.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "components/drive/file_errors.h"
#include "components/lens/lens_constants.h"
#include "components/lens/lens_metadata.mojom-shared.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/video_capture_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_util.h"

namespace {

ChromeCaptureModeDelegate* g_instance = nullptr;

constexpr char kConsumerName[] = "ChromeCaptureModeDelegate";

// The image quality when encoding the image being searched into the body of the
// POST request.
constexpr int kEncodingQualityJpeg = 40;

// Lens POST request parameters.
constexpr char kQueryParamEntryPointName[] = "ep";
constexpr char kQueryParamEntryPointValueLauncher[] = "crosl";
constexpr char kQueryParamEntryPointValueScreenshot[] = "crosrs";
constexpr char kQueryParamSurfaceName[] = "s";
constexpr char kQueryParamSurfaceValue[] = "43";
constexpr char kQueryParamViewportWidthName[] = "vpw";
constexpr char kQueryParamViewportHeightName[] = "vph";
constexpr char kQueryParamStartTimeName[] = "st";

// The default HTTP status code we set if the response header does not contain
// a successful status code.
constexpr int kHttpPostFailNoConnection = -1;

constexpr char kLensWebQFMetadataURL[] = "https://lens.google.com/qfmetadata";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chromeos_lens_web_image_search",
                                        R"(
        semantics {
          sender: "Select to Search on ChromeOS"
          description:
            "ChromeOS allows image search and text detection for anything on "
            "your screen. By selecting a desired region to search, ChromeOS "
            "will send your image data to a server to be processed, then "
            "return a page of relevant search results, along with the ability "
            "to copy the selected text."
          trigger:
            "In a standalone session, the user selects a region on their "
            "screen. In a capture mode screenshot session, the user selects "
            "a region on their screen, then presses the search button."
          internal {
            contacts {
                email: "chromeos-wm@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: IMAGE
            type: USAGE_AND_PERFORMANCE_METRICS
          }
          data: "Image data from the region selected by the user. An access "
                "token associated with the user's primary account."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2025-03-27"
        }
        policy {
          cookies_allowed: NO
          chrome_policy {
            DisableScreenshots {
              DisableScreenshots: false
            }
            LensOverlaySettings {
              LensOverlaySettings: 1
            }
          }
        }
        comments:
         "There is no user setting to control this feature, it is generally "
         "made available to all users with Google as their default search "
         "engine unless disabled by a policy."
      )");

// The expected message id for the query forumlation metadata response body.
constexpr char kQFMetadataResponseMessageId[] =
    "fetch_query_formulation_metadata_response";

// Query Formulation Metadata Response constants.
constexpr int kQFMetadataResponseMinSize = 3;
constexpr int kQFMetadataResponseFieldMessageIdIdx = 0;
constexpr int kQFMetadataResponseFieldDetectedText = 2;
constexpr int kDetectedTextFieldTextLayout = 0;
constexpr int kTextLayoutFieldParagraphs = 0;
constexpr int kParagraphMinSize = 2;
constexpr int kParagraphFieldLines = 1;
constexpr int kLineFieldWords = 0;
constexpr int kWordMinSize = 3;
constexpr int kWordFieldPlainText = 1;
constexpr int kWordFieldTextSeparator = 2;

ScreenshotArea ConvertToScreenshotArea(const aura::Window* window,
                                       const gfx::Rect& bounds) {
  return window->IsRootWindow()
             ? ScreenshotArea::CreateForPartialWindow(window, bounds)
             : ScreenshotArea::CreateForWindow(window);
}

bool IsScreenCaptureDisabledByPolicy(PrefService& local_state) {
  return local_state.GetBoolean(prefs::kDisableScreenshots);
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

std::vector<unsigned char> EncodeImage(const gfx::Image& image,
                                       std::string& content_type,
                                       lens::mojom::ImageFormat& image_format) {
  std::optional<std::vector<uint8_t>> data =
      gfx::JPEGCodec::Encode(image.AsBitmap(), kEncodingQualityJpeg);

  if (data) {
    content_type = "image/jpeg";
    image_format = lens::mojom::ImageFormat::JPEG;
    return data.value();
  }

  // Get the front and end of the image bytes in order to store them in the
  // search_args to be sent as part of the PostContent in the request
  content_type = "image/png";
  image_format = lens::mojom::ImageFormat::PNG;
  auto bytes = image.As1xPNGBytes();
  return {bytes->begin(), bytes->end()};
}

lens::mojom::ImageFormat EncodeImageIntoSearchArgs(
    const gfx::Image& image,
    size_t& encoded_size_bytes,
    TemplateURLRef::SearchTermsArgs& search_args) {
  lens::mojom::ImageFormat image_format;
  std::string content_type;
  std::vector<uint8_t> data = EncodeImage(image, content_type, image_format);
  encoded_size_bytes = sizeof(unsigned char) * data.size();
  search_args.image_thumbnail_content.assign(data.begin(), data.end());
  search_args.image_thumbnail_content_type = content_type;
  return image_format;
}

// Returns true if the given `image` is too large to be uploaded to the Lens Web
// API as-is and needs to be downscaled first.
bool NeedsDownscale(const gfx::Image& image) {
  return (image.Height() * image.Width() > lens::kMaxAreaForImageSearch) &&
         (image.Width() > lens::kMaxPixelsForImageSearch ||
          image.Height() > lens::kMaxPixelsForImageSearch);
}

scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
  const user_manager::User* const active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  CHECK(active_user);

  return ash::BrowserContextHelper::Get()
      ->GetBrowserContextByUser(active_user)
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

// Attempts to parse the `response` as if it was a
// `FetchQueryFormulationMetadataResponse` encoded in JSON, and store the
// formatted text in `extracted_text`. Returns true if the response was
// successfully parsed (even if the text was empty), and false otherwise. See
// `google3/google/internal/lens/frontend/api/v1/service.proto` for more details
// about the expected response.
bool ParseQueryFormulationMetadataResponse(
    const data_decoder::DataDecoder::ValueOrError& response,
    std::string& extracted_text) {
  if (!response.has_value() || !response->is_list() ||
      response->GetList().empty()) {
    return false;
  }

  const base::Value::List* metadata_response =
      response->GetList()[0].GetIfList();
  if (!metadata_response ||
      metadata_response->size() < kQFMetadataResponseMinSize) {
    return false;
  }

  // Verify we have the right type of response message.
  const std::string* message_id =
      (*metadata_response)[kQFMetadataResponseFieldMessageIdIdx].GetIfString();
  if (!message_id || (*message_id) != kQFMetadataResponseMessageId) {
    return false;
  }

  // Deconstruct the metadata response in order to build our string for Copy
  // Text.
  const base::Value::List* detected_text =
      (*metadata_response)[kQFMetadataResponseFieldDetectedText].GetIfList();
  if (!detected_text || detected_text->empty()) {
    return false;
  }

  // If we don't have a `text_layout` object, then there may not be any text to
  // detect, so we should return true.
  const base::Value::List* text_layout =
      (*detected_text)[kDetectedTextFieldTextLayout].GetIfList();
  if (!text_layout) {
    return true;
  }
  if (text_layout->empty()) {
    return false;
  }

  const base::Value::List* paragraph_list =
      (*text_layout)[kTextLayoutFieldParagraphs].GetIfList();
  if (!paragraph_list || paragraph_list->empty()) {
    return false;
  }

  // Begin constructing the extracted text by looping through a sequence of
  // paragraphs, lines, and words.
  for (int i = 0; i < static_cast<int>(paragraph_list->size()); i++) {
    const base::Value::List* paragraph = (*paragraph_list)[i].GetIfList();
    if (!paragraph || paragraph->size() < kParagraphMinSize) {
      continue;
    }

    const base::Value::List* line_list =
        (*paragraph)[kParagraphFieldLines].GetIfList();
    if (!line_list || line_list->empty()) {
      continue;
    }

    // Add an extra newline between each paragraph (i.e., before each
    // paragraph after the first).
    if (i > 0) {
      extracted_text += "\n";
    }

    for (int j = 0; j < static_cast<int>(line_list->size()); j++) {
      const base::Value::List* line = (*line_list)[j].GetIfList();
      if (!line || line->empty()) {
        continue;
      }

      const base::Value::List* word_list = (*line)[kLineFieldWords].GetIfList();
      if (!word_list || word_list->empty()) {
        continue;
      }

      // Add a newline between each line (i.e., before each line after the
      // first).
      if (j > 0) {
        extracted_text += "\n";
      }

      for (const base::Value& word_value : *word_list) {
        const base::Value::List* word = word_value.GetIfList();
        if (!word || word->size() < kWordMinSize) {
          continue;
        }

        const std::string* plain_text =
            (*word)[kWordFieldPlainText].GetIfString();
        if (!plain_text) {
          continue;
        }

        extracted_text += *plain_text;

        // Add the text separator if it exists.
        const std::string* separator =
            (*word)[kWordFieldTextSeparator].GetIfString();
        if (!separator) {
          continue;
        }

        extracted_text += *separator;
      }
    }
  }

  return true;
}

}  // namespace

ChromeCaptureModeDelegate::ChromeCaptureModeDelegate(
    PrefService* local_state,
    ApplicationLocaleStorage* application_locale_storage)
    : local_state_(CHECK_DEREF(local_state)),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)) {
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

bool ChromeCaptureModeDelegate::IsSearchAllowedByPolicy() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  return profile && profile->GetPrefs() &&
         profile->GetPrefs()->GetInteger(lens::prefs::kLensOverlaySettings) ==
             static_cast<int>(
                 lens::prefs::LensOverlaySettingsPolicyValue::kEnabled);
}

void ChromeCaptureModeDelegate::SetIsScreenCaptureLocked(bool locked) {
  is_screen_capture_locked_ = locked;
  if (is_screen_capture_locked_) {
    InterruptVideoRecordingIfAny();
  }
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
  if (!profile) {
    return;
  }

  ash::SystemAppLaunchParams params;
  params.launch_paths = {file_path};
  params.launch_source = apps::LaunchSource::kFromFileManager;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::MEDIA, params);
}

bool ChromeCaptureModeDelegate::Uses24HourFormat() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // TODO(afakhry): Consider moving |prefs::kUse24HourClock| to ash/public so
  // we can do this entirely in ash.
  if (profile) {
    return profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  }
  return base::GetHourClockType() == base::k24HourClock;
}

void ChromeCaptureModeDelegate::CheckCaptureModeInitRestrictionByDlp(
    bool shutting_down,
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  policy::DlpContentManagerAsh::Get()->CheckCaptureModeInitRestriction(
      shutting_down, std::move(callback));
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
  return !is_screen_capture_locked_ &&
         !IsScreenCaptureDisabledByPolicy(local_state_.get());
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
  if (!ash::LoginState::Get()->IsUserLoggedIn()) {
    return false;
  }

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          ProfileManager::GetActiveUserProfile());
  if (!integration_service || !integration_service->IsMounted()) {
    return false;
  }

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
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    return base::FilePath();
  }
  const auto odfs_info = ash::cloud_upload::GetODFSInfo(profile);
  return odfs_info ? odfs_info->mount_path() : base::FilePath();
}

base::FilePath ChromeCaptureModeDelegate::GetOneDriveVirtualPath() const {
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
                              &local_state_.get());
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
    const gfx::Image& thumbnail,
    bool for_video) {
  auto* profile = ProfileManager::GetActiveUserProfile();
  if (!odfs_temp_dir_.GetPath().empty() &&
      odfs_temp_dir_.GetPath().IsParent(path) && profile) {
    // Passing the notification to the callback so that it's destructed once
    // file upload finishes.
    auto notification =
        std::make_unique<policy::skyvault::SkyvaultCaptureUploadNotification>(
            path, for_video);
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
  base::FilePath odfs_path = GetOneDriveVirtualPath();
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
  CHECK(ash::features::IsCaptureModeOnDeviceOcrEnabled());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run(std::nullopt);
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
  // so if there is a previous request then respond to it with nullopt and
  // create a new request with the new `image` and `callback`.
  if (!pending_ocr_request_callback_.is_null()) {
    std::move(pending_ocr_request_callback_).Run(std::nullopt);
  }
  pending_ocr_request_image_ = image;
  pending_ocr_request_callback_ = std::move(callback);

  if (!optical_character_recognizer_) {
    ocr_service_initialized_callback_.Reset(
        base::BindOnce(&ChromeCaptureModeDelegate::OnOcrServiceInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
    optical_character_recognizer_ =
        screen_ai::OpticalCharacterRecognizer::CreateWithStatusCallback(
            profile, screen_ai::mojom::OcrClientType::kScreenshotTextDetection,
            ocr_service_initialized_callback_.callback());
  }
}

void ChromeCaptureModeDelegate::SendLensWebRegionSearch(
    const gfx::Image& image,
    const bool is_standalone_session,
    ash::OnSearchUrlFetchedCallback search_callback,
    ash::OnTextDetectionComplete text_callback,
    base::OnceCallback<void()> error_callback) {
  on_search_url_fetched_callback_ = std::move(search_callback);
  on_text_detection_complete_callback_ = std::move(text_callback);
  on_error_callback_ = std::move(error_callback);

  // Increment the `lens_request_id_` to represent a new request id.
  ++lens_request_id_;

  GetPrimaryAccountAccessToken(base::BindRepeating(
      &ChromeCaptureModeDelegate::OnAccessTokenAvailableForImageSearch,
      weak_ptr_factory_.GetWeakPtr(), image, is_standalone_session,
      lens_request_id_));
}

bool ChromeCaptureModeDelegate::IsNetworkConnectionOffline() const {
  return content::GetNetworkConnectionTracker()->IsOffline();
}

void ChromeCaptureModeDelegate::DeleteRemoteFile(
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetOneDriveMountPointPath().IsParent(path));
  ash::cloud_upload::OdfsFileDeleter::Delete(path, std::move(callback));
}

bool ChromeCaptureModeDelegate::ActiveUserDefaultSearchProviderIsGoogle()
    const {
  const user_manager::User* const active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  CHECK(active_user);

  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);

  return search::DefaultSearchProviderIsGoogle(template_url_service);
}

void ChromeCaptureModeDelegate::HandleStartQueryResponse(
    std::vector<lens::OverlayObject> objects,
    lens::Text text,
    bool is_error) {
  if (is_error || !on_text_detection_complete_callback_ ||
      !text.has_text_layout()) {
    return;
  }

  std::string extracted_text;
  const lens::TextLayout& text_layout = text.text_layout();

  for (int i = 0; i < text_layout.paragraphs_size(); i++) {
    const auto& paragraph = text_layout.paragraphs()[i];

    // Add an extra newline between each paragraph (i.e., before each
    // paragraph after the first).
    if (i > 0) {
      extracted_text += "\n";
    }

    for (int j = 0; j < paragraph.lines().size(); j++) {
      const auto& line = paragraph.lines()[j];

      // Add a newline between each line (i.e., before each line after the
      // first).
      if (j > 0) {
        extracted_text += "\n";
      }

      for (const auto& word : line.words()) {
        extracted_text += word.plain_text();

        // Add the text separator if it exists.
        if (word.has_text_separator()) {
          extracted_text += word.text_separator();
        }
      }
    }
  }

  std::move(on_text_detection_complete_callback_)
      .Run(std::move(extracted_text));
}

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
  CHECK(optical_character_recognizer_);
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
    std::move(callback).Run(std::nullopt);
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
  ocr_service_initialized_callback_.Cancel();
  optical_character_recognizer_ = nullptr;
  pending_ocr_request_image_.reset();
  if (!pending_ocr_request_callback_.is_null()) {
    std::move(pending_ocr_request_callback_).Run(std::nullopt);
  }
}

void ChromeCaptureModeDelegate::GetPrimaryAccountAccessToken(
    base::RepeatingCallback<void(const std::string& access_token)> callback) {
  const user_manager::User* const active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  CHECK(active_user);

  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // TODO: crbug.com/399914333 - Determine error handling for the access
    // token.
    return;
  }

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kSupportContentOAuth2Scope);
  primary_account_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kConsumerName, identity_manager, scopes,
          base::BindOnce(
              &ChromeCaptureModeDelegate::PrimaryAccountAccessTokenAvailable,
              base::Unretained(this), std::move(callback)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

void ChromeCaptureModeDelegate::PrimaryAccountAccessTokenAvailable(
    base::RepeatingCallback<void(const std::string& access_token)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // Reset token fetcher for the next request.
  DCHECK(primary_account_token_fetcher_);
  primary_account_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
    return;
  }

  DCHECK(!access_token_info.token.empty());
  std::move(callback).Run(access_token_info.token);
}

void ChromeCaptureModeDelegate::OnAccessTokenAvailableForImageSearch(
    const gfx::Image& original_image,
    const bool is_standalone_session,
    const int request_id,
    const std::string& access_token) {
  // If the access token is empty, let the user know that an error has occurred.
  if (access_token.empty()) {
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
    return;
  }

  // Create the POST request and add the access token for authentication.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token.c_str()));

  gfx::Image image = original_image;
  if (NeedsDownscale(original_image)) {
    image = gfx::ResizedImageForMaxDimensions(
        original_image, lens::kMaxPixelsForImageSearch,
        lens::kMaxPixelsForImageSearch, lens::kMaxAreaForImageSearch);
  }

  TemplateURLRef::PostContent post_content;
  const user_manager::User* const active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  CHECK(active_user);

  // Get the search provider (must be Google) so we can get the base URL for
  // image search.
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);
  CHECK(search::DefaultSearchProviderIsGoogle(template_url_service));
  const TemplateURL* const default_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_provider);

  // Encode the image into the search args.
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());
  size_t encoded_size_bytes;
  EncodeImageIntoSearchArgs(image, encoded_size_bytes, search_args);

  search_args.processed_image_dimensions =
      base::NumberToString(image.Size().width()) + "," +
      base::NumberToString(image.Size().height());
  search_args.image_original_size = original_image.Size();

  // Create the search URL and encode the image data into `post_content`.
  GURL search_url(default_provider->image_url_ref().ReplaceSearchTerms(
      search_args, template_url_service->search_terms_data(), &post_content));

  // Append necessary parameters to the URL.
  // Entry point.
  std::string entry_point_value = is_standalone_session
                                      ? kQueryParamEntryPointValueLauncher
                                      : kQueryParamEntryPointValueScreenshot;
  search_url = net::AppendOrReplaceQueryParameter(
      search_url, kQueryParamEntryPointName, entry_point_value);

  // Client surface (e.g., Photos, YouTube, Chromnient, etc.).
  search_url = net::AppendOrReplaceQueryParameter(
      search_url, kQueryParamSurfaceName, kQueryParamSurfaceValue);

  // Viewport dimensions.
  search_url = net::AppendOrReplaceQueryParameter(
      search_url, kQueryParamViewportWidthName,
      base::NumberToString(ash::capture_mode::kSearchResultsPanelWebViewWidth));
  search_url = net::AppendOrReplaceQueryParameter(
      search_url, kQueryParamViewportHeightName,
      base::NumberToString(
          ash::capture_mode::kSearchResultsPanelWebViewHeight));

  // Start time.
  const std::string epoch_time =
      base::NumberToString(base::Time::Now().InMillisecondsSinceUnixEpoch());
  search_url = net::AppendOrReplaceQueryParameter(
      search_url, kQueryParamStartTimeName, epoch_time);

  resource_request->url = search_url;

  // Create a `SimpleURLLoader` to upload the image data and send the resource
  // request.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kTrafficAnnotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  simple_url_loader->AttachStringForUpload(post_content.second,
                                           post_content.first);
  uploads_in_progress_.insert(uploads_in_progress_.begin(),
                              std::move(simple_url_loader));

  if (!url_loader_factory_) {
    // Lazily create the URLLoaderFactory.
    url_loader_factory_ = GetSharedURLLoaderFactory();
    CHECK(url_loader_factory_);
  }

  simple_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(
          &ChromeCaptureModeDelegate::OnDispatchCompleteForImageSearch,
          weak_ptr_factory_.GetWeakPtr(), simple_url_loader_ptr->GetWeakPtr(),
          access_token, request_id));
}

void ChromeCaptureModeDelegate::OnAccessTokenAvailableForCopyText(
    const std::string vsr_id,
    const int request_id,
    const std::string& access_token) {
  // If the access token is empty, let the user know that an error has occurred.
  if (access_token.empty()) {
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
    return;
  }

  // Create a new GET request for the text metadata.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token.c_str()));

  // Add the VSR ID as a URL parameter so Lens knows which image search we are
  // trying to get the metadata for.
  GURL text_url(kLensWebQFMetadataURL);
  text_url = net::AppendOrReplaceQueryParameter(text_url, "vsrid", vsr_id);
  resource_request->url = text_url;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kTrafficAnnotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  uploads_in_progress_.insert(uploads_in_progress_.begin(),
                              std::move(simple_url_loader));

  simple_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ChromeCaptureModeDelegate::OnDispatchCompleteForCopyText,
                     weak_ptr_factory_.GetWeakPtr(),
                     simple_url_loader_ptr->GetWeakPtr(), access_token,
                     request_id));
}

void ChromeCaptureModeDelegate::OnDispatchCompleteForImageSearch(
    base::WeakPtr<const network::SimpleURLLoader> url_loader,
    const std::string& access_token,
    const int request_id,
    std::unique_ptr<std::string> response_body) {
  absl::Cleanup deferred_runner = [this, url_loader]() {
    uploads_in_progress_.remove_if(base::MatchesUniquePtr(url_loader.get()));
  };

  // If the given `request_id` does not match the current request id, we should
  // not run the callback, and just wait for the most recent request to resolve.
  if (request_id != lens_request_id_) {
    return;
  }

  const network::SimpleURLLoader* simple_url_loader = url_loader.get();
  CHECK(simple_url_loader);

  // We only consider the request a success if we both get a response and the
  // header is present, otherwise it's a failure.
  int response_code = kHttpPostFailNoConnection;
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();
  }

  // If the response code is not a success, return early and let the user know
  // an error has occurred.
  if (!network::IsSuccessfulStatus(response_code)) {
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
    return;
  }

  // Pass in an empty image, as the Lens Web API uses its own thumbnail from the
  // image we uploaded previously.
  const GURL final_url = simple_url_loader->GetFinalURL();
  if (on_search_url_fetched_callback_) {
    std::move(on_search_url_fetched_callback_).Run(final_url);
  }

  // Get the vsr ID from the redirect URL so it can be used again in the
  // /qfmetadata request.
  std::string vsr_id;
  if (!net::GetValueForKeyInQuery(final_url, "vsrid", &vsr_id)) {
    return;
  }

  // Get a new access token, as they are short lived and we don't want to risk
  // the original expiring.
  GetPrimaryAccountAccessToken(base::BindRepeating(
      &ChromeCaptureModeDelegate::OnAccessTokenAvailableForCopyText,
      weak_ptr_factory_.GetWeakPtr(), vsr_id, request_id));
}

void ChromeCaptureModeDelegate::OnDispatchCompleteForCopyText(
    base::WeakPtr<const network::SimpleURLLoader> url_loader,
    const std::string& access_token,
    const int request_id,
    std::unique_ptr<std::string> response_body) {
  absl::Cleanup deferred_runner = [this, url_loader]() {
    uploads_in_progress_.remove_if(base::MatchesUniquePtr(url_loader.get()));
  };

  // If the given `request_id` does not match the current request id, we should
  // not run the callback, and just wait for the most recent request to resolve.
  if (request_id != lens_request_id_) {
    return;
  }

  // If there is no response body, return early and let the user know an error
  // has occurred.
  if (!response_body) {
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
    return;
  }

  // Response body that has a form of JSON contains protection characters
  // against XSSI that have to be removed. See go/xssi.
  std::string json_data = std::move(*response_body);
  json_data =
      json_data.substr(std::min(json_data.find('\n'), json_data.size()));

  data_decoder::DataDecoder::ParseJsonIsolated(
      json_data, base::BindOnce(&ChromeCaptureModeDelegate::OnJsonParsed,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ChromeCaptureModeDelegate::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  std::string extracted_text;
  // Attempty to parse the JSON further to get the extracted text. If
  // unsuccessful, return early and let the user know an error has occurred.
  if (!ParseQueryFormulationMetadataResponse(result, extracted_text)) {
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
    return;
  }

  if (on_text_detection_complete_callback_) {
    std::move(on_text_detection_complete_callback_).Run(extracted_text);
  }
}
