// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include <string>

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

// Stringify the `MetricState` enum.
std::ostream& operator<<(std::ostream& os, MetricState metric_state) {
  switch (metric_state) {
    case MetricState::kCorrectlyNotLogged:
      return os << "NL";
    case MetricState::kCorrectlyLogged:
      return os << "L";
    case MetricState::kIncorrectlyNotLogged:
      return os << "INL";
    case MetricState::kIncorrectlyLogged:
      return os << "IL";
    case MetricState::kIncorrectlyLoggedMultipleTimes:
      return os << "ILM";
    case MetricState::kWrongValueLogged:
      return os << "WVL";
  }
}

// Stringify enums (`MetricType`) that are not the `MetricState`.
template <
    typename MetricType,
    class = std::enable_if<std::is_enum<MetricType>::value &&
                           !std::is_same<MetricType, MetricState>::value>::type>
std::ostream& operator<<(std::ostream& os, const MetricType& value) {
  return os << static_cast<std::underlying_type<MetricType>::type>(value);
}

// Print debug information about this metric.
template <typename MetricType>
std::ostream& operator<<(std::ostream& os, const Metric<MetricType>& metric) {
  os << metric.metric_name;
  os << ": ";
  os << metric.state;
  if (metric.state != MetricState::kCorrectlyNotLogged &&
      metric.state != MetricState::kIncorrectlyNotLogged) {
    os << ", value: ";
    os << metric.value;
    if (metric.state == MetricState::kIncorrectlyLoggedMultipleTimes) {
      os << ", old value: ";
      os << metric.old_value;
    }
  }
  return os;
}

CloudOpenMetrics::CloudOpenMetrics(CloudProvider cloud_provider,
                                   size_t file_count)
    : multiple_files_(file_count > 1),
      cloud_provider_(cloud_provider),
      drive_copy_error_(kGoogleDriveCopyErrorMetricName,
                        kGoogleDriveCopyErrorMetricStateMetricName),
      one_drive_copy_error_(kOneDriveCopyErrorMetricName,
                            kOneDriveCopyErrorMetricStateMetricName),
      drive_move_error_(kGoogleDriveMoveErrorMetricName,
                        kGoogleDriveMoveErrorMetricStateMetricName),
      one_drive_move_error_(kOneDriveMoveErrorMetricName,
                            kOneDriveMoveErrorMetricStateMetricName),
      drive_open_error_(kDriveErrorMetricName,
                        kDriveErrorMetricStateMetricName),
      one_drive_open_error_(kOneDriveErrorMetricName,
                            kOneDriveErrorMetricStateMetricName),
      drive_source_volume_(kDriveOpenSourceVolumeMetric,
                           kDriveOpenSourceVolumeMetricStateMetric),
      one_drive_source_volume_(kOneDriveOpenSourceVolumeMetric,
                               kOneDriveOpenSourceVolumeMetricStateMetric),
      drive_task_result_(kGoogleDriveTaskResultMetricName,
                         kGoogleDriveTaskResultMetricStateMetricName),
      one_drive_task_result_(kOneDriveTaskResultMetricName,
                             kOneDriveTaskResultMetricStateMetricName),
      drive_transfer_required_(kDriveTransferRequiredMetric,
                               kDriveTransferRequiredMetricStateMetric),
      one_drive_transfer_required_(kOneDriveTransferRequiredMetric,
                                   kOneDriveTransferRequiredMetricStateMetric),
      drive_upload_result_(kGoogleDriveUploadResultMetricName,
                           kGoogleDriveUploadResultMetricStateMetricName),
      one_drive_upload_result_(kOneDriveUploadResultMetricName,
                               kOneDriveUploadResultMetricStateMetricName) {}

