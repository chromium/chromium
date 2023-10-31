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
      return os << "Correctly not logged";
    case MetricState::kCorrectlyLogged:
      return os << "Correctly logged";
    case MetricState::kIncorrectlyNotLogged:
      return os << "Incorrectly not logged";
    case MetricState::kIncorrectlyLogged:
      return os << "Incorrectly logged";
    case MetricState::kIncorrectlyLoggedMultipleTimes:
      return os << "Incorrectly logged multiple times";
    case MetricState::kWrongValueLogged:
      return os << "Wrong value logged";
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
      copy_error_(cloud_provider_ == CloudProvider::kGoogleDrive
                      ? kGoogleDriveCopyErrorMetricName
                      : kOneDriveCopyErrorMetricName),
      move_error_(cloud_provider_ == CloudProvider::kGoogleDrive
                      ? kGoogleDriveMoveErrorMetricName
                      : kOneDriveMoveErrorMetricName),
      drive_open_error_(kDriveErrorMetricName),
      one_drive_open_error_(kOneDriveErrorMetricName),
      source_volume_(cloud_provider_ == CloudProvider::kGoogleDrive
                         ? kDriveOpenSourceVolumeMetric
                         : kOneDriveOpenSourceVolumeMetric),
      task_result_(cloud_provider_ == CloudProvider::kGoogleDrive
                       ? kGoogleDriveTaskResultMetricName
                       : kOneDriveTaskResultMetricName),
      transfer_required_(cloud_provider_ == CloudProvider::kGoogleDrive
                             ? kDriveTransferRequiredMetric
                             : kOneDriveTransferRequiredMetric),
      upload_result_(cloud_provider_ == CloudProvider::kGoogleDrive
                         ? kGoogleDriveUploadResultMetricName
                         : kOneDriveUploadResultMetricName) {}

// Check metric consistency and update metric states as required. Log the
// companion metrics with the final metric states. Dump without crashing if an
// inconsistency was found.
CloudOpenMetrics::~CloudOpenMetrics() {
  if (multiple_files_) {
    // TODO(b/242685536): Define CloudOpenMetrics for multiple files.
    return;
  }

  bool google_drive = cloud_provider_ == CloudProvider::kGoogleDrive;
  ExpectLogged(task_result_);
  if (task_result_.logged()) {
    if (task_result_.value == OfficeTaskResult::kFallbackQuickOffice ||
        task_result_.value == OfficeTaskResult::kCancelledAtFallback ||
        task_result_.value == OfficeTaskResult::kCancelledAtSetup ||
        task_result_.value == OfficeTaskResult::kLocalFileTask) {
      ExpectNotLogged(transfer_required_);
      ExpectNotLogged(upload_result_);
      if (task_result_.value == OfficeTaskResult::kFallbackQuickOffice ||
          task_result_.value == OfficeTaskResult::kCancelledAtFallback) {
        if (google_drive) {
          ExpectLogged(drive_open_error_);
          if (drive_open_error_.logged()) {
            switch (drive_open_error_.value) {
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
                SetWrongValueLogged(drive_open_error_);
                break;
            }
          }
        } else {
          ExpectLogged(one_drive_open_error_);
          if (one_drive_open_error_.logged()) {
            switch (one_drive_open_error_.value) {
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
                SetWrongValueLogged(one_drive_open_error_);
                break;
            }
          }
        }
      }
    } else {
      ExpectLogged(source_volume_);
      ExpectLogged(transfer_required_);
      if (task_result_.value == OfficeTaskResult::kCancelledAtConfirmation) {
        ExpectNotLogged(upload_result_);
        ExpectNotLogged(drive_open_error_);
        ExpectNotLogged(one_drive_open_error_);
        ExpectLogged(transfer_required_);
        if (transfer_required_.logged()) {
          switch (transfer_required_.value) {
            case OfficeFilesTransferRequired::kMove:
            case OfficeFilesTransferRequired::kCopy:
              break;
            case OfficeFilesTransferRequired::kNotRequired:
              SetWrongValueLogged(transfer_required_);
              break;
          }
        }
      } else if (task_result_.value == OfficeTaskResult::kFailedToOpen) {
        if (google_drive) {
          ExpectLogged(drive_open_error_);
          if (drive_open_error_.logged()) {
            switch (drive_open_error_.value) {
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
                SetWrongValueLogged(drive_open_error_);
                break;
            }
          }

        } else {
          ExpectLogged(one_drive_open_error_);
          if (one_drive_open_error_.logged()) {
            switch (one_drive_open_error_.value) {
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
                SetWrongValueLogged(one_drive_open_error_);
                break;
            }
          }
        }
      } else if (task_result_.value == OfficeTaskResult::kOpened ||
                 task_result_.value == OfficeTaskResult::kCopied ||
                 task_result_.value == OfficeTaskResult::kMoved) {
        if (google_drive) {
          ExpectLogged(drive_open_error_);
          if (drive_open_error_.logged()) {
            switch (drive_open_error_.value) {
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
                SetWrongValueLogged(drive_open_error_);
                break;
            }
          }
        } else {
          ExpectLogged(one_drive_open_error_);
          if (one_drive_open_error_.logged()) {
            switch (one_drive_open_error_.value) {
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
                SetWrongValueLogged(one_drive_open_error_);
                break;
            }
          }
        }
        ExpectLogged(transfer_required_);
        if (task_result_.value == OfficeTaskResult::kOpened) {
          ExpectNotLogged(upload_result_);
          if (transfer_required_.logged()) {
            switch (transfer_required_.value) {
              case OfficeFilesTransferRequired::kNotRequired:
                break;
              case OfficeFilesTransferRequired::kMove:
              case OfficeFilesTransferRequired::kCopy:
                SetWrongValueLogged(transfer_required_);
                break;
            }
          }
        } else {
          if (task_result_.value == OfficeTaskResult::kCopied) {
            if (transfer_required_.logged()) {
              switch (transfer_required_.value) {
                case OfficeFilesTransferRequired::kCopy:
                  break;
                case OfficeFilesTransferRequired::kNotRequired:
                case OfficeFilesTransferRequired::kMove:
                  SetWrongValueLogged(transfer_required_);
                  break;
              }
            }
          } else {
            if (transfer_required_.logged()) {
              switch (transfer_required_.value) {
                case OfficeFilesTransferRequired::kMove:
                  break;
                case OfficeFilesTransferRequired::kNotRequired:
                case OfficeFilesTransferRequired::kCopy:
                  SetWrongValueLogged(transfer_required_);
                  break;
              }
            }
          }
          ExpectLogged(upload_result_);
          if (upload_result_.logged()) {
            switch (upload_result_.value) {
              case OfficeFilesUploadResult::kSuccess:
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
                SetWrongValueLogged(upload_result_);
                break;
            }
          }
        }
      }
    }
  }

  if (transfer_required_.logged()) {
    ExpectLogged(source_volume_);
    if (transfer_required_.value == OfficeFilesTransferRequired::kNotRequired) {
      ExpectNotLogged(upload_result_);
      if (google_drive) {
        ExpectLogged(drive_open_error_);
        if (source_volume_.logged()) {
          switch (source_volume_.value) {
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
              SetWrongValueLogged(source_volume_);
              break;
          }
        }
      } else {
        ExpectLogged(one_drive_open_error_);
        if (source_volume_.logged()) {
          switch (source_volume_.value) {
            case OfficeFilesSourceVolume::kMicrosoftOneDrive:
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
              SetWrongValueLogged(source_volume_);
              break;
          }
        }
      }
    } else {
      if (task_result_.logged() &&
          task_result_.value != OfficeTaskResult::kCancelledAtConfirmation) {
        ExpectLogged(upload_result_);
      }
      if (google_drive) {
        if (source_volume_.logged()) {
          switch (source_volume_.value) {
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
              break;
            case OfficeFilesSourceVolume::kGoogleDrive:
              SetWrongValueLogged(source_volume_);
              break;
          }
        }
      } else {
        if (source_volume_.logged()) {
          switch (source_volume_.value) {
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
              SetWrongValueLogged(source_volume_);
              break;
          }
        }
      }
    }
  }

  if (upload_result_.logged()) {
    if (upload_result_.value == OfficeFilesUploadResult::kCopyOperationError) {
      ExpectLogged(copy_error_);
    } else if (upload_result_.value ==
               OfficeFilesUploadResult::kMoveOperationError) {
      ExpectLogged(move_error_);
    }
  }

  if (copy_error_.logged() || move_error_.logged()) {
    ExpectLogged(upload_result_);
  }

  if (google_drive) {
    base::UmaHistogramEnumeration(kGoogleDriveCopyErrorMetricStateMetricName,
                                  copy_error_.state);
    base::UmaHistogramEnumeration(kGoogleDriveMoveErrorMetricStateMetricName,
                                  move_error_.state);
    base::UmaHistogramEnumeration(kDriveErrorMetricStateMetricName,
                                  drive_open_error_.state);
    base::UmaHistogramEnumeration(kDriveOpenSourceVolumeMetricStateMetric,
                                  source_volume_.state);
    base::UmaHistogramEnumeration(kGoogleDriveTaskResultMetricStateMetricName,
                                  task_result_.state);
    base::UmaHistogramEnumeration(kDriveTransferRequiredMetricStateMetric,
                                  transfer_required_.state);
    base::UmaHistogramEnumeration(kGoogleDriveUploadResultMetricStateMetricName,
                                  upload_result_.state);
  } else {
    base::UmaHistogramEnumeration(kOneDriveCopyErrorMetricStateMetricName,
                                  copy_error_.state);
    base::UmaHistogramEnumeration(kOneDriveMoveErrorMetricStateMetricName,
                                  move_error_.state);
    base::UmaHistogramEnumeration(kOneDriveErrorMetricStateMetricName,
                                  one_drive_open_error_.state);
    base::UmaHistogramEnumeration(kOneDriveOpenSourceVolumeMetricStateMetric,
                                  source_volume_.state);
    base::UmaHistogramEnumeration(kOneDriveTaskResultMetricStateMetricName,
                                  task_result_.state);
    base::UmaHistogramEnumeration(kOneDriveTransferRequiredMetricStateMetric,
                                  transfer_required_.state);
    base::UmaHistogramEnumeration(kOneDriveUploadResultMetricStateMetricName,
                                  upload_result_.state);
  }

  if (delayed_dump_) {
    base::debug::DumpWithoutCrashing();
  }
}

