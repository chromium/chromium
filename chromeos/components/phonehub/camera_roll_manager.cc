// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager.h"

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

}  // namespace

CameraRollManager::CameraRollManager(MessageReceiver* message_receiver,
                                     MessageSender* message_sender)
    : message_receiver_(message_receiver),
      message_sender_(message_sender),
      thumbnail_decoder_(std::make_unique<CameraRollThumbnailDecoderImpl>()) {
  message_receiver->AddObserver(this);
}

CameraRollManager::~CameraRollManager() {
  message_receiver_->RemoveObserver(this);
}

void CameraRollManager::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  if (!IsCameraRollSupportedOnAndroidDevice(
          phone_status_snapshot.properties().camera_roll_access_state())) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
    return;
  }

  SendFetchCameraRollItemsRequest();
}

void CameraRollManager::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  if (!IsCameraRollSupportedOnAndroidDevice(
          phone_status_update.properties().camera_roll_access_state())) {
    ClearCurrentItems();
    CancelPendingThumbnailRequests();
    return;
  }

  if (phone_status_update.has_camera_roll_updates()) {
    SendFetchCameraRollItemsRequest();
  }
}

void CameraRollManager::SendFetchCameraRollItemsRequest() {
  // Clears pending thumbnail decode requests to avoid changing the current item
  // set after sending it with the |FetchCameraRollItemsRequest|. These pending
  // thumbnails will be invalidated anyway when the new response is received.
  CancelPendingThumbnailRequests();

  proto::FetchCameraRollItemsRequest request;
  for (const CameraRollItem& current_item : current_items_) {
    *request.add_current_item_metadata() = current_item.metadata();
  }
  message_sender_->SendFetchCameraRollItemsRequest(request);
}

void CameraRollManager::ClearCurrentItems() {
  if (current_items_.empty()) {
    return;
  }

  current_items_.clear();
  for (auto& observer : observer_list_) {
    observer.OnCameraRollItemsChanged();
  }
}

void CameraRollManager::OnFetchCameraRollItemsResponseReceived(
    const proto::FetchCameraRollItemsResponse& response) {
  thumbnail_decoder_->BatchDecode(
      response, current_items(),
      base::BindOnce(&CameraRollManager::OnItemThumbnailsDecoded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CameraRollManager::OnItemThumbnailsDecoded(
    CameraRollThumbnailDecoder::BatchDecodeResult result,
    const std::vector<CameraRollItem>& items) {
  if (result == CameraRollThumbnailDecoder::BatchDecodeResult::kSuccess) {
    current_items_ = items;
    // The phone only sends FetchCameraRollItemsResponse when the set of items
    // has changed. Always alert the observers in this case.
    for (auto& observer : observer_list_) {
      observer.OnCameraRollItemsChanged();
    }
  }
  // TODO(http://crbug.com/1221297): log and handle failed decode requests.
}

void CameraRollManager::CancelPendingThumbnailRequests() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void CameraRollManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CameraRollManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace phonehub
}  // namespace chromeos