void CloudOpenMetrics::CheckForInconsistencies(
    Metric<base::File::Error>& copy_error,
    Metric<base::File::Error>& move_error,
    Metric<OfficeDriveOpenErrors>& drive_open_error,
    Metric<OfficeOneDriveOpenErrors>& one_drive_open_error,
    Metric<OfficeFilesSourceVolume>& source_volume,
    Metric<OfficeTaskResult>& task_result,
    Metric<OfficeFilesTransferRequired>& transfer_required,
    Metric<OfficeFilesUploadResult>& upload_result) {
  bool google_drive = cloud_provider_ == CloudProvider::kGoogleDrive;
  // Task result should always be logged.
  ExpectLogged(task_result);
  if (task_result.logged()) {
    if (task_result.value == OfficeTaskResult::kFallbackQuickOffice ||
        task_result.value == OfficeTaskResult::kCancelledAtFallback ||
        task_result.value == OfficeTaskResult::kCancelledAtSetup ||
        task_result.value == OfficeTaskResult::kLocalFileTask) {
      // The cloud open/upload flow was exited at the Fallback Dialog or Setup
      // flow.
      ExpectNotLogged(transfer_required);
      ExpectNotLogged(upload_result);
      if (task_result.value == OfficeTaskResult::kFallbackQuickOffice ||
          task_result.value == OfficeTaskResult::kCancelledAtFallback) {
        // The cloud open/upload flow was exited at the Fallback Dialog.
        // OpenErrors should give a fallback reason.
        if (google_drive) {
          ExpectLogged(drive_open_error);
          if (drive_open_error.logged()) {
            switch (drive_open_error.value) {
              case OfficeDriveOpenErrors::kOffline:
              case OfficeDriveOpenErrors::kDriveFsInterface:
              case OfficeDriveOpenErrors::kDriveDisabled:
              case OfficeDriveOpenErrors::kNoDriveService:
              case OfficeDriveOpenErrors::kDriveAuthenticationNotReady:
              case OfficeDriveOpenErrors::kMeteredConnection:
                break;
              case OfficeDriveOpenErrors::kTimeout:
              case OfficeDriveOpenErrors::kNoMetadata:
              case OfficeDriveOpenErrors::kInvalidAlternateUrl:
              case OfficeDriveOpenErrors::kDriveAlternateUrl:
              case OfficeDriveOpenErrors::kUnexpectedAlternateUrl:
              case OfficeDriveOpenErrors::kSuccess:
                SetWrongValueLogged(drive_open_error);
                break;
            }
          }
        } else {
          ExpectLogged(one_drive_open_error);
          if (one_drive_open_error.logged()) {
            switch (one_drive_open_error.value) {
              case OfficeOneDriveOpenErrors::kOffline:
                break;
              case OfficeOneDriveOpenErrors::kSuccess:
              case OfficeOneDriveOpenErrors::kNoProfile:
              case OfficeOneDriveOpenErrors::kNoFileSystemURL:
              case OfficeOneDriveOpenErrors::kInvalidFileSystemURL:
              case OfficeOneDriveOpenErrors::kGetActionsGenericError:
              case OfficeOneDriveOpenErrors::kGetActionsReauthRequired:
              case OfficeOneDriveOpenErrors::kGetActionsInvalidUrl:
              case OfficeOneDriveOpenErrors::kGetActionsNoUrl:
              case OfficeOneDriveOpenErrors::kGetActionsAccessDenied:
              case OfficeOneDriveOpenErrors::kGetActionsNoEmail:
              case OfficeOneDriveOpenErrors::kConversionToODFSUrlError:
              case OfficeOneDriveOpenErrors::kEmailsDoNotMatch:
                SetWrongValueLogged(one_drive_open_error);
                break;
            }
          }
        }
      }
    } else {
      // The cloud open/upload flow was not exited at the Fallback Dialog or
      // Setup flow.
      ExpectLogged(source_volume);
      ExpectLogged(transfer_required);
      if (task_result.value == OfficeTaskResult::kCancelledAtConfirmation) {
        // The cloud upload flow was exited at the Move Confirmation Dialog.
        ExpectNotLogged(upload_result);
        ExpectNotLogged(drive_open_error);
        ExpectNotLogged(one_drive_open_error);
        // TransferRequired should be kMove or kCopy.
        ExpectLogged(transfer_required);
        if (transfer_required.logged()) {
          switch (transfer_required.value) {
            case OfficeFilesTransferRequired::kMove:
            case OfficeFilesTransferRequired::kCopy:
              break;
            case OfficeFilesTransferRequired::kNotRequired:
              SetWrongValueLogged(transfer_required);
              break;
          }
        }
      } else if (task_result.value == OfficeTaskResult::kFailedToUpload) {
        ExpectNotLogged(drive_open_error);
        ExpectNotLogged(one_drive_open_error);
        switch (upload_result.value) {
          case OfficeFilesUploadResult::kOtherError:
          case OfficeFilesUploadResult::kFileSystemNotFound:
          case OfficeFilesUploadResult::kMoveOperationCancelled:
          case OfficeFilesUploadResult::kMoveOperationError:
          case OfficeFilesUploadResult::kMoveOperationNeedPassword:
          case OfficeFilesUploadResult::kCopyOperationCancelled:
          case OfficeFilesUploadResult::kCopyOperationError:
          case OfficeFilesUploadResult::kCopyOperationNeedPassword:
          case OfficeFilesUploadResult::kPinningFailedDiskFull:
          case OfficeFilesUploadResult::kCloudAccessDenied:
          case OfficeFilesUploadResult::kCloudMetadataError:
          case OfficeFilesUploadResult::kCloudQuotaFull:
          case OfficeFilesUploadResult::kCloudError:
          case OfficeFilesUploadResult::kNoConnection:
          case OfficeFilesUploadResult::kDestinationUrlError:
          case OfficeFilesUploadResult::kInvalidURL:
          case OfficeFilesUploadResult::kCloudReauthRequired:
          case OfficeFilesUploadResult::kInvalidAlternateUrl:
          case OfficeFilesUploadResult::kUnexpectedAlternateUrlHost:
          case OfficeFilesUploadResult::kSyncError:
          case OfficeFilesUploadResult::kSyncCancelledAndDeleted:
          case OfficeFilesUploadResult::kSyncCancelledAndTrashed:
          case OfficeFilesUploadResult::
              kUploadNotStartedReauthenticationRequired:
            break;
          case OfficeFilesUploadResult::kSuccess:
          case OfficeFilesUploadResult::kSuccessAfterReauth:
            SetWrongValueLogged(upload_result);
            break;
        }
      } else if (task_result.value == OfficeTaskResult::kFailedToOpen) {
        // The cloud open flow failed.
        // OpenErrors should be an error.
        if (google_drive) {
          ExpectLogged(drive_open_error);
          if (drive_open_error.logged()) {
            switch (drive_open_error.value) {
              case OfficeDriveOpenErrors::kOffline:
              case OfficeDriveOpenErrors::kDriveFsInterface:
              case OfficeDriveOpenErrors::kTimeout:
              case OfficeDriveOpenErrors::kNoMetadata:
              case OfficeDriveOpenErrors::kInvalidAlternateUrl:
              case OfficeDriveOpenErrors::kDriveAlternateUrl:
              case OfficeDriveOpenErrors::kUnexpectedAlternateUrl:
              case OfficeDriveOpenErrors::kDriveDisabled:
              case OfficeDriveOpenErrors::kNoDriveService:
              case OfficeDriveOpenErrors::kDriveAuthenticationNotReady:
              case OfficeDriveOpenErrors::kMeteredConnection:
                break;
              case OfficeDriveOpenErrors::kSuccess:
                SetWrongValueLogged(drive_open_error);
                break;
            }
          }

        } else {
          ExpectLogged(one_drive_open_error);
          if (one_drive_open_error.logged()) {
            switch (one_drive_open_error.value) {
              case OfficeOneDriveOpenErrors::kOffline:
              case OfficeOneDriveOpenErrors::kNoProfile:
              case OfficeOneDriveOpenErrors::kNoFileSystemURL:
              case OfficeOneDriveOpenErrors::kInvalidFileSystemURL:
              case OfficeOneDriveOpenErrors::kGetActionsGenericError:
              case OfficeOneDriveOpenErrors::kGetActionsReauthRequired:
              case OfficeOneDriveOpenErrors::kGetActionsInvalidUrl:
              case OfficeOneDriveOpenErrors::kGetActionsNoUrl:
              case OfficeOneDriveOpenErrors::kGetActionsAccessDenied:
              case OfficeOneDriveOpenErrors::kGetActionsNoEmail:
              case OfficeOneDriveOpenErrors::kConversionToODFSUrlError:
              case OfficeOneDriveOpenErrors::kEmailsDoNotMatch:
                break;
              case OfficeOneDriveOpenErrors::kSuccess:
                SetWrongValueLogged(one_drive_open_error);
                break;
            }
          }
        }
      } else if (task_result.value == OfficeTaskResult::kOpened ||
                 task_result.value == OfficeTaskResult::kCopied ||
                 task_result.value == OfficeTaskResult::kMoved) {
        // The cloud open/upload flow was successful.
        // The OpenErrors should be success.
        if (google_drive) {
          ExpectLogged(drive_open_error);
          if (drive_open_error.logged()) {
            switch (drive_open_error.value) {
              case OfficeDriveOpenErrors::kSuccess:
                break;
              case OfficeDriveOpenErrors::kOffline:
              case OfficeDriveOpenErrors::kDriveFsInterface:
              case OfficeDriveOpenErrors::kTimeout:
              case OfficeDriveOpenErrors::kNoMetadata:
              case OfficeDriveOpenErrors::kInvalidAlternateUrl:
              case OfficeDriveOpenErrors::kDriveAlternateUrl:
              case OfficeDriveOpenErrors::kUnexpectedAlternateUrl:
              case OfficeDriveOpenErrors::kDriveDisabled:
              case OfficeDriveOpenErrors::kNoDriveService:
              case OfficeDriveOpenErrors::kDriveAuthenticationNotReady:
              case OfficeDriveOpenErrors::kMeteredConnection:
                SetWrongValueLogged(drive_open_error);
                break;
            }
          }
        } else {
          ExpectLogged(one_drive_open_error);
          if (one_drive_open_error.logged()) {
            switch (one_drive_open_error.value) {
              case OfficeOneDriveOpenErrors::kSuccess:
                break;
              case OfficeOneDriveOpenErrors::kOffline:
              case OfficeOneDriveOpenErrors::kNoProfile:
              case OfficeOneDriveOpenErrors::kNoFileSystemURL:
              case OfficeOneDriveOpenErrors::kInvalidFileSystemURL:
              case OfficeOneDriveOpenErrors::kGetActionsGenericError:
              case OfficeOneDriveOpenErrors::kGetActionsReauthRequired:
              case OfficeOneDriveOpenErrors::kGetActionsInvalidUrl:
              case OfficeOneDriveOpenErrors::kGetActionsNoUrl:
              case OfficeOneDriveOpenErrors::kGetActionsAccessDenied:
              case OfficeOneDriveOpenErrors::kGetActionsNoEmail:
              case OfficeOneDriveOpenErrors::kConversionToODFSUrlError:
              case OfficeOneDriveOpenErrors::kEmailsDoNotMatch:
                SetWrongValueLogged(one_drive_open_error);
                break;
            }
          }
        }
        ExpectLogged(transfer_required);
        if (task_result.value == OfficeTaskResult::kOpened) {
          // The cloud open flow was successful.
          ExpectNotLogged(upload_result);
          // TransferRequired should be kNotRequired.
          if (transfer_required.logged()) {
            switch (transfer_required.value) {
              case OfficeFilesTransferRequired::kNotRequired:
                break;
              case OfficeFilesTransferRequired::kMove:
              case OfficeFilesTransferRequired::kCopy:
                SetWrongValueLogged(transfer_required);
                break;
            }
          }
        } else {
          // The cloud upload flow was successful.
          // The UploadResult should be success.
          ExpectLogged(upload_result);
          if (upload_result.logged()) {
            switch (upload_result.value) {
              case OfficeFilesUploadResult::kSuccess:
              case OfficeFilesUploadResult::kSuccessAfterReauth:
                break;
              case OfficeFilesUploadResult::kOtherError:
              case OfficeFilesUploadResult::kFileSystemNotFound:
              case OfficeFilesUploadResult::kMoveOperationCancelled:
              case OfficeFilesUploadResult::kMoveOperationError:
              case OfficeFilesUploadResult::kMoveOperationNeedPassword:
              case OfficeFilesUploadResult::kCopyOperationCancelled:
              case OfficeFilesUploadResult::kCopyOperationError:
              case OfficeFilesUploadResult::kCopyOperationNeedPassword:
              case OfficeFilesUploadResult::kPinningFailedDiskFull:
              case OfficeFilesUploadResult::kCloudAccessDenied:
              case OfficeFilesUploadResult::kCloudMetadataError:
              case OfficeFilesUploadResult::kCloudQuotaFull:
              case OfficeFilesUploadResult::kCloudError:
              case OfficeFilesUploadResult::kNoConnection:
              case OfficeFilesUploadResult::kDestinationUrlError:
              case OfficeFilesUploadResult::kInvalidURL:
              case OfficeFilesUploadResult::kCloudReauthRequired:
              case OfficeFilesUploadResult::kInvalidAlternateUrl:
              case OfficeFilesUploadResult::kUnexpectedAlternateUrlHost:
              case OfficeFilesUploadResult::kSyncError:
              case OfficeFilesUploadResult::kSyncCancelledAndDeleted:
              case OfficeFilesUploadResult::kSyncCancelledAndTrashed:
              case OfficeFilesUploadResult::
                  kUploadNotStartedReauthenticationRequired:
                SetWrongValueLogged(upload_result);
                break;
            }
          }
          if (task_result.value == OfficeTaskResult::kCopied) {
            // The cloud upload (copy) flow was successful.
            // TransferRequired should be kCopy.
            if (transfer_required.logged()) {
              switch (transfer_required.value) {
                case OfficeFilesTransferRequired::kCopy:
                  break;
                case OfficeFilesTransferRequired::kNotRequired:
                case OfficeFilesTransferRequired::kMove:
                  SetWrongValueLogged(transfer_required);
                  break;
              }
            }
          } else {
            // The cloud upload (move) flow was successful.
            // TransferRequired should be kMove.
            if (transfer_required.logged()) {
              switch (transfer_required.value) {
                case OfficeFilesTransferRequired::kMove:
                  break;
                case OfficeFilesTransferRequired::kNotRequired:
                case OfficeFilesTransferRequired::kCopy:
                  SetWrongValueLogged(transfer_required);
                  break;
              }
            }
          }
        }
      }
    }
  }

  if (transfer_required.logged()) {
    ExpectLogged(source_volume);
    if (transfer_required.value == OfficeFilesTransferRequired::kNotRequired) {
      ExpectNotLogged(upload_result);
      // SourceVolume should match the CloudProvider.
      if (google_drive) {
        ExpectLogged(drive_open_error);
        if (source_volume.logged()) {
          switch (source_volume.value) {
            case OfficeFilesSourceVolume::kGoogleDrive:
              break;
            case OfficeFilesSourceVolume::kDownloadsDirectory:
            case OfficeFilesSourceVolume::kRemovableDiskPartition:
            case OfficeFilesSourceVolume::kMountedArchiveFile:
            case OfficeFilesSourceVolume::kProvided:
            case OfficeFilesSourceVolume::kMtp:
            case OfficeFilesSourceVolume::kMediaView:
            case OfficeFilesSourceVolume::kCrostini:
            case OfficeFilesSourceVolume::kAndriodFiles:
            case OfficeFilesSourceVolume::kDocumentsProvider:
            case OfficeFilesSourceVolume::kSmb:
            case OfficeFilesSourceVolume::kSystemInternal:
            case OfficeFilesSourceVolume::kGuestOS:
            case OfficeFilesSourceVolume::kUnknown:
            case OfficeFilesSourceVolume::kMicrosoftOneDrive:
            case OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider:
              SetWrongValueLogged(source_volume);
              break;
          }
        }
      } else {
        ExpectLogged(one_drive_open_error);
        if (source_volume.logged()) {
          switch (source_volume.value) {
            case OfficeFilesSourceVolume::kMicrosoftOneDrive:
            case OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider:
              break;
            case OfficeFilesSourceVolume::kDownloadsDirectory:
            case OfficeFilesSourceVolume::kRemovableDiskPartition:
            case OfficeFilesSourceVolume::kMountedArchiveFile:
            case OfficeFilesSourceVolume::kProvided:
            case OfficeFilesSourceVolume::kMtp:
            case OfficeFilesSourceVolume::kMediaView:
            case OfficeFilesSourceVolume::kCrostini:
            case OfficeFilesSourceVolume::kAndriodFiles:
            case OfficeFilesSourceVolume::kDocumentsProvider:
            case OfficeFilesSourceVolume::kSmb:
            case OfficeFilesSourceVolume::kSystemInternal:
            case OfficeFilesSourceVolume::kGuestOS:
            case OfficeFilesSourceVolume::kUnknown:
            case OfficeFilesSourceVolume::kGoogleDrive:
              SetWrongValueLogged(source_volume);
              break;
          }
        }
      }
    } else {
      // TransferRequired was kCopy or kMove.
      if (task_result.logged() &&
          task_result.value != OfficeTaskResult::kCancelledAtConfirmation) {
        // The cloud upload flow was not exited at the Move Confirmation Dialog.
        ExpectLogged(upload_result);
      }
      // SourceVolume should not match the CloudProvider.
      if (google_drive) {
        if (source_volume.logged()) {
          switch (source_volume.value) {
            case OfficeFilesSourceVolume::kDownloadsDirectory:
            case OfficeFilesSourceVolume::kRemovableDiskPartition:
            case OfficeFilesSourceVolume::kMountedArchiveFile:
            case OfficeFilesSourceVolume::kProvided:
            case OfficeFilesSourceVolume::kMtp:
            case OfficeFilesSourceVolume::kMediaView:
            case OfficeFilesSourceVolume::kCrostini:
            case OfficeFilesSourceVolume::kAndriodFiles:
            case OfficeFilesSourceVolume::kDocumentsProvider:
            case OfficeFilesSourceVolume::kSmb:
            case OfficeFilesSourceVolume::kSystemInternal:
            case OfficeFilesSourceVolume::kGuestOS:
            case OfficeFilesSourceVolume::kUnknown:
            case OfficeFilesSourceVolume::kMicrosoftOneDrive:
            case OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider:
              break;
            case OfficeFilesSourceVolume::kGoogleDrive:
              SetWrongValueLogged(source_volume);
              break;
          }
        }
      } else {
        if (source_volume.logged()) {
          switch (source_volume.value) {
            case OfficeFilesSourceVolume::kDownloadsDirectory:
            case OfficeFilesSourceVolume::kRemovableDiskPartition:
            case OfficeFilesSourceVolume::kMountedArchiveFile:
            case OfficeFilesSourceVolume::kProvided:
            case OfficeFilesSourceVolume::kMtp:
            case OfficeFilesSourceVolume::kMediaView:
            case OfficeFilesSourceVolume::kCrostini:
            case OfficeFilesSourceVolume::kAndriodFiles:
            case OfficeFilesSourceVolume::kDocumentsProvider:
            case OfficeFilesSourceVolume::kSmb:
            case OfficeFilesSourceVolume::kSystemInternal:
            case OfficeFilesSourceVolume::kGuestOS:
            case OfficeFilesSourceVolume::kUnknown:
            case OfficeFilesSourceVolume::kGoogleDrive:
              break;
            case OfficeFilesSourceVolume::kMicrosoftOneDrive:
            case OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider:
              SetWrongValueLogged(source_volume);
              break;
          }
        }
      }
    }
  }

  if (upload_result.logged()) {
    if (upload_result.value == OfficeFilesUploadResult::kCopyOperationError) {
      ExpectLogged(copy_error);
    } else if (upload_result.value ==
               OfficeFilesUploadResult::kMoveOperationError) {
      ExpectLogged(move_error);
    } else if (upload_result.value ==
               OfficeFilesUploadResult::
                   kUploadNotStartedReauthenticationRequired) {
      ExpectNotLogged(copy_error);
      ExpectNotLogged(move_error);
      // TaskResult should be kFailedToUpload.
      ExpectLogged(task_result);
      // UploadHandler tests don't log TaskResult.
      if (task_result.logged()) {
        switch (task_result.value) {
          case OfficeTaskResult::kFailedToUpload:
            break;
          case OfficeTaskResult::kFallbackQuickOffice:
          case OfficeTaskResult::kFallbackOther:
          case OfficeTaskResult::kOpened:
          case OfficeTaskResult::kMoved:
          case OfficeTaskResult::kCancelledAtConfirmation:
          case OfficeTaskResult::kFailedToOpen:
          case OfficeTaskResult::kCopied:
          case OfficeTaskResult::kCancelledAtFallback:
          case OfficeTaskResult::kCancelledAtSetup:
          case OfficeTaskResult::kLocalFileTask:
            SetWrongValueLogged(task_result);
            break;
        }
      }
    }
  }

  if (copy_error.logged() || move_error.logged()) {
    ExpectLogged(upload_result);
  }
}

