// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"

#include "ash/constants/web_app_id_constants.h"
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
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
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
#include "chrome/browser/ui/webui/ash/cloud_upload/hats_office_trigger.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/common/constants.h"
#include "net/base/url_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/geometry/size.h"
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

// Handle system error notification "Sign in" click.
void HandleSignInClick(Profile* profile, std::optional<int> button_index) {
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
// TODO(b/242685536) Use "files" in the title for multi-files when support for
// multi-files is added.
// Show system notification to communicate that their file can't be opened. If
// the user needs to reauthenticate to OneDrive, prompt the user to
// reauthenticate to ODFS via a "Sign in" button.
void ShowUnableToOpenNotification(
    Profile* profile,
    std::string message = GetGenericErrorMessage(),
    std::string title =
        l10n_util::GetPluralStringFUTF8(IDS_OFFICE_UPLOAD_ERROR_CANT_OPEN_FILE,
                                        1),
    message_center::SystemNotificationWarningLevel warning_level =
        message_center::SystemNotificationWarningLevel::WARNING) {
  std::vector<message_center::ButtonInfo> notification_buttons;

  if (message == GetReauthenticationRequiredMessage()) {
    // Special case of |FILE_ERROR_ACCESS_DENIED| where the user needs to
    // reauthenticate to OneDrive.
    //  Add "Sign in" button.
    notification_buttons.emplace_back(
        l10n_util::GetStringUTF16(IDS_OFFICE_NOTIFICATION_SIGN_IN_BUTTON));
  }

  auto notification = ash::CreateSystemNotificationPtr(
      /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
      /*id=*/kNotificationId,
      /*title*/ base::UTF8ToUTF16(title),
      /*message=*/base::UTF8ToUTF16(message),
      /*display_source=*/
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES),
      /*origin_url=*/GURL(),
      /*notifier_id=*/message_center::NotifierId(),
      /*optional_fields=*/{},
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleSignInClick, profile)),
      /*small_image=*/ash::kFolderIcon, warning_level);

  notification->set_buttons(notification_buttons);
  // Set never_timeout with the highest priority, SYSTEM_PRIORITY, so that the
  // notification never times out.
  notification->set_never_timeout(true);
  notification->SetSystemPriority();
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
    // TODO(b/330786891): Only query account_state once
    // reauthentication_required is no longer needed for backwards compatibility
    // with ODFS.
    reauthentication_required =
        metadata->reauthentication_required ||
        (metadata->account_state.has_value() &&
         metadata->account_state.value() ==
             OdfsAccountState::kReauthenticationRequired);
  } else {
    LOG(ERROR) << "Failed to get reauthentication required state: "
               << metadata.error();
  }
  if (reauthentication_required) {
    ShowUnableToOpenNotification(profile, GetReauthenticationRequiredMessage());
    std::move(callback).Run(
        OfficeOneDriveOpenErrors::kGetActionsReauthRequired);
    return;
  }
  ShowUnableToOpenNotification(profile);
  std::move(callback).Run(OfficeOneDriveOpenErrors::kGetActionsAccessDenied);
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
            if (base::FeatureList::IsEnabled(
                    ::features::kHappinessTrackingOffice)) {
              ash::cloud_upload::HatsOfficeTrigger::Get()
                  .ShowSurveyAfterAppInactive(
                      web_app::kMicrosoft365AppId,
                      ash::cloud_upload::HatsOfficeLaunchingApp::kMS365);
            }
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

mojom::OperationType UploadTypeToOperationType(UploadType upload_type) {
  switch (upload_type) {
    case UploadType::kMove:
      return mojom::OperationType::kMove;
    case UploadType::kCopy:
      return mojom::OperationType::kCopy;
  }
}

void OnWaitingForAndroidUnsupportedPathFallbackChoiceReceived(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    ash::office_fallback::FallbackReason fallback_reason,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics,
    std::optional<const std::string> choice) {
  if (!choice.has_value()) {
    // The user's choice was unable to be retrieved.
    fm_tasks::LogOneDriveMetricsAfterFallback(
        fallback_reason,
        ash::cloud_upload::OfficeTaskResult::kCannotGetFallbackChoiceAfterOpen,
        std::move(cloud_open_metrics));
    return;
  }

  if (choice.value() == ash::office_fallback::kDialogChoiceQuickOffice) {
    fm_tasks::LogOneDriveMetricsAfterFallback(
        fallback_reason,
        ash::cloud_upload::OfficeTaskResult::kFallbackQuickOfficeAfterOpen,
        std::move(cloud_open_metrics));
    fm_tasks::LaunchQuickOffice(profile, file_urls);
  } else if (choice.value() == ash::office_fallback::kDialogChoiceTryAgain) {
    LOG(ERROR) << "Unexpected response: " << choice.value();
  } else if (choice.value() == ash::office_fallback::kDialogChoiceCancel) {
    fm_tasks::LogOneDriveMetricsAfterFallback(
        fallback_reason,
        ash::cloud_upload::OfficeTaskResult::kCancelledAtFallbackAfterOpen,
        std::move(cloud_open_metrics));
  } else if (choice.value() == ash::office_fallback::kDialogChoiceOk) {
    fm_tasks::LogOneDriveMetricsAfterFallback(
        fallback_reason,
        ash::cloud_upload::OfficeTaskResult::kOkAtFallbackAfterOpen,
        std::move(cloud_open_metrics));
  } else if (!choice.value().empty()) {
    LOG(ERROR) << "Unhandled response: " << choice.value();
  } else {
    // Always map an empty user response to a Cancel user response.
    // This can occur when the user logs out of the session. However,
    // since there could be other unknown causes, leave a log.
    LOG(ERROR) << "Empty user response";
    fm_tasks::LogOneDriveMetricsAfterFallback(
        fallback_reason,
        ash::cloud_upload::OfficeTaskResult::kCancelledAtFallbackAfterOpen,
        std::move(cloud_open_metrics));
  }
}

