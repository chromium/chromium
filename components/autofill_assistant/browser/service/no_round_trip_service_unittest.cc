// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/no_round_trip_service.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace {
const char kEndpointRPC[] = "https://www.fake.backend.com/no_roundtrip";
const char kProgressReportRPC[] = "https://www.fake.backend.com/progress";
const char kFakeUrl[] = "https://www.example.com";
constexpr uint32_t kHashPrefixLength = 15;

GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo CreateExampleMatch(
    const std::string& domain,
    const std::string& path,
    int action_delay) {
  GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo match;
  auto* supports_site_response = match.mutable_supports_site_response();
  auto* script = supports_site_response->add_scripts();
  script->set_path(path);
  match.set_domain(domain);
  auto* routine_script = match.add_routine_scripts();
  auto* action_response = routine_script->mutable_action_response();
  routine_script->set_script_path(path);
  action_response->add_actions()->set_action_delay_ms(action_delay);

  return match;
}

class NoRoundTripServiceTest : public testing::Test {
 protected:
  NoRoundTripServiceTest() {
    client_context_.mutable_chrome()->set_chrome_version(
        version_info::GetProductNameAndVersionForUserAgent());
    mock_request_sender_ =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
  }

  ~NoRoundTripServiceTest() override = default;
  NiceMock<MockClient> mock_client_;
  ClientContextProto client_context_;
  std::unique_ptr<NiceMock<MockServiceRequestSender>> mock_request_sender_;

  base::MockCallback<ServiceRequestSender::ResponseCallback>
      mock_response_callback_;

  SupportsScriptResponseProto supports_site_response_;
  std::string domain_;
  std::vector<
      GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
      routines_;

  NoRoundTripService GetService() {
    return NoRoundTripService(std::make_unique<LocalScriptStore>(
        routines_, domain_, supports_site_response_));
  }

  NoRoundTripService GetCompleteService() {
    return NoRoundTripService(std::move(mock_request_sender_),
                              GURL(kEndpointRPC), GURL(kProgressReportRPC),
                              &mock_client_);
  }

  uint64_t GetHashPrefix() {
    return AutofillAssistant::GetHashPrefix(
        kHashPrefixLength, url::Origin::Create(GURL(kFakeUrl)));
  }
};

TEST_F(NoRoundTripServiceTest, CallsEndpointAndPopulatesStore) {
  GetNoRoundTripScriptsByHashPrefixResponseProto resp;
  const std::string path = "test_path_1";
  *resp.add_match_infos() = CreateExampleMatch(kFakeUrl, path, 37);
  const std::string resp_string = resp.SerializeAsString();

  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kEndpointRPC),
                    ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
                        kHashPrefixLength, GetHashPrefix(), client_context_,
                        ScriptParameters()),
                    _, RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, resp_string,
                                   ServiceRequestSender::ResponseInfo{}));

  const SupportsScriptResponseProto& supports_script =
      resp.match_infos()[0].supports_site_response();

  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, supports_script.SerializeAsString(), _));

  NoRoundTripService service = GetCompleteService();
  service.GetScriptsForUrl(GURL(kFakeUrl), TriggerContext(),
                           mock_response_callback_.Get());

  const LocalScriptStore* local_script_store = service.GetStore();

  ASSERT_THAT(local_script_store, Not(IsNull()));
  EXPECT_EQ(local_script_store->GetDomain(), kFakeUrl);
  ASSERT_EQ(local_script_store->GetRoutines().size(), 1ul);
  auto routine_script = local_script_store->GetRoutines()[0];
  auto action_response = routine_script.action_response();
  ASSERT_EQ(action_response.actions().size(), 1);
  auto action = action_response.actions()[0];
  EXPECT_EQ(action.action_delay_ms(), 37);
  EXPECT_EQ(routine_script.script_path(), path);
  ASSERT_EQ(local_script_store->GetSupportsSiteResponse().scripts().size(), 1);
  EXPECT_EQ(local_script_store->GetSupportsSiteResponse().scripts()[0].path(),
            path);
}

TEST_F(NoRoundTripServiceTest, DoesNotPopulateStoreOnRPCError) {
  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kEndpointRPC),
                    ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
                        kHashPrefixLength, GetHashPrefix(), client_context_,
                        ScriptParameters()),
                    _, RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(
          RunOnceCallback<2>(500, "", ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_INTERNAL_SERVER_ERROR, "", _));

  NoRoundTripService service = GetCompleteService();
  service.GetScriptsForUrl(GURL(kFakeUrl), TriggerContext(),
                           mock_response_callback_.Get());

  EXPECT_THAT(service.GetStore(), IsNull());
}