// Check metric consistency and update metric states as required. Log the
// companion metrics with the final metric states. Dump without crashing if an
// inconsistency was found.
CloudOpenMetrics::~CloudOpenMetrics() {
  if (multiple_files_) {
    // TODO(b/242685536): Define CloudOpenMetrics for multiple files.
    return;
  }

  if (cloud_provider_ == CloudProvider::kGoogleDrive) {
    CheckForInconsistencies(drive_copy_error_, drive_move_error_,
                            drive_open_error_, one_drive_open_error_,
                            drive_source_volume_, drive_task_result_,
                            drive_transfer_required_, drive_upload_result_);
    ExpectNotLogged(one_drive_copy_error_);
    ExpectNotLogged(one_drive_move_error_);
    ExpectNotLogged(one_drive_open_error_);
    ExpectNotLogged(one_drive_source_volume_);
    ExpectNotLogged(one_drive_task_result_);
    ExpectNotLogged(one_drive_transfer_required_);
    ExpectNotLogged(one_drive_upload_result_);
  } else {
    CheckForInconsistencies(
        one_drive_copy_error_, one_drive_move_error_, drive_open_error_,
        one_drive_open_error_, one_drive_source_volume_, one_drive_task_result_,
        one_drive_transfer_required_, one_drive_upload_result_);
    ExpectNotLogged(drive_copy_error_);
    ExpectNotLogged(drive_move_error_);
    ExpectNotLogged(drive_open_error_);
    ExpectNotLogged(drive_source_volume_);
    ExpectNotLogged(drive_task_result_);
    ExpectNotLogged(drive_transfer_required_);
    ExpectNotLogged(drive_upload_result_);
  }

  drive_copy_error_.LogCompanionMetric();
  one_drive_copy_error_.LogCompanionMetric();
  drive_move_error_.LogCompanionMetric();
  one_drive_move_error_.LogCompanionMetric();
  drive_open_error_.LogCompanionMetric();
  one_drive_open_error_.LogCompanionMetric();
  drive_source_volume_.LogCompanionMetric();
  one_drive_source_volume_.LogCompanionMetric();
  drive_task_result_.LogCompanionMetric();
  one_drive_task_result_.LogCompanionMetric();
  drive_transfer_required_.LogCompanionMetric();
  one_drive_transfer_required_.LogCompanionMetric();
  drive_upload_result_.LogCompanionMetric();
  one_drive_upload_result_.LogCompanionMetric();

  if (delayed_dump_) {
    base::debug::DumpWithoutCrashing();
  }
}

