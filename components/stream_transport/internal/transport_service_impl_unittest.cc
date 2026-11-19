// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/transport_service_impl.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/stream_transport/internal/client_session_starter.h"
#include "components/stream_transport/internal/features.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/internal/session_stream_recorder.h"
#include "components/stream_transport/internal/transport_channel_impl.h"
#include "components/stream_transport/internal/transport_message_observer.h"
#include "components/stream_transport/internal/upstream_message_client/upstream_message_client.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_channel.h"
#include "components/stream_transport/public/transport_handler_factory_registry.h"
#include "components/stream_transport/public/transport_service_factory_helper.h"
#include "components/stream_transport/public/transport_session.h"
#include "components/stream_transport/public/transport_session_registry.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace stream_transport {
namespace {

class TransportServiceImplTest : public testing::Test {
 protected:
  TransportServiceImplTest() : identity_test_env_(&test_url_loader_factory_) {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }
  ~TransportServiceImplTest() override = default;

  std::unique_ptr<TransportServiceImpl> CreateService(bool record_sessions) {
    TransportServiceConfig config;
    config.watch_sessions_endpoint =
        GURL("https://transport.test/v1:watchSessions");
    config.send_session_message_endpoint =
        GURL("https://transport.test/v1:sendSessionMessage");
    config.record_sessions = record_sessions;
    return std::make_unique<TransportServiceImpl>(
        test_shared_url_loader_factory_, identity_test_env_.identity_manager(),
        config);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(TransportServiceImplTest, GetChannelReturnsValidChannel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  EXPECT_NE(service->GetChannel(), nullptr);
}

TEST_F(TransportServiceImplTest,
       CreateTransportServicePublicFactoryCreatesWorkingService) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);
  TransportServiceConfig config;
  config.watch_sessions_endpoint =
      GURL("https://transport.test/v1:watchSessions");
  config.send_session_message_endpoint =
      GURL("https://transport.test/v1:sendSessionMessage");
  config.record_sessions = true;
  std::unique_ptr<TransportService> service = CreateTransportService(
      test_shared_url_loader_factory_, identity_test_env_.identity_manager(),
      std::move(config));
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetChannel(), nullptr);
  EXPECT_NE(service->GetStreamRecorderFactory(), nullptr);
}

TEST_F(TransportServiceImplTest, GetChannelReturnsNullIfChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  EXPECT_EQ(service->GetChannel(), nullptr);
}

TEST_F(TransportServiceImplTest, RecorderStartsRecordingWhenSessionIsCreated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/true);

  SessionStreamRecorderFactory* recorder_factory =
      service->GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);
  ASSERT_NE(service->GetChannel(), nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 0u);

  // Creating the session is enough; no payload has been dispatched yet.
  ASSERT_NE(service->CreateSession("s1", InitIngressSource::kUnspecified),
            nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  // The recorder is not a handler factory, so it does not take part in
  // payload routing.
  EXPECT_THAT(service->GetChannel()->GetHandlerFactoryRegistry()->GetFactories(
                  PayloadType::kActuation),
              testing::IsEmpty());

  // Removing the session retires its recorder.
  service->GetChannel()->GetSessionRegistry()->DestroySession("s1");
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 0u);
  std::vector<SessionSnapshot> sessions =
      recorder_factory->GetAllSessions(/*max_events=*/10,
                                       /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_TRUE(sessions[0].end_wall_time.has_value());
}

TEST_F(TransportServiceImplTest,
       GetStreamRecorderFactoryReturnsNullWhenNotRecordingSessions) {
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  EXPECT_EQ(service->GetStreamRecorderFactory(), nullptr);
}

TEST_F(TransportServiceImplTest,
       GetStreamRecorderFactoryWorksWhenChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/true);

  EXPECT_EQ(service->GetChannel(), nullptr);
  EXPECT_NE(service->GetStreamRecorderFactory(), nullptr);
}

