// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

// Responsible for receiving message updates from the remote phone device.
namespace chromeos {
namespace phonehub {

class MessageReceiver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the remote phone's snapshot has been updated which includes
    // phone properties and notification updates.
    virtual void OnPhoneStatusSnapshotReceived(
        proto::PhoneStatusSnapshot phone_status_snapshot) {}

    // Called when the remote phone status has been updated. Include phone
    // properties, updated notifications, and removed notifications.
    virtual void OnPhoneStatusUpdateReceived(
        proto::PhoneStatusUpdate phone_status_update) {}
  };

  MessageReceiver(const MessageReceiver&) = delete;
  MessageReceiver& operator=(const MessageReceiver&) = delete;
  virtual ~MessageReceiver();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  MessageReceiver();

  void NotifyPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot);
  void NotifyPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_RECEIVER_H_