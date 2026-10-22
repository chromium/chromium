// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browser_actuator/internal/transport/features.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/session_stream_recorder.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_channel.h"
#include "components/browser_actuator/public/transport/transport_handler.h"
#include "components/browser_actuator/public/transport/transport_handler_factory_registry.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

class BrowserActuatorServiceImplTest : public testing::Test {
 protected:
  BrowserActuatorServiceImplTest()
      : identity_test_env_(&test_url_loader_factory_) {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }
  ~BrowserActuatorServiceImplTest() override = default;

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(BrowserActuatorServiceImplTest, IsInitialized) {
  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());
  EXPECT_TRUE(service.IsInitialized());
}

TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsValidChannel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(transport::kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());
  EXPECT_NE(service.GetChannel(), nullptr);
}

TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsNullIfChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(transport::kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl disabled_service(
      test_shared_url_loader_factory_, identity_test_env_.identity_manager());
  EXPECT_EQ(disabled_service.GetChannel(), nullptr);
}

TEST_F(BrowserActuatorServiceImplTest,
       RecorderFactoryIsRegisteredWithChannelWhenInternalsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{transport::kBrowserActuatorChannelEnabled,
                            kBrowserActuatorInternals},
      /*disabled_features=*/{});

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());

  transport::SessionStreamRecorderFactory* recorder_factory =
      service.GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);
  ASSERT_NE(service.GetChannel(), nullptr);
  ASSERT_NE(service.GetChannel()->GetHandlerFactoryRegistry(), nullptr);
  const auto& factories =
      service.GetChannel()->GetHandlerFactoryRegistry()->GetFactories(
          transport::PayloadType::kControl);
  EXPECT_THAT(factories, testing::Contains(recorder_factory));
}

TEST_F(BrowserActuatorServiceImplTest,
       GetStreamRecorderFactoryReturnsNullWhenInternalsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorInternals);

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());

  EXPECT_EQ(service.GetStreamRecorderFactory(), nullptr);
}

TEST_F(BrowserActuatorServiceImplTest,
       GetStreamRecorderFactoryWorksWhenChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuatorInternals},
      /*disabled_features=*/{transport::kBrowserActuatorChannelEnabled});

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());

  EXPECT_EQ(service.GetChannel(), nullptr);
  EXPECT_NE(service.GetStreamRecorderFactory(), nullptr);
}

TEST_F(
    BrowserActuatorServiceImplTest,
    SessionStreamRecorderFactoryObservesUpstreamMessagesWhenInternalsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{transport::kBrowserActuatorChannelEnabled,
                            kBrowserActuatorInternals},
      /*disabled_features=*/{});

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());

  ASSERT_NE(service.GetChannel(), nullptr);
  transport::SessionStreamRecorderFactory* recorder_factory =
      service.GetStreamRecorderFactory();
  ASSERT_NE(recorder_factory, nullptr);

  transport::TransportSession* session = service.GetOrCreateSession("s1");
  ASSERT_NE(session, nullptr);
  std::unique_ptr<transport::TransportHandler> handler =
      recorder_factory->OnNewSession(session);
  ASSERT_NE(handler, nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  ControlCommand command;
  command.mutable_close_channel();
  service.GetChannel()->SendUpstreamMessage(
      "s1", transport::PayloadType::kControl, command, 1);

  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* s1_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s1_dict, nullptr);
  EXPECT_EQ(s1_dict->FindInt("total_upstream_messages"), 1);
}

}  // namespace browser_actuator