bool BringDialogToFrontIfItExists(const std::string& id) {
  SystemWebDialogDelegate* existing_dialog =
      SystemWebDialogDelegate::FindInstance(id);
  if (!existing_dialog) {
    return false;
  }
  existing_dialog->StackAtTop();
  return true;
}

}  // namespace

// static
// Creates an instance of CloudOpenTask that effectively owns itself by keeping
// a reference alive in the TaskFinished callback.
bool CloudOpenTask::Execute(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const fm_tasks::TaskDescriptor& task,
    const CloudProvider cloud_provider,
    std::unique_ptr<CloudOpenMetrics> cloud_open_metrics) {
  DCHECK(!file_urls.empty());
  auto* event_router = file_manager::EventRouterFactory::GetForProfile(profile);
  // TODO(b/242685536) add support for multiple files.
  if (event_router) {
    if (!event_router->AddCloudOpenTask(file_urls.front())) {
      LOG(ERROR) << "File already being opened";
      // If a cloud upload dialog already exists, bring it to the front to
      // prompt the user to keep going.
      BringDialogToFrontIfItExists(chrome::kChromeUICloudUploadURL);
      // Notify the user that a file is already being opened. Nothing is wrong
      // when the file is already being opened, so use a normal level
      // notification
      ShowUnableToOpenNotification(
          profile, GetAlreadyBeingOpenedMessage(), GetAlreadyBeingOpenedTitle(),
          /*warning_level=*/
          message_center::SystemNotificationWarningLevel::NORMAL);
      cloud_open_metrics->LogTaskResult(
          OfficeTaskResult::kFileAlreadyBeingOpened);
      return false;
    }
  } else {
    LOG(ERROR) << "Cannot get EventRouter";
  }

  scoped_refptr<CloudOpenTask> upload_task = WrapRefCounted(new CloudOpenTask(
      profile, file_urls, task, cloud_provider, std::move(cloud_open_metrics)));
  // Keep `upload_task` alive until `TaskFinished` executes.
  bool status = upload_task->ExecuteInternal();
  return status;
}

CloudOpenTask::CloudOpenTask(
    Profile* profile,
    std::vector<storage::FileSystemURL> file_urls,
    const fm_tasks::TaskDescriptor& task,
    const CloudProvider cloud_provider,
    std::unique_ptr<CloudOpenMetrics> cloud_open_metrics)
    : profile_(profile),
      file_urls_(file_urls),
      task_(task),
      cloud_provider_(cloud_provider),
      cloud_open_metrics_(std::move(cloud_open_metrics)) {
  BrowserList::AddObserver(this);
}

CloudOpenTask::~CloudOpenTask() {
  auto* event_router =
      file_manager::EventRouterFactory::GetForProfile(profile_);
  DCHECK(!file_urls_.empty());
  if (event_router) {
    event_router->RemoveCloudOpenTask(file_urls_.front());
  } else {
    LOG(ERROR) << "Cannot get EventRouter";
  }
  BrowserList::RemoveObserver(this);
}

// Runs setup if it's never been completed. Runs the fixup version of setup if
// there are any issues, e.g. ODFS is not mounted. Otherwise, attempts to move
// files to the correct cloud or open the files if they are already there.
bool CloudOpenTask::ExecuteInternal() {
  if (file_urls_.empty()) {
    LOG(ERROR) << "No files to open";
    cloud_open_metrics_->LogTaskResult(OfficeTaskResult::kNoFilesToOpen);
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
    return InitAndShowSetupOrMoveDialog(
        SetupOrMoveDialogPage::kFileHandlerDialog);
  }

  return MaybeRunFixupFlow();
}

// Runs the fixup version of setup if there are any issues, e.g. ODFS is not
// mounted. Otherwise, attempts to move files to the correct cloud or open the
// files if they are already there.
bool CloudOpenTask::MaybeRunFixupFlow() {
  if (ShouldFixUpOffice(profile_, cloud_provider_)) {
    // TODO(cassycc): Use page specifically for fix up.
    return InitAndShowSetupOrMoveDialog(SetupOrMoveDialogPage::kOneDriveSetup);
  }
  return OpenOrMoveFiles();
}

// Opens office files if they are in the correct cloud already. Otherwise moves
// the files before opening.
bool CloudOpenTask::OpenOrMoveFiles() {
  // Record the source volume type of the opened file.
  OfficeFilesSourceVolume source_volume;
  if (UrlIsOnODFS(file_urls_.front())) {
    source_volume = OfficeFilesSourceVolume::kMicrosoftOneDrive;
  } else if (UrlIsOnAndroidOneDrive(profile_, file_urls_.front())) {
    source_volume = OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider;
  } else {
    auto* volume_manager = file_manager::VolumeManager::Get(profile_);
    base::WeakPtr<file_manager::Volume> source =
        volume_manager->FindVolumeFromPath(file_urls_.front().path());
    if (source) {
      source_volume = VolumeTypeToSourceVolume(source->type());
    } else {
      source_volume = OfficeFilesSourceVolume::kUnknown;
    }
  }
  cloud_open_metrics_->LogSourceVolume(source_volume);

  if (cloud_provider_ == CloudProvider::kGoogleDrive &&
      PathIsOnDriveFS(profile_, file_urls_.front().path())) {
    // The files are on Drive already.
    transfer_required_ = OfficeFilesTransferRequired::kNotRequired;
    cloud_open_metrics_->LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    OpenAlreadyHostedDriveUrls();
    return true;
  }

  if (cloud_provider_ == CloudProvider::kOneDrive &&
      source_volume == OfficeFilesSourceVolume::kMicrosoftOneDrive) {
    // The files are on OneDrive already, selected from ODFS.
    transfer_required_ = OfficeFilesTransferRequired::kNotRequired;
    cloud_open_metrics_->LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    OpenODFSUrls(OfficeTaskResult::kOpened);
    return true;
  }

  if (cloud_provider_ == CloudProvider::kOneDrive &&
      source_volume ==
          OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider) {
    // The files are on OneDrive already, selected from Android OneDrive.
    transfer_required_ = OfficeFilesTransferRequired::kNotRequired;
    cloud_open_metrics_->LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    // Get ODFS email address, compare against Android OneDrive's email address
    // and open URLs.
    GetODFSMetadata(
        GetODFS(profile_),
        base::BindOnce(&CloudOpenTask::CheckEmailAndOpenAndroidOneDriveURLs,
                       this));
    return true;
  }

    // The files need to be moved.
    auto operation =
        GetUploadType(profile_, file_urls_.front()) == UploadType::kCopy
            ? OfficeFilesTransferRequired::kCopy
            : OfficeFilesTransferRequired::kMove;
    transfer_required_ = operation;
    cloud_open_metrics_->LogTransferRequired(operation);
    return ConfirmMoveOrStartUpload();
}

