// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "chromeos/components/phonehub/camera_roll_thumbnail_decoder_impl.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/message_sender.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace phonehub {
namespace {

bool IsCameraRollSupportedOnAndroidDevice(
    const proto::CameraRollAccessState& access_state) {
  return access_state.feature_enabled() &&
         access_state.storage_permission_granted();
}

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
}

CameraRollManagerImpl::~CameraRollManagerImpl() {
  message_receiver_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
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
    // TODO(http://crbug.com/1221297): notify the user that the item cannot be
    // downloaded.
    return;
  }

  camera_roll_download_manager_->CreatePayloadFiles(
      response.payload_id(), response.metadata(),
      base::BindOnce(&CameraRollManagerImpl::OnPayloadFilesCreated,
                     weak_ptr_factory_.GetWeakPtr(), response));
}

void CameraRollManagerImpl::OnPayloadFilesCreated(
    const proto::FetchCameraRollItemDataResponse& response,
    absl::optional<secure_channel::mojom::PayloadFilesPtr> payload_files) {
  if (!payload_files) {
    // TODO(http://crbug.com/1221297): notify the user that the item cannot be
    // downloaded.
    return;
  }

  connection_manager_->RegisterPayloadFile(
      response.payload_id(), std::move(payload_files.value()),
      base::BindRepeating(&CameraRollManagerImpl::OnFileTransferUpdate,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CameraRollManagerImpl::OnPayloadFileRegistered,
                     weak_ptr_factory_.GetWeakPtr(), response.metadata(),
                     response.payload_id()));
}

void CameraRollManagerImpl::OnPayloadFileRegistered(
    const proto::CameraRollItemMetadata& metadata,
    int64_t payload_id,
    bool success) {
  if (!success) {
    camera_roll_download_manager_->DeleteFile(payload_id);
    // TODO(http://crbug.com/1221297): notify the user that the item cannot be
    // downloaded.
    return;
  }

  proto::InitiateCameraRollItemTransferRequest request;
  *request.mutable_metadata() = metadata;
  request.set_payload_id(payload_id);
  message_sender_->SendInitiateCameraRollItemTransferRequest(request);
}

void CameraRollManagerImpl::OnFileTransferUpdate(
    chromeos::secure_channel::mojom::FileTransferUpdatePtr update) {
  camera_roll_download_manager_->UpdateDownloadProgress(std::move(update));
}

void CameraRollManagerImpl::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  if (!IsCameraRollSupportedOnAndroidDevice(
          phone_status_snapshot.properties().camera_roll_access_state()) ||
      !IsCameraRollSettingEnabled()) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
    return;
  }

  SendFetchCameraRollItemsRequest();
}

void CameraRollManagerImpl::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  if (!IsCameraRollSupportedOnAndroidDevice(
          phone_status_update.properties().camera_roll_access_state()) ||
      !IsCameraRollSettingEnabled()) {
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
                     weak_ptr_factory_.GetWeakPtr()));
}

void CameraRollManagerImpl::SendFetchCameraRollItemsRequest() {
  // Clears pending thumbnail decode requests to avoid changing the current item
  // set after sending it with the |FetchCameraRollItemsRequest|. These pending
  // thumbnails will be invalidated anyway when the new response is received.
  CancelPendingThumbnailRequests();

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
  if (result == CameraRollThumbnailDecoder::BatchDecodeResult::kSuccess) {
    SetCurrentItems(items);
  }
  // TODO(http://crbug.com/1221297): log and handle failed decode requests.
}

void CameraRollManagerImpl::CancelPendingThumbnailRequests() {
  weak_ptr_factory_.InvalidateWeakPtrs();
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
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
  }
}

}  // namespace phonehub
}  // namespace chromeos
