// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_MESSAGE_SENDER_H_
#define COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_MESSAGE_SENDER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_sender.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSharingMessageSender : public SharingMessageSender {
 public:
  MockSharingMessageSender();
  MockSharingMessageSender(const MockSharingMessageSender&) = delete;
  MockSharingMessageSender& operator=(const MockSharingMessageSender&) = delete;
  ~MockSharingMessageSender() override;

  MOCK_METHOD5(SendMessageToDevice,
               base::OnceClosure(const SharingTargetDeviceInfo&,
                                 base::TimeDelta,
                                 components_sharing_message::SharingMessage,
                                 DelegateType,
                                 ResponseCallback));

  MOCK_METHOD2(OnAckReceived,
               void(const std::string& fcm_message_id,
                    std::unique_ptr<components_sharing_message::ResponseMessage>
                        response));

  MOCK_METHOD4(SendUnencryptedMessageToDevice,
               base::OnceClosure(const SharingTargetDeviceInfo&,
                                 sync_pb::UnencryptedSharingMessage,
                                 DelegateType,
                                 ResponseCallback));
};

#endif  // COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_MESSAGE_SENDER_H_
