// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_H_
#define COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_H_

#include <string_view>

#include "base/functional/callback.h"
#include "components/stream_transport/public/transport_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace stream_transport {

class MockTransportHandler : public TransportHandler {
 public:
  explicit MockTransportHandler(TransportSession* session = nullptr);
  ~MockTransportHandler() override;

  using TransportHandler::SendUpstreamMessage;

  MOCK_METHOD(void,
              OnMessage,
              (PayloadType payload_type, std::string_view serialized_payload),
              (override));
};

class CallbackTransportHandler : public TransportHandler {
 public:
  explicit CallbackTransportHandler(base::OnceClosure on_message_cb);
  ~CallbackTransportHandler() override;

  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  base::OnceClosure on_message_cb_;
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_H_
