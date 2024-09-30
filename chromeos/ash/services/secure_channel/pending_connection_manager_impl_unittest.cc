// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/pending_connection_manager_impl.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/ble_listener_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/fake_ble_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/fake_nearby_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_pending_connection_manager.h"
#include "chromeos/ash/services/secure_channel/fake_pending_connection_request.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/pending_ble_initiator_connection_request.h"
#include "chromeos/ash/services/secure_channel/pending_ble_listener_connection_request.h"
#include "chromeos/ash/services/secure_channel/pending_nearby_initiator_connection_request.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const char kTestFeature[] = "testFeature";

class FakeBleInitiatorConnectionAttemptFactory
    : public BleInitiatorConnectionAttempt::Factory {
 public:
  FakeBleInitiatorConnectionAttemptFactory(
      FakeBleConnectionManager* expected_ble_connection_manager)
      : expected_ble_connection_manager_(expected_ble_connection_manager) {}

  FakeBleInitiatorConnectionAttemptFactory(
      const FakeBleInitiatorConnectionAttemptFactory&) = delete;
  FakeBleInitiatorConnectionAttemptFactory& operator=(
      const FakeBleInitiatorConnectionAttemptFactory&) = delete;

  ~FakeBleInitiatorConnectionAttemptFactory() override = default;

  void set_expected_connection_attempt_details(
      const ConnectionAttemptDetails& expected_connection_attempt_details) {
    expected_connection_attempt_details_ = expected_connection_attempt_details;
  }

  base::flat_map<
      ConnectionAttemptDetails,
      raw_ptr<FakeConnectionAttempt<BleInitiatorFailureType>, CtnExperimental>>&
  details_to_active_attempt_map() {
    return details_to_active_attempt_map_;
  }

  size_t num_instances_created() const { return num_instances_created_; }
  size_t num_instances_deleted() const { return num_instances_deleted_; }

  FakeConnectionAttempt<BleInitiatorFailureType>* last_created_instance() {
    return last_created_instance_;
  }

 private:
  // BleInitiatorConnectionAttempt::Factory:
  std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>> CreateInstance(
      BleConnectionManager* ble_connection_manager,
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details) override {
    EXPECT_EQ(ConnectionRole::kInitiatorRole,
              connection_attempt_details.connection_role());
    EXPECT_EQ(expected_ble_connection_manager_, ble_connection_manager);
    EXPECT_EQ(*expected_connection_attempt_details_,
              connection_attempt_details);

    auto instance =
        std::make_unique<FakeConnectionAttempt<BleInitiatorFailureType>>(
            delegate, connection_attempt_details,
            base::BindOnce(
                &FakeBleInitiatorConnectionAttemptFactory::OnAttemptDeleted,
                base::Unretained(this), connection_attempt_details));

    ++num_instances_created_;
    last_created_instance_ = instance.get();
    details_to_active_attempt_map_[connection_attempt_details] =
        last_created_instance_;

    return instance;
  }

  void OnAttemptDeleted(
      const ConnectionAttemptDetails& connection_attempt_details) {
    size_t num_erased =
        details_to_active_attempt_map_.erase(connection_attempt_details);
    EXPECT_EQ(1u, num_erased);
    ++num_instances_deleted_;
  }

  raw_ptr<FakeBleConnectionManager> expected_ble_connection_manager_;
  std::optional<ConnectionAttemptDetails> expected_connection_attempt_details_;

  base::flat_map<
      ConnectionAttemptDetails,
      raw_ptr<FakeConnectionAttempt<BleInitiatorFailureType>, CtnExperimental>>
      details_to_active_attempt_map_;

  size_t num_instances_created_ = 0u;
  size_t num_instances_deleted_ = 0u;
  raw_ptr<FakeConnectionAttempt<BleInitiatorFailureType>, DanglingUntriaged>
      last_created_instance_ = nullptr;
};

class FakeBleListenerConnectionAttemptFactory
    : public BleListenerConnectionAttempt::Factory {
 public:
  FakeBleListenerConnectionAttemptFactory(
      FakeBleConnectionManager* expected_ble_connection_manager)
      : expected_ble_connection_manager_(expected_ble_connection_manager) {}

  FakeBleListenerConnectionAttemptFactory(
      const FakeBleListenerConnectionAttemptFactory&) = delete;
  FakeBleListenerConnectionAttemptFactory& operator=(
      const FakeBleListenerConnectionAttemptFactory&) = delete;

  ~FakeBleListenerConnectionAttemptFactory() override = default;

  void set_expected_connection_attempt_details(
      const ConnectionAttemptDetails& expected_connection_attempt_details) {
    expected_connection_attempt_details_ = expected_connection_attempt_details;
  }

  base::flat_map<
      ConnectionAttemptDetails,
      raw_ptr<FakeConnectionAttempt<BleListenerFailureType>, CtnExperimental>>&
  details_to_active_attempt_map() {
    return details_to_active_attempt_map_;
  }

  size_t num_instances_created() const { return num_instances_created_; }
  size_t num_instances_deleted() const { return num_instances_deleted_; }

  FakeConnectionAttempt<BleListenerFailureType>* last_created_instance() {
    return last_created_instance_;
  }

 private:
  // BleListenerConnectionAttempt::Factory:
  std::unique_ptr<ConnectionAttempt<BleListenerFailureType>> CreateInstance(
      BleConnectionManager* ble_connection_manager,
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details) override {
    EXPECT_EQ(ConnectionRole::kListenerRole,
              connection_attempt_details.connection_role());
    EXPECT_EQ(expected_ble_connection_manager_, ble_connection_manager);
    EXPECT_EQ(*expected_connection_attempt_details_,
              connection_attempt_details);

    auto instance =
        std::make_unique<FakeConnectionAttempt<BleListenerFailureType>>(
            delegate, connection_attempt_details,
            base::BindOnce(
                &FakeBleListenerConnectionAttemptFactory::OnAttemptDeleted,
                base::Unretained(this), connection_attempt_details));

    ++num_instances_created_;
    last_created_instance_ = instance.get();
    details_to_active_attempt_map_[connection_attempt_details] =
        last_created_instance_;

    return instance;
  }

  void OnAttemptDeleted(
      const ConnectionAttemptDetails& connection_attempt_details) {
    size_t num_erased =
        details_to_active_attempt_map_.erase(connection_attempt_details);
    EXPECT_EQ(1u, num_erased);
    ++num_instances_deleted_;
  }

  raw_ptr<FakeBleConnectionManager> expected_ble_connection_manager_;
  std::optional<ConnectionAttemptDetails> expected_connection_attempt_details_;

  base::flat_map<
      ConnectionAttemptDetails,
      raw_ptr<FakeConnectionAttempt<BleListenerFailureType>, CtnExperimental>>
      details_to_active_attempt_map_;

  size_t num_instances_created_ = 0u;
  size_t num_instances_deleted_ = 0u;
  raw_ptr<FakeConnectionAttempt<BleListenerFailureType>, DanglingUntriaged>
      last_created_instance_ = nullptr;
};

