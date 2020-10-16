// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_impl.h"

#include "components/autofill_assistant/browser/mock_client_context.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/service/service.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const char kScriptServerUrl[] = "https://www.fake.backend.com/script_server";
const char kActionServerUrl[] = "https://www.fake.backend.com/action_server";

class ServiceImplTest : public testing::Test {
 public:
  ServiceImplTest() {
    auto mock_client_context = std::make_unique<NiceMock<MockClientContext>>();
    mock_client_context_ = mock_client_context.get();

    auto mock_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_request_sender_ = mock_request_sender.get();

    service_ = std::make_unique<ServiceImpl>(
        std::move(mock_request_sender), GURL(kScriptServerUrl),
        GURL(kActionServerUrl), std::move(mock_client_context));
  }
  ~ServiceImplTest() override = default;

 protected:
  base::MockCallback<Service::ResponseCallback> mock_response_callback_;
  NiceMock<MockClientContext>* mock_client_context_;
  NiceMock<MockServiceRequestSender>* mock_request_sender_;
  std::unique_ptr<ServiceImpl> service_;
};

TEST_F(ServiceImplTest, GetScriptsForUrl) {
  EXPECT_CALL(*mock_client_context_, Update).Times(1);
  // TODO(b/158998456), here and in other tests of service_impl: check that
  // protocol utils is called with the correct parameters.
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")))
      .Times(1);

  TriggerContextImpl trigger_context;
  service_->GetScriptsForUrl(GURL("https://www.example.com"), trigger_context,
                             mock_response_callback_.Get());
}

TEST_F(ServiceImplTest, GetActions) {
  EXPECT_CALL(*mock_client_context_, Update).Times(1);
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kActionServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")))
      .Times(1);

  TriggerContextImpl trigger_context;
  service_->GetActions(
      std::string("fake_script_path"), GURL("https://www.example.com"),
      trigger_context, std::string("fake_global_payload"),
      std::string("fake_script_payload"), mock_response_callback_.Get());
}

TEST_F(ServiceImplTest, GetNextActions) {
  EXPECT_CALL(*mock_client_context_, Update).Times(1);
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kActionServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")))
      .Times(1);

  TriggerContextImpl trigger_context;
  service_->GetNextActions(
      trigger_context, std::string("fake_previous_global_payload"),
      std::string("fake_previous_script_payload"), /* processed_actions = */ {},
      /* timing_stats = */ RoundtripTimingStats(),
      mock_response_callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