TEST_F(
    TransportServiceImplTest,
    SessionStreamRecorderFactoryObservesUpstreamMessagesWhenRecordingSessions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/true);

  ASSERT_NE(service->GetChannel(), nullptr);
  SessionStreamRecorderFactory* recorder_factory =
      service->GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);

  ASSERT_NE(service->CreateSession("s1", InitIngressSource::kUnspecified),
            nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();
  service->GetChannel()->SendUpstreamMessage("s1", PayloadType::kControl,
                                             command, 1);

  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* s1_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s1_dict, nullptr);
  EXPECT_EQ(s1_dict->FindInt("total_upstream_messages"), 1);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(TransportServiceImplTest,
       ClearsSessionsAndRecordedHistoryOnPrimaryAccountCleared) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  identity_test_env_.MakePrimaryAccountAvailable("user@example.com",
                                                 signin::ConsentLevel::kSignin);
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/true);

  TransportSession* session = service->CreateSession(
      "session-before-signout", InitIngressSource::kUnspecified);
  ASSERT_NE(session, nullptr);
  SessionStreamRecorderFactory* recorder_factory =
      service->GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();
  service->GetChannel()->SendUpstreamMessage("session-before-signout",
                                             PayloadType::kControl, command, 1);
  ASSERT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  // Signing out must destroy active sessions and wipe recorded session history
  // while keeping the channel available if the user signs back in.
  identity_test_env_.ClearPrimaryAccount();

  EXPECT_EQ(service->GetSession("session-before-signout"), nullptr);
  EXPECT_NE(service->GetChannel(), nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 0u);
  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  EXPECT_TRUE(sessions->empty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(TransportServiceImplTest,
       ClearsSessionsAndRecordedHistoryOnPrimaryAccountSwitched) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  identity_test_env_.MakePrimaryAccountAvailable("first@example.com",
                                                 signin::ConsentLevel::kSignin);
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/true);

  TransportSession* session = service->CreateSession(
      "session-first-account", InitIngressSource::kUnspecified);
  ASSERT_NE(session, nullptr);
  SessionStreamRecorderFactory* recorder_factory =
      service->GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);
  ASSERT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  // Directly switching primary accounts without an intermediate sign-out must
  // also reset sessions and recorded history.
  identity_test_env_.MakePrimaryAccountAvailable("second@example.com",
                                                 signin::ConsentLevel::kSignin);

  EXPECT_EQ(service->GetSession("session-first-account"), nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 0u);
  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  EXPECT_TRUE(sessions->empty());
}

TEST_F(TransportServiceImplTest, ShutdownTearsDownChannelBeforeDestruction) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/true);
  ASSERT_NE(service->GetChannel(), nullptr);
  SessionStreamRecorderFactory* recorder_factory =
      service->GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);

  ASSERT_NE(
      service->CreateSession("session-closed", InitIngressSource::kUnspecified),
      nullptr);
  service->GetChannel()->GetSessionRegistry()->DestroySession("session-closed");
  ASSERT_NE(
      service->CreateSession("session-active", InitIngressSource::kUnspecified),
      nullptr);
  ASSERT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  service->Shutdown();

  EXPECT_EQ(service->GetChannel(), nullptr);
  EXPECT_EQ(service->GetSession("session-active"), nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 0u);
  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  EXPECT_TRUE(sessions->empty());
}

TEST_F(TransportServiceImplTest, StartClientSessionSuccessPath) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "OK", net::HTTP_OK);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_cs", PayloadType::kControl, command, future.GetCallback());
  ASSERT_NE(pending_session, nullptr);
  EXPECT_EQ(pending_session->GetSessionId(), "session_cs");

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_TRUE(result.has_value());
  TransportSession* session = result.value();
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(session->GetSessionId(), "session_cs");
  EXPECT_EQ(service->GetSession("session_cs"), session);
}

TEST_F(TransportServiceImplTest, StartClientSessionFailsWhenChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_cs", PayloadType::kControl, command, future.GetCallback());
  EXPECT_EQ(pending_session, nullptr);

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kChannelUnavailable);
}

TEST_F(TransportServiceImplTest, StartClientSessionFailsOnSessionLimit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kBrowserActuatorChannelEnabled, {}},
       {kBrowserActuatorProtoStreamTransport,
        {{kMaxTransportSessions.name, "1"}}}},
      {});

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  // Fill the concurrent session limit with an active session.
  TransportSession* existing = service->CreateSession(
      "session_existing", InitIngressSource::kUnspecified);
  ASSERT_NE(existing, nullptr);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_new", PayloadType::kControl, command, future.GetCallback());
  EXPECT_EQ(pending_session, nullptr);

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kSessionLimitReached);
}

TEST_F(TransportServiceImplTest, StartClientSessionFailsOnHttpStatusError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "Conflict",
      net::HTTP_CONFLICT);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_conflict", PayloadType::kControl, command, future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kSessionIdConflict);
  EXPECT_EQ(service->GetSession("session_conflict"), nullptr);
}

