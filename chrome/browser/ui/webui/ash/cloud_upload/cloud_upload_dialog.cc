// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/containers/enum_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/common/constants.h"
#include "net/base/url_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash::cloud_upload {
namespace {

namespace fm_tasks = file_manager::file_tasks;

using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::Service;

constexpr char kAndroidOneDriveAuthority[] =
    "com.microsoft.skydrive.content.StorageAccessProvider";
constexpr char kNotificationId[] = "cloud_upload_open_failure";

constexpr char kFileHandlerSelectionMetricName[] =
    "FileBrowser.OfficeFiles.Setup.FileHandlerSelection";

constexpr char kFirstTimeMicrosoft365AvailabilityMetric[] =
    "FileBrowser.OfficeFiles.Setup.FirstTimeMicrosoft365Availability";

// Records the file handler selected on the first page of Office setup.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OfficeSetupFileHandler {
  kGoogleDocs = 0,
  kGoogleSheets = 1,
  kGoogleSlides = 2,
  kMicrosoft365 = 3,
  kOtherLocalHandler = 4,
  kQuickOffice = 5,
  kMaxValue = kQuickOffice,
};

// Represents (as a bitmask) whether or not Microsoft 365 PWA and ODFS are set
// up. Used to record this state when setup is launched.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Microsoft365Availability {
  kPWA = 0,
  kODFS = 1,

  kMinValue = kPWA,
  kMaxValue = kODFS,
};

// Opens the file specified by |url| in a new tab. |url| must be a
// docs.google.com URL for an office file.
fm_tasks::OfficeDriveOpenErrors OpenDriveUrl(const GURL& url) {
  if (!url.is_valid()) {
    LOG(ERROR) << "Invalid URL";
    return fm_tasks::OfficeDriveOpenErrors::kInvalidAlternateUrl;
  }
  if (url.host() == "drive.google.com") {
    LOG(ERROR) << "URL was from drive.google.com";
    return fm_tasks::OfficeDriveOpenErrors::kDriveAlternateUrl;
  }
  if (url.host() != "docs.google.com") {
    LOG(ERROR) << "URL was not from docs.google.com";
    return fm_tasks::OfficeDriveOpenErrors::kUnexpectedAlternateUrl;
  }

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      net::AppendOrReplaceQueryParameter(url, "cros_files", "true"),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
  return fm_tasks::OfficeDriveOpenErrors::kSuccess;
}

// Logs UMA when the Drive task ends with an attempt to open a file.
void LogGoogleDriveOpenResultUMA(OfficeTaskResult success_task_result,
                                 fm_tasks::OfficeDriveOpenErrors open_result) {
  UMA_HISTOGRAM_ENUMERATION(fm_tasks::kDriveErrorMetricName, open_result);
  UMA_HISTOGRAM_ENUMERATION(
      kGoogleDriveTaskResultMetricName,
      open_result == fm_tasks::OfficeDriveOpenErrors::kSuccess
          ? success_task_result
          : OfficeTaskResult::kFailedToOpen);
}

// Open a hosted MS Office file e.g. .docx, from a url hosted in
// DriveFS. Check the file was successfully uploaded to DriveFS.
void OpenUploadedDriveUrl(const GURL& url, const OfficeTaskResult task_result) {
  // TODO(b/296950967): This function logs both open result and task result (but
  // only if open fails) metrics internally, pull them up to a higher level so
  // all the metrics are logged in one place.
  fm_tasks::OfficeDriveOpenErrors open_result = OpenDriveUrl(url);
  LogGoogleDriveOpenResultUMA(task_result, open_result);
}

// Open an already hosted MS Office file e.g. .docx, from a url hosted in
// DriveFS. Check there was no error retrieving the file's metadata.
void OnGoogleDriveGetMetadata(drive::FileError error,
                              drivefs::mojom::FileMetadataPtr metadata) {
  fm_tasks::OfficeDriveOpenErrors open_result =
      fm_tasks::OfficeDriveOpenErrors::kSuccess;
  if (error == drive::FILE_ERROR_OK) {
    GURL hosted_url(metadata->alternate_url);
    open_result = OpenDriveUrl(hosted_url);
  } else {
    LOG(ERROR) << "Drive metadata error: " << error;
    open_result = fm_tasks::OfficeDriveOpenErrors::kNoMetadata;
  }
  LogGoogleDriveOpenResultUMA(OfficeTaskResult::kOpened, open_result);
}

// Logs UMA when the OneDrive task ends with an attempt to open a file.
void LogOneDriveOpenResultUMA(OfficeTaskResult success_task_result,
                              OfficeOneDriveOpenErrors open_result) {
  UMA_HISTOGRAM_ENUMERATION(fm_tasks::kOneDriveErrorMetricName, open_result);
  UMA_HISTOGRAM_ENUMERATION(kOneDriveTaskResultMetricName,
                            open_result == OfficeOneDriveOpenErrors::kSuccess
                                ? success_task_result
                                : OfficeTaskResult::kFailedToOpen);
}

// Handle system error notification "Sign in" click.
void HandleSignInClick(Profile* profile, absl::optional<int> button_index) {
  // If the "Sign in" button was pressed, rather than a click to somewhere
  // else in the notification.
  if (button_index) {
    // TODO(b/282619291) decide what callback should be.
    // Request an ODFS mount which will trigger reauthentication.
    RequestODFSMount(profile, base::DoNothing());
  }
  NotificationDisplayService* notification_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  notification_service->Close(NotificationHandler::Type::TRANSIENT,
                              kNotificationId);
}

// TODO(b/288038136): Use a notification manager to handle error notifications.
// Show system error notification to communicate that their file can't be
// opened. If the user needs to reauthenticate to OneDrive, prompt the user to
// reauthenticate to ODFS via a "Sign in" button.
void ShowUnableToOpenNotification(Profile* profile,
                                  bool reauthentication_required = false) {
  std::string message = GetGenericErrorMessage();
  std::vector<message_center::ButtonInfo> notification_buttons;

  // Special case of |FILE_ERROR_ACCESS_DENIED| where the user needs to
  // reauthenticate to OneDrive.
  if (reauthentication_required) {
    message = GetReauthenticationRequiredMessage();
    //  Add "Sign in" button.
    notification_buttons.emplace_back(
        l10n_util::GetStringUTF16(IDS_OFFICE_NOTIFICATION_SIGN_IN_BUTTON));
  }

  auto notification = ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/kNotificationId,
      // TODO(b/242685536) Use "files" for multi-files when support for
      // multi-files is added.
      /*title=*/
      l10n_util::GetPluralStringFUTF16(IDS_OFFICE_UPLOAD_ERROR_CANT_OPEN_FILE,
                                       1),
      /*message=*/base::UTF8ToUTF16(message),
      /*display_source=*/
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES),
      /*origin_url=*/GURL(),
      /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleSignInClick, profile)),
      /*small_image=*/ash::kFolderIcon,
      /*warning_level=*/
      message_center::SystemNotificationWarningLevel::WARNING);

  notification->set_buttons(notification_buttons);
  notification->set_never_timeout(true);
  NotificationDisplayService* notification_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  notification_service->Display(NotificationHandler::Type::TRANSIENT,
                                *notification,
                                /*metadata=*/nullptr);
}