void CloudOpenTask::OpenAlreadyHostedDriveUrls() {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  base::FilePath relative_path;
  for (const auto& file_url : file_urls_) {
    if (integration_service->GetRelativeDrivePath(file_url.path(),
                                                  &relative_path)) {
      integration_service->GetDriveFsInterface()->GetMetadata(
          relative_path,
          base::BindOnce(&CloudOpenTask::OnGoogleDriveGetMetadata, this));
    } else {
      LOG(ERROR) << "Unexpected error obtaining the relative path ";
      LogGoogleDriveOpenResultUMA(
          OfficeTaskResult::kOpened,
          OfficeDriveOpenErrors::kCannotGetRelativePath);
    }
  }
}

// Open an already hosted MS Office file e.g. .docx, from a url hosted in
// DriveFS. Check there was no error retrieving the file's metadata.
void CloudOpenTask::OnGoogleDriveGetMetadata(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  OfficeDriveOpenErrors open_result = OfficeDriveOpenErrors::kSuccess;
  GURL hosted_url(metadata->alternate_url);
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Drive metadata error: " << error;
    open_result = OfficeDriveOpenErrors::kNoMetadata;
  } else if (hosted_url.is_empty() &&
             metadata->item_id.value_or("").starts_with("local-")) {
    LOG(ERROR) << "Local item id, the file hasn't been uploaded";
    open_result = OfficeDriveOpenErrors::kWaitingForUpload;
    GetUserFallbackChoice(
        profile_, task_, file_urls_,
        ash::office_fallback::FallbackReason::kWaitingForUpload,
        base::DoNothing());
  } else if (hosted_url.is_empty()) {
    LOG(ERROR) << "Empty URL";
    open_result = OfficeDriveOpenErrors::kEmptyAlternateUrl;
  } else if (!hosted_url.is_valid()) {
    LOG(ERROR) << "Invalid URL";
    open_result = OfficeDriveOpenErrors::kInvalidAlternateUrl;
  } else if (hosted_url.host() == "drive.google.com") {
    LOG(ERROR) << "URL was from drive.google.com";
    open_result = OfficeDriveOpenErrors::kDriveAlternateUrl;
  } else if (hosted_url.host() != "docs.google.com") {
    LOG(ERROR) << "URL was not from docs.google.com";
    open_result = OfficeDriveOpenErrors::kUnexpectedAlternateUrl;
  } else {
    // TODO(b/242685536) add support for multiple files.
    ::file_manager::util::OpenHostedFileInNewTabOrApp(
        profile_, file_urls_.front().path(), base::DoNothing(),
        net::AppendOrReplaceQueryParameter(hosted_url, "cros_files", "true"));
  }
  LogGoogleDriveOpenResultUMA(OfficeTaskResult::kOpened, open_result);
}

// Open a hosted MS Office file e.g. .docx, from a url hosted in
// DriveFS. Check the file was successfully uploaded to DriveFS.
void CloudOpenTask::OpenUploadedDriveUrl(const GURL& url,
                                         const OfficeTaskResult task_result) {
  // TODO(b/242685536) add support for multiple files.
  ::file_manager::util::OpenHostedFileInNewTabOrApp(
      profile_, file_urls_.front().path(), base::DoNothing(),
      net::AppendOrReplaceQueryParameter(url, "cros_files", "true"));
  // TODO(b/296950967): This function logs both open result and task result (but
  // only if open fails) metrics internally, pull them up to a higher level so
  // all the metrics are logged in one place.
  LogGoogleDriveOpenResultUMA(task_result, OfficeDriveOpenErrors::kSuccess);
}