void CloudOpenMetrics::LogCopyError(base::File::Error value) {
  LogAndCheckForInconsistency(drive_copy_error_, one_drive_copy_error_, value);
}

void CloudOpenMetrics::LogMoveError(base::File::Error value) {
  LogAndCheckForInconsistency(drive_move_error_, one_drive_move_error_, value);
}

void CloudOpenMetrics::LogGoogleDriveOpenError(OfficeDriveOpenErrors value) {
  if (!drive_open_error_.Log(value)) {
    OnInconsistencyFound(drive_open_error_);
  }
}

void CloudOpenMetrics::LogOneDriveOpenError(OfficeOneDriveOpenErrors value) {
  if (!one_drive_open_error_.Log(value)) {
    OnInconsistencyFound(one_drive_open_error_);
  }
}

void CloudOpenMetrics::LogSourceVolume(OfficeFilesSourceVolume value) {
  LogAndCheckForInconsistency(drive_source_volume_, one_drive_source_volume_,
                              value);
}

void CloudOpenMetrics::LogTaskResult(OfficeTaskResult value) {
  LogAndCheckForInconsistency(drive_task_result_, one_drive_task_result_,
                              value);
}

void CloudOpenMetrics::LogTransferRequired(OfficeFilesTransferRequired value) {
  LogAndCheckForInconsistency(drive_transfer_required_,
                              one_drive_transfer_required_, value);
}