TEST_F(TransportServiceImplTest, StartClientSessionCancelledBeforeResponse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::vector<browser_actuator::SendSessionMessageRequest> sent_requests;
  test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [&sent_requests](const network::ResourceRequest& request) {
        browser_actuator::SendSessionMessageRequest req;
        if (req.ParseFromString(network::GetUploadData(request))) {
          sent_requests.push_back(std::move(req));
        }
      }));

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  bool callback_run = false;
  TransportSession* pending_session = service->StartClientSession(
      "session_cancel", PayloadType::kControl, command,
      base::BindLambdaForTesting(
          [&callback_run](
              base::expected<TransportSession*, StartClientSessionError>) {
            callback_run = true;
          }));
  ASSERT_NE(pending_session, nullptr);

  // Calling `Close()` on the pending session cancels the pending start.
  pending_session->Close();

  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "OK", net::HTTP_OK);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(callback_run);
  EXPECT_EQ(service->GetSession("session_cancel"), nullptr);
  ASSERT_EQ(sent_requests.size(), 2u);
  EXPECT_EQ(
      sent_requests[0].actuator_upstream_message().client_sequence_number(), 1);
  EXPECT_EQ(
      sent_requests[1].actuator_upstream_message().client_sequence_number(), 2);
  browser_actuator::ControlCommand close_cmd;
  ASSERT_TRUE(close_cmd.ParseFromString(sent_requests[1]
                                            .actuator_upstream_message()
                                            .typed_payloads(0)
                                            .proto_payload()
                                            .value()));
  EXPECT_TRUE(close_cmd.has_close_session());
}

TEST_F(TransportServiceImplTest, StartClientSessionFailsOnNetworkError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  test_url_loader_factory_.AddResponse(
      GURL("https://transport.test/v1:sendSessionMessage"),
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_neterr", PayloadType::kControl, command, future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kTransientError);
  EXPECT_EQ(service->GetSession("session_neterr"), nullptr);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(TransportServiceImplTest, StartClientSessionAccountSignOutFailsPending) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_signout", PayloadType::kControl, command, future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  identity_test_env_.ClearPrimaryAccount();

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kChannelUnavailable);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(TransportServiceImplTest, StartClientSessionAccountSwitchFailsPending) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_switch", PayloadType::kControl, command, future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  identity_test_env_.MakePrimaryAccountAvailable("second@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kChannelUnavailable);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionMiraclePtrSafeOnGraduationAndClose) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "OK", net::HTTP_OK);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  raw_ptr<TransportSession> session_holder = nullptr;
  session_holder = service->StartClientSession(
      "session_mptr", PayloadType::kControl, command,
      base::BindLambdaForTesting(
          [&session_holder](
              base::expected<TransportSession*, StartClientSessionError> res) {
            ASSERT_TRUE(res.has_value());
            session_holder = res.value();
          }));
  ASSERT_NE(session_holder, nullptr);
  EXPECT_EQ(session_holder->GetSessionId(), "session_mptr");

  task_environment_.RunUntilIdle();

  ASSERT_NE(session_holder, nullptr);
  EXPECT_EQ(session_holder->GetSessionId(), "session_mptr");
  EXPECT_EQ(service->GetSession("session_mptr"), session_holder);

  session_holder.ExtractAsDangling()->Close();
  EXPECT_EQ(service->GetSession("session_mptr"), nullptr);
}

TEST_F(TransportServiceImplTest,
       ClientSessionStarterStopsAfterChannelDestroyed) {
  auto channel = std::make_unique<TransportChannelImpl>(
      std::make_unique<UpstreamMessageClient>(
          test_shared_url_loader_factory_,
          identity_test_env_.identity_manager(),
          GURL("https://transport.test/v1:sendSessionMessage"),
          TRAFFIC_ANNOTATION_FOR_TESTS));
  ClientSessionStarter starter(channel.get(),
                               identity_test_env_.identity_manager());

  channel.reset();

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = starter.StartClientSession(
      "session_destroyed_channel", PayloadType::kControl, command,
      future.GetCallback());
  EXPECT_EQ(pending_session, nullptr);

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kChannelUnavailable);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionFailsOnDuplicatePendingSessionId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      first_future;
  TransportSession* first_pending =
      service->StartClientSession("session_dup_pending", PayloadType::kControl,
                                  command, first_future.GetCallback());
  ASSERT_NE(first_pending, nullptr);
  EXPECT_EQ(service->GetSession("session_dup_pending"), nullptr);

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      second_future;
  TransportSession* second_pending =
      service->StartClientSession("session_dup_pending", PayloadType::kControl,
                                  command, second_future.GetCallback());
  EXPECT_EQ(second_pending, nullptr);

  base::expected<TransportSession*, StartClientSessionError> second_result =
      second_future.Take();
  ASSERT_FALSE(second_result.has_value());
  EXPECT_EQ(second_result.error(), StartClientSessionError::kSessionIdConflict);

  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "OK", net::HTTP_OK);
  base::expected<TransportSession*, StartClientSessionError> first_result =
      first_future.Take();
  ASSERT_TRUE(first_result.has_value());
  EXPECT_EQ(first_result.value()->GetSessionId(), "session_dup_pending");
}

