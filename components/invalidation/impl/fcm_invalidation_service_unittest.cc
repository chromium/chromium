// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/gcm_invalidation_bridge.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_service_test_template.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using instance_id::InstanceID;
using instance_id::InstanceIDDriver;
using testing::_;
using testing::StrictMock;

namespace invalidation {

namespace {

class TestFCMSyncNetworkChannel : public syncer::FCMSyncNetworkChannel {
 public:
  void StartListening() override {}
  void StopListening() override {}
};

// TODO: Make FCMInvalidationListener class abstract and explicitly make all the
// methods virtual. Provide FCMInvalidationListenerImpl and
// FakeFCMInvalidationListener classes that will inherit from
// FCMInvalidationListener. The reason for such a change is that
// FCMInvalidationService relies of FCMInvalidationListener class.
class FakeFCMInvalidationListener : public syncer::FCMInvalidationListener {
 public:
  FakeFCMInvalidationListener(
      std::unique_ptr<syncer::FCMSyncNetworkChannel> network_channel);
  ~FakeFCMInvalidationListener() override;

  void RequestDetailedStatus(
      const base::RepeatingCallback<void(const base::DictionaryValue&)>&
          callback) const override;
};

FakeFCMInvalidationListener::FakeFCMInvalidationListener(
    std::unique_ptr<syncer::FCMSyncNetworkChannel> network_channel)
    : syncer::FCMInvalidationListener(std::move(network_channel)) {}

FakeFCMInvalidationListener::~FakeFCMInvalidationListener() {}

void FakeFCMInvalidationListener::RequestDetailedStatus(
    const base::RepeatingCallback<void(const base::DictionaryValue&)>& callback)
    const {
  callback.Run(base::DictionaryValue());
}

}  // namespace

const char kApplicationName[] = "com.google.chrome.fcm.invalidations";

class MockInstanceID : public InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;

  MOCK_METHOD1(GetID, void(const GetIDCallback& callback));
  MOCK_METHOD1(GetCreationTime, void(const GetCreationTimeCallback& callback));
  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                std::set<Flags> flags,
                GetTokenCallback callback) override {
    GetToken_(authorized_entity, scope, options, std::move(flags), callback);
  }
  MOCK_METHOD5(GetToken_,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::map<std::string, std::string>& options,
                    std::set<Flags> flags,
                    GetTokenCallback& callback));
  MOCK_METHOD4(ValidateToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::string& token,
                    const ValidateTokenCallback& callback));

 protected:
  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {
    DeleteTokenImpl_(authorized_entity, scope, callback);
  }
  MOCK_METHOD3(DeleteTokenImpl_,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    DeleteTokenCallback& callback));
  void DeleteIDImpl(DeleteIDCallback callback) override {
    DeleteIDImpl_(callback);
  }
  MOCK_METHOD1(DeleteIDImpl_, void(DeleteIDCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceID);
};

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID, InstanceID*(const std::string& app_id));
  MOCK_METHOD1(RemoveInstanceID, void(const std::string& app_id));
  MOCK_CONST_METHOD1(ExistsInstanceID, bool(const std::string& app_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class FCMInvalidationServiceTestDelegate {
 public:
  FCMInvalidationServiceTestDelegate() {
    pref_service_.registry()->RegisterStringPref(
        prefs::kFCMInvalidationClientIDCacheDeprecated,
        /*default_value=*/std::string());
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kInvalidationClientIDCache);
    syncer::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
        pref_service_.registry());
  }

  ~FCMInvalidationServiceTestDelegate() {}

  void CreateInvalidationService() {
    CreateUninitializedInvalidationService();
    InitializeInvalidationService();
  }

  void CreateUninitializedInvalidationService() {
    gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();

    identity_provider_ = std::make_unique<ProfileIdentityProvider>(
        identity_test_env_.identity_manager());

    mock_instance_id_driver_ = std::make_unique<MockInstanceIDDriver>();
    mock_instance_id_ = std::make_unique<MockInstanceID>();
    EXPECT_CALL(*mock_instance_id_driver_, GetInstanceID(kApplicationName))
        .WillRepeatedly(testing::Return(mock_instance_id_.get()));

    invalidation_service_ = std::make_unique<FCMInvalidationService>(
        identity_provider_.get(),
        base::BindRepeating(&syncer::FCMNetworkHandler::Create,
                            gcm_driver_.get(), mock_instance_id_driver_.get()),
        base::BindRepeating(&syncer::PerUserTopicRegistrationManager::Create,
                            identity_provider_.get(), &pref_service_,
                            &url_loader_factory_),
        mock_instance_id_driver_.get(), &pref_service_);
  }

  void InitializeInvalidationService() {
    fake_listener_ = new FakeFCMInvalidationListener(
        std::make_unique<TestFCMSyncNetworkChannel>());
    invalidation_service_->InitForTest(base::WrapUnique(fake_listener_));
  }

  InvalidationService* GetInvalidationService() {
    return invalidation_service_.get();
  }

  void DestroyInvalidationService() { invalidation_service_.reset(); }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    fake_listener_->EmitStateChangeForTest(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) {
    fake_listener_->EmitSavedInvalidationsForTest(
        ConvertObjectIdInvalidationMapToTopicInvalidationMap(invalidation_map));
  }

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<gcm::GCMDriver> gcm_driver_;
  std::unique_ptr<MockInstanceIDDriver> mock_instance_id_driver_;
  std::unique_ptr<MockInstanceID> mock_instance_id_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<invalidation::IdentityProvider> identity_provider_;
  syncer::FCMInvalidationListener* fake_listener_;  // Owned by the service.
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;

  // The service has to be below the provider since the service keeps
  // a non-owned pointer to the provider.
  std::unique_ptr<FCMInvalidationService> invalidation_service_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(FCMInvalidationServiceTest,
                               InvalidationServiceTest,
                               FCMInvalidationServiceTestDelegate);

namespace internal {

class FakeCallbackContainer {
 public:
  FakeCallbackContainer() : called_(false) {}

  void FakeCallback(const base::DictionaryValue& value) { called_ = true; }

  bool called_;
  base::WeakPtrFactory<FakeCallbackContainer> weak_ptr_factory_{this};
};

}  // namespace internal

// Test that requesting for detailed status doesn't crash even if the
// underlying invalidator is not initialized.
TEST(FCMInvalidationServiceLoggingTest, DetailedStatusCallbacksWork) {
  std::unique_ptr<FCMInvalidationServiceTestDelegate> delegate(
      new FCMInvalidationServiceTestDelegate());

  delegate->CreateUninitializedInvalidationService();
  invalidation::InvalidationService* const invalidator =
      delegate->GetInvalidationService();

  internal::FakeCallbackContainer fake_container;
  invalidator->RequestDetailedStatus(
      base::BindRepeating(&internal::FakeCallbackContainer::FakeCallback,
                          fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_container.called_);

  delegate->InitializeInvalidationService();

  invalidator->RequestDetailedStatus(
      base::BindRepeating(&internal::FakeCallbackContainer::FakeCallback,
                          fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_container.called_);
}

}  // namespace invalidation