// Check if reauthentication to OneDrive is required from the ODFS metadata
// and show the reuathentication is required notification if true. Otherwise
// show the generic access error notification.
void OnGetReauthenticationRequired(
    Profile* profile,
    base::OnceCallback<void(OfficeOneDriveOpenErrors)> callback,
    base::expected<ODFSMetadata, base::File::Error> metadata) {
  bool reauthentication_required = false;
  if (metadata.has_value()) {
    reauthentication_required = metadata->reauthentication_required;
  } else {
    LOG(ERROR) << "Failed to get reauthentication required state: "
               << metadata.error();
  }
  ShowUnableToOpenNotification(profile, reauthentication_required);
  std::move(callback).Run(
      reauthentication_required
          ? OfficeOneDriveOpenErrors::kGetActionsReauthRequired
          : OfficeOneDriveOpenErrors::kGetActionsAccessDenied);
}

// Open file with |file_path| from ODFS |file_system|. Open in the OneDrive PWA
// without link capturing.
void OpenFileFromODFS(
    Profile* profile,
    file_system_provider::ProvidedFileSystemInterface* file_system,
    const base::FilePath& file_path,
    base::OnceCallback<void(OfficeOneDriveOpenErrors)> callback) {
  GetODFSEntryMetadata(
      file_system, file_path,
      base::BindOnce(
          [](Profile* profile,
             file_system_provider::ProvidedFileSystemInterface* file_system,
             base::OnceCallback<void(OfficeOneDriveOpenErrors)> callback,
             base::expected<ODFSEntryMetadata, base::File::Error> metadata) {
            if (!metadata.has_value()) {
              switch (metadata.error()) {
                case base::File::Error::FILE_ERROR_ACCESS_DENIED:
                  // Query authentication state to determine which error message
                  // to show.
                  GetODFSMetadata(file_system,
                                  base::BindOnce(&OnGetReauthenticationRequired,
                                                 profile, std::move(callback)));
                  break;
                default:
                  ShowUnableToOpenNotification(profile);
                  std::move(callback).Run(
                      OfficeOneDriveOpenErrors::kGetActionsGenericError);
                  break;
              }
              return;
            }
            if (!metadata->url) {
              ShowUnableToOpenNotification(profile);
              std::move(callback).Run(
                  OfficeOneDriveOpenErrors::kGetActionsNoUrl);
              return;
            }
            GURL url(*metadata->url);
            if (!url.is_valid()) {
              ShowUnableToOpenNotification(profile);
              std::move(callback).Run(
                  OfficeOneDriveOpenErrors::kGetActionsInvalidUrl);
              return;
            }
            auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
            proxy->LaunchAppWithUrl(web_app::kMicrosoft365AppId,
                                    /*event_flags=*/ui::EF_NONE, url,
                                    apps::LaunchSource::kFromFileManager,
                                    /*window_info=*/nullptr);
            std::move(callback).Run(OfficeOneDriveOpenErrors::kSuccess);
          },
          profile, file_system, std::move(callback)));
}

// Open office file using the ODFS |url|.
void OpenODFSUrl(Profile* profile,
                 const storage::FileSystemURL& url,
                 base::OnceCallback<void(OfficeOneDriveOpenErrors)> callback) {
  if (!url.is_valid()) {
    LOG(ERROR) << "Invalid uploaded file URL";
    std::move(callback).Run(OfficeOneDriveOpenErrors::kNoFileSystemURL);
    return;
  }
  ash::file_system_provider::util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    LOG(ERROR) << "Path not in FSP";
    std::move(callback).Run(OfficeOneDriveOpenErrors::kInvalidFileSystemURL);
    return;
  }
  OpenFileFromODFS(profile, parser.file_system(), parser.file_path(),
                   std::move(callback));
}

// Open office files from ODFS that were originally selected from Android
// OneDrive. First convert the |android_onedrive_urls| to ODFS file paths, then
// open them from ODFS in the MS 365 PWA.
void OpenAndroidOneDriveUrls(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& android_onedrive_urls,
    base::OnceCallback<void(OfficeOneDriveOpenErrors)> callback) {
  for (const auto& android_onedrive_url : android_onedrive_urls) {
    absl::optional<ODFSFileSystemAndPath> fs_and_path =
        AndroidOneDriveUrlToODFS(profile, android_onedrive_url);
    if (!fs_and_path.has_value()) {
      // TODO(b/269364287): Handle when Android OneDrive file can't be opened.
      LOG(ERROR) << "Android OneDrive Url cannot be converted to ODFS";
      std::move(callback).Run(
          OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
      return;
    }
    OpenFileFromODFS(profile, fs_and_path->file_system,
                     fs_and_path->file_path_within_odfs, std::move(callback));
  }
}

bool PathIsOnDriveFS(Profile* profile, const base::FilePath& file_path) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  base::FilePath relative_path;
  return integration_service->GetRelativeDrivePath(file_path, &relative_path);
}

bool HasFileWithExtensionFromSet(
    const std::vector<storage::FileSystemURL>& file_urls,
    const std::set<std::string>& extensions) {
  return base::ranges::any_of(file_urls, [&extensions](const auto& file_url) {
    return base::ranges::any_of(extensions, [&file_url](const auto& extension) {
      return file_url.path().MatchesExtension(extension);
    });
  });
}

bool HasWordFile(const std::vector<storage::FileSystemURL>& file_urls) {
  return HasFileWithExtensionFromSet(file_urls,
                                     fm_tasks::WordGroupExtensions());
}

bool HasExcelFile(const std::vector<storage::FileSystemURL>& file_urls) {
  return HasFileWithExtensionFromSet(file_urls,
                                     fm_tasks::ExcelGroupExtensions());
}

bool HasPowerPointFile(const std::vector<storage::FileSystemURL>& file_urls) {
  return HasFileWithExtensionFromSet(file_urls,
                                     fm_tasks::PowerPointGroupExtensions());
}

// This indicates we ran Office setup and set a preference, or the user had a
// pre-existing preference for these file types.
bool HaveExplicitFileHandlers(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls) {
  return base::ranges::all_of(file_urls, [profile](const auto& url) {
    return fm_tasks::HasExplicitDefaultFileHandler(profile,
                                                   url.path().FinalExtension());
  });
}

