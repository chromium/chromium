// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/message_receiver.h"

namespace chromeos {
namespace phonehub {

MessageReceiver::MessageReceiver() = default;
MessageReceiver::~MessageReceiver() = default;

void MessageReceiver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MessageReceiver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MessageReceiver::NotifyPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  for (auto& observer : observer_list_)
    observer.OnPhoneStatusSnapshotReceived(phone_status_snapshot);
}

void MessageReceiver::NotifyPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  for (auto& observer : observer_list_)
    observer.OnPhoneStatusUpdateReceived(phone_status_update);
}

}  // namespace phonehub
}  // namespace chromeos
