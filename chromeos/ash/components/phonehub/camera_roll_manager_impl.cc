// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/camera_roll_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/camera_roll_item.h"
#include "chromeos/ash/components/phonehub/camera_roll_thumbnail_decoder_impl.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/message_sender.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash {
namespace phonehub {

namespace {

constexpr int kMaxCameraRollItemCount = 4;

}  // namespace

CameraRollManagerImpl::CameraRollManagerImpl(
    MessageReceiver* message_receiver,
    MessageSender* message_sender,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::ConnectionManager* connection_manager,
    std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager)
    : message_receiver_(message_receiver),
      message_sender_(message_sender),
      multidevice_setup_client_(multidevice_setup_client),
      connection_manager_(connection_manager),
      camera_roll_download_manager_(std::move(camera_roll_download_manager)),
      thumbnail_decoder_(std::make_unique<CameraRollThumbnailDecoderImpl>()) {
  message_receiver->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
  connection_manager_->AddObserver(this);
}

CameraRollManagerImpl::~CameraRollManagerImpl() {
  message_receiver_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
}

void CameraRollManagerImpl::DownloadItem(
    const proto::CameraRollItemMetadata& item_metadata) {
  proto::FetchCameraRollItemDataRequest request;
  *request.mutable_metadata() = item_metadata;
  message_sender_->SendFetchCameraRollItemDataRequest(request);
}

void CameraRollManagerImpl::OnFetchCameraRollItemDataResponseReceived(
    const proto::FetchCameraRollItemDataResponse& response) {
  if (response.file_availability() !=
      proto::FetchCameraRollItemDataResponse::AVAILABLE) {
    util::LogCameraRollDownloadResult(
        util::CameraRollDownloadResult::kFileNotAvailable);
    NotifyCameraRollDownloadError(
        CameraRollManager::Observer::DownloadErrorType::kGenericError,
        response.metadata());
    return;
  }

  camera_roll_download_manager_->CreatePayloadFiles(
      response.payload_id(), response.metadata(),
      base::BindOnce(&CameraRollManagerImpl::OnPayloadFilesCreated,
                     weak_ptr_factory_.GetWeakPtr(), response));
}

void CameraRollManagerImpl::OnPayloadFilesCreated(
    const proto::FetchCameraRollItemDataResponse& response,
    CameraRollDownloadManager::CreatePayloadFilesResult result,
    std::optional<secure_channel::mojom::PayloadFilesPtr> payload_files) {
  switch (result) {
    case CameraRollDownloadManager::CreatePayloadFilesResult::kSuccess:
      connection_manager_->RegisterPayloadFile(
          response.payload_id(), std::move(payload_files.value()),
          base::BindRepeating(&CameraRollManagerImpl::OnFileTransferUpdate,
                              weak_ptr_factory_.GetWeakPtr(),
                              response.metadata()),
          base::BindOnce(&CameraRollManagerImpl::OnPayloadFileRegistered,
                         weak_ptr_factory_.GetWeakPtr(), response.metadata(),
                         response.payload_id()));
      break;
    case CameraRollDownloadManager::CreatePayloadFilesResult::
        kInsufficientDiskSpace:
      util::LogCameraRollDownloadResult(
          util::CameraRollDownloadResult::kInsufficientDiskSpace);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kInsufficientStorage,
          response.metadata());
      break;
    case CameraRollDownloadManager::CreatePayloadFilesResult::kInvalidFileName:
      util::LogCameraRollDownloadResult(
          util::CameraRollDownloadResult::kInvalidFileName);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kGenericError,
          response.metadata());
      break;
    case CameraRollDownloadManager::CreatePayloadFilesResult::
        kPayloadAlreadyExists:
      util::LogCameraRollDownloadResult(
          util::CameraRollDownloadResult::kPayloadAlreadyExists);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kGenericError,
          response.metadata());
      break;
    case CameraRollDownloadManager::CreatePayloadFilesResult::
        kNotUniqueFilePath:
      util::LogCameraRollDownloadResult(
          util::CameraRollDownloadResult::kNotUniqueFilePath);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kGenericError,
          response.metadata());
      break;
  }
}

void CameraRollManagerImpl::OnPayloadFileRegistered(
    const proto::CameraRollItemMetadata& metadata,
    int64_t payload_id,
    bool success) {
  if (!success) {
    camera_roll_download_manager_->DeleteFile(payload_id);
    util::LogCameraRollDownloadResult(
        util::CameraRollDownloadResult::kTargetFileNotAccessible);
    NotifyCameraRollDownloadError(
        CameraRollManager::Observer::DownloadErrorType::kGenericError,
        metadata);
    return;
  }

  proto::InitiateCameraRollItemTransferRequest request;
  *request.mutable_metadata() = metadata;
  request.set_payload_id(payload_id);
  message_sender_->SendInitiateCameraRollItemTransferRequest(request);
}

void CameraRollManagerImpl::OnFileTransferUpdate(
    const proto::CameraRollItemMetadata& metadata,
    secure_channel::mojom::FileTransferUpdatePtr update) {
  switch (update->status) {
    case secure_channel::mojom::FileTransferStatus::kInProgress:
      break;
    case secure_channel::mojom::FileTransferStatus::kSuccess:
      util::LogCameraRollDownloadResult(
          util::CameraRollDownloadResult::kSuccess);
      break;
    case secure_channel::mojom::FileTransferStatus::kFailure:
      util::LogCameraRollDownloadResult(
          util::CameraRollDownloadResult::kTransferFailed);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kNetworkConnection,
          metadata);
      break;
    case secure_channel::mojom::FileTransferStatus::kCanceled:
      util::LogCameraRollDownloadResult(
          util::CameraRollDownloadResult::kTransferCanceled);
      NotifyCameraRollDownloadError(
          CameraRollManager::Observer::DownloadErrorType::kNetworkConnection,
          metadata);
      break;
  }

  camera_roll_download_manager_->UpdateDownloadProgress(std::move(update));
}