// This indicates we ran Office setup and set a preference, or the user had a
// pre-existing preference for these file types.
bool HaveExplicitFileHandlers(Profile* profile,
                              const std::set<std::string>& extensions) {
  return base::ranges::all_of(extensions, [profile](const auto& extension) {
    return fm_tasks::HasExplicitDefaultFileHandler(profile, extension);
  });
}

void RecordMicrosoft365Availability(const char* metric, Profile* profile) {
  base::EnumSet<Microsoft365Availability, Microsoft365Availability::kMinValue,
                Microsoft365Availability::kMaxValue>
      ms365_state;
  if (IsOfficeWebAppInstalled(profile)) {
    ms365_state.Put(Microsoft365Availability::kPWA);
  }
  if (IsODFSMounted(profile)) {
    ms365_state.Put(Microsoft365Availability::kODFS);
  }
  base::UmaHistogramExactLinear(
      metric, ms365_state.ToEnumBitmask(),
      decltype(ms365_state)::All().ToEnumBitmask() + 1);
}

}  // namespace

// static
// Creates an instance of CloudOpenTask that effectively owns itself by keeping
// a reference alive in the TaskFinished callback.
bool CloudOpenTask::Execute(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const CloudProvider cloud_provider,
    gfx::NativeWindow modal_parent) {
  scoped_refptr<CloudOpenTask> upload_task = WrapRefCounted(
      new CloudOpenTask(profile, file_urls, cloud_provider, modal_parent));
  // Keep `upload_task` alive until `TaskFinished` executes.
  bool status = upload_task->ExecuteInternal();
  return status;
}

CloudOpenTask::CloudOpenTask(Profile* profile,
                             std::vector<storage::FileSystemURL> file_urls,
                             const CloudProvider cloud_provider,
                             gfx::NativeWindow modal_parent)
    : profile_(profile),
      file_urls_(file_urls),
      cloud_provider_(cloud_provider),
      modal_parent_(modal_parent) {}

CloudOpenTask::~CloudOpenTask() = default;

// Runs setup if it's never been completed. Runs the fixup version of setup if
// there are any issues, e.g. ODFS is not mounted. Otherwise, attempts to move
// files to the correct cloud or open the files if they are already there.
bool CloudOpenTask::ExecuteInternal() {
  DCHECK(!file_urls_.empty());
  if (file_urls_.empty()) {
    return false;
  }

  // Run the setup flow if we don't have explicit default file handlers set for
  // these files in preferences. This indicates we haven't run setup, because
  // setup sets default handlers at the end. If the user has a default set for
  // another, non-office handler, then we won't get here except via the 'Open
  // With' menu. In that case we might need to run fixup or just open/move the
  // file, but without changing stored user file handler preferences.
  if (!HaveExplicitFileHandlers(profile_, file_urls_)) {
    RecordMicrosoft365Availability(kFirstTimeMicrosoft365AvailabilityMetric,
                                   profile_);
    return InitAndShowDialog(mojom::DialogPage::kFileHandlerDialog);
  }

  if (ShouldFixUpOffice(profile_, cloud_provider_)) {
    // TODO(cassycc): Use page specifically for fix up.
    return InitAndShowDialog(mojom::DialogPage::kOneDriveSetup);
  }
  OpenOrMoveFiles();
  return true;
}

// Opens office files if they are in the correct cloud already. Otherwise moves
// the files before opening.
void CloudOpenTask::OpenOrMoveFiles() {
  // Record the source volume type of the opened file.
  int source_type;
  if (UrlIsOnODFS(profile_, file_urls_.front())) {
    source_type = static_cast<int>(OfficeFilesSourceVolume::kMicrosoftOneDrive);
  } else {
    auto* volume_manager = file_manager::VolumeManager::Get(profile_);
    base::WeakPtr<file_manager::Volume> source =
        volume_manager->FindVolumeFromPath(file_urls_.front().path());
    if (source) {
      source_type = source->type();
    } else {
      source_type = static_cast<int>(OfficeFilesSourceVolume::kUnknown);
    }
  }
  if (cloud_provider_ == CloudProvider::kGoogleDrive) {
    UMA_HISTOGRAM_SPARSE(kDriveOpenSourceVolumeMetric, source_type);
  } else if (cloud_provider_ == CloudProvider::kOneDrive) {
    UMA_HISTOGRAM_SPARSE(kOneDriveOpenSourceVolumeMetric, source_type);
  }

  if (cloud_provider_ == CloudProvider::kGoogleDrive &&
      PathIsOnDriveFS(profile_, file_urls_.front().path())) {
    // The files are on Drive already.
    transfer_required_ = OfficeFilesTransferRequired::kNotRequired;
    UMA_HISTOGRAM_ENUMERATION(kDriveTransferRequiredMetric,
                              OfficeFilesTransferRequired::kNotRequired);
    OpenAlreadyHostedDriveUrls();
  } else if (cloud_provider_ == CloudProvider::kOneDrive &&
             UrlIsOnODFS(profile_, file_urls_.front())) {
    // The files are on OneDrive already, selected from ODFS.
    transfer_required_ = OfficeFilesTransferRequired::kNotRequired;
    UMA_HISTOGRAM_ENUMERATION(kOneDriveTransferRequiredMetric,
                              OfficeFilesTransferRequired::kNotRequired);
    OpenODFSUrls(OfficeTaskResult::kOpened);
  } else if (cloud_provider_ == CloudProvider::kOneDrive &&
             UrlIsOnAndroidOneDrive(profile_, file_urls_.front())) {
    // The files are on OneDrive already, selected from Android OneDrive.
    transfer_required_ = OfficeFilesTransferRequired::kNotRequired;
    UMA_HISTOGRAM_ENUMERATION(kOneDriveTransferRequiredMetric,
                              OfficeFilesTransferRequired::kNotRequired);
    OpenAndroidOneDriveUrlsIfAccountMatchedODFS(
        base::BindOnce(&LogOneDriveOpenResultUMA, OfficeTaskResult::kOpened));
  } else {
    // The files need to be moved.
    auto operation =
        GetUploadType(profile_, file_urls_.front()) == UploadType::kCopy
            ? OfficeFilesTransferRequired::kCopy
            : OfficeFilesTransferRequired::kMove;
    transfer_required_ = operation;
    switch (cloud_provider_) {
      case CloudProvider::kGoogleDrive:
        UMA_HISTOGRAM_ENUMERATION(kDriveTransferRequiredMetric, operation);
        break;
      case CloudProvider::kOneDrive:
        UMA_HISTOGRAM_ENUMERATION(kOneDriveTransferRequiredMetric, operation);
        break;
    }
    ConfirmMoveOrStartUpload();
  }
}

