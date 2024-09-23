// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_SERVICE_H_
#define COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_SERVICE_H_

#include <optional>

#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "components/sharing_message/sharing_message_sender.h"
#include "components/sharing_message/sharing_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSharingService : public SharingService {
 public:
  MockSharingService();

  MockSharingService(const MockSharingService&) = delete;
  MockSharingService& operator=(const MockSharingService&) = delete;

  ~MockSharingService() override;

  MOCK_CONST_METHOD1(
      GetDeviceCandidates,
      std::vector<SharingTargetDeviceInfo>(
          sync_pb::SharingSpecificFields::EnabledFeatures required_feature));

  MOCK_METHOD4(
      SendMessageToDevice,
      base::OnceClosure(const SharingTargetDeviceInfo& device,
                        base::TimeDelta response_timeout,
                        components_sharing_message::SharingMessage message,
                        SharingMessageSender::ResponseCallback callback));

  MOCK_CONST_METHOD1(
      GetDeviceByGuid,
      std::optional<SharingTargetDeviceInfo>(const std::string& guid));

  MOCK_METHOD2(RegisterSharingHandler,
               void(std::unique_ptr<SharingMessageHandler> handler,
                    components_sharing_message::SharingMessage::PayloadCase
                        payload_case));

  MOCK_METHOD1(UnregisterSharingHandler,
               void(components_sharing_message::SharingMessage::PayloadCase
                        payload_case));
};

#endif  // COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_SERVICE_H_
