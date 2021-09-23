// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "chromeos/components/phonehub/camera_roll_thumbnail_decoder_impl.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/message_sender.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

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
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : message_receiver_(message_receiver),
      message_sender_(message_sender),
      multidevice_setup_client_(multidevice_setup_client),
      thumbnail_decoder_(std::make_unique<CameraRollThumbnailDecoderImpl>()) {
  message_receiver->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
}

CameraRollManagerImpl::~CameraRollManagerImpl() {
  message_receiver_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
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
