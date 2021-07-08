// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/camera_roll_thumbnail_decoder.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

class CameraRollItem;
class MessageSender;

// Manages camera roll items sent from the connected Android device.
class CameraRollManager : public MessageReceiver::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Notifies observers that the set of current camera roll items has changed.
    // The item set can be retrieved from CameraRollManager::GetCurrentItems().
    virtual void OnCameraRollItemsChanged() = 0;
  };

  CameraRollManager(MessageReceiver* message_receiver,
                    MessageSender* message_sender);
  CameraRollManager(const CameraRollManager&) = delete;
  CameraRollManager& operator=(const CameraRollManager&) = delete;
  ~CameraRollManager() override;

  // Returns the set of current camera roll items in the order in which they
  // should be displayed
  const std::vector<CameraRollItem>& current_items() const {
    return current_items_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class CameraRollManagerTest;

  // MessageReceiver::Observer
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;
  void OnFetchCameraRollItemsResponseReceived(
      const proto::FetchCameraRollItemsResponse& response) override;

  void SendFetchCameraRollItemsRequest();
  void ClearCurrentItems();
  void OnItemThumbnailsDecoded(
      CameraRollThumbnailDecoder::BatchDecodeResult result,
      const std::vector<CameraRollItem>& items);
  void CancelPendingThumbnailRequests();

  MessageReceiver* message_receiver_;
  MessageSender* message_sender_;

  std::vector<CameraRollItem> current_items_;

  base::ObserverList<Observer> observer_list_;

  std::unique_ptr<CameraRollThumbnailDecoder> thumbnail_decoder_;
  // Contains pending callbacks passed to the |CameraRollThumbnailDecoder|.
  base::WeakPtrFactory<CameraRollManager> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