void CloudOpenTask::OpenAlreadyHostedDriveUrls() {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  base::FilePath relative_path;
  for (const auto& file_url : file_urls_) {
    if (integration_service->GetRelativeDrivePath(file_url.path(),
                                                  &relative_path)) {
      integration_service->GetDriveFsInterface()->GetMetadata(
          relative_path, base::BindOnce(&OnGoogleDriveGetMetadata));
    } else {
      LOG(ERROR) << "Unexpected error obtaining the relative path ";
    }
  }
}

void CloudOpenTask::OpenODFSUrls(const OfficeTaskResult task_result_uma) {
  for (const auto& file_url : file_urls_) {
    OpenODFSUrl(profile_, file_url,
                base::BindOnce(&LogOneDriveOpenResultUMA, task_result_uma));
  }
}

// Returns True if the confirmation dialog should be shown before uploading a
// file to a cloud location and opening it.
bool CloudOpenTask::ShouldShowConfirmationDialog() {
  bool force_show_confirmation_dialog = false;
  SourceType source_type = GetSourceType(profile_, file_urls_[0]);

  if (cloud_provider_ == CloudProvider::kGoogleDrive) {
    switch (source_type) {
      case SourceType::READ_ONLY:
        force_show_confirmation_dialog =
            !fm_tasks::GetOfficeMoveConfirmationShownForLocalToDrive(
                profile_) &&
            !fm_tasks::GetOfficeMoveConfirmationShownForCloudToDrive(profile_);
        break;
      case SourceType::LOCAL:
        force_show_confirmation_dialog =
            !fm_tasks::GetOfficeMoveConfirmationShownForLocalToDrive(profile_);
        break;
      case SourceType::CLOUD:
        force_show_confirmation_dialog =
            !fm_tasks::GetOfficeMoveConfirmationShownForCloudToDrive(profile_);
        break;
    }
    return force_show_confirmation_dialog ||
           !fm_tasks::GetAlwaysMoveOfficeFilesToDrive(profile_);
  } else if (cloud_provider_ == CloudProvider::kOneDrive) {
    switch (source_type) {
      case SourceType::READ_ONLY:
        force_show_confirmation_dialog =
            !fm_tasks::GetOfficeMoveConfirmationShownForLocalToOneDrive(
                profile_) &&
            !fm_tasks::GetOfficeMoveConfirmationShownForCloudToOneDrive(
                profile_);
        break;
      case SourceType::LOCAL:
        force_show_confirmation_dialog =
            !fm_tasks::GetOfficeMoveConfirmationShownForLocalToOneDrive(
                profile_);
        break;
      case SourceType::CLOUD:
        force_show_confirmation_dialog =
            !fm_tasks::GetOfficeMoveConfirmationShownForCloudToOneDrive(
                profile_);
        break;
    }
    return force_show_confirmation_dialog ||
           !fm_tasks::GetAlwaysMoveOfficeFilesToOneDrive(profile_);
  }
  NOTREACHED();
  return true;
}

void CloudOpenTask::ConfirmMoveOrStartUpload() {
  bool show_confirmation_dialog = ShouldShowConfirmationDialog();
  if (show_confirmation_dialog) {
    mojom::DialogPage dialog_page =
        cloud_provider_ == CloudProvider::kGoogleDrive
            ? mojom::DialogPage::kMoveConfirmationGoogleDrive
            : mojom::DialogPage::kMoveConfirmationOneDrive;
    InitAndShowDialog(dialog_page);
  } else {
    StartUpload();
  }
}

bool ShouldFixUpOffice(Profile* profile, const CloudProvider cloud_provider) {
  return cloud_provider == CloudProvider::kOneDrive &&
         !(IsODFSMounted(profile) && IsOfficeWebAppInstalled(profile));
}

bool UrlIsOnODFS(Profile* profile, const FileSystemURL& url) {
  ash::file_system_provider::util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    return false;
  }

  file_system_provider::ProviderId provider_id =
      file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId);
  if (parser.file_system()->GetFileSystemInfo().provider_id() != provider_id) {
    return false;
  }
  return true;
}

bool UrlIsOnAndroidOneDrive(Profile* profile, const FileSystemURL& url) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;
  return arc::ParseDocumentsProviderUrl(url, &authority, &root_document_id,
                                        &path) &&
         authority == kAndroidOneDriveAuthority;
}

absl::optional<std::string> GetEmailFromAndroidOneDriveRootDoc(
    const std::string& root_document_id) {
  // After escaping the '/', the Root Document Id is:
  // pivots%2F<user-microsoft-account-email>.
  // Convert back to:
  // pivots/<user-microsoft-account-email>
  std::string root_document_id_unescaped = base::UnescapeURLComponent(
      root_document_id, base::UnescapeRule::PATH_SEPARATORS);
  std::vector<base::FilePath::StringType> components =
      base::FilePath(root_document_id_unescaped).GetComponents();
  if (components.size() != 2) {
    LOG(ERROR) << "Android OneDrive documents provider root document id is not "
                  "as expected.";
    return absl::nullopt;
  }
  if (components[0] != "pivots") {
    LOG(ERROR) << "Android OneDrive documents provider root document id is not "
                  "as expected.";
    return absl::nullopt;
  }
  return components[1];
}

