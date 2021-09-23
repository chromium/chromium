// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/camera_roll_manager.h"
#include "chromeos/components/phonehub/camera_roll_thumbnail_decoder.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {
namespace phonehub {

class CameraRollItem;
class MessageSender;

// Manages camera roll items sent from the connected Android device.
class CameraRollManagerImpl
    : public CameraRollManager,
      public MessageReceiver::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  CameraRollManagerImpl(
      MessageReceiver* message_receiver,
      MessageSender* message_sender,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~CameraRollManagerImpl() override;

 private:
  friend class CameraRollManagerImplTest;

  // MessageReceiver::Observer
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;
  void OnFetchCameraRollItemsResponseReceived(
      const proto::FetchCameraRollItemsResponse& response) override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  void SendFetchCameraRollItemsRequest();
  void OnItemThumbnailsDecoded(
      CameraRollThumbnailDecoder::BatchDecodeResult result,
      const std::vector<CameraRollItem>& items);
  void CancelPendingThumbnailRequests();
  bool IsCameraRollSettingEnabled();

  MessageReceiver* message_receiver_;
  MessageSender* message_sender_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  std::unique_ptr<CameraRollThumbnailDecoder> thumbnail_decoder_;
  base::WeakPtrFactory<CameraRollManagerImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_IMPL_H_