TEST_F(TransportServiceImplTest,
       StartClientSessionFailsOnCombinedActiveAndPendingLimit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kBrowserActuatorChannelEnabled, {}},
       {kBrowserActuatorProtoStreamTransport,
        {{kMaxTransportSessions.name, "2"}}}},
      {});

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      pending_future;
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  ASSERT_NE(service->CreateSession("active_1", InitIngressSource::kUnspecified),
            nullptr);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  TransportSession* pending_session =
      service->StartClientSession("pending_1", PayloadType::kControl, command,
                                  pending_future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      overflow_future;
  TransportSession* overflow_session =
      service->StartClientSession("pending_2", PayloadType::kControl, command,
                                  overflow_future.GetCallback());
  EXPECT_EQ(overflow_session, nullptr);

  base::expected<TransportSession*, StartClientSessionError> overflow_result =
      overflow_future.Take();
  ASSERT_FALSE(overflow_result.has_value());
  EXPECT_EQ(overflow_result.error(),
            StartClientSessionError::kSessionLimitReached);

  pending_session->Close();
}

TEST_F(TransportServiceImplTest,
       StartClientSessionChannelTeardownFailsPendingWithChannelUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  TransportSession* pending_session = service->StartClientSession(
      "session_teardown", PayloadType::kControl, command, future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  service->Shutdown();

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kChannelUnavailable);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionFailsWithTransientErrorOnTransientOAuthFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_transient_oauth", PayloadType::kControl, command,
      future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kTransientError);
  EXPECT_EQ(service->GetSession("session_transient_oauth"), nullptr);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionFailsOnActiveSessionIdConflict) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  ASSERT_NE(
      service->CreateSession("session_active", InitIngressSource::kUnspecified),
      nullptr);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending = service->StartClientSession(
      "session_active", PayloadType::kControl, command, future.GetCallback());
  EXPECT_EQ(pending, nullptr);

  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kSessionIdConflict);
}

class RecordingUpstreamObserver : public TransportMessageObserver {
 public:
  void OnDownstreamMessage(
      std::string_view session_id,
      const browser_actuator::ActuatorDownstreamMessage& message,
      DownstreamDeliveryStatus status) override {}

  void OnUpstreamMessage(
      std::string_view session_id,
      const browser_actuator::ActuatorUpstreamMessage& message,
      int http_response_code) override {
    if (http_response_code == 0) {
      messages_.push_back(message);
    }
  }

  const std::vector<browser_actuator::ActuatorUpstreamMessage>& messages()
      const {
    return messages_;
  }

 private:
  std::vector<browser_actuator::ActuatorUpstreamMessage> messages_;
};

class ResetOnSessionRegisteredObserver
    : public TransportSessionRegistry::Observer {
 public:
  ResetOnSessionRegisteredObserver(signin::IdentityTestEnvironment* env,
                                   std::string_view target_session_id)
      : env_(env), target_session_id_(target_session_id) {}

  void OnSessionRegistered(TransportSession* session) override {
    if (session->GetSessionId() == target_session_id_) {
      env_->MakePrimaryAccountAvailable("second@gmail.com",
                                        signin::ConsentLevel::kSignin);
    }
  }

  void OnSessionRemoved(std::string_view session_id) override {}

 private:
  raw_ptr<signin::IdentityTestEnvironment> env_;
  std::string target_session_id_;
};