void CloudOpenTask::OpenAndroidOneDriveUrlsIfAccountMatchedODFS(
    base::OnceCallback<void(OfficeOneDriveOpenErrors)> callback) {
  // Get email account associated with Android OneDrive.
  std::string authority;
  std::string root_document_id;
  base::FilePath path;
  if (!arc::ParseDocumentsProviderUrl(file_urls_.front(), &authority,
                                      &root_document_id, &path)) {
    std::move(callback).Run(OfficeOneDriveOpenErrors::kInvalidFileSystemURL);
    return;
  }

  absl::optional<std::string> android_onedrive_email =
      GetEmailFromAndroidOneDriveRootDoc(root_document_id);
  if (!android_onedrive_email.has_value()) {
    std::move(callback).Run(
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
    return;
  }

  // Get email account associated with ODFS.
  absl::optional<ODFSFileSystemAndPath> fs_and_path =
      AndroidOneDriveUrlToODFS(profile_, file_urls_.front());
  if (!fs_and_path.has_value()) {
    // TODO(b/269364287): Handle when Android OneDrive file can't be opened.
    LOG(ERROR) << "Android OneDrive Url cannot be converted to ODFS";
    std::move(callback).Run(
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
    return;
  }
  GetODFSMetadata(
      fs_and_path->file_system,
      base::BindOnce(&CloudOpenTask::CheckEmailAndOpenURLs, this,
                     android_onedrive_email.value(), std::move(callback)));
}

absl::optional<ODFSFileSystemAndPath> AndroidOneDriveUrlToODFS(
    Profile* profile,
    const FileSystemURL& android_onedrive_file_url) {
  if (!UrlIsOnAndroidOneDrive(profile, android_onedrive_file_url)) {
    LOG(ERROR) << "File not on Android OneDrive";
    return absl::nullopt;
  }

  // Get the ODFS mount path.
  absl::optional<ProvidedFileSystemInfo> odfs_file_system_info =
      GetODFSInfo(profile);
  if (!odfs_file_system_info.has_value()) {
    return absl::nullopt;
  }
  base::FilePath odfs_path = odfs_file_system_info->mount_path();

  // Find the relative path from Android OneDrive Url.
  std::string authority;
  std::string root_document_id;
  base::FilePath path;
  if (!arc::ParseDocumentsProviderUrl(android_onedrive_file_url, &authority,
                                      &root_document_id, &path)) {
    return absl::nullopt;
  }
  // Format for Android OneDrive documents provider `path` is:
  // Files/<rel_path>
  std::vector<base::FilePath::StringType> components =
      base::FilePath(path.value()).GetComponents();
  if (components.size() < 2) {
    LOG(ERROR)
        << "Android OneDrive documents provider path is not as expected.";
    return absl::nullopt;
  }
  if (components[0] != "Files") {
    LOG(ERROR)
        << "Android OneDrive documents provider path is not as expected.";
    return absl::nullopt;
  }
  // Append relative path from Android OneDrive Url.
  for (size_t i = 1; i < components.size(); i++) {
    odfs_path = odfs_path.Append(components[i]);
  }

  ash::file_system_provider::util::LocalPathParser parser(profile, odfs_path);
  if (!parser.Parse()) {
    LOG(ERROR) << "Path not in FSP";
    return absl::nullopt;
  }
  return ODFSFileSystemAndPath{parser.file_system(), parser.file_path()};
}

void CloudOpenTask::CheckEmailAndOpenURLs(
    const std::string& android_onedrive_email,
    base::OnceCallback<void(OfficeOneDriveOpenErrors)> callback,
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error) {
  if (!metadata_or_error.has_value()) {
    LOG(ERROR) << "Failed to get user email: " << metadata_or_error.error();
    std::move(callback).Run(OfficeOneDriveOpenErrors::kGetActionsGenericError);
    return;
  }
  if (metadata_or_error->user_email.empty()) {
    LOG(ERROR) << "User email is empty";
    std::move(callback).Run(OfficeOneDriveOpenErrors::kGetActionsNoEmail);
    return;
  }
  // Query whether the account logged into Android OneDrive is the
  // same as ODFS.
  if (android_onedrive_email == metadata_or_error->user_email) {
    OpenAndroidOneDriveUrls(profile_, file_urls_, std::move(callback));
  } else {
    LOG(ERROR) << "Email accounts associated with ODFS and "
                  "Android OneDrive don't match.";
    std::move(callback).Run(OfficeOneDriveOpenErrors::kEmailsDoNotMatch);
  }
}

void CloudOpenTask::StartUpload() {
  DCHECK_EQ(pending_uploads_, 0UL);
  pending_uploads_ = file_urls_.size();
  upload_timer_ = base::ElapsedTimer();

  if (cloud_provider_ == CloudProvider::kGoogleDrive) {
    for (const auto& file_url : file_urls_) {
      DriveUploadHandler::Upload(
          profile_, file_url,
          base::BindOnce(&CloudOpenTask::FinishedDriveUpload, this));
    }
  } else if (cloud_provider_ == CloudProvider::kOneDrive) {
    for (const auto& file_url : file_urls_) {
      OneDriveUploadHandler::Upload(
          profile_, file_url,
          base::BindOnce(&CloudOpenTask::FinishedOneDriveUpload, this,
                         profile_->GetWeakPtr()));
    }
  }
}

void CloudOpenTask::FinishedDriveUpload(absl::optional<GURL> url,
                                        int64_t size) {
  DCHECK_GT(pending_uploads_, 0UL);
  if (url.has_value()) {
    upload_total_size_ += size;
    fm_tasks::SetOfficeFileMovedToGoogleDrive(profile_, base::Time::Now());
    // Open the URL.
    const OfficeTaskResult task_result_uma =
        transfer_required_ == OfficeFilesTransferRequired::kCopy
            ? OfficeTaskResult::kCopied
            : OfficeTaskResult::kMoved;
    OpenUploadedDriveUrl(url.value(), task_result_uma);
  } else {
    has_upload_errors_ = true;
    UMA_HISTOGRAM_ENUMERATION(kGoogleDriveTaskResultMetricName,
                              OfficeTaskResult::kFailedToUpload);
  }
  if (--pending_uploads_) {
    return;
  }
  if (!has_upload_errors_) {
    RecordUploadLatencyUMA();
  }
}

void CloudOpenTask::FinishedOneDriveUpload(
    base::WeakPtr<Profile> profile_weak_ptr,
    absl::optional<storage::FileSystemURL> url,
    int64_t size) {
  DCHECK_GT(pending_uploads_, 0UL);
  if (url.has_value()) {
    upload_total_size_ += size;
    Profile* profile = profile_weak_ptr.get();
    if (!profile) {
      // TODO(b/296950967): metric to log here?
      return;
    }
    fm_tasks::SetOfficeFileMovedToOneDrive(profile, base::Time::Now());
    const OfficeTaskResult task_result_uma =
        transfer_required_ == OfficeFilesTransferRequired::kCopy
            ? OfficeTaskResult::kCopied
            : OfficeTaskResult::kMoved;
    OpenODFSUrl(profile, url.value(),
                base::BindOnce(&LogOneDriveOpenResultUMA, task_result_uma));
  } else {
    has_upload_errors_ = true;
    UMA_HISTOGRAM_ENUMERATION(kOneDriveTaskResultMetricName,
                              OfficeTaskResult::kFailedToUpload);
  }
  if (--pending_uploads_) {
    return;
  }
  if (!has_upload_errors_) {
    RecordUploadLatencyUMA();
  }
}

void CloudOpenTask::RecordUploadLatencyUMA() {
  constexpr int64_t kMegabyte = 1000 * 1000;
  std::string uma_size;
  if (upload_total_size_ > 1000 * kMegabyte) {
    uma_size = "1000MB-and-above";
  } else if (upload_total_size_ > 100 * kMegabyte) {
    uma_size = "0100MB-to-1GB";
  } else if (upload_total_size_ > 10 * kMegabyte) {
    uma_size = "0010MB-to-100MB";
  } else if (upload_total_size_ > 1 * kMegabyte) {
    uma_size = "0001MB-to-10MB";
  } else if (upload_total_size_ <= 1 * kMegabyte) {
    uma_size = "0000MB-to-1MB";
  }
  auto* transfer =
      (transfer_required_ == OfficeFilesTransferRequired::kCopy ? "Copy"
                                                                : "Move");
  auto* provider =
      (cloud_provider_ == CloudProvider::kGoogleDrive ? "GoogleDrive"
                                                      : "OneDrive");
  const auto metric = base::StrCat({"FileBrowser.OfficeFiles.FileOpen.Time.",
                                    transfer, ".", uma_size, ".To.", provider});
  base::UmaHistogramMediumTimes(metric, upload_timer_.Elapsed());
}

// Create the arguments necessary for showing the dialog. We first need to
// collect local file tasks, if we are trying to show the kFileHandlerDialog
// page.
bool CloudOpenTask::InitAndShowDialog(mojom::DialogPage dialog_page) {
  // Allow no more than one upload dialog at a time. In the case of multiple
  // upload requests, they should either be handled simultaneously or queued.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUICloudUploadURL))) {
    return false;
  }

  mojom::DialogArgsPtr args = CreateDialogArgs(dialog_page);

  // Display local file handlers (tasks) only for the file handler dialog.
  if (dialog_page == mojom::DialogPage::kFileHandlerDialog) {
    // Callback to show the dialog after the tasks have been found.
    fm_tasks::FindTasksCallback find_all_types_of_tasks_callback =
        base::BindOnce(IgnoreResult(&CloudOpenTask::ShowDialog), this,
                       std::move(args), dialog_page);
    // Find the file tasks that can open the `file_urls_` and then run
    // `ShowDialog`.
    FindTasksForDialog(std::move(find_all_types_of_tasks_callback));
  } else {
    ShowDialog(std::move(args), dialog_page, nullptr);
  }
  return true;
}

mojom::DialogArgsPtr CloudOpenTask::CreateDialogArgs(
    mojom::DialogPage dialog_page) {
  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  for (const auto& file_url : file_urls_) {
    args->file_names.push_back(file_url.path().BaseName().value());
  }
  args->dialog_page = dialog_page;
  args->set_office_as_default_handler =
      !HaveExplicitFileHandlers(profile_, file_urls_);
  const UploadType upload_type = GetUploadType(profile_, file_urls_[0]);
  switch (upload_type) {
    case UploadType::kMove:
      args->operation_type = mojom::OperationType::kMove;
      break;
    case UploadType::kCopy:
      args->operation_type = mojom::OperationType::kCopy;
      break;
  }
  return args;
}

// Creates and shows a new dialog for the cloud upload workflow. If there are
// local file tasks from `resulting_tasks`, include them in the dialog
// arguments. These tasks are can be selected by the user to open the files
// instead of using a cloud provider. If no modal_parent was provided, first
// launches a new Files app window, which we listen for in OnBrowserAdded().
void CloudOpenTask::ShowDialog(
    mojom::DialogArgsPtr args,
    const mojom::DialogPage dialog_page,
    std::unique_ptr<fm_tasks::ResultingTasks> resulting_tasks) {
  if (resulting_tasks) {
    SetTaskArgs(args, std::move(resulting_tasks));
  }

  bool office_move_confirmation_shown =
      cloud_provider_ == CloudProvider::kGoogleDrive
          ? fm_tasks::GetOfficeMoveConfirmationShownForDrive(profile_)
          : fm_tasks::GetOfficeMoveConfirmationShownForOneDrive(profile_);
  // This CloudUploadDialog pointer is managed by an instance of
  // `views::WebDialogView` and deleted in
  // `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog = new CloudUploadDialog(
      std::move(args), base::BindOnce(&CloudOpenTask::OnDialogComplete, this),
      dialog_page, office_move_confirmation_shown);

  if (!modal_parent_) {
    BrowserList::AddObserver(this);
    DCHECK(!pending_dialog_);
    pending_dialog_ = dialog;
    // Create a files app window and use it as the modal parent. CloudOpenTask
    // is kept alive by the callback passed to CloudUploadDialog above. We
    // expect this to trigger OnBrowserAdded, which then shows the dialog.
    file_manager::util::ShowItemInFolder(profile_, file_urls_.at(0).path(),
                                         base::DoNothing());
  } else {
    dialog->ShowSystemDialog(modal_parent_);
  }
}