TEST_F(NoRoundTripServiceTest, SelectsRightMatchWhenMultipleAreReturned) {
  GetNoRoundTripScriptsByHashPrefixResponseProto resp;
  std::string script_path = "test_path_2";
  *resp.add_match_infos() =
      CreateExampleMatch("www.bad.com", "not_relevant_for_test", 0);
  GetNoRoundTripScriptsByHashPrefixResponseProto_MatchInfo* good_match =
      resp.add_match_infos();
  *good_match = CreateExampleMatch(kFakeUrl, script_path, 42);
  const std::string resp_string = resp.SerializeAsString();

  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kEndpointRPC),
                    ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
                        kHashPrefixLength, GetHashPrefix(), client_context_,
                        ScriptParameters()),
                    _, RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, resp_string,
                                   ServiceRequestSender::ResponseInfo{}));

  const SupportsScriptResponseProto& supports_script =
      good_match->supports_site_response();

  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, supports_script.SerializeAsString(), _));

  NoRoundTripService service = GetCompleteService();
  service.GetScriptsForUrl(GURL(kFakeUrl), TriggerContext(),
                           mock_response_callback_.Get());

  const LocalScriptStore* local_script_store = service.GetStore();

  ASSERT_THAT(local_script_store, Not(IsNull()));
  EXPECT_EQ(local_script_store->GetDomain(), kFakeUrl);
  ASSERT_THAT(local_script_store->GetRoutines(), Not(IsEmpty()));
  auto routine_script = local_script_store->GetRoutines()[0];
  auto action_response = routine_script.action_response();
  ASSERT_THAT(action_response.actions(), Not(IsEmpty()));
  auto action = action_response.actions()[0];
  EXPECT_EQ(action.action_delay_ms(), 42);
  EXPECT_EQ(routine_script.script_path(), script_path);
  ASSERT_THAT(local_script_store->GetSupportsSiteResponse().scripts(),
              Not(IsEmpty()));
  EXPECT_EQ(local_script_store->GetSupportsSiteResponse().scripts()[0].path(),
            script_path);
}

TEST_F(NoRoundTripServiceTest, ReturnsEmptyStoreWhenNoUrlMatches) {
  GetNoRoundTripScriptsByHashPrefixResponseProto resp;
  *resp.add_match_infos() =
      CreateExampleMatch("bad.com", "not_relevant_for_test", 0);
  *resp.add_match_infos() =
      CreateExampleMatch("worse.com", "not_relevant_for_test", 0);
  const std::string resp_string = resp.SerializeAsString();

  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kEndpointRPC),
                    ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
                        kHashPrefixLength, GetHashPrefix(), client_context_,
                        ScriptParameters()),
                    _, RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, resp_string,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_BAD_REQUEST, "", _));

  NoRoundTripService service = GetCompleteService();
  service.GetScriptsForUrl(GURL(kFakeUrl), TriggerContext(),
                           mock_response_callback_.Get());

  ASSERT_THAT(service.GetStore(), IsNull());
}