void CloudOpenMetrics::LogCopyError(base::File::Error value) {
  if (!copy_error_.Log(value)) {
    OnInconsistencyFound(copy_error_);
  }
}

void CloudOpenMetrics::LogMoveError(base::File::Error value) {
  if (!move_error_.Log(value)) {
    OnInconsistencyFound(move_error_);
  }
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
  if (!source_volume_.Log(value)) {
    OnInconsistencyFound(source_volume_);
  }
}

void CloudOpenMetrics::LogTaskResult(OfficeTaskResult value) {
  if (!task_result_.Log(value)) {
    OnInconsistencyFound(task_result_);
  }
}

void CloudOpenMetrics::LogTransferRequired(OfficeFilesTransferRequired value) {
  if (!transfer_required_.Log(value)) {
    OnInconsistencyFound(transfer_required_);
  }
}

void CloudOpenMetrics::LogUploadResult(OfficeFilesUploadResult value) {
  if (!upload_result_.Log(value)) {
    OnInconsistencyFound(upload_result_);
  }
}

void CloudOpenMetrics::UpdateCloudProvider(CloudProvider cloud_provider) {
  cloud_provider_ = cloud_provider;
  if (cloud_provider == CloudProvider::kGoogleDrive) {
    copy_error_.set_metric_name(kGoogleDriveCopyErrorMetricName);
    move_error_.set_metric_name(kGoogleDriveMoveErrorMetricName);
    source_volume_.set_metric_name(kDriveOpenSourceVolumeMetric);
    task_result_.set_metric_name(kGoogleDriveTaskResultMetricName);
    transfer_required_.set_metric_name(kDriveTransferRequiredMetric);
    upload_result_.set_metric_name(kGoogleDriveUploadResultMetricName);
  } else if (cloud_provider == CloudProvider::kOneDrive) {
    copy_error_.set_metric_name(kOneDriveCopyErrorMetricName);
    move_error_.set_metric_name(kOneDriveMoveErrorMetricName);
    source_volume_.set_metric_name(kOneDriveOpenSourceVolumeMetric);
    task_result_.set_metric_name(kOneDriveTaskResultMetricName);
    transfer_required_.set_metric_name(kOneDriveTransferRequiredMetric);
    upload_result_.set_metric_name(kOneDriveUploadResultMetricName);
  }
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
  LOG(ERROR) << "Inconsistent metric found: " << metric;
  LOG(ERROR) << "Metrics: " << std::endl
             << copy_error_ << std::endl
             << move_error_ << std::endl
             << drive_open_error_ << std::endl
             << one_drive_open_error_ << std::endl
             << source_volume_ << std::endl
             << task_result_ << std::endl
             << transfer_required_ << std::endl
             << upload_result_;
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

}  // namespace ash::cloud_upload
