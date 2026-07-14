// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TEST_SUPPORT_MOCK_STREAM_CONNECTION_DELEGATE_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TEST_SUPPORT_MOCK_STREAM_CONNECTION_DELEGATE_H_

#include <memory>
#include <string>

#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_actuator {

class MockStreamConnectionDelegate : public StreamConnectionDelegate {
 public:
  MockStreamConnectionDelegate();
  ~MockStreamConnectionDelegate() override;

  MOCK_METHOD(void, OnMessageDispatched, (const std::string&), (override));
  MOCK_METHOD(void, OnConnectionEstablished, (), (override));
  MOCK_METHOD(void,
              PrepareRequest,
              (std::unique_ptr<network::ResourceRequest>,
               PrepareRequestCallback),
              (override));
  MOCK_METHOD(bool, ShouldRetryOnHttpFailure, (int), (override));
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TEST_SUPPORT_MOCK_STREAM_CONNECTION_DELEGATE_H_
