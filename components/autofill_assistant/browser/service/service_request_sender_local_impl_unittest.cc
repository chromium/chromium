// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_request_sender_local_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::Return;

namespace {

class ServiceRequestSenderLocalImplTest : public testing::Test {
 public:
  ServiceRequestSenderLocalImplTest() = default;
  ~ServiceRequestSenderLocalImplTest() override = default;

 protected:
  base::MockCallback<ServiceRequestSender::ResponseCallback>
      mock_response_callback_;
};

TEST_F(ServiceRequestSenderLocalImplTest, SendRequestAlwaysReturnsResponse) {
  ServiceRequestSenderLocalImpl service_request_sender = {"response"};
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "response")).Times(2);
  service_request_sender.SendRequest(
      GURL(), "request_1", ServiceRequestSender::AuthMode::OAUTH_STRICT,
      mock_response_callback_.Get(), RpcType::UNKNOWN);
  service_request_sender.SendRequest(
      GURL(), "request_2", ServiceRequestSender::AuthMode::OAUTH_STRICT,
      mock_response_callback_.Get(), RpcType::UNKNOWN);
}

}  // namespace
}  // namespace autofill_assistant
