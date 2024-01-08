// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/camera_roll_thumbnail_decoder.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash {
namespace phonehub {

class CameraRollDownloadManager;
class CameraRollItem;
class MessageSender;

// Manages camera roll items sent from the connected Android device.
class CameraRollManagerImpl
    : public CameraRollManager,
      public MessageReceiver::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public secure_channel::ConnectionManager::Observer {
 public:
  CameraRollManagerImpl(
      MessageReceiver* message_receiver,
      MessageSender* message_sender,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::ConnectionManager* connection_manager,
      std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager);
  ~CameraRollManagerImpl() override;

 private:
  friend class CameraRollManagerImplTest;

  // CameraRollManager:
  void DownloadItem(
      const proto::CameraRollItemMetadata& item_metadata) override;

  // MessageReceiver::Observer
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;
  void OnFetchCameraRollItemsResponseReceived(
      const proto::FetchCameraRollItemsResponse& response) override;
  void OnFetchCameraRollItemDataResponseReceived(
      const proto::FetchCameraRollItemDataResponse& response) override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  void SendFetchCameraRollItemsRequest();
  void OnItemThumbnailsDecoded(
      CameraRollThumbnailDecoder::BatchDecodeResult result,
      const std::vector<CameraRollItem>& items);
  void CancelPendingThumbnailRequests();

  void OnPayloadFilesCreated(
      const proto::FetchCameraRollItemDataResponse& response,
      CameraRollDownloadManager::CreatePayloadFilesResult result,
      std::optional<secure_channel::mojom::PayloadFilesPtr> payload_files);
  void OnPayloadFileRegistered(const proto::CameraRollItemMetadata& metadata,
                               int64_t payload_id,
                               bool success);
  void OnFileTransferUpdate(
      const proto::CameraRollItemMetadata& metadata,
      secure_channel::mojom::FileTransferUpdatePtr update);

  bool IsCameraRollSettingEnabled();
  void UpdateCameraRollAccessStateAndNotifyIfNeeded(
      const proto::CameraRollAccessState& access_state);
  void ComputeAndUpdateUiState() override;

  bool is_android_feature_enabled_ = false;
  bool is_android_storage_granted_ = false;
  std::optional<base::TimeTicks> fetch_items_request_start_timestamp_;

  raw_ptr<MessageReceiver> message_receiver_;
  raw_ptr<MessageSender> message_sender_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_;

  std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager_;
  std::unique_ptr<CameraRollThumbnailDecoder> thumbnail_decoder_;

  base::WeakPtrFactory<CameraRollManagerImpl> weak_ptr_factory_{this};
  // WeakPtrFactory dedicated to thumbanil decoder callbacks that need to be
  // invalidated when the current item set updates.
  base::WeakPtrFactory<CameraRollManagerImpl>
      thumbnail_decoder_weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_IMPL_H_
