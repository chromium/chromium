// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_CHANNEL_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_CHANNEL_H_

#include "base/memory/weak_ptr.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_actuator {

class MockTransportChannel : public TransportChannel {
 public:
  MockTransportChannel();
  ~MockTransportChannel() override;

  MOCK_METHOD(TransportHandlerFactoryRegistry*,
              GetHandlerFactoryRegistry,
              (),
              (override));
  MOCK_METHOD(TransportSessionRegistry*, GetSessionRegistry, (), (override));
  MOCK_METHOD(void,
              SendUpstreamMessage,
              (std::string_view session_id,
               PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));

  base::WeakPtr<MockTransportChannel> GetWeakPtr();

 private:
  base::WeakPtrFactory<MockTransportChannel> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_CHANNEL_H_