void CloudOpenMetrics::LogUploadResult(OfficeFilesUploadResult value) {
  LogAndCheckForInconsistency(drive_upload_result_, one_drive_upload_result_,
                              value);
}

void CloudOpenMetrics::set_cloud_provider(CloudProvider cloud_provider) {
  cloud_provider_ = cloud_provider;
}

base::SafeRef<CloudOpenMetrics> CloudOpenMetrics::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

// For testing.
base::WeakPtr<CloudOpenMetrics> CloudOpenMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

template <typename MetricType>
void CloudOpenMetrics::OnInconsistencyFound(Metric<MetricType>& metric,
                                            bool immediately_dump) {
  if (multiple_files_) {
    // TODO(b/242685536): Define CloudOpenMetrics for multiple files.
    return;
  }
  LOG(ERROR) << "Inconsistent metric found: " << metric
             << ". ----- Cloud provider: " << cloud_provider_
             << ". ----- Drive metrics: " << drive_copy_error_ << ". "
             << drive_move_error_ << ". " << drive_open_error_ << ". "
             << drive_source_volume_ << ". " << drive_task_result_ << ". "
             << drive_transfer_required_ << ". " << drive_upload_result_
             << ". -----  OneDrive metrics: " << one_drive_copy_error_ << ". "
             << one_drive_move_error_ << ". " << one_drive_open_error_ << ". "
             << one_drive_source_volume_ << ". " << one_drive_task_result_
             << ". " << one_drive_transfer_required_ << ". "
             << one_drive_upload_result_ << ".";
  if (immediately_dump) {
    base::debug::DumpWithoutCrashing();
  } else {
    delayed_dump_ = true;
  }
}