class FakeNearbyInitiatorConnectionAttemptFactory
    : public NearbyInitiatorConnectionAttempt::Factory {
 public:
  FakeNearbyInitiatorConnectionAttemptFactory(
      FakeNearbyConnectionManager* expected_nearby_connection_manager)
      : expected_nearby_connection_manager_(
            expected_nearby_connection_manager) {}

  FakeNearbyInitiatorConnectionAttemptFactory(
      const FakeNearbyInitiatorConnectionAttemptFactory&) = delete;
  FakeNearbyInitiatorConnectionAttemptFactory& operator=(
      const FakeNearbyInitiatorConnectionAttemptFactory&) = delete;

  ~FakeNearbyInitiatorConnectionAttemptFactory() override = default;

  void set_expected_connection_attempt_details(
      const ConnectionAttemptDetails& expected_connection_attempt_details) {
    expected_connection_attempt_details_ = expected_connection_attempt_details;
  }

  base::flat_map<ConnectionAttemptDetails,
                 raw_ptr<FakeConnectionAttempt<NearbyInitiatorFailureType>,
                         CtnExperimental>>&
  details_to_active_attempt_map() {
    return details_to_active_attempt_map_;
  }

  size_t num_instances_created() const { return num_instances_created_; }
  size_t num_instances_deleted() const { return num_instances_deleted_; }

  FakeConnectionAttempt<NearbyInitiatorFailureType>* last_created_instance() {
    return last_created_instance_;
  }

 private:
  // NearbyInitiatorConnectionAttempt::Factory:
  std::unique_ptr<ConnectionAttempt<NearbyInitiatorFailureType>> CreateInstance(
      NearbyConnectionManager* nearby_connection_manager,
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details) override {
    EXPECT_EQ(ConnectionRole::kInitiatorRole,
              connection_attempt_details.connection_role());
    EXPECT_EQ(expected_nearby_connection_manager_, nearby_connection_manager);
    EXPECT_EQ(*expected_connection_attempt_details_,
              connection_attempt_details);

    auto instance =
        std::make_unique<FakeConnectionAttempt<NearbyInitiatorFailureType>>(
            delegate, connection_attempt_details,
            base::BindOnce(
                &FakeNearbyInitiatorConnectionAttemptFactory::OnAttemptDeleted,
                base::Unretained(this), connection_attempt_details));

    ++num_instances_created_;
    last_created_instance_ = instance.get();
    details_to_active_attempt_map_[connection_attempt_details] =
        last_created_instance_;

    return instance;
  }

  void OnAttemptDeleted(
      const ConnectionAttemptDetails& connection_attempt_details) {
    size_t num_erased =
        details_to_active_attempt_map_.erase(connection_attempt_details);
    EXPECT_EQ(1u, num_erased);
    ++num_instances_deleted_;
  }

  raw_ptr<FakeNearbyConnectionManager> expected_nearby_connection_manager_;
  std::optional<ConnectionAttemptDetails> expected_connection_attempt_details_;

  base::flat_map<ConnectionAttemptDetails,
                 raw_ptr<FakeConnectionAttempt<NearbyInitiatorFailureType>,
                         CtnExperimental>>
      details_to_active_attempt_map_;

  size_t num_instances_created_ = 0u;
  size_t num_instances_deleted_ = 0u;
  raw_ptr<FakeConnectionAttempt<NearbyInitiatorFailureType>, DanglingUntriaged>
      last_created_instance_ = nullptr;
};