void CloudOpenTask::OpenODFSUrls(const OfficeTaskResult task_result_uma) {
  for (const auto& file_url : file_urls_) {
    OpenODFSUrl(profile_, file_url,
                base::BindOnce(&CloudOpenTask::LogOneDriveOpenResultUMA, this,
                               task_result_uma));
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
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool CloudOpenTask::ConfirmMoveOrStartUpload() {
  bool show_confirmation_dialog = ShouldShowConfirmationDialog();
  if (show_confirmation_dialog) {
    SetupOrMoveDialogPage dialog_page =
        cloud_provider_ == CloudProvider::kGoogleDrive
            ? SetupOrMoveDialogPage::kMoveConfirmationGoogleDrive
            : SetupOrMoveDialogPage::kMoveConfirmationOneDrive;
    return InitAndShowSetupOrMoveDialog(dialog_page);
  }
  StartUpload();
  return true;
}

bool ShouldFixUpOffice(Profile* profile, const CloudProvider cloud_provider) {
  return cloud_provider == CloudProvider::kOneDrive &&
         !(IsODFSMounted(profile) && IsOfficeWebAppInstalled(profile));
}

bool UrlIsOnAndroidOneDrive(Profile* profile, const FileSystemURL& url) {
  std::string authority;
  std::string root_id;
  base::FilePath path;
  return arc::ParseDocumentsProviderUrl(url, &authority, &root_id, &path) &&
         authority == kAndroidOneDriveAuthority;
}

void CloudOpenTask::CheckEmailAndOpenAndroidOneDriveURLs(
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error) {
  if (!metadata_or_error.has_value()) {
    LOG(ERROR) << "Failed to get user email: " << metadata_or_error.error();
    LogOneDriveOpenResultUMA(OfficeTaskResult::kOpened,
                             OfficeOneDriveOpenErrors::kGetActionsGenericError);
    return;
  }
  if (metadata_or_error->user_email.empty()) {
    LOG(ERROR) << "User email is empty";
    LogOneDriveOpenResultUMA(OfficeTaskResult::kOpened,
                             OfficeOneDriveOpenErrors::kGetActionsNoEmail);
    return;
  }

  // In Android OneDrive, the DocumentsProvider uses the email account
  // associated with it as the root_id.
  std::string authority;
  std::string android_onedrive_email;
  base::FilePath path;
  if (!arc::ParseDocumentsProviderUrl(file_urls_.front(), &authority,
                                      &android_onedrive_email, &path)) {
    LogOneDriveOpenResultUMA(OfficeTaskResult::kOpened,
                             OfficeOneDriveOpenErrors::kInvalidFileSystemURL);
    return;
  }
  // Proceed only if the Android OneDrive and ODFS email addresses match.
  if (base::ToLowerASCII(android_onedrive_email) !=
      base::ToLowerASCII(metadata_or_error->user_email)) {
    LOG(ERROR) << "Email accounts associated with ODFS and "
                  "Android OneDrive don't match.";
    LogOneDriveOpenResultUMA(OfficeTaskResult::kOpened,
                             OfficeOneDriveOpenErrors::kEmailsDoNotMatch);
    return;
  }

  // TODO(b/242685536) add support for multiple files.
  OpenAndroidOneDriveUrl(file_urls_[0]);
}

// Open office file, originally selected from Android OneDrive, from ODFS. First
// convert the |android_onedrive_urls| to ODFS file paths, then open them from
// ODFS in the MS 365 PWA.
void CloudOpenTask::OpenAndroidOneDriveUrl(
    const FileSystemURL& android_onedrive_file_url) {
  // TODO(b/269364287): Handle when Android OneDrive file can't be opened.
  if (!UrlIsOnAndroidOneDrive(profile_, android_onedrive_file_url)) {
    LOG(ERROR) << "File not on Android OneDrive";
    LogOneDriveOpenResultUMA(
        OfficeTaskResult::kOpened,
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
    return;
  }

  // Get the ODFS mount path.
  std::optional<ProvidedFileSystemInfo> odfs_file_system_info =
      GetODFSInfo(profile_);
  if (!odfs_file_system_info.has_value()) {
    LOG(ERROR) << "ODFS not found";
    LogOneDriveOpenResultUMA(
        OfficeTaskResult::kOpened,
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
    return;
  }
  base::FilePath odfs_path = odfs_file_system_info->mount_path();

  // Find the relative path from Android OneDrive Url.
  std::string authority;
  std::string root_id;
  base::FilePath path;
  if (!arc::ParseDocumentsProviderUrl(android_onedrive_file_url, &authority,
                                      &root_id, &path)) {
    LOG(ERROR) << "Could not parse Android OneDrive Url";
    LogOneDriveOpenResultUMA(
        OfficeTaskResult::kOpened,
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
    return;
  }
  // Format for Android OneDrive documents provider `path` is:
  // Files/<rel_path>
  std::vector<base::FilePath::StringType> components =
      base::FilePath(path.value()).GetComponents();
  if (components.size() < 2) {
    LOG(ERROR)
        << "Android OneDrive documents provider path is not as expected.";
    LogOneDriveOpenResultUMA(
        OfficeTaskResult::kOpened,
        OfficeOneDriveOpenErrors::kAndroidOneDriveInvalidUrl);
    return;
  }
  if (components[0] != "Files") {
    ash::office_fallback::FallbackReason fallback_reason = ash::
        office_fallback::FallbackReason::kAndroidOneDriveUnsupportedLocation;
    // `cloud_open_metrics_` can be safely moved since CloudUploadTask is
    // expected to be destructed straight after.
    GetUserFallbackChoice(
        profile_, task_, file_urls_, fallback_reason,
        base::BindOnce(
            &OnWaitingForAndroidUnsupportedPathFallbackChoiceReceived, profile_,
            file_urls_, fallback_reason, std::move(cloud_open_metrics_)));

    return;
  }
  // Append relative path from Android OneDrive Url.
  for (size_t i = 1; i < components.size(); i++) {
    odfs_path = odfs_path.Append(components[i]);
  }

  ash::file_system_provider::util::LocalPathParser parser(profile_, odfs_path);
  if (!parser.Parse()) {
    LOG(ERROR) << "Path not in FSP";
    LogOneDriveOpenResultUMA(
        OfficeTaskResult::kOpened,
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
    return;
  }

  OpenFileFromODFS(profile_, parser.file_system(), parser.file_path(),
                   base::BindOnce(&CloudOpenTask::LogOneDriveOpenResultUMA,
                                  this, OfficeTaskResult::kOpened));
  return;
}

void CloudOpenTask::StartUpload() {
  DCHECK_EQ(file_urls_idx_, 0UL);
  upload_timer_ = base::ElapsedTimer();
  // CloudOpenTask is the only owner of the `CloudOpenMetrics` object and will
  // still be alive after the upload handler completes. Thus, pass a `SafeRef`
  // of `CloudOpenMetrics` to the upload handler.

  if (cloud_provider_ == CloudProvider::kGoogleDrive) {
    StartNextGoogleDriveUpload();
  } else if (cloud_provider_ == CloudProvider::kOneDrive) {
    StartNextOneDriveUpload();
  }
}

void CloudOpenTask::StartNextGoogleDriveUpload() {
  DCHECK_LT(file_urls_idx_, file_urls_.size());
  drive_upload_handler_ = std::make_unique<DriveUploadHandler>(
      profile_, file_urls_[file_urls_idx_],
      base::BindOnce(&CloudOpenTask::FinishedDriveUpload, this),
      cloud_open_metrics_->GetSafeRef());
  drive_upload_handler_->Run();
}

void CloudOpenTask::StartNextOneDriveUpload() {
  DCHECK_LT(file_urls_idx_, file_urls_.size());
  one_drive_upload_handler_ = std::make_unique<OneDriveUploadHandler>(
      profile_, file_urls_[file_urls_idx_],
      base::BindOnce(&CloudOpenTask::FinishedOneDriveUpload, this,
                     profile_->GetWeakPtr()),
      cloud_open_metrics_->GetSafeRef());
  one_drive_upload_handler_->Run();
}

void CloudOpenTask::FinishedDriveUpload(OfficeTaskResult task_result,
                                        std::optional<GURL> url,
                                        int64_t size) {
  DCHECK_LT(file_urls_idx_, file_urls_.size());
  if (url.has_value()) {
    upload_total_size_ += size;
    fm_tasks::SetOfficeFileMovedToGoogleDrive(profile_, base::Time::Now());
    // Log TaskResult after open is tried.
    OpenUploadedDriveUrl(url.value(), task_result);
  } else {
    cloud_open_metrics_->LogTaskResult(task_result);
    has_upload_errors_ = has_upload_errors_ ||
                         (task_result == OfficeTaskResult::kFailedToUpload);
  }
  file_urls_idx_++;
  if (file_urls_idx_ < file_urls_.size()) {
    StartNextGoogleDriveUpload();
    return;
  }
  if (!has_upload_errors_) {
    RecordUploadLatencyUMA();
  }
}

void CloudOpenTask::FinishedOneDriveUpload(
    base::WeakPtr<Profile> profile_weak_ptr,
    OfficeTaskResult task_result,
    std::optional<storage::FileSystemURL> url,
    int64_t size) {
  DCHECK_LT(file_urls_idx_, file_urls_.size());
  if (url.has_value()) {
    upload_total_size_ += size;
    Profile* profile = profile_weak_ptr.get();
    if (!profile) {
      // TODO(b/296950967): metric to log here?
      return;
    }
    fm_tasks::SetOfficeFileMovedToOneDrive(profile, base::Time::Now());
    // Log TaskResult after open is tried.
    OpenODFSUrl(profile, url.value(),
                base::BindOnce(&CloudOpenTask::LogOneDriveOpenResultUMA, this,
                               task_result));
  } else {
    cloud_open_metrics_->LogTaskResult(task_result);
    has_upload_errors_ = has_upload_errors_ ||
                         (task_result == OfficeTaskResult::kFailedToUpload);
  }
  file_urls_idx_++;
  if (file_urls_idx_ < file_urls_.size()) {
    StartNextOneDriveUpload();
    return;
  }
  if (!has_upload_errors_) {
    RecordUploadLatencyUMA();
  }
}

// Logs UMA when the Drive task ends with an attempt to open a file.
void CloudOpenTask::LogGoogleDriveOpenResultUMA(
    OfficeTaskResult success_task_result,
    OfficeDriveOpenErrors open_result) {
  cloud_open_metrics_->LogGoogleDriveOpenError(open_result);
  cloud_open_metrics_->LogTaskResult(open_result ==
                                             OfficeDriveOpenErrors::kSuccess
                                         ? success_task_result
                                         : OfficeTaskResult::kFailedToOpen);
}

// Logs UMA when the OneDrive task ends with an attempt to open a file.
void CloudOpenTask::LogOneDriveOpenResultUMA(
    OfficeTaskResult success_task_result,
    OfficeOneDriveOpenErrors open_result) {
  cloud_open_metrics_->LogOneDriveOpenError(open_result);
  cloud_open_metrics_->LogTaskResult(open_result ==
                                             OfficeOneDriveOpenErrors::kSuccess
                                         ? success_task_result
                                         : OfficeTaskResult::kFailedToOpen);
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
bool CloudOpenTask::InitAndShowSetupOrMoveDialog(
    SetupOrMoveDialogPage dialog_page) {
  // Allow no more than one upload dialog at a time. If one already exists,
  // bring it to the front to prompt the user to keep going. In the case of
  // multiple upload requests, they should either be handled simultaneously or
  // queued.
  if (BringDialogToFrontIfItExists(chrome::kChromeUICloudUploadURL)) {
    LOG(WARNING) << "Another cloud upload dialog is already being shown";
    if (dialog_page == SetupOrMoveDialogPage::kMoveConfirmationGoogleDrive ||
        dialog_page == SetupOrMoveDialogPage::kMoveConfirmationOneDrive) {
      cloud_open_metrics_->LogTaskResult(
          OfficeTaskResult::kCannotShowMoveConfirmation);
    } else {
      cloud_open_metrics_->LogTaskResult(
          OfficeTaskResult::kCannotShowSetupDialog);
    }
    return false;
  }

  mojom::DialogArgsPtr args = CreateDialogArgs(dialog_page);

  // Display local file handlers (tasks) only for the file handler dialog.
  if (dialog_page == SetupOrMoveDialogPage::kFileHandlerDialog) {
    // Callback to show the dialog after the tasks have been found.
    fm_tasks::FindTasksCallback find_all_types_of_tasks_callback =
        base::BindOnce(&CloudOpenTask::ShowDialog, this, dialog_page,
                       std::move(args));
    // Find the file tasks that can open the `file_urls_` and then run
    // `ShowDialog`.
    FindTasksForDialog(std::move(find_all_types_of_tasks_callback));
  } else {
    ShowDialog(dialog_page, std::move(args), nullptr);
  }
  return true;
}

mojom::DialogArgsPtr CloudOpenTask::CreateDialogArgs(
    SetupOrMoveDialogPage dialog_page) {
  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  for (const auto& file_url : file_urls_) {
    args->file_names.push_back(file_url.path().BaseName().value());
  }
  switch (dialog_page) {
    case SetupOrMoveDialogPage::kFileHandlerDialog: {
      auto file_handler_dialog_args = mojom::FileHandlerDialogArgs::New();
      file_handler_dialog_args->show_google_workspace_task =
          chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAllowed(profile_);
      file_handler_dialog_args->show_microsoft_office_task =
          chromeos::cloud_upload::IsMicrosoftOfficeCloudUploadAllowed(profile_);
      args->dialog_specific_args =
          mojom::DialogSpecificArgs::NewFileHandlerDialogArgs(
              std::move(file_handler_dialog_args));
      break;
    }
    case SetupOrMoveDialogPage::kOneDriveSetup: {
      auto one_drive_setup_dialog_args = mojom::OneDriveSetupDialogArgs::New();
      one_drive_setup_dialog_args->set_office_as_default_handler =
          !HaveExplicitFileHandlers(profile_, file_urls_);
      args->dialog_specific_args =
          mojom::DialogSpecificArgs::NewOneDriveSetupDialogArgs(
              std::move(one_drive_setup_dialog_args));
      break;
    }
    case SetupOrMoveDialogPage::kMoveConfirmationOneDrive: {
      auto move_confirmation_one_drive_dialog_args =
          mojom::MoveConfirmationOneDriveDialogArgs::New();
      move_confirmation_one_drive_dialog_args->operation_type =
          UploadTypeToOperationType(GetUploadType(profile_, file_urls_[0]));
      args->dialog_specific_args =
          mojom::DialogSpecificArgs::NewMoveConfirmationOneDriveDialogArgs(
              std::move(move_confirmation_one_drive_dialog_args));
      break;
    }
    case SetupOrMoveDialogPage::kMoveConfirmationGoogleDrive: {
      auto move_confirmation_google_drive_dialog_args =
          mojom::MoveConfirmationGoogleDriveDialogArgs::New();
      move_confirmation_google_drive_dialog_args->operation_type =
          UploadTypeToOperationType(GetUploadType(profile_, file_urls_[0]));
      args->dialog_specific_args =
          mojom::DialogSpecificArgs::NewMoveConfirmationGoogleDriveDialogArgs(
              std::move(move_confirmation_google_drive_dialog_args));
      break;
    }
  }
  return args;
}

// Creates and shows a new dialog for the cloud upload workflow. If there are
// local file tasks from `resulting_tasks`, include them in the dialog
// arguments. These tasks are can be selected by the user to open the files
// instead of using a cloud provider. If there is no Files app window currently
// open to use as a modal parent for the dialog, first launches a new Files app
// window, which we listen for in OnBrowserAdded().
void CloudOpenTask::ShowDialog(
    SetupOrMoveDialogPage dialog_page,
    mojom::DialogArgsPtr args,
    std::unique_ptr<fm_tasks::ResultingTasks> resulting_tasks) {
  if (resulting_tasks) {
    SetTaskArgs(args, std::move(resulting_tasks));

    if (chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
      const auto& file_handler_dialog_args =
          args->dialog_specific_args->get_file_handler_dialog_args();
      // When there is only one possible task (Microsoft or Google) and no
      // further local tasks, skip the file handler page and either show the
      // OneDrive setup if necessary, or go straight to opening/moving the
      // files.
      if ((!file_handler_dialog_args->show_microsoft_office_task ||
           !file_handler_dialog_args->show_google_workspace_task) &&
          local_tasks_.empty()) {
        // Validate that `cloud_provider_` differs from the disabled task.
        CHECK(!(cloud_provider_ == CloudProvider::kOneDrive &&
                !file_handler_dialog_args->show_microsoft_office_task));
        CHECK(!(cloud_provider_ == CloudProvider::kGoogleDrive &&
                !file_handler_dialog_args->show_google_workspace_task));
        MaybeRunFixupFlow();
        return;
      }
    }
  }

  bool office_move_confirmation_shown =
      cloud_provider_ == CloudProvider::kGoogleDrive
          ? fm_tasks::GetOfficeMoveConfirmationShownForDrive(profile_)
          : fm_tasks::GetOfficeMoveConfirmationShownForOneDrive(profile_);
  base::OnceCallback<void(const std::string&)> dialog_callback;
  switch (dialog_page) {
    case SetupOrMoveDialogPage::kFileHandlerDialog:
    case SetupOrMoveDialogPage::kOneDriveSetup:
      dialog_callback =
          base::BindOnce(&CloudOpenTask::OnSetupDialogComplete, this);
      break;
    case SetupOrMoveDialogPage::kMoveConfirmationOneDrive:
    case SetupOrMoveDialogPage::kMoveConfirmationGoogleDrive:
      dialog_callback =
          base::BindOnce(&CloudOpenTask::OnMoveConfirmationComplete, this);
      break;
  }
  // This CloudUploadDialog pointer is managed by an instance of
  // `views::WebDialogView` and deleted in
  // `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), std::move(dialog_callback),
                            office_move_confirmation_shown);

  // Get Files App window, if it exists.
  files_app_browser_ =
      FindSystemWebAppBrowser(profile_, ash::SystemWebAppType::FILE_MANAGER);
  gfx::NativeWindow modal_parent =
      files_app_browser_ ? files_app_browser_->window()->GetNativeWindow()
                         : nullptr;

  if (!modal_parent) {
    need_new_files_app_ = true;
    DCHECK(!pending_dialog_);
    pending_dialog_ = dialog;
    // Create a files app window and use it as the modal parent. CloudOpenTask
    // is kept alive by the callback passed to CloudUploadDialog above. We
    // expect this to trigger OnBrowserAdded, which then shows the dialog.
    file_manager::util::ShowItemInFolder(profile_, file_urls_.at(0).path(),
                                         base::DoNothing());
  } else {
    dialog->ShowSystemDialog(modal_parent);
  }
}

// Stores constructed tasks into
// `args->dialog_specific_args->file_handler_dialog_args->local_tasks` and
// `local_tasks_`.
void CloudOpenTask::SetTaskArgs(
    mojom::DialogArgsPtr& args,
    std::unique_ptr<fm_tasks::ResultingTasks> resulting_tasks) {
  int nextPosition = 0;

  auto& file_handler_dialog_args =
      args->dialog_specific_args->get_file_handler_dialog_args();
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

    file_handler_dialog_args->local_tasks.push_back(std::move(dialog_task));
    local_tasks_.push_back(std::move(task.task_descriptor));
  }
}

void CloudOpenTask::OnBrowserAdded(Browser* browser) {
  if (!need_new_files_app_) {
    return;
  }
  // TODO(petermarshall): Add a timeout. If Files app never launches for some
  // reason, then we will never show the dialog.
  DCHECK(pending_dialog_);
  if (!IsBrowserForSystemWebApp(browser, SystemWebAppType::FILE_MANAGER)) {
    // Wait for Files app to launch.
    LOG(WARNING) << "Browser did not match Files app";
    return;
  }
  need_new_files_app_ = false;
  files_app_browser_ = browser;
  pending_dialog_->ShowSystemDialog(
      files_app_browser_->window()->GetNativeWindow());
  // The dialog is deleted in `SystemWebDialogDelegate::OnDialogClosed`.
  pending_dialog_ = nullptr;
}

void CloudOpenTask::OnBrowserClosing(Browser* browser) {
  if (browser == files_app_browser_) {
    // The Files app that the dialog is modal to is closed. This will close the
    // dialog with an empty user response.
    files_app_closed_ = true;
  }
}

// Receive user's setup dialog response and acts accordingly. `user_response` is
// either a particular ash::cloud_upload::mojom::UserAction or the id (position)
// of the task in `local_tasks_` to launch. We never use the return value but
// it's necessary to make sure that we delete CloudOpenTask when we're done.
void CloudOpenTask::OnSetupDialogComplete(const std::string& user_response) {
  if (user_response == kUserActionConfirmOrUploadToGoogleDrive) {
    cloud_provider_ = CloudProvider::kGoogleDrive;
    cloud_open_metrics_->set_cloud_provider(cloud_provider_);

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
  } else if (user_response == kUserActionSetUpOneDrive) {
    UMA_HISTOGRAM_ENUMERATION(kFileHandlerSelectionMetricName,
                              OfficeSetupFileHandler::kMicrosoft365);
    cloud_provider_ = CloudProvider::kOneDrive;
    cloud_open_metrics_->set_cloud_provider(cloud_provider_);
    InitAndShowSetupOrMoveDialog(SetupOrMoveDialogPage::kOneDriveSetup);
  } else if (user_response == kUserActionCancel) {
    cloud_open_metrics_->LogTaskResult(OfficeTaskResult::kCancelledAtSetup);
    // Do nothing.
  } else if (!user_response.empty()) {
    cloud_open_metrics_->LogTaskResult(OfficeTaskResult::kLocalFileTask);
    LaunchLocalFileTask(user_response);
  } else {
    // Always map an empty user response to a Cancel user response. This can
    // occur when the Files app the dialog was modal to is closed.
    if (!files_app_closed_) {
      // This can also occur when the user logs out of the session. However,
      // since there could be other unknown causes, leave a log.
      LOG(ERROR) << "Empty user response not due to the files app closing";
    }
    cloud_open_metrics_->LogTaskResult(OfficeTaskResult::kCancelledAtSetup);
  }
}

// Receive user's move confirmation response and acts accordingly.
// `user_response` is a particular ash::cloud_upload::mojom::UserAction. We
// never use the return value but it's necessary to make sure that we delete
// CloudOpenTask when we're done.
void CloudOpenTask::OnMoveConfirmationComplete(
    const std::string& user_response) {
  // TODO(petermarshall): Don't need separate actions for drive/onedrive now
  // (and for StartUpload?).
  if (user_response == kUserActionUploadToGoogleDrive) {
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
  } else if (user_response == kUserActionCancelGoogleDrive ||
             user_response == kUserActionCancelOneDrive) {
    cloud_open_metrics_->LogTaskResult(
        OfficeTaskResult::kCancelledAtConfirmation);
  } else if (!user_response.empty()) {
    LOG(ERROR) << "Unhandled response: " << user_response;
  } else {
    // Always map an empty user response to a Cancel user response. This can
    // occur when the Files app the dialog was modal to is closed.
    if (!files_app_closed_) {
      // This can also occur when the user logs out of the session. However,
      // since there could be other unknown causes, leave a log.
      LOG(ERROR) << "Empty user response not due to the files app closing";
    }
    cloud_open_metrics_->LogTaskResult(
        OfficeTaskResult::kCancelledAtConfirmation);
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
      profile_, task, file_urls_,
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
               << error_message
               << " and result: " << base::to_underlying(result);
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
  CHECK(dialog_args_);
  SystemWebDialogDelegate::OnDialogShown(webui);
  static_cast<CloudUploadUI*>(webui->GetController())
      ->SetDialogArgs(dialog_args_.Clone());
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
                                     bool office_move_confirmation_shown)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICloudUploadURL),
                              std::u16string() /* title */),
      dialog_args_(std::move(args)),
      callback_(std::move(callback)),
      office_move_confirmation_shown_(office_move_confirmation_shown) {}

CloudUploadDialog::~CloudUploadDialog() = default;

ui::mojom::ModalType CloudUploadDialog::GetDialogModalType() const {
  return ui::mojom::ModalType::kWindow;
}

bool CloudUploadDialog::ShouldCloseDialogOnEscape() const {
  // All the dialogs handle an Escape keydown.
  return false;
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
constexpr int kDialogHeightForFileHandlerDialogOneHandlerMissing = 295;

constexpr int kDialogWidthForMoveConfirmation = 512;
constexpr int kDialogHeightForMoveConfirmationWithCheckbox = 524;

constexpr int kDialogHeightForMoveConfirmationWithoutCheckbox = 472;

constexpr int kDialogWidthForConnectToOneDrive = 512;
constexpr int kDialogHeightForConnectToOneDrive = 556;
}  // namespace

void CloudUploadDialog::GetDialogSize(gfx::Size* size) const {
  const auto& dialog_specific_args = dialog_args_->dialog_specific_args;
  if (dialog_specific_args->is_file_handler_dialog_args()) {
    const auto& file_handler_dialog_args =
        dialog_specific_args->get_file_handler_dialog_args();
    const bool has_local_tasks = !file_handler_dialog_args->local_tasks.empty();
    const bool is_microsoft_office_or_google_workspace_disabled_by_policy =
        !file_handler_dialog_args->show_microsoft_office_task ||
        !file_handler_dialog_args->show_google_workspace_task;
    size->set_width(kDialogWidthForFileHandlerDialog);
    if (is_microsoft_office_or_google_workspace_disabled_by_policy) {
      CHECK(has_local_tasks);
      size->set_height(kDialogHeightForFileHandlerDialogOneHandlerMissing);
    } else {
      size->set_height(has_local_tasks
                           ? kDialogHeightForFileHandlerDialog
                           : kDialogHeightForFileHandlerDialogNoLocalApp);
    }
  } else if (dialog_specific_args->is_one_drive_setup_dialog_args()) {
    size->set_width(kDialogWidthForOneDriveSetup);
    size->set_height(kDialogHeightForOneDriveSetup);
  } else if (dialog_specific_args
                 ->is_move_confirmation_google_drive_dialog_args() ||
             dialog_specific_args
                 ->is_move_confirmation_one_drive_dialog_args()) {
    size->set_width(kDialogWidthForMoveConfirmation);
    if (office_move_confirmation_shown_) {
      size->set_height(kDialogHeightForMoveConfirmationWithCheckbox);
    } else {
      size->set_height(kDialogHeightForMoveConfirmationWithoutCheckbox);
    }
  } else if (dialog_specific_args->is_connect_to_one_drive_dialog_args()) {
    size->set_width(kDialogWidthForConnectToOneDrive);
    size->set_height(kDialogHeightForConnectToOneDrive);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

bool ShowConnectOneDriveDialog(gfx::NativeWindow modal_parent) {
  // Allow no more than one upload dialog at a time. If one already exists,
  // bring it to the front to prompt the user to keep going. Only one of either
  // this dialog, or CloudOpenTask can be shown at a time because they use the
  // same WebUI for dialogs.
  if (BringDialogToFrontIfItExists(chrome::kChromeUICloudUploadURL)) {
    LOG(WARNING) << "Another cloud upload dialog is already being shown";
    return false;
  }

  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  args->dialog_specific_args =
      mojom::DialogSpecificArgs::NewConnectToOneDriveDialogArgs(
          mojom::ConnectToOneDriveDialogArgs::New());

  // This CloudUploadDialog pointer is managed by an instance of
  // `views::WebDialogView` and deleted in
  // `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), base::DoNothing(),
                            /*office_move_confirmation_shown=*/false);

  dialog->ShowSystemDialog(modal_parent);
  return true;
}

void LaunchMicrosoft365Setup(Profile* profile, gfx::NativeWindow modal_parent) {
  mojom::DialogArgsPtr args = mojom::DialogArgs::New();

  auto one_drive_setup_dialog_args = mojom::OneDriveSetupDialogArgs::New();
  // If `set_office_as_default_handler` is false, it indicates that we already
  // ran the Office setup and set file handler preferences for all handled
  // Office file types, or that the user has pre-existing preferences for these
  // file types.
  one_drive_setup_dialog_args->set_office_as_default_handler =
      !HaveExplicitFileHandlers(profile, fm_tasks::WordGroupExtensions()) ||
      !HaveExplicitFileHandlers(profile, fm_tasks::ExcelGroupExtensions()) ||
      !HaveExplicitFileHandlers(profile, fm_tasks::PowerPointGroupExtensions());

  args->dialog_specific_args =
      mojom::DialogSpecificArgs::NewOneDriveSetupDialogArgs(
          std::move(one_drive_setup_dialog_args));

  // This CloudUploadDialog pointer is managed by an instance of
  // `views::WebDialogView` and deleted in
  // `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), base::DoNothing(),
                            /*office_move_confirmation_shown=*/false);

  dialog->ShowSystemDialog(modal_parent);
}

}  // namespace ash::cloud_upload