template <typename MetricType>
void CloudOpenMetrics::ExpectNotLogged(Metric<MetricType>& metric) {
  if (!metric.IsNotLogged()) {
    OnInconsistencyFound(metric, /*immediately_dump=*/false);
  }
}

template <typename MetricType>
void CloudOpenMetrics::ExpectLogged(Metric<MetricType>& metric) {
  if (!metric.IsLogged()) {
    OnInconsistencyFound(metric, /*immediately_dump=*/false);
  }
}

template <typename MetricType>
void CloudOpenMetrics::SetWrongValueLogged(Metric<MetricType>& metric) {
  metric.set_state(MetricState::kWrongValueLogged);
  OnInconsistencyFound(metric, /*immediately_dump=*/false);
}

template <typename MetricType>
void CloudOpenMetrics::LogAndCheckForInconsistency(
    Metric<MetricType>& drive_metric,
    Metric<MetricType>& one_drive_metric,
    MetricType value) {
  if (cloud_provider_ == CloudProvider::kGoogleDrive) {
    if (!drive_metric.Log(value)) {
      OnInconsistencyFound(drive_metric);
    }
  } else if (!one_drive_metric.Log(value)) {
    OnInconsistencyFound(one_drive_metric);
  }
}

}  // namespace ash::cloud_upload