// Stores constructed tasks into `args->tasks` and `local_tasks_`.
void CloudOpenTask::SetTaskArgs(
    mojom::DialogArgsPtr& args,
    std::unique_ptr<fm_tasks::ResultingTasks> resulting_tasks) {
  int nextPosition = 0;
  for (fm_tasks::FullTaskDescriptor& task : resulting_tasks->tasks) {
    // Ignore Google Docs and MS Office tasks as they are already
    // set up to show in the dialog.
    if (fm_tasks::IsWebDriveOfficeTask(task.task_descriptor) ||
        fm_tasks::IsOpenInOfficeTask(task.task_descriptor)) {
      continue;
    }
    mojom::DialogTaskPtr dialog_task = mojom::DialogTask::New();
    // The (unique and positive) `position` of the task in the `tasks` vector.
    // If the user responds with the `position`, the task will be launched via
    // `LaunchLocalFileTask()`.
    dialog_task->position = nextPosition++;
    dialog_task->title = task.task_title;
    dialog_task->icon_url = task.icon_url.spec();
    dialog_task->app_id = task.task_descriptor.app_id;

    args->local_tasks.push_back(std::move(dialog_task));
    local_tasks_.push_back(std::move(task.task_descriptor));
  }
}

void CloudOpenTask::OnBrowserAdded(Browser* browser) {
  // TODO(petermarshall): Add a timeout. If Files app never launches for some
  // reason, then we will never show the dialog.
  DCHECK(pending_dialog_);
  if (!IsBrowserForSystemWebApp(browser, SystemWebAppType::FILE_MANAGER)) {
    // Wait for Files app to launch.
    LOG(WARNING) << "Browser did not match Files app";
    return;
  }
  BrowserList::RemoveObserver(this);

  modal_parent_ = browser->window()->GetNativeWindow();
  pending_dialog_->ShowSystemDialog(modal_parent_);
  // The dialog is deleted in `SystemWebDialogDelegate::OnDialogClosed`.
  pending_dialog_ = nullptr;
}

