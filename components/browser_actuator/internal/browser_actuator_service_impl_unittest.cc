// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/session_stream_recorder.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

namespace {

class FakeHandlerFactory : public TransportHandlerFactory {
 public:
  explicit FakeHandlerFactory(FactoryId id,
                              std::vector<PayloadType> supported_types =
                                  {PayloadType::kExperimentalTriggering},
                              size_t* get_supported_types_calls = nullptr)
      : id_(id),
        supported_types_(std::move(supported_types)),
        get_supported_types_calls_(get_supported_types_calls) {}

  FactoryId GetFactoryId() const override { return id_; }
  std::vector<PayloadType> GetSupportedPayloadTypes() const override {
    if (get_supported_types_calls_) {
      ++(*get_supported_types_calls_);
    }
    return supported_types_;
  }
  std::unique_ptr<TransportHandler> OnNewSession(
      TransportSession* session) override {
    return nullptr;
  }

 private:
  const FactoryId id_;
  const std::vector<PayloadType> supported_types_;
  raw_ptr<size_t> get_supported_types_calls_;
};

}  // namespace

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
                                     identity_test_env_.identity_manager(),
                                     /*extra_factories=*/{});
  EXPECT_TRUE(service.IsInitialized());
}
TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsValidChannel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager(),
                                     /*extra_factories=*/{});
  EXPECT_NE(service.GetChannel(), nullptr);
}
TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsNullIfChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl disabled_service(
      test_shared_url_loader_factory_, identity_test_env_.identity_manager(),
      /*extra_factories=*/{});
  EXPECT_EQ(disabled_service.GetChannel(), nullptr);
}

TEST_F(BrowserActuatorServiceImplTest, ExtraFactoryIsRegisteredWithChannel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories;
  auto factory =
      std::make_unique<FakeHandlerFactory>(FactoryId::kSessionStreamRecorder);
  FakeHandlerFactory* factory_ptr = factory.get();
  extra_factories.push_back(std::move(factory));

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager(),
                                     std::move(extra_factories));

  ASSERT_NE(service.GetChannel(), nullptr);
  ASSERT_NE(service.GetChannel()->GetHandlerFactoryRegistry(), nullptr);
  const auto& factories =
      service.GetChannel()->GetHandlerFactoryRegistry()->GetFactories(
          PayloadType::kExperimentalTriggering);
  EXPECT_THAT(factories, testing::Contains(factory_ptr));
}

TEST_F(BrowserActuatorServiceImplTest, GetFactoryFindsRegisteredFactory) {
  std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories;
  auto triggering_factory =
      std::make_unique<FakeHandlerFactory>(FactoryId::kExperimentalTriggering);
  FakeHandlerFactory* triggering_ptr = triggering_factory.get();
  auto recorder_factory =
      std::make_unique<FakeHandlerFactory>(FactoryId::kSessionStreamRecorder);
  FakeHandlerFactory* recorder_ptr = recorder_factory.get();
  extra_factories.push_back(std::move(triggering_factory));
  extra_factories.push_back(std::move(recorder_factory));

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager(),
                                     std::move(extra_factories));

  EXPECT_EQ(service.GetFactory(FactoryId::kExperimentalTriggering),
            triggering_ptr);
  EXPECT_EQ(service.GetFactory(FactoryId::kSessionStreamRecorder),
            recorder_ptr);
}

TEST_F(BrowserActuatorServiceImplTest, GetFactoryReturnsNullForUnknownId) {
  std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories;
  extra_factories.push_back(
      std::make_unique<FakeHandlerFactory>(FactoryId::kSessionStreamRecorder));

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager(),
                                     std::move(extra_factories));

  EXPECT_EQ(service.GetFactory(FactoryId::kUnset), nullptr);
}

TEST_F(BrowserActuatorServiceImplTest, GetFactoryWorksWhenChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);

  std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories;
  auto factory =
      std::make_unique<FakeHandlerFactory>(FactoryId::kSessionStreamRecorder);
  FakeHandlerFactory* factory_ptr = factory.get();
  extra_factories.push_back(std::move(factory));

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager(),
                                     std::move(extra_factories));

  EXPECT_EQ(service.GetChannel(), nullptr);
  EXPECT_EQ(service.GetFactory(FactoryId::kSessionStreamRecorder), factory_ptr);
}

TEST_F(BrowserActuatorServiceImplTest, DestructionUnregistersFactory) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);

  size_t get_supported_types_calls = 0;
  std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories;
  extra_factories.push_back(std::make_unique<FakeHandlerFactory>(
      FactoryId::kSessionStreamRecorder,
      std::vector<PayloadType>{PayloadType::kExperimentalTriggering},
      &get_supported_types_calls));

  {
    BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                       identity_test_env_.identity_manager(),
                                       std::move(extra_factories));
    ASSERT_NE(service.GetChannel(), nullptr);
    // Called once during RegisterFactory() in the constructor.
    EXPECT_EQ(1u, get_supported_types_calls);
  }
  // Called a second time during UnregisterFactory() in the destructor.
  EXPECT_EQ(2u, get_supported_types_calls);
}

TEST_F(
    BrowserActuatorServiceImplTest,
    SessionStreamRecorderFactoryObservesUpstreamMessagesWhenInternalsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kBrowserActuatorChannelEnabled,
                            kBrowserActuatorInternals},
      /*disabled_features=*/{});

  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager(),
                                     /*extra_factories=*/{});

  ASSERT_NE(service.GetChannel(), nullptr);
  TransportHandlerFactory* factory =
      service.GetFactory(FactoryId::kSessionStreamRecorder);
  ASSERT_NE(factory, nullptr);
  auto* recorder_factory = static_cast<SessionStreamRecorderFactory*>(factory);

  TransportSession* session = service.GetOrCreateSession("s1");
  ASSERT_NE(session, nullptr);
  std::unique_ptr<TransportHandler> handler =
      recorder_factory->OnNewSession(session);
  ASSERT_NE(handler, nullptr);
  EXPECT_EQ(recorder_factory->GetActiveRecordersCountForTesting(), 1u);

  ControlCommand command;
  command.mutable_close_channel();
  service.GetChannel()->SendUpstreamMessage("s1", PayloadType::kControl,
                                            command);

  base::DictValue dump = recorder_factory->ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* s1_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s1_dict, nullptr);
  EXPECT_EQ(s1_dict->FindInt("total_upstream_messages"), 1);
}

}  // namespace browser_actuator