TEST_F(TransportServiceImplTest,
       StartClientSessionBuffersOutboundMessagesAndFlushesInOrder) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  auto* channel = static_cast<TransportChannelImpl*>(service->GetChannel());
  ASSERT_NE(channel, nullptr);
  RecordingUpstreamObserver observer;
  base::ScopedObservation<TransportChannelImpl, TransportMessageObserver>
      observation(&observer);
  observation.Observe(channel);

  browser_actuator::ControlCommand init_cmd;
  init_cmd.mutable_start_session_ack();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  raw_ptr<TransportSession> pending_session = nullptr;
  std::optional<SendUpstreamMessageError> retired_send_error;
  pending_session = service->StartClientSession(
      "session_buf", PayloadType::kControl, init_cmd,
      base::BindLambdaForTesting(
          [&](base::expected<TransportSession*, StartClientSessionError>
                  result) {
            browser_actuator::ControlCommand stale_cmd;
            stale_cmd.mutable_close_channel();
            base::expected<void, SendUpstreamMessageError> stale_send =
                pending_session->SendUpstreamMessage(PayloadType::kControl,
                                                     stale_cmd);
            if (!stale_send.has_value()) {
              retired_send_error = stale_send.error();
            }
            pending_session = nullptr;
            future.SetValue(std::move(result));
          }));
  ASSERT_NE(pending_session, nullptr);

  // Buffer two outbound messages while sequence 1 is still in flight.
  browser_actuator::ControlCommand msg2;
  msg2.mutable_close_channel();
  browser_actuator::ControlCommand msg3;
  msg3.mutable_close_session();
  EXPECT_TRUE(pending_session->SendUpstreamMessage(PayloadType::kControl, msg2)
                  .has_value());
  EXPECT_TRUE(pending_session->SendUpstreamMessage(PayloadType::kControl, msg3)
                  .has_value());

  // Neither buffered message should be dispatched before sequence 1 succeeds.
  task_environment_.RunUntilIdle();
  ASSERT_EQ(observer.messages().size(), 1u);
  EXPECT_EQ(observer.messages()[0].client_sequence_number(), 1);

  // Completing sequence 1 graduates the session and flushes buffered messages
  // in FIFO order as sequence 2 and sequence 3.
  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "OK", net::HTTP_OK);
  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(retired_send_error, SendUpstreamMessageError::kChannelDisconnected);
  ASSERT_EQ(observer.messages().size(), 3u);
  EXPECT_EQ(observer.messages()[1].client_sequence_number(), 2);
  EXPECT_EQ(observer.messages()[2].client_sequence_number(), 3);

  browser_actuator::ControlCommand parsed2;
  ASSERT_TRUE(parsed2.ParseFromString(
      observer.messages()[1].typed_payloads(0).proto_payload().value()));
  EXPECT_TRUE(parsed2.has_close_channel());

  browser_actuator::ControlCommand parsed3;
  ASSERT_TRUE(parsed3.ParseFromString(
      observer.messages()[2].typed_payloads(0).proto_payload().value()));
  EXPECT_TRUE(parsed3.has_close_session());

  // Subsequent sends on the graduated session continue at sequence 4.
  browser_actuator::ControlCommand msg4;
  msg4.mutable_start_session_ack();
  EXPECT_TRUE(result.value()
                  ->SendUpstreamMessage(PayloadType::kControl, msg4)
                  .has_value());
  ASSERT_EQ(observer.messages().size(), 4u);
  EXPECT_EQ(observer.messages()[3].client_sequence_number(), 4);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionDropsBufferedMessagesOnFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  auto* channel = static_cast<TransportChannelImpl*>(service->GetChannel());
  ASSERT_NE(channel, nullptr);
  RecordingUpstreamObserver observer;
  base::ScopedObservation<TransportChannelImpl, TransportMessageObserver>
      observation(&observer);
  observation.Observe(channel);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  raw_ptr<TransportSession> pending_session = nullptr;
  std::optional<SendUpstreamMessageError> retired_send_error;
  pending_session = service->StartClientSession(
      "session_fail_buf", PayloadType::kControl, command,
      base::BindLambdaForTesting(
          [&](base::expected<TransportSession*, StartClientSessionError>
                  result) {
            base::expected<void, SendUpstreamMessageError> stale_send =
                pending_session->SendUpstreamMessage(PayloadType::kControl,
                                                     command);
            if (!stale_send.has_value()) {
              retired_send_error = stale_send.error();
            }
            pending_session = nullptr;
            future.SetValue(std::move(result));
          }));
  ASSERT_NE(pending_session, nullptr);
  EXPECT_TRUE(
      pending_session->SendUpstreamMessage(PayloadType::kControl, command)
          .has_value());

  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "Error",
      net::HTTP_INTERNAL_SERVER_ERROR);
  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(retired_send_error, SendUpstreamMessageError::kChannelDisconnected);
  EXPECT_EQ(result.error(), StartClientSessionError::kTransientError);

  // Only sequence 1 was sent; the buffered message was dropped on failure.
  EXPECT_EQ(observer.messages().size(), 1u);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionDropsBufferedMessagesOnClose) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  auto* channel = static_cast<TransportChannelImpl*>(service->GetChannel());
  ASSERT_NE(channel, nullptr);
  RecordingUpstreamObserver observer;
  base::ScopedObservation<TransportChannelImpl, TransportMessageObserver>
      observation(&observer);
  observation.Observe(channel);

  browser_actuator::ControlCommand init_cmd;
  init_cmd.mutable_start_session_ack();

  bool callback_run = false;
  TransportSession* pending_session = service->StartClientSession(
      "session_close_buf", PayloadType::kControl, init_cmd,
      base::BindLambdaForTesting(
          [&callback_run](
              base::expected<TransportSession*, StartClientSessionError>) {
            callback_run = true;
          }));
  ASSERT_NE(pending_session, nullptr);

  browser_actuator::ControlCommand buffered_cmd;
  buffered_cmd.mutable_close_channel();
  EXPECT_TRUE(
      pending_session->SendUpstreamMessage(PayloadType::kControl, buffered_cmd)
          .has_value());

  pending_session->Close();
  EXPECT_EQ(
      pending_session->SendUpstreamMessage(PayloadType::kControl, buffered_cmd)
          .error(),
      SendUpstreamMessageError::kChannelDisconnected);
  EXPECT_EQ(service->GetSession("session_close_buf"), nullptr);

  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "OK", net::HTTP_OK);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(callback_run);
  // Sequence 1 succeeded after `Close()`, so only the teardown `CloseSession`
  // message (sequence 2) is sent; the buffered `close_channel` was dropped.
  ASSERT_EQ(observer.messages().size(), 2u);
  EXPECT_EQ(observer.messages()[0].client_sequence_number(), 1);
  EXPECT_EQ(observer.messages()[1].client_sequence_number(), 2);
  browser_actuator::ControlCommand teardown_cmd;
  ASSERT_TRUE(teardown_cmd.ParseFromString(
      observer.messages()[1].typed_payloads(0).proto_payload().value()));
  EXPECT_TRUE(teardown_cmd.has_close_session());
  EXPECT_FALSE(teardown_cmd.has_close_channel());
  EXPECT_EQ(service->GetSession("session_close_buf"), nullptr);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionRejectsOutboundMessagesWhenBufferFull) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  TransportSession* pending_session = service->StartClientSession(
      "session_full", PayloadType::kControl, command, future.GetCallback());
  ASSERT_NE(pending_session, nullptr);

  for (size_t i = 0;
       i < PendingTransportSessionImpl::kMaxBufferedOutboundMessages; ++i) {
    EXPECT_TRUE(
        pending_session->SendUpstreamMessage(PayloadType::kControl, command)
            .has_value());
  }
  EXPECT_EQ(pending_session->SendUpstreamMessage(PayloadType::kControl, command)
                .error(),
            SendUpstreamMessageError::kBufferFull);
}