class FakePendingBleInitiatorConnectionRequestFactory
    : public PendingBleInitiatorConnectionRequest::Factory {
 public:
  FakePendingBleInitiatorConnectionRequestFactory() = default;

  FakePendingBleInitiatorConnectionRequestFactory(
      const FakePendingBleInitiatorConnectionRequestFactory&) = delete;
  FakePendingBleInitiatorConnectionRequestFactory& operator=(
      const FakePendingBleInitiatorConnectionRequestFactory&) = delete;

  ~FakePendingBleInitiatorConnectionRequestFactory() override = default;

  void SetExpectationsForNextCall(
      ClientConnectionParameters* expected_client_connection_parameters,
      ConnectionPriority expected_connection_priority) {
    expected_client_connection_parameters_ =
        expected_client_connection_parameters;
    expected_connection_priority_ = expected_connection_priority;
  }

  FakePendingConnectionRequest<BleInitiatorFailureType>*
  last_created_instance() {
    return last_created_instance_;
  }

 private:
  // PendingBleInitiatorConnectionRequest::Factory:
  std::unique_ptr<PendingConnectionRequest<BleInitiatorFailureType>>
  CreateInstance(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override {
    EXPECT_EQ(expected_client_connection_parameters_,
              client_connection_parameters.get());
    EXPECT_EQ(*expected_connection_priority_, connection_priority);

    auto instance =
        std::make_unique<FakePendingConnectionRequest<BleInitiatorFailureType>>(
            delegate, connection_priority);
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<ClientConnectionParameters, DanglingUntriaged>
      expected_client_connection_parameters_ = nullptr;
  std::optional<ConnectionPriority> expected_connection_priority_;

  raw_ptr<FakePendingConnectionRequest<BleInitiatorFailureType>,
          DanglingUntriaged>
      last_created_instance_ = nullptr;
};

class FakePendingBleListenerConnectionRequestFactory
    : public PendingBleListenerConnectionRequest::Factory {
 public:
  FakePendingBleListenerConnectionRequestFactory() = default;

  FakePendingBleListenerConnectionRequestFactory(
      const FakePendingBleListenerConnectionRequestFactory&) = delete;
  FakePendingBleListenerConnectionRequestFactory& operator=(
      const FakePendingBleListenerConnectionRequestFactory&) = delete;

  ~FakePendingBleListenerConnectionRequestFactory() override = default;

  void SetExpectationsForNextCall(
      ClientConnectionParameters* expected_client_connection_parameters,
      ConnectionPriority expected_connection_priority) {
    expected_client_connection_parameters_ =
        expected_client_connection_parameters;
    expected_connection_priority_ = expected_connection_priority;
  }

  FakePendingConnectionRequest<BleListenerFailureType>*
  last_created_instance() {
    return last_created_instance_;
  }

 private:
  // PendingBleListenerConnectionRequest::Factory:
  std::unique_ptr<PendingConnectionRequest<BleListenerFailureType>>
  CreateInstance(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override {
    EXPECT_EQ(expected_client_connection_parameters_,
              client_connection_parameters.get());
    EXPECT_EQ(*expected_connection_priority_, connection_priority);

    auto instance =
        std::make_unique<FakePendingConnectionRequest<BleListenerFailureType>>(
            delegate, connection_priority);
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<ClientConnectionParameters, DanglingUntriaged>
      expected_client_connection_parameters_ = nullptr;
  std::optional<ConnectionPriority> expected_connection_priority_;

  raw_ptr<FakePendingConnectionRequest<BleListenerFailureType>,
          DanglingUntriaged>
      last_created_instance_ = nullptr;
};

class FakePendingNearbyInitiatorConnectionRequestFactory
    : public PendingNearbyInitiatorConnectionRequest::Factory {
 public:
  FakePendingNearbyInitiatorConnectionRequestFactory() = default;

  FakePendingNearbyInitiatorConnectionRequestFactory(
      const FakePendingNearbyInitiatorConnectionRequestFactory&) = delete;
  FakePendingNearbyInitiatorConnectionRequestFactory& operator=(
      const FakePendingNearbyInitiatorConnectionRequestFactory&) = delete;

  ~FakePendingNearbyInitiatorConnectionRequestFactory() override = default;

  void SetExpectationsForNextCall(
      ClientConnectionParameters* expected_client_connection_parameters,
      ConnectionPriority expected_connection_priority) {
    expected_client_connection_parameters_ =
        expected_client_connection_parameters;
    expected_connection_priority_ = expected_connection_priority;
  }

  FakePendingConnectionRequest<NearbyInitiatorFailureType>*
  last_created_instance() {
    return last_created_instance_;
  }

 private:
  // PendingNearbyInitiatorConnectionRequest::Factory:
  std::unique_ptr<PendingConnectionRequest<NearbyInitiatorFailureType>>
  CreateInstance(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override {
    EXPECT_EQ(expected_client_connection_parameters_,
              client_connection_parameters.get());
    EXPECT_EQ(*expected_connection_priority_, connection_priority);

    auto instance = std::make_unique<
        FakePendingConnectionRequest<NearbyInitiatorFailureType>>(
        delegate, connection_priority);
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<ClientConnectionParameters, DanglingUntriaged>
      expected_client_connection_parameters_ = nullptr;
  std::optional<ConnectionPriority> expected_connection_priority_;

  raw_ptr<FakePendingConnectionRequest<NearbyInitiatorFailureType>,
          DanglingUntriaged>
      last_created_instance_ = nullptr;
};

std::vector<std::unique_ptr<ClientConnectionParameters>>
GenerateFakeClientParameters(size_t num_to_generate) {
  std::vector<std::unique_ptr<ClientConnectionParameters>> client_parameters;

  for (size_t i = 0; i < num_to_generate; ++i) {
    // Generate a unique feature name.
    std::stringstream ss;
    ss << kTestFeature << "_" << i;
    client_parameters.push_back(
        std::make_unique<FakeClientConnectionParameters>(ss.str()));
  }

  return client_parameters;
}

std::vector<ClientConnectionParameters*> ClientParamsListToRawPtrs(
    const std::vector<std::unique_ptr<ClientConnectionParameters>>&
        unique_ptr_list) {
  return base::ToVector(unique_ptr_list,
                        &std::unique_ptr<ClientConnectionParameters>::get);
}

}  // namespace

class SecureChannelPendingConnectionManagerImplTest : public testing::Test {
 public:
  SecureChannelPendingConnectionManagerImplTest(
      const SecureChannelPendingConnectionManagerImplTest&) = delete;
  SecureChannelPendingConnectionManagerImplTest& operator=(
      const SecureChannelPendingConnectionManagerImplTest&) = delete;

 protected:
  SecureChannelPendingConnectionManagerImplTest() = default;
  ~SecureChannelPendingConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_delegate_ = std::make_unique<FakePendingConnectionManagerDelegate>();
    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();
    fake_nearby_connection_manager_ =
        std::make_unique<FakeNearbyConnectionManager>();

    fake_ble_initiator_connection_attempt_factory_ =
        std::make_unique<FakeBleInitiatorConnectionAttemptFactory>(
            fake_ble_connection_manager_.get());
    BleInitiatorConnectionAttempt::Factory::SetFactoryForTesting(
        fake_ble_initiator_connection_attempt_factory_.get());

    fake_ble_listener_connection_attempt_factory_ =
        std::make_unique<FakeBleListenerConnectionAttemptFactory>(
            fake_ble_connection_manager_.get());
    BleListenerConnectionAttempt::Factory::SetFactoryForTesting(
        fake_ble_listener_connection_attempt_factory_.get());

    fake_nearby_initiator_connection_attempt_factory_ =
        std::make_unique<FakeNearbyInitiatorConnectionAttemptFactory>(
            fake_nearby_connection_manager_.get());
    NearbyInitiatorConnectionAttempt::Factory::SetFactoryForTesting(
        fake_nearby_initiator_connection_attempt_factory_.get());

    fake_pending_ble_initiator_connection_request_factory_ =
        std::make_unique<FakePendingBleInitiatorConnectionRequestFactory>();
    PendingBleInitiatorConnectionRequest::Factory::SetFactoryForTesting(
        fake_pending_ble_initiator_connection_request_factory_.get());

    fake_pending_ble_listener_connection_request_factory_ =
        std::make_unique<FakePendingBleListenerConnectionRequestFactory>();
    PendingBleListenerConnectionRequest::Factory::SetFactoryForTesting(
        fake_pending_ble_listener_connection_request_factory_.get());

    fake_pending_nearby_initiator_connection_request_factory_ =
        std::make_unique<FakePendingNearbyInitiatorConnectionRequestFactory>();
    PendingNearbyInitiatorConnectionRequest::Factory::SetFactoryForTesting(
        fake_pending_nearby_initiator_connection_request_factory_.get());

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    manager_ = PendingConnectionManagerImpl::Factory::Create(
        fake_delegate_.get(), fake_ble_connection_manager_.get(),
        fake_nearby_connection_manager_.get(), mock_adapter_);
  }

  void TearDown() override {
    BleInitiatorConnectionAttempt::Factory::SetFactoryForTesting(nullptr);
    BleListenerConnectionAttempt::Factory::SetFactoryForTesting(nullptr);
    NearbyInitiatorConnectionAttempt::Factory::SetFactoryForTesting(nullptr);
    PendingBleInitiatorConnectionRequest::Factory::SetFactoryForTesting(
        nullptr);
    PendingBleListenerConnectionRequest::Factory::SetFactoryForTesting(nullptr);
    PendingNearbyInitiatorConnectionRequest::Factory::SetFactoryForTesting(
        nullptr);
  }

  void HandleCanceledRequestAndVerifyNoInstancesCreated(
      const ConnectionAttemptDetails& connection_attempt_details,
      ConnectionPriority connection_priority) {
    FakeConnectionAttempt<
        BleInitiatorFailureType>* last_ble_initiator_attempt_before_call =
        fake_ble_initiator_connection_attempt_factory_->last_created_instance();
    FakeConnectionAttempt<
        BleListenerFailureType>* last_ble_listener_attempt_before_call =
        fake_ble_listener_connection_attempt_factory_->last_created_instance();
    FakeConnectionAttempt<NearbyInitiatorFailureType>*
        last_nearby_initiator_attempt_before_call =
            fake_nearby_initiator_connection_attempt_factory_
                ->last_created_instance();
    FakePendingConnectionRequest<BleInitiatorFailureType>*
        last_ble_initiator_request_before_call =
            fake_pending_ble_initiator_connection_request_factory_
                ->last_created_instance();
    FakePendingConnectionRequest<BleListenerFailureType>*
        last_ble_listener_request_before_call =
            fake_pending_ble_listener_connection_request_factory_
                ->last_created_instance();
    FakePendingConnectionRequest<NearbyInitiatorFailureType>*
        last_nearby_initiator_request_before_call =
            fake_pending_nearby_initiator_connection_request_factory_
                ->last_created_instance();

    HandleConnectionRequest(connection_attempt_details, connection_priority,
                            true /* cancel_request_before_adding */);

    // Since the request was canceled before it was added, no new attempts or
    // operations should have been created.
    EXPECT_EQ(last_ble_initiator_attempt_before_call,
              fake_ble_initiator_connection_attempt_factory_
                  ->last_created_instance());
    EXPECT_EQ(
        last_ble_listener_attempt_before_call,
        fake_ble_listener_connection_attempt_factory_->last_created_instance());
    EXPECT_EQ(last_nearby_initiator_attempt_before_call,
              fake_nearby_initiator_connection_attempt_factory_
                  ->last_created_instance());
    EXPECT_EQ(last_ble_initiator_request_before_call,
              fake_pending_ble_initiator_connection_request_factory_
                  ->last_created_instance());
    EXPECT_EQ(last_ble_listener_request_before_call,
              fake_pending_ble_listener_connection_request_factory_
                  ->last_created_instance());
    EXPECT_EQ(last_nearby_initiator_request_before_call,
              fake_pending_nearby_initiator_connection_request_factory_
                  ->last_created_instance());
  }

  void HandleRequestAndVerifyHandledByConnectionAttempt(
      const ConnectionAttemptDetails& connection_attempt_details,
      ConnectionPriority connection_priority) {
    HandleConnectionRequest(connection_attempt_details, connection_priority,
                            false /* cancel_request_before_adding */);
    switch (connection_attempt_details.connection_medium()) {
      case ConnectionMedium::kBluetoothLowEnergy:
        switch (connection_attempt_details.connection_role()) {
          case ConnectionRole::kInitiatorRole: {
            FakeConnectionAttempt<BleInitiatorFailureType>* active_attempt =
                GetActiveBleInitiatorAttempt(connection_attempt_details);
            base::UnguessableToken token_for_last_init_request =
                fake_pending_ble_initiator_connection_request_factory_
                    ->last_created_instance()
                    ->GetRequestId();
            EXPECT_TRUE(base::Contains(active_attempt->id_to_request_map(),
                                       token_for_last_init_request));
            break;
          }

          case ConnectionRole::kListenerRole: {
            FakeConnectionAttempt<BleListenerFailureType>* active_attempt =
                GetActiveBleListenerAttempt(connection_attempt_details);
            base::UnguessableToken token_for_last_listen_request =
                fake_pending_ble_listener_connection_request_factory_
                    ->last_created_instance()
                    ->GetRequestId();
            EXPECT_TRUE(base::Contains(active_attempt->id_to_request_map(),
                                       token_for_last_listen_request));
            break;
          }
        }
        break;

      case ConnectionMedium::kNearbyConnections:
        switch (connection_attempt_details.connection_role()) {
          case ConnectionRole::kInitiatorRole: {
            FakeConnectionAttempt<NearbyInitiatorFailureType>* active_attempt =
                GetActiveNearbyInitiatorAttempt(connection_attempt_details);
            base::UnguessableToken token_for_last_init_request =
                fake_pending_nearby_initiator_connection_request_factory_
                    ->last_created_instance()
                    ->GetRequestId();
            EXPECT_TRUE(base::Contains(active_attempt->id_to_request_map(),
                                       token_for_last_init_request));
            break;
          }

          case ConnectionRole::kListenerRole:
            NOTREACHED_IN_MIGRATION();
        }
        break;
    }
  }

  void FinishBleInitiatorAttemptWithoutConnection(
      FakeConnectionAttempt<BleInitiatorFailureType>* attempt) {
    ConnectionAttemptDetails details_for_attempt =
        attempt->connection_attempt_details();
    EXPECT_EQ(GetActiveBleInitiatorAttempt(details_for_attempt), attempt);

    attempt->OnConnectionAttemptFinishedWithoutConnection();
    EXPECT_FALSE(GetActiveBleInitiatorAttempt(details_for_attempt));
  }

  void FinishBleListenerAttemptWithoutConnection(
      FakeConnectionAttempt<BleListenerFailureType>* attempt) {
    ConnectionAttemptDetails details_for_attempt =
        attempt->connection_attempt_details();
    EXPECT_EQ(GetActiveBleListenerAttempt(details_for_attempt), attempt);

    attempt->OnConnectionAttemptFinishedWithoutConnection();
    EXPECT_FALSE(GetActiveBleListenerAttempt(details_for_attempt));
  }

  void FinishNearbyInitiatorAttemptWithoutConnection(
      FakeConnectionAttempt<NearbyInitiatorFailureType>* attempt) {
    ConnectionAttemptDetails details_for_attempt =
        attempt->connection_attempt_details();
    EXPECT_EQ(GetActiveNearbyInitiatorAttempt(details_for_attempt), attempt);

    attempt->OnConnectionAttemptFinishedWithoutConnection();
    EXPECT_FALSE(GetActiveNearbyInitiatorAttempt(details_for_attempt));
  }

  void FinishBleInitiatorAttemptWithConnection(
      FakeConnectionAttempt<BleInitiatorFailureType>* attempt,
      size_t num_extracted_clients_to_generate) {
    ConnectionAttemptDetails details_for_attempt =
        attempt->connection_attempt_details();
    EXPECT_EQ(GetActiveBleInitiatorAttempt(details_for_attempt), attempt);

    FakePendingConnectionManagerDelegate::ReceivedConnectionsList&
        received_connections = fake_delegate_->received_connections_list();
    size_t num_received_connections_before_call = received_connections.size();

    auto fake_authenticated_channel =
        std::make_unique<FakeAuthenticatedChannel>();
    FakeAuthenticatedChannel* fake_authenticated_channel_raw =
        fake_authenticated_channel.get();

    std::vector<std::unique_ptr<ClientConnectionParameters>> clients_to_send =
        GenerateFakeClientParameters(num_extracted_clients_to_generate);
    std::vector<ClientConnectionParameters*> raw_client_list =
        ClientParamsListToRawPtrs(clients_to_send);

    attempt->set_client_data_for_extraction(std::move(clients_to_send));
    attempt->OnConnectionAttemptSucceeded(
        std::move(fake_authenticated_channel));

    EXPECT_FALSE(GetActiveBleInitiatorAttempt(details_for_attempt));
    EXPECT_EQ(num_received_connections_before_call + 1u,
              received_connections.size());
    EXPECT_EQ(fake_authenticated_channel_raw,
              std::get<0>(received_connections.back()).get());
    EXPECT_EQ(raw_client_list, ClientParamsListToRawPtrs(
                                   std::get<1>(received_connections.back())));
    EXPECT_EQ(details_for_attempt.GetAssociatedConnectionDetails(),
              std::get<2>(received_connections.back()));
  }

  void FinishBleListenerAttemptWithConnection(
      FakeConnectionAttempt<BleListenerFailureType>* attempt,
      size_t num_extracted_clients_to_generate) {
    ConnectionAttemptDetails details_for_attempt =
        attempt->connection_attempt_details();
    EXPECT_EQ(GetActiveBleListenerAttempt(details_for_attempt), attempt);

    FakePendingConnectionManagerDelegate::ReceivedConnectionsList&
        received_connections = fake_delegate_->received_connections_list();
    size_t num_received_connections_before_call = received_connections.size();

    auto fake_authenticated_channel =
        std::make_unique<FakeAuthenticatedChannel>();
    FakeAuthenticatedChannel* fake_authenticated_channel_raw =
        fake_authenticated_channel.get();

    std::vector<std::unique_ptr<ClientConnectionParameters>> clients_to_send =
        GenerateFakeClientParameters(num_extracted_clients_to_generate);
    std::vector<ClientConnectionParameters*> raw_client_list =
        ClientParamsListToRawPtrs(clients_to_send);

    attempt->set_client_data_for_extraction(std::move(clients_to_send));
    attempt->OnConnectionAttemptSucceeded(
        std::move(fake_authenticated_channel));

    EXPECT_FALSE(GetActiveBleListenerAttempt(details_for_attempt));
    EXPECT_EQ(num_received_connections_before_call + 1u,
              received_connections.size());
    EXPECT_EQ(fake_authenticated_channel_raw,
              std::get<0>(received_connections.back()).get());
    EXPECT_EQ(raw_client_list, ClientParamsListToRawPtrs(
                                   std::get<1>(received_connections.back())));
    EXPECT_EQ(details_for_attempt.GetAssociatedConnectionDetails(),
              std::get<2>(received_connections.back()));
  }

  void FinishNearbyInitiatorAttemptWithConnection(
      FakeConnectionAttempt<NearbyInitiatorFailureType>* attempt,
      size_t num_extracted_clients_to_generate) {
    ConnectionAttemptDetails details_for_attempt =
        attempt->connection_attempt_details();
    EXPECT_EQ(GetActiveNearbyInitiatorAttempt(details_for_attempt), attempt);

    FakePendingConnectionManagerDelegate::ReceivedConnectionsList&
        received_connections = fake_delegate_->received_connections_list();
    size_t num_received_connections_before_call = received_connections.size();

    auto fake_authenticated_channel =
        std::make_unique<FakeAuthenticatedChannel>();
    FakeAuthenticatedChannel* fake_authenticated_channel_raw =
        fake_authenticated_channel.get();

    std::vector<std::unique_ptr<ClientConnectionParameters>> clients_to_send =
        GenerateFakeClientParameters(num_extracted_clients_to_generate);
    std::vector<ClientConnectionParameters*> raw_client_list =
        ClientParamsListToRawPtrs(clients_to_send);

    attempt->set_client_data_for_extraction(std::move(clients_to_send));
    attempt->OnConnectionAttemptSucceeded(
        std::move(fake_authenticated_channel));

    EXPECT_FALSE(GetActiveNearbyInitiatorAttempt(details_for_attempt));
    EXPECT_EQ(num_received_connections_before_call + 1u,
              received_connections.size());
    EXPECT_EQ(fake_authenticated_channel_raw,
              std::get<0>(received_connections.back()).get());
    EXPECT_EQ(raw_client_list, ClientParamsListToRawPtrs(
                                   std::get<1>(received_connections.back())));
    EXPECT_EQ(details_for_attempt.GetAssociatedConnectionDetails(),
              std::get<2>(received_connections.back()));
  }

  FakeConnectionAttempt<BleInitiatorFailureType>* GetActiveBleInitiatorAttempt(
      const ConnectionAttemptDetails& connection_attempt_details) {
    return fake_ble_initiator_connection_attempt_factory_
        ->details_to_active_attempt_map()[connection_attempt_details];
  }

  FakeConnectionAttempt<BleListenerFailureType>* GetActiveBleListenerAttempt(
      const ConnectionAttemptDetails& connection_attempt_details) {
    return fake_ble_listener_connection_attempt_factory_
        ->details_to_active_attempt_map()[connection_attempt_details];
  }

  FakeConnectionAttempt<NearbyInitiatorFailureType>*
  GetActiveNearbyInitiatorAttempt(
      const ConnectionAttemptDetails& connection_attempt_details) {
    return fake_nearby_initiator_connection_attempt_factory_
        ->details_to_active_attempt_map()[connection_attempt_details];
  }

  size_t GetNumBleInitiatorAttemptsCreated() {
    return fake_ble_initiator_connection_attempt_factory_
        ->num_instances_created();
  }

  size_t GetNumBleListenerAttemptsCreated() {
    return fake_ble_listener_connection_attempt_factory_
        ->num_instances_created();
  }

  size_t GetNumNearbyInitiatorAttemptsCreated() {
    return fake_nearby_initiator_connection_attempt_factory_
        ->num_instances_created();
  }

  size_t GetNumBleInitiatorAttemptsDeleted() {
    return fake_ble_initiator_connection_attempt_factory_
        ->num_instances_deleted();
  }

  size_t GetNumBleListenerAttemptsDeleted() {
    return fake_ble_listener_connection_attempt_factory_
        ->num_instances_deleted();
  }

  size_t GetNumNearbyInitiatorAttemptsDeleted() {
    return fake_nearby_initiator_connection_attempt_factory_
        ->num_instances_deleted();
  }

 private:
  void HandleConnectionRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      ConnectionPriority connection_priority,
      bool cancel_request_before_adding) {
    auto fake_client_connection_parameters =
        std::make_unique<FakeClientConnectionParameters>(kTestFeature);
    FakeClientConnectionParameters* fake_client_connection_parameters_raw =
        fake_client_connection_parameters.get();

    if (cancel_request_before_adding) {
      fake_client_connection_parameters->CancelClientRequest();
    } else {
      switch (connection_attempt_details.connection_medium()) {
        case ConnectionMedium::kBluetoothLowEnergy:
          switch (connection_attempt_details.connection_role()) {
            case ConnectionRole::kInitiatorRole:
              fake_ble_initiator_connection_attempt_factory_
                  ->set_expected_connection_attempt_details(
                      connection_attempt_details);
              fake_pending_ble_initiator_connection_request_factory_
                  ->SetExpectationsForNextCall(
                      fake_client_connection_parameters_raw,
                      connection_priority);
              break;

            case ConnectionRole::kListenerRole:
              fake_ble_listener_connection_attempt_factory_
                  ->set_expected_connection_attempt_details(
                      connection_attempt_details);
              fake_pending_ble_listener_connection_request_factory_
                  ->SetExpectationsForNextCall(
                      fake_client_connection_parameters_raw,
                      connection_priority);
              break;
          }
          break;

        case ConnectionMedium::kNearbyConnections:
          switch (connection_attempt_details.connection_role()) {
            case ConnectionRole::kInitiatorRole:
              fake_nearby_initiator_connection_attempt_factory_
                  ->set_expected_connection_attempt_details(
                      connection_attempt_details);
              fake_pending_nearby_initiator_connection_request_factory_
                  ->SetExpectationsForNextCall(
                      fake_client_connection_parameters_raw,
                      connection_priority);
              break;

            case ConnectionRole::kListenerRole:
              NOTREACHED_IN_MIGRATION();
              break;
          }
          break;
      }
    }

    manager_->HandleConnectionRequest(
        connection_attempt_details,
        std::move(fake_client_connection_parameters), connection_priority);
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakePendingConnectionManagerDelegate> fake_delegate_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<FakeNearbyConnectionManager> fake_nearby_connection_manager_;

  std::unique_ptr<FakeBleInitiatorConnectionAttemptFactory>
      fake_ble_initiator_connection_attempt_factory_;
  std::unique_ptr<FakeBleListenerConnectionAttemptFactory>
      fake_ble_listener_connection_attempt_factory_;
  std::unique_ptr<FakeNearbyInitiatorConnectionAttemptFactory>
      fake_nearby_initiator_connection_attempt_factory_;
  std::unique_ptr<FakePendingBleInitiatorConnectionRequestFactory>
      fake_pending_ble_initiator_connection_request_factory_;
  std::unique_ptr<FakePendingBleListenerConnectionRequestFactory>
      fake_pending_ble_listener_connection_request_factory_;
  std::unique_ptr<FakePendingNearbyInitiatorConnectionRequestFactory>
      fake_pending_nearby_initiator_connection_request_factory_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<PendingConnectionManager> manager_;
};

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       CanceledRequestNotProcessed) {
  // BLE initiator.
  HandleCanceledRequestAndVerifyNoInstancesCreated(
      ConnectionAttemptDetails("remoteDeviceId", "localDeviceId",
                               ConnectionMedium::kBluetoothLowEnergy,
                               ConnectionRole::kInitiatorRole),
      ConnectionPriority::kLow);

  // BLE listener.
  HandleCanceledRequestAndVerifyNoInstancesCreated(
      ConnectionAttemptDetails("remoteDeviceId", "localDeviceId",
                               ConnectionMedium::kBluetoothLowEnergy,
                               ConnectionRole::kListenerRole),
      ConnectionPriority::kLow);

  // Nearby initiator.
  HandleCanceledRequestAndVerifyNoInstancesCreated(
      ConnectionAttemptDetails("remoteDeviceId", "localDeviceId",
                               ConnectionMedium::kNearbyConnections,
                               ConnectionRole::kInitiatorRole),
      ConnectionPriority::kLow);
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       AttemptFinishesWithoutConnection_BleInitiator) {
  ConnectionAttemptDetails details("remoteDeviceId", "localDeviceId",
                                   ConnectionMedium::kBluetoothLowEnergy,
                                   ConnectionRole::kInitiatorRole);

  // One request by itself.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kLow);
  FinishBleInitiatorAttemptWithoutConnection(
      GetActiveBleInitiatorAttempt(details));
  EXPECT_EQ(1u, GetNumBleInitiatorAttemptsCreated());

  // Two requests at the same time.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kHigh);
  FinishBleInitiatorAttemptWithoutConnection(
      GetActiveBleInitiatorAttempt(details));
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       AttemptFinishesWithoutConnection_BleListener) {
  ConnectionAttemptDetails details("remoteDeviceId", "localDeviceId",
                                   ConnectionMedium::kBluetoothLowEnergy,
                                   ConnectionRole::kListenerRole);

  // One request by itself.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kLow);
  FinishBleListenerAttemptWithoutConnection(
      GetActiveBleListenerAttempt(details));
  EXPECT_EQ(1u, GetNumBleListenerAttemptsCreated());

  // Two requests at the same time.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kHigh);
  FinishBleListenerAttemptWithoutConnection(
      GetActiveBleListenerAttempt(details));
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       AttemptFinishesWithoutConnection_NearbyInitiator) {
  ConnectionAttemptDetails details("remoteDeviceId", "localDeviceId",
                                   ConnectionMedium::kNearbyConnections,
                                   ConnectionRole::kInitiatorRole);

  // One request by itself.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kLow);
  FinishNearbyInitiatorAttemptWithoutConnection(
      GetActiveNearbyInitiatorAttempt(details));
  EXPECT_EQ(1u, GetNumNearbyInitiatorAttemptsCreated());

  // Two requests at the same time.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kHigh);
  FinishNearbyInitiatorAttemptWithoutConnection(
      GetActiveNearbyInitiatorAttempt(details));
  EXPECT_EQ(2u, GetNumNearbyInitiatorAttemptsCreated());
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       AttemptSucceeds_BleInitiator) {
  ConnectionAttemptDetails details("remoteDeviceId", "localDeviceId",
                                   ConnectionMedium::kBluetoothLowEnergy,
                                   ConnectionRole::kInitiatorRole);

  // One request by itself.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kLow);
  FinishBleInitiatorAttemptWithConnection(
      GetActiveBleInitiatorAttempt(details),
      1u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(1u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(1u, GetNumBleInitiatorAttemptsDeleted());

  // Two requests at the same time.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kHigh);
  FinishBleInitiatorAttemptWithConnection(
      GetActiveBleInitiatorAttempt(details),
      2u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsDeleted());
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       AttemptSucceeds_BleListener) {
  ConnectionAttemptDetails details("remoteDeviceId", "localDeviceId",
                                   ConnectionMedium::kBluetoothLowEnergy,
                                   ConnectionRole::kListenerRole);

  // One request by itself.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kLow);
  FinishBleListenerAttemptWithConnection(
      GetActiveBleListenerAttempt(details),
      1u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(1u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(1u, GetNumBleListenerAttemptsDeleted());

  // Two requests at the same time.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kHigh);
  FinishBleListenerAttemptWithConnection(
      GetActiveBleListenerAttempt(details),
      2u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleListenerAttemptsDeleted());
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       AttemptSucceeds_NearbyInitiator) {
  ConnectionAttemptDetails details("remoteDeviceId", "localDeviceId",
                                   ConnectionMedium::kNearbyConnections,
                                   ConnectionRole::kInitiatorRole);

  // One request by itself.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kLow);
  FinishNearbyInitiatorAttemptWithConnection(
      GetActiveNearbyInitiatorAttempt(details),
      1u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(1u, GetNumNearbyInitiatorAttemptsCreated());
  EXPECT_EQ(1u, GetNumNearbyInitiatorAttemptsDeleted());

  // Two requests at the same time.
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(details,
                                                   ConnectionPriority::kHigh);
  FinishNearbyInitiatorAttemptWithConnection(
      GetActiveNearbyInitiatorAttempt(details),
      2u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumNearbyInitiatorAttemptsCreated());
  EXPECT_EQ(2u, GetNumNearbyInitiatorAttemptsDeleted());
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       SimultaneousRequestsToSameRemoteDevice) {
  // Four ConnectionDetails objects, which all share the same remote device.
  ConnectionAttemptDetails local_device_1_initiator_details(
      "remoteDeviceId", "localDeviceId1", ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kInitiatorRole);
  ConnectionAttemptDetails local_device_1_listener_details(
      "remoteDeviceId", "localDeviceId1", ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kListenerRole);
  ConnectionAttemptDetails local_device_2_initiator_details(
      "remoteDeviceId", "localDeviceId2", ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kInitiatorRole);
  ConnectionAttemptDetails local_device_2_listener_details(
      "remoteDeviceId", "localDeviceId2", ConnectionMedium::kBluetoothLowEnergy,
      ConnectionRole::kListenerRole);

  // Register all of them.
  HandleRequestAndVerifyHandledByConnectionAttempt(
      local_device_1_initiator_details, ConnectionPriority::kLow);
  HandleRequestAndVerifyHandledByConnectionAttempt(
      local_device_1_listener_details, ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(
      local_device_2_initiator_details, ConnectionPriority::kHigh);
  HandleRequestAndVerifyHandledByConnectionAttempt(
      local_device_2_listener_details, ConnectionPriority::kLow);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(0u, GetNumBleListenerAttemptsDeleted());
  EXPECT_EQ(0u, GetNumBleInitiatorAttemptsDeleted());

  // Find a connection, arbitrarily choosing |local_device_1_initiator_details|
  // as the request which produces the connection. Since all 4 of these requests
  // were to the same remote device, all 4 of them should be deleted when the
  // connection is established.
  FinishBleInitiatorAttemptWithConnection(
      GetActiveBleInitiatorAttempt(local_device_1_initiator_details),
      4u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleListenerAttemptsDeleted());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsDeleted());
}

TEST_F(SecureChannelPendingConnectionManagerImplTest,
       SimultaneousRequestsToDifferentDevices) {
  // Four ConnectionDetails objects, which all have different remote devices.
  ConnectionAttemptDetails details_1("remoteDeviceId1", "localDeviceId1",
                                     ConnectionMedium::kBluetoothLowEnergy,
                                     ConnectionRole::kInitiatorRole);
  ConnectionAttemptDetails details_2("remoteDeviceId2", "localDeviceId2",
                                     ConnectionMedium::kBluetoothLowEnergy,
                                     ConnectionRole::kListenerRole);
  ConnectionAttemptDetails details_3("remoteDeviceId3", "localDeviceId3",
                                     ConnectionMedium::kBluetoothLowEnergy,
                                     ConnectionRole::kInitiatorRole);
  ConnectionAttemptDetails details_4("remoteDeviceId4", "localDeviceId4",
                                     ConnectionMedium::kBluetoothLowEnergy,
                                     ConnectionRole::kListenerRole);

  // Register all of them.
  HandleRequestAndVerifyHandledByConnectionAttempt(details_1,
                                                   ConnectionPriority::kLow);
  HandleRequestAndVerifyHandledByConnectionAttempt(details_2,
                                                   ConnectionPriority::kMedium);
  HandleRequestAndVerifyHandledByConnectionAttempt(details_3,
                                                   ConnectionPriority::kHigh);
  HandleRequestAndVerifyHandledByConnectionAttempt(details_4,
                                                   ConnectionPriority::kLow);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(0u, GetNumBleListenerAttemptsDeleted());
  EXPECT_EQ(0u, GetNumBleInitiatorAttemptsDeleted());

  // Find a connection for |details_1|; only one ConnectionAttempt should have
  // been deleted.
  FinishBleInitiatorAttemptWithConnection(
      GetActiveBleInitiatorAttempt(details_1),
      1u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(0u, GetNumBleListenerAttemptsDeleted());
  EXPECT_EQ(1u, GetNumBleInitiatorAttemptsDeleted());

  // Find a connection for |details_2|.
  FinishBleListenerAttemptWithConnection(
      GetActiveBleListenerAttempt(details_2),
      1u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(1u, GetNumBleListenerAttemptsDeleted());
  EXPECT_EQ(1u, GetNumBleInitiatorAttemptsDeleted());

  // |details_3|.
  FinishBleInitiatorAttemptWithConnection(
      GetActiveBleInitiatorAttempt(details_3),
      1u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(1u, GetNumBleListenerAttemptsDeleted());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsDeleted());

  // |details_4|.
  FinishBleListenerAttemptWithConnection(
      GetActiveBleListenerAttempt(details_4),
      1u /* num_extracted_clients_to_generate */);
  EXPECT_EQ(2u, GetNumBleListenerAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsCreated());
  EXPECT_EQ(2u, GetNumBleListenerAttemptsDeleted());
  EXPECT_EQ(2u, GetNumBleInitiatorAttemptsDeleted());
}

}  // namespace ash::secure_channel