void CameraRollManagerImpl::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  UpdateCameraRollAccessStateAndNotifyIfNeeded(
      phone_status_snapshot.properties().camera_roll_access_state());
  if (!is_android_storage_granted_ || !IsCameraRollSettingEnabled()) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
    return;
  }

  SendFetchCameraRollItemsRequest();
}

void CameraRollManagerImpl::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  UpdateCameraRollAccessStateAndNotifyIfNeeded(
      phone_status_update.properties().camera_roll_access_state());
  if (!is_android_storage_granted_ || !IsCameraRollSettingEnabled()) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
    return;
  }

  if (phone_status_update.has_camera_roll_updates()) {
    SendFetchCameraRollItemsRequest();
  }
}

void CameraRollManagerImpl::OnFetchCameraRollItemsResponseReceived(
    const proto::FetchCameraRollItemsResponse& response) {
  thumbnail_decoder_->BatchDecode(
      response, current_items(),
      base::BindOnce(&CameraRollManagerImpl::OnItemThumbnailsDecoded,
                     thumbnail_decoder_weak_ptr_factory_.GetWeakPtr()));
}

void CameraRollManagerImpl::SendFetchCameraRollItemsRequest() {
  // Clears pending thumbnail decode requests to avoid changing the current item
  // set after sending it with the |FetchCameraRollItemsRequest|. These pending
  // thumbnails will be invalidated anyway when the new response is received.
  CancelPendingThumbnailRequests();

  // Do not update the timestamp if it is already set. It means that there's an
  // in-progress request. We want to measure the time it takes from the first
  // time we request an update to when the UI is updated. This is the time the
  // user spends waiting.
  if (!fetch_items_request_start_timestamp_) {
    fetch_items_request_start_timestamp_ = base::TimeTicks::Now();
  }

  proto::FetchCameraRollItemsRequest request;
  request.set_max_item_count(kMaxCameraRollItemCount);
  for (const CameraRollItem& current_item : current_items()) {
    *request.add_current_item_metadata() = current_item.metadata();
  }
  message_sender_->SendFetchCameraRollItemsRequest(request);
}

void CameraRollManagerImpl::OnItemThumbnailsDecoded(
    CameraRollThumbnailDecoder::BatchDecodeResult result,
    const std::vector<CameraRollItem>& items) {
  if (result == CameraRollThumbnailDecoder::BatchDecodeResult::kCompleted) {
    if (fetch_items_request_start_timestamp_) {
      base::UmaHistogramMediumTimes(
          "PhoneHub.CameraRoll.Latency.RefreshItems",
          base::TimeTicks::Now() - *fetch_items_request_start_timestamp_);
      fetch_items_request_start_timestamp_.reset();
    }
    SetCurrentItems(items);
  }
}

void CameraRollManagerImpl::CancelPendingThumbnailRequests() {
  thumbnail_decoder_weak_ptr_factory_.InvalidateWeakPtrs();
}

bool CameraRollManagerImpl::IsCameraRollSettingEnabled() {
  multidevice_setup::mojom::FeatureState camera_roll_feature_state =
      multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kPhoneHubCameraRoll);
  return camera_roll_feature_state ==
         multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

void CameraRollManagerImpl::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  if (!IsCameraRollSettingEnabled()) {
    // ClearCurrentItems() would also call ComputeAndUpdateUiState()
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
  } else {
    ComputeAndUpdateUiState();
  }
}

void CameraRollManagerImpl::OnConnectionStatusChanged() {
  if (connection_manager_->GetStatus() ==
      secure_channel::ConnectionManager::Status::kDisconnected) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
  }
}

void CameraRollManagerImpl::UpdateCameraRollAccessStateAndNotifyIfNeeded(
    const proto::CameraRollAccessState& access_state) {
  bool updated_storage_granted = access_state.storage_permission_granted();
  if (is_android_storage_granted_ != updated_storage_granted) {
    is_android_storage_granted_ = updated_storage_granted;

    util::LogCameraRollAndroidHasStorageAccessPermission(
        is_android_storage_granted_);
    ComputeAndUpdateUiState();
  }
}

void CameraRollManagerImpl::ComputeAndUpdateUiState() {
  if (!is_android_storage_granted_) {
    ui_state_ = CameraRollUiState::NO_STORAGE_PERMISSION;
    NotifyCameraRollViewUiStateUpdated();
    return;
  }

  multidevice_setup::mojom::FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kPhoneHubCameraRoll);
  switch (feature_state) {
    case multidevice_setup::mojom::FeatureState::kDisabledByUser:
      ui_state_ = CameraRollUiState::SHOULD_HIDE;
      ;
      break;
    case multidevice_setup::mojom::FeatureState::kEnabledByUser:
      if (current_items().empty()) {
        ui_state_ = CameraRollUiState::SHOULD_HIDE;
      } else {
        ui_state_ = CameraRollUiState::ITEMS_VISIBLE;
      }
      break;
    default:
      ui_state_ = CameraRollUiState::SHOULD_HIDE;
      break;
  }
  NotifyCameraRollViewUiStateUpdated();
}

}  // namespace phonehub
}  // namespace ash