TEST_F(TransportServiceImplTest,
       StartClientSessionHandlesSessionResetDuringGraduationFlush) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  TransportSessionRegistry* registry =
      service->GetChannel()->GetSessionRegistry();
  ASSERT_NE(registry, nullptr);

  ResetOnSessionRegisteredObserver observer(&identity_test_env_,
                                            "session_reset_flush");
  base::ScopedObservation<TransportSessionRegistry,
                          TransportSessionRegistry::Observer>
      observation(&observer);
  observation.Observe(registry);

  browser_actuator::ControlCommand command;
  command.mutable_close_channel();

  base::test::TestFuture<
      base::expected<TransportSession*, StartClientSessionError>>
      future;
  TransportSession* pending_session = service->StartClientSession(
      "session_reset_flush", PayloadType::kControl, command,
      future.GetCallback());
  ASSERT_NE(pending_session, nullptr);
  EXPECT_TRUE(
      pending_session->SendUpstreamMessage(PayloadType::kControl, command)
          .has_value());

  test_url_loader_factory_.AddResponse(
      "https://transport.test/v1:sendSessionMessage", "OK", net::HTTP_OK);
  base::expected<TransportSession*, StartClientSessionError> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StartClientSessionError::kChannelUnavailable);
  EXPECT_EQ(service->GetSession("session_reset_flush"), nullptr);
}

}  // namespace
}  // namespace stream_transport