// Receive user's dialog response and acts accordingly. `user_response` is
// either an ash::cloud_upload::mojom::UserAction or the id (position) of the
// task in `local_tasks_` to launch. We never use the return value but it's
// necessary to make sure that we delete CloudOpenTask when we're done.
void CloudOpenTask::OnDialogComplete(const std::string& user_response) {
  // TODO(petermarshall): Don't need separate actions for drive/onedrive now
  // (and for StartUpload?).
  if (user_response == kUserActionConfirmOrUploadToGoogleDrive) {
    cloud_provider_ = CloudProvider::kGoogleDrive;

    // Because we treat Docs/Sheets/Slides as three separate apps, only set
    // the default handler for the types that we are dealing with.
    // We don't currently check MIME types, which could mean we get into edge
    // cases if the MIME type doesn't match the file extension.
    if (HasWordFile(file_urls_)) {
      UMA_HISTOGRAM_ENUMERATION(kFileHandlerSelectionMetricName,
                                OfficeSetupFileHandler::kGoogleDocs);
      fm_tasks::SetWordFileHandlerToFilesSWA(
          profile_, fm_tasks::kActionIdWebDriveOfficeWord);
    }
    if (HasExcelFile(file_urls_)) {
      UMA_HISTOGRAM_ENUMERATION(kFileHandlerSelectionMetricName,
                                OfficeSetupFileHandler::kGoogleSheets);
      fm_tasks::SetExcelFileHandlerToFilesSWA(
          profile_, fm_tasks::kActionIdWebDriveOfficeExcel);
    }
    if (HasPowerPointFile(file_urls_)) {
      UMA_HISTOGRAM_ENUMERATION(kFileHandlerSelectionMetricName,
                                OfficeSetupFileHandler::kGoogleSlides);
      fm_tasks::SetPowerPointFileHandlerToFilesSWA(
          profile_, fm_tasks::kActionIdWebDriveOfficePowerPoint);
    }
    OpenOrMoveFiles();
  } else if (user_response == kUserActionConfirmOrUploadToOneDrive) {
    // Default handlers have already been set by this point for
    // Office/OneDrive.
    OpenOrMoveFiles();
  } else if (user_response == kUserActionUploadToGoogleDrive) {
    cloud_provider_ = CloudProvider::kGoogleDrive;
    fm_tasks::SetOfficeMoveConfirmationShownForDrive(profile_, true);
    SourceType source_type = GetSourceType(profile_, file_urls_[0]);
    switch (source_type) {
      case SourceType::LOCAL:
        fm_tasks::SetOfficeMoveConfirmationShownForLocalToDrive(profile_, true);
        break;
      case SourceType::CLOUD:
        fm_tasks::SetOfficeMoveConfirmationShownForCloudToDrive(profile_, true);
        break;
      case SourceType::READ_ONLY:
        // TODO (jboulic): Clarify UX.
        break;
    }
    StartUpload();
  } else if (user_response == kUserActionUploadToOneDrive) {
    fm_tasks::SetOfficeMoveConfirmationShownForOneDrive(profile_, true);
    SourceType source_type = GetSourceType(profile_, file_urls_[0]);
    switch (source_type) {
      case SourceType::LOCAL:
        fm_tasks::SetOfficeMoveConfirmationShownForLocalToOneDrive(profile_,
                                                                   true);
        break;
      case SourceType::CLOUD:
        fm_tasks::SetOfficeMoveConfirmationShownForCloudToOneDrive(profile_,
                                                                   true);
        break;
      case SourceType::READ_ONLY:
        // TODO (jboulic): Clarify UX.
        break;
    }
    StartUpload();
  } else if (user_response == kUserActionSetUpOneDrive) {
    UMA_HISTOGRAM_ENUMERATION(kFileHandlerSelectionMetricName,
                              OfficeSetupFileHandler::kMicrosoft365);
    cloud_provider_ = CloudProvider::kOneDrive;
    InitAndShowDialog(mojom::DialogPage::kOneDriveSetup);
  } else if (user_response == kUserActionCancel) {
    // Do nothing.
  } else if (user_response == kUserActionCancelGoogleDrive) {
    UMA_HISTOGRAM_ENUMERATION(kGoogleDriveTaskResultMetricName,
                              OfficeTaskResult::kCancelledAtConfirmation);
  } else if (user_response == kUserActionCancelOneDrive) {
    UMA_HISTOGRAM_ENUMERATION(kOneDriveTaskResultMetricName,
                              OfficeTaskResult::kCancelledAtConfirmation);
  } else {
    LaunchLocalFileTask(user_response);
  }
}

// Launch the local file task in `local_tasks_` with the position specified by
// `string_task_position`.
void CloudOpenTask::LaunchLocalFileTask(
    const std::string& string_task_position) {
  // Convert the `string_task_position` - the string of the task position in
  // `local_tasks_` - to an int. Ensure that it is within the range of
  // `local_tasks_`.
  int task_position;
  if (!base::StringToInt(string_task_position, &task_position) ||
      task_position < 0 ||
      static_cast<size_t>(task_position) >= local_tasks_.size()) {
    LOG(ERROR) << "Position for local file task is unexpectedly unable to be "
                  "retrieved. Retrieved position: "
               << string_task_position
               << " from user response: " << string_task_position;
    return;
  }
  // Launch the task.
  fm_tasks::TaskDescriptor& task = local_tasks_[task_position];
  UMA_HISTOGRAM_ENUMERATION(kFileHandlerSelectionMetricName,
                            extension_misc::IsQuickOfficeExtension(task.app_id)
                                ? OfficeSetupFileHandler::kQuickOffice
                                : OfficeSetupFileHandler::kOtherLocalHandler);
  fm_tasks::ExecuteFileTask(
      profile_, task, file_urls_, nullptr,
      base::BindOnce(&CloudOpenTask::LocalTaskExecuted, this, task));
}

// We never use the return value but it's necessary to make sure that we delete
// CloudOpenTask when we're done.
void CloudOpenTask::LocalTaskExecuted(
    const fm_tasks::TaskDescriptor& task,
    extensions::api::file_manager_private::TaskResult result,
    std::string error_message) {
  if (!error_message.empty()) {
    LOG(ERROR) << "Execution of local file task with app id " << task.app_id
               << " to open office files. Led to error message: "
               << error_message << " and result: " << result;
    return;
  }

  if (HasWordFile(file_urls_)) {
    fm_tasks::SetWordFileHandler(profile_, task);
  }
  if (HasExcelFile(file_urls_)) {
    fm_tasks::SetExcelFileHandler(profile_, task);
  }
  if (HasPowerPointFile(file_urls_)) {
    fm_tasks::SetPowerPointFileHandler(profile_, task);
  }
}

// Find the file tasks that can open the `file_urls` and pass them to the
// `find_all_types_of_tasks_callback`.
void CloudOpenTask::FindTasksForDialog(
    fm_tasks::FindTasksCallback find_all_types_of_tasks_callback) {
  using extensions::app_file_handler_util::MimeTypeCollector;
  // Get the file info for finding the tasks.
  std::vector<base::FilePath> local_paths;
  std::vector<GURL> gurls;
  for (const auto& file_url : file_urls_) {
    local_paths.push_back(file_url.path());
    gurls.push_back(file_url.ToGURL());
  }

  // Get the mime types of the files and then pass them to the callback to
  // get the entries.
  std::unique_ptr<MimeTypeCollector> mime_collector =
      std::make_unique<MimeTypeCollector>(profile_);
  auto* mime_collector_ptr = mime_collector.get();
  mime_collector_ptr->CollectForLocalPaths(
      local_paths,
      base::BindOnce(&CloudOpenTask::ConstructEntriesAndFindTasks, this,
                     local_paths, gurls, std::move(mime_collector),
                     std::move(find_all_types_of_tasks_callback)));
}

