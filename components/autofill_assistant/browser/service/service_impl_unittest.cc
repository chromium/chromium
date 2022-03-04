// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/service_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/mock_client_context.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

const char kScriptServerUrl[] = "https://www.fake.backend.com/script_server";
const char kActionServerUrl[] = "https://www.fake.backend.com/action_server";
const char kUserDataServerUrl[] =
    "https://www.fake.backend.com/user_data_server";

// TODO(b/207744539): In all tests, check that protocol utils is called with
// the correct parameters.
class ServiceImplTest : public testing::Test {
 public:
  ServiceImplTest() {
    auto mock_client_context = std::make_unique<NiceMock<MockClientContext>>();
    mock_client_context_ = mock_client_context.get();

    auto mock_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_request_sender_ = mock_request_sender.get();

    service_ = std::make_unique<ServiceImpl>(
        &mock_client_, std::move(mock_request_sender), GURL(kScriptServerUrl),
        GURL(kActionServerUrl), GURL(kUserDataServerUrl),
        std::move(mock_client_context));
  }
  ~ServiceImplTest() override = default;

 protected:
  base::MockCallback<Service::ResponseCallback> mock_response_callback_;
  NiceMock<MockClient> mock_client_;
  raw_ptr<NiceMock<MockClientContext>> mock_client_context_;
  raw_ptr<NiceMock<MockServiceRequestSender>> mock_request_sender_;
  std::unique_ptr<ServiceImpl> service_;
};

TEST_F(ServiceImplTest, GetScriptsForUrl) {
  EXPECT_CALL(*mock_client_context_, Update);
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kScriptServerUrl), _, _,
                                                   RpcType::SUPPORTS_SCRIPT))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")));

  service_->GetScriptsForUrl(GURL("https://www.example.com"), TriggerContext(),
                             mock_response_callback_.Get());
}

TEST_F(ServiceImplTest, GetActions) {
  EXPECT_CALL(*mock_client_context_, Update);
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kActionServerUrl), _, _, RpcType::GET_ACTIONS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")));

  service_->GetActions(
      std::string("fake_script_path"), GURL("https://www.example.com"),
      TriggerContext(), std::string("fake_global_payload"),
      std::string("fake_script_payload"), mock_response_callback_.Get());
}

TEST_F(ServiceImplTest, GetActionsForwardsScriptStoreConfig) {
  EXPECT_CALL(*mock_client_context_, Update);

  ScriptActionRequestProto expected_get_actions;
  ScriptStoreConfig* config = expected_get_actions.mutable_initial_request()
                                  ->mutable_script_store_config();
  config->set_bundle_path("bundle/path");
  config->set_bundle_version(12);

  std::string get_actions_request;
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kActionServerUrl), _, _, RpcType::GET_ACTIONS))
      .WillOnce(SaveArg<1>(&get_actions_request));

  ScriptStoreConfig set_config;
  set_config.set_bundle_path("bundle/path");
  set_config.set_bundle_version(12);
  service_->SetScriptStoreConfig(set_config);

  service_->GetActions(
      std::string("fake_script_path"), GURL("https://www.example.com"),
      TriggerContext(), std::string("fake_global_payload"),
      std::string("fake_script_payload"), mock_response_callback_.Get());

  ScriptActionRequestProto get_actions_request_proto;
  EXPECT_TRUE(get_actions_request_proto.ParseFromString(get_actions_request));
  EXPECT_EQ("bundle/path", get_actions_request_proto.initial_request()
                               .script_store_config()
                               .bundle_path());
  EXPECT_EQ(12, get_actions_request_proto.initial_request()
                    .script_store_config()
                    .bundle_version());
}

TEST_F(ServiceImplTest, GetActionsWithoutClientToken) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillAssistantGetPaymentsClientToken);

  EXPECT_CALL(*mock_client_context_, Update);
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kActionServerUrl), _, _, RpcType::GET_ACTIONS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")));

  service_->GetActions(
      std::string("fake_script_path"), GURL("https://www.example.com"),
      TriggerContext(), std::string("fake_global_payload"),
      std::string("fake_script_payload"), mock_response_callback_.Get());
}

TEST_F(ServiceImplTest, GetNextActions) {
  EXPECT_CALL(*mock_client_context_, Update);
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kActionServerUrl), _, _, RpcType::GET_ACTIONS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")));

  service_->GetNextActions(
      TriggerContext(), std::string("fake_previous_global_payload"),
      std::string("fake_previous_script_payload"), /* processed_actions = */ {},
      /* timing_stats = */ RoundtripTimingStats(),
      mock_response_callback_.Get());
}

TEST_F(ServiceImplTest, GetUserDataWithPayments) {
  EXPECT_CALL(mock_client_, FetchPaymentsClientToken)
      .WillOnce(RunOnceCallback<0>("token"));
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kUserDataServerUrl), _,
                                                   _, RpcType::GET_USER_DATA))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")));

  CollectUserDataOptions options;
  options.request_payment_method = true;
  service_->GetUserData(options, /* run_id= */ 1,
                        mock_response_callback_.Get());
}

TEST_F(ServiceImplTest, GetUserDataWithoutPayments) {
  EXPECT_CALL(mock_client_, FetchPaymentsClientToken).Times(0);
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kUserDataServerUrl), _,
                                                   _, RpcType::GET_USER_DATA))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string("response")));
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, std::string("response")));

  CollectUserDataOptions options;
  service_->GetUserData(options, /* run_id= */ 1,
                        mock_response_callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