TEST_F(NoRoundTripServiceTest, GetActionsReturnsBadRequestWithEmptyStore) {
  GetNoRoundTripScriptsByHashPrefixResponseProto resp;
  *resp.add_match_infos() =
      CreateExampleMatch("bad.com", "not_relevant_for_test", 0);
  const std::string resp_string = resp.SerializeAsString();

  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kEndpointRPC),
                    ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
                        kHashPrefixLength, GetHashPrefix(), client_context_,
                        ScriptParameters()),
                    _, RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, resp_string,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_BAD_REQUEST, "", _));

  NoRoundTripService service = GetCompleteService();
  service.GetScriptsForUrl(GURL(kFakeUrl), TriggerContext(),
                           mock_response_callback_.Get());

  const LocalScriptStore* local_script_store = service.GetStore();

  ASSERT_THAT(local_script_store, IsNull());

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_BAD_REQUEST, "", _));
  service.GetActions("not_relevant_for_the_test",
                     GURL("not_relevant_for_the_test"), TriggerContext(), "",
                     "", mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest,
       GetNextActionsReturnsInternalServerErrorWithEmptyStore) {
  GetNoRoundTripScriptsByHashPrefixResponseProto resp;
  *resp.add_match_infos() =
      CreateExampleMatch("bad.com", "not_relevant_for_test", 0);
  const std::string resp_string = resp.SerializeAsString();

  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kEndpointRPC),
                    ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
                        kHashPrefixLength, GetHashPrefix(), client_context_,
                        ScriptParameters()),
                    _, RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, resp_string,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_BAD_REQUEST, "", _));

  NoRoundTripService service = GetCompleteService();
  service.GetScriptsForUrl(GURL(kFakeUrl), TriggerContext(),
                           mock_response_callback_.Get());

  const LocalScriptStore* local_script_store = service.GetStore();

  ASSERT_THAT(local_script_store, IsNull());

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_BAD_REQUEST, "", _));
  service.GetNextActions(TriggerContext(), "", "",
                         std::vector<ProcessedActionProto>(),
                         RoundtripTimingStats(), RoundtripNetworkStats(),
                         mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest, ReportProgress) {
  const std::string token = "token";
  const std::string payload = "payload";

  EXPECT_CALL(mock_client_, GetMakeSearchesAndBrowsingBetterEnabled)
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_client_, GetMetricsReportingEnabled)
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kProgressReportRPC),
                    ProtocolUtils::CreateReportProgressRequest(token, payload),
                    _, RpcType::REPORT_PROGRESS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(""),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, std::string(""), _));

  GetCompleteService().ReportProgress("token", "payload",
                                      mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest, ReportProgressMSBBDisabled) {
  const std::string token = "token";
  const std::string payload = "payload";

  EXPECT_CALL(mock_client_, GetMakeSearchesAndBrowsingBetterEnabled)
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_client_, GetMetricsReportingEnabled).Times(0);

  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kProgressReportRPC),
                    ProtocolUtils::CreateReportProgressRequest(token, payload),
                    _, RpcType::REPORT_PROGRESS))
      .Times(0);

  GetCompleteService().ReportProgress("token", "payload",
                                      mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest, ReportProgressMetricsDisabled) {
  const std::string token = "token";
  const std::string payload = "payload";

  EXPECT_CALL(mock_client_, GetMakeSearchesAndBrowsingBetterEnabled)
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_client_, GetMetricsReportingEnabled)
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(
      *mock_request_sender_.get(),
      OnSendRequest(GURL(kProgressReportRPC),
                    ProtocolUtils::CreateReportProgressRequest(token, payload),
                    _, RpcType::REPORT_PROGRESS))
      .Times(0);

  GetCompleteService().ReportProgress("token", "payload",
                                      mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest, WithExistingPathGetActionsSucceeds) {
  domain_ = "example.com";
  GetNoRoundTripScriptsByHashPrefixResponseProto_MatchInfo_RoutineScript
      routine;
  routine.set_script_path("script_path");
  routine.mutable_action_response()->set_run_id(0);
  routines_.push_back(routine);

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, routine.action_response().SerializeAsString(), _));

  GetService().GetActions("script_path", GURL(kFakeUrl), TriggerContext(), "",
                          "", mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest, GetActionsFailsGivenWrongPath) {
  domain_ = "example.com";
  GetNoRoundTripScriptsByHashPrefixResponseProto_MatchInfo_RoutineScript
      routine;
  routine.set_script_path("not_relevant_for_test");
  routine.mutable_action_response()->set_run_id(0);
  routines_.push_back(routine);

  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_BAD_REQUEST, /* response= */ "", _));

  GetService().GetActions("non_existing_path", GURL(kFakeUrl), TriggerContext(),
                          "", "", mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest, GetNextActionsReturnsEmptyResponse) {
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_OK, /* response= */ "", _));

  const std::vector<ProcessedActionProto> processed_actions;

  GetService().GetNextActions(TriggerContext(), "", "", processed_actions,
                              RoundtripTimingStats(), RoundtripNetworkStats(),
                              mock_response_callback_.Get());
}

TEST_F(NoRoundTripServiceTest, GetUserDataFails) {
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_METHOD_NOT_ALLOWED, /* response= */ "", _));
  const UserData user_data;
  GetService().GetUserData(CollectUserDataOptions(), 0, &user_data,
                           mock_response_callback_.Get());
}

}  // namespace

}  // namespace autofill_assistant
