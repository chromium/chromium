// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_

#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

class FakeMessageReceiver : public MessageReceiver {
 public:
  FakeMessageReceiver() = default;
  ~FakeMessageReceiver() override = default;

  using MessageReceiver::NotifyPhoneStatusSnapshotReceived;
  using MessageReceiver::NotifyPhoneStatusUpdateReceived;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_
