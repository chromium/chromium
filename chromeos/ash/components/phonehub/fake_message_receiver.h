// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_

#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

class FakeMessageReceiver : public MessageReceiver {
 public:
  FakeMessageReceiver() = default;
  ~FakeMessageReceiver() override = default;

  using MessageReceiver::NotifyAppListIncrementalUpdateReceived;
  using MessageReceiver::NotifyAppListUpdateReceived;
  using MessageReceiver::NotifyAppStreamUpdateReceived;
  using MessageReceiver::NotifyFeatureSetupResponseReceived;
  using MessageReceiver::NotifyFetchCameraRollItemDataResponseReceived;
  using MessageReceiver::NotifyFetchCameraRollItemsResponseReceived;
  using MessageReceiver::NotifyPhoneStatusSnapshotReceived;
  using MessageReceiver::NotifyPhoneStatusUpdateReceived;
  using MessageReceiver::NotifyPingResponseReceived;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_
