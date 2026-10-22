// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TRANSPORT_MOCK_TRANSPORT_CHANNEL_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TRANSPORT_MOCK_TRANSPORT_CHANNEL_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "components/browser_actuator/public/transport/transport_channel.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_actuator::transport {

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
               const google::protobuf::MessageLite& message,
               std::optional<int64_t> responding_to_sequence_number),
              (override));

  base::WeakPtr<MockTransportChannel> GetWeakPtr();

 private:
  base::WeakPtrFactory<MockTransportChannel> weak_ptr_factory_{this};
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_TRANSPORT_MOCK_TRANSPORT_CHANNEL_H_
