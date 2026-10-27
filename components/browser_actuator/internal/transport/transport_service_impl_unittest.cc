// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/transport_service_impl.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browser_actuator/internal/transport/features.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/session_stream_recorder.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_channel.h"
#include "components/browser_actuator/public/transport/transport_handler.h"
#include "components/browser_actuator/public/transport/transport_handler_factory_registry.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_actuator::transport {
namespace {

class TransportServiceImplTest : public testing::Test {
 protected:
  TransportServiceImplTest() : identity_test_env_(&test_url_loader_factory_) {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
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

TEST_F(TransportServiceImplTest, GetChannelReturnsNullIfChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);
  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/false);
  EXPECT_EQ(service->GetChannel(), nullptr);
}

TEST_F(TransportServiceImplTest,
       RecorderFactoryIsRegisteredWithChannelWhenRecordingSessions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::unique_ptr<TransportServiceImpl> service =
      CreateService(/*record_sessions=*/true);

  SessionStreamRecorderFactory* recorder_factory =
      service->GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);
  ASSERT_NE(service->GetChannel(), nullptr);
  ASSERT_NE(service->GetChannel()->GetHandlerFactoryRegistry(), nullptr);
  const auto& factories =
      service->GetChannel()->GetHandlerFactoryRegistry()->GetFactories(
          PayloadType::kControl);
  EXPECT_THAT(factories, testing::Contains(recorder_factory));
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

  TransportSession* session = service->CreateSession("s1");
  ASSERT_NE(session, nullptr);
  std::unique_ptr<TransportHandler> handler =
      recorder_factory->OnNewSession(session);
  ASSERT_NE(handler, nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  ControlCommand command;
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

}  // namespace
}  // namespace browser_actuator::transport
