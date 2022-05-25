// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/mock_common_dependencies.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAreArray;

const char kScriptServerUrl[] = "https://www.fake.backend.com/script_server";

class AutofillAssistantImpTest : public testing::Test {
 public:
  AutofillAssistantImpTest() {
    auto mock_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_request_sender_ = mock_request_sender.get();

    auto mock_common_dependencies = std::make_unique<MockCommonDependencies>();
    mock_dependencies_ = mock_common_dependencies.get();
    ON_CALL(*mock_dependencies_, GetCountryCode).WillByDefault(Return("US"));
    ON_CALL(*mock_dependencies_, GetLocale).WillByDefault(Return("en-US"));
    ON_CALL(*mock_dependencies_, IsSupervisedUser).WillByDefault(Return(false));

    // As long as the `BrowserContext` is only passed as an argument during
    // `CommonDependencies` calls, we do not need to set up a test environment
    // for it.
    service_ = std::make_unique<AutofillAssistantImpl>(
        /* browser_context= */ nullptr, std::move(mock_request_sender),
        std::move(mock_common_dependencies), GURL(kScriptServerUrl));
  }
  ~AutofillAssistantImpTest() override = default;

 protected:
  base::MockCallback<AutofillAssistant::GetCapabilitiesResponseCallback>
      mock_response_callback_;
  raw_ptr<NiceMock<MockServiceRequestSender>> mock_request_sender_;
  raw_ptr<MockCommonDependencies> mock_dependencies_;
  std::unique_ptr<AutofillAssistantImpl> service_;
};

}  // namespace

bool operator==(const AutofillAssistant::CapabilitiesInfo& lhs,
                const AutofillAssistant::CapabilitiesInfo& rhs) {
  return std::tie(lhs.url, lhs.script_parameters) ==
         std::tie(rhs.url, rhs.script_parameters);
}

TEST_F(AutofillAssistantImpTest, GetCapabilitiesByHashPrefixEmptyRespose) {
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImpTest, BackendRequestFailed) {
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_FORBIDDEN, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_FORBIDDEN,
                  std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImpTest, ParsingError) {
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "invalid",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImpTest, GetCapabilitiesByHashPrefix) {
  GetCapabilitiesByHashPrefixResponseProto proto;
  GetCapabilitiesByHashPrefixResponseProto::MatchInfoProto* match_info =
      proto.add_match_info();
  match_info->set_url_match("http://exampleA.com");
  ScriptParameterProto* script_parameter =
      match_info->add_script_parameters_override();
  script_parameter->set_name("EXPERIMENT_IDS");
  script_parameter->set_value("3345172");

  GetCapabilitiesByHashPrefixResponseProto::MatchInfoProto* match_info2 =
      proto.add_match_info();
  match_info2->set_url_match("http://exampleB.com");

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_proto,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK,
          UnorderedElementsAreArray(
              std::vector<AutofillAssistant::CapabilitiesInfo>{
                  {"http://exampleA.com", {{"EXPERIMENT_IDS", "3345172"}}},
                  {"http://exampleB.com", {}}})));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImpTest,
       GetCapabilitiesByHashPrefixDoesNotExecuteForSupervisedUsers) {
  EXPECT_CALL(*mock_dependencies_, IsSupervisedUser).WillOnce(Return(true));

  EXPECT_CALL(*mock_request_sender_, OnSendRequest).Times(0);

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

}  // namespace autofill_assistant
