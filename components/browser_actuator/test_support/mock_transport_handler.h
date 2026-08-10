// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_H_

#include <string_view>

#include "base/functional/callback.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator {

class MockTransportHandler : public TransportHandler {
 public:
  MockTransportHandler();
  ~MockTransportHandler() override;

  MOCK_METHOD(void,
              OnMessage,
              (const google::protobuf::MessageLite& message),
              (override));
};

class CallbackTransportHandler : public TransportHandler {
 public:
  explicit CallbackTransportHandler(base::OnceClosure on_message_cb);
  ~CallbackTransportHandler() override;

  void OnMessage(const google::protobuf::MessageLite& message) override;

 private:
  base::OnceClosure on_message_cb_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_TRANSPORT_HANDLER_H_