void CloudOpenTask::ConstructEntriesAndFindTasks(
    const std::vector<base::FilePath>& file_paths,
    const std::vector<GURL>& gurls,
    std::unique_ptr<extensions::app_file_handler_util::MimeTypeCollector>
        mime_collector,
    fm_tasks::FindTasksCallback find_all_types_of_tasks_callback,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  std::vector<extensions::EntryInfo> entries;
  DCHECK_EQ(file_paths.size(), mime_types->size());
  for (size_t i = 0; i < file_paths.size(); ++i) {
    entries.emplace_back(file_paths[i], (*mime_types)[i], false);
  }

  const std::vector<std::string> dlp_source_urls(entries.size(), "");
  fm_tasks::FindAllTypesOfTasks(profile_, entries, gurls, dlp_source_urls,
                                std::move(find_all_types_of_tasks_callback));
}

void CloudOpenTask::SetTasksForTest(
    const std::vector<fm_tasks::TaskDescriptor>& tasks) {
  local_tasks_ = tasks;
}

void CloudUploadDialog::OnDialogShown(content::WebUI* webui) {
  DCHECK(dialog_args_);
  SystemWebDialogDelegate::OnDialogShown(webui);
  static_cast<CloudUploadUI*>(webui->GetController())
      ->SetDialogArgs(std::move(dialog_args_));
}

void CloudUploadDialog::OnDialogClosed(const std::string& json_retval) {
  UploadRequestCallback callback = std::move(callback_);
  // Deletes this, so we store the `callback` first.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
  // The callback can create a new dialog. It must be called last because we
  // can only have one of these dialogs at a time.
  if (callback) {
    std::move(callback).Run(json_retval);
  }
}

CloudUploadDialog::CloudUploadDialog(mojom::DialogArgsPtr args,
                                     UploadRequestCallback callback,
                                     const mojom::DialogPage dialog_page,
                                     bool office_move_confirmation_shown)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICloudUploadURL),
                              std::u16string() /* title */),
      dialog_args_(std::move(args)),
      callback_(std::move(callback)),
      dialog_page_(dialog_page),
      num_local_tasks_(dialog_args_->local_tasks.size()),
      office_move_confirmation_shown_(office_move_confirmation_shown) {}

CloudUploadDialog::~CloudUploadDialog() = default;

ui::ModalType CloudUploadDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool CloudUploadDialog::ShouldCloseDialogOnEscape() const {
  // The One Drive setup dialog handles escape in the webui as it needs to
  // display a confirmation dialog on cancellation.
  return dialog_page_ != mojom::DialogPage::kOneDriveSetup;
}

bool CloudUploadDialog::ShouldShowCloseButton() const {
  return false;
}

namespace {
constexpr int kDialogWidthForOneDriveSetup = 512;
constexpr int kDialogHeightForOneDriveSetup = 556;

constexpr int kDialogWidthForFileHandlerDialog = 512;
constexpr int kDialogHeightForFileHandlerDialog = 379;
constexpr int kDialogHeightForFileHandlerDialogNoLocalApp = 315;

constexpr int kDialogWidthForMoveConfirmation = 512;
constexpr int kDialogHeightForMoveConfirmationWithCheckbox = 524;

constexpr int kDialogHeightForMoveConfirmationWithoutCheckbox = 472;

constexpr int kDialogWidthForConnectToOneDrive = 512;
constexpr int kDialogHeightForConnectToOneDrive = 556;
}  // namespace

void CloudUploadDialog::GetDialogSize(gfx::Size* size) const {
  switch (dialog_page_) {
    case mojom::DialogPage::kFileHandlerDialog: {
      size->set_width(kDialogWidthForFileHandlerDialog);
      size->set_height(num_local_tasks_ == 0
                           ? kDialogHeightForFileHandlerDialogNoLocalApp
                           : kDialogHeightForFileHandlerDialog);
      return;
    }
    case mojom::DialogPage::kOneDriveSetup: {
      size->set_width(kDialogWidthForOneDriveSetup);
      size->set_height(kDialogHeightForOneDriveSetup);
      return;
    }
    case mojom::DialogPage::kMoveConfirmationGoogleDrive:
    case mojom::DialogPage::kMoveConfirmationOneDrive: {
      size->set_width(kDialogWidthForMoveConfirmation);
      if (office_move_confirmation_shown_) {
        size->set_height(kDialogHeightForMoveConfirmationWithCheckbox);
      } else {
        size->set_height(kDialogHeightForMoveConfirmationWithoutCheckbox);
      }
      return;
    }
    case mojom::DialogPage::kConnectToOneDrive: {
      size->set_width(kDialogWidthForConnectToOneDrive);
      size->set_height(kDialogHeightForConnectToOneDrive);
    }
  }
}

bool ShowConnectOneDriveDialog(gfx::NativeWindow modal_parent) {
  // Allow no more than one upload dialog at a time. Only one of either this
  // dialog, or CloudOpenTask can be shown at a time because they use the same
  // WebUI for dialogs.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUICloudUploadURL))) {
    return false;
  }

  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  args->dialog_page = mojom::DialogPage::kConnectToOneDrive;

  // This CloudUploadDialog pointer is managed by an instance of
  // `views::WebDialogView` and deleted in
  // `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog = new CloudUploadDialog(
      std::move(args), base::DoNothing(), mojom::DialogPage::kConnectToOneDrive,
      /*office_move_confirmation_shown=*/false);

  dialog->ShowSystemDialog(modal_parent);
  return true;
}

void LaunchMicrosoft365Setup(Profile* profile, gfx::NativeWindow modal_parent) {
  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  args->dialog_page = mojom::DialogPage::kOneDriveSetup;

  // If `set_office_as_default_handler` is false, it indicates that we already
  // ran the Office setup and set file handler preferences for all handled
  // Office file types, or that the user has pre-existing preferences for these
  // file types.
  args->set_office_as_default_handler =
      !HaveExplicitFileHandlers(profile, fm_tasks::WordGroupExtensions()) ||
      !HaveExplicitFileHandlers(profile, fm_tasks::ExcelGroupExtensions()) ||
      !HaveExplicitFileHandlers(profile, fm_tasks::PowerPointGroupExtensions());

  // This CloudUploadDialog pointer is managed by an instance of
  // `views::WebDialogView` and deleted in
  // `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog = new CloudUploadDialog(
      std::move(args), base::DoNothing(), mojom::DialogPage::kOneDriveSetup,
      /*office_move_confirmation_shown=*/false);

  dialog->ShowSystemDialog(modal_parent);
}

}  // namespace ash::cloud_upload
