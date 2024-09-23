// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_connection_manager_impl.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel_impl.h"
#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/fake_ble_scanner.h"
#include "chromeos/ash/services/secure_channel/fake_connection.h"
#include "chromeos/ash/services/secure_channel/fake_secure_channel_connection.h"
#include "chromeos/ash/services/secure_channel/fake_secure_channel_disconnector.h"
#include "chromeos/ash/services/secure_channel/nearby_connection.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/secure_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const size_t kNumTestDevices = 3;

class FakeNearbyConnectionFactory : public NearbyConnection::Factory {
 public:
  FakeNearbyConnectionFactory() = default;
  FakeNearbyConnectionFactory(const FakeNearbyConnectionFactory&) = delete;
  FakeNearbyConnectionFactory& operator=(const FakeNearbyConnectionFactory&) =
      delete;
  ~FakeNearbyConnectionFactory() override = default;

  FakeConnection* last_created_instance() { return last_created_instance_; }

 private:
  // cryptauth::NearbyConnection::Factory:
  std::unique_ptr<Connection> CreateInstance(
      multidevice::RemoteDeviceRef remote_device,
      const std::vector<uint8_t>& eid,
      mojom::NearbyConnector* nearby_connector) override {
    auto instance = std::make_unique<FakeConnection>(remote_device);
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeConnection, DanglingUntriaged> last_created_instance_ = nullptr;
};

class FakeSecureChannelFactory : public SecureChannel::Factory {
 public:
  FakeSecureChannelFactory() = default;
  FakeSecureChannelFactory(const FakeSecureChannelFactory&) = delete;
  FakeSecureChannelFactory& operator=(const FakeSecureChannelFactory&) = delete;
  virtual ~FakeSecureChannelFactory() = default;

  FakeSecureChannelConnection* last_created_instance() {
    return last_created_instance_;
  }

 private:
  // SecureChannel::Factory:
  std::unique_ptr<SecureChannel> CreateInstance(
      std::unique_ptr<Connection> connection) override {
    auto instance =
        std::make_unique<FakeSecureChannelConnection>(std::move(connection));
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeSecureChannelConnection, DanglingUntriaged>
      last_created_instance_ = nullptr;
};

class FakeAuthenticatedChannelFactory
    : public AuthenticatedChannelImpl::Factory {
 public:
  FakeAuthenticatedChannelFactory() = default;
  FakeAuthenticatedChannelFactory(const FakeAuthenticatedChannelFactory&) =
      delete;
  FakeAuthenticatedChannelFactory& operator=(
      const FakeAuthenticatedChannelFactory&) = delete;
  ~FakeAuthenticatedChannelFactory() override = default;

  void SetExpectationsForNextCall(
      FakeSecureChannelConnection* expected_fake_secure_channel) {
    expected_fake_secure_channel_ = expected_fake_secure_channel;
  }

  FakeAuthenticatedChannel* last_created_instance() {
    return last_created_instance_;
  }

 private:
  // AuthenticatedChannelImpl::Factory:
  std::unique_ptr<AuthenticatedChannel> CreateInstance(
      const std::vector<mojom::ConnectionCreationDetail>&
          connection_creation_details,
      std::unique_ptr<SecureChannel> secure_channel) override {
    EXPECT_EQ(expected_fake_secure_channel_, secure_channel.get());

    auto instance = std::make_unique<FakeAuthenticatedChannel>();
    last_created_instance_ = instance.get();
    return instance;
  }

  raw_ptr<FakeSecureChannelConnection, DanglingUntriaged>
      expected_fake_secure_channel_ = nullptr;
  raw_ptr<FakeAuthenticatedChannel, DanglingUntriaged> last_created_instance_ =
      nullptr;
};

}  // namespace

class SecureChannelNearbyConnectionManagerImplTest : public testing::Test {
 protected:
  SecureChannelNearbyConnectionManagerImplTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  SecureChannelNearbyConnectionManagerImplTest(
      const SecureChannelNearbyConnectionManagerImplTest&) = delete;
  SecureChannelNearbyConnectionManagerImplTest& operator=(
      const SecureChannelNearbyConnectionManagerImplTest&) = delete;
  ~SecureChannelNearbyConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_nearby_connection_factory_ =
        std::make_unique<FakeNearbyConnectionFactory>();
    NearbyConnection::Factory::SetFactoryForTesting(
        fake_nearby_connection_factory_.get());

    fake_secure_channel_factory_ = std::make_unique<FakeSecureChannelFactory>();
    SecureChannel::Factory::SetFactoryForTesting(
        fake_secure_channel_factory_.get());

    fake_authenticated_channel_factory_ =
        std::make_unique<FakeAuthenticatedChannelFactory>();
    AuthenticatedChannelImpl::Factory::SetFactoryForTesting(
        fake_authenticated_channel_factory_.get());

    fake_ble_scanner_ = std::make_unique<FakeBleScanner>();
    fake_secure_channel_disconnector_ =
        std::make_unique<FakeSecureChannelDisconnector>();

    manager_ = NearbyConnectionManagerImpl::Factory::Create(
        fake_ble_scanner_.get(), fake_secure_channel_disconnector_.get());

    EXPECT_FALSE(manager_->IsNearbyConnectorSet());
    fake_nearby_connector_ = std::make_unique<FakeNearbyConnector>();
    manager_->SetNearbyConnector(
        fake_nearby_connector_->GeneratePendingRemote());
    EXPECT_TRUE(manager_->IsNearbyConnectorSet());
  }

  void TearDown() override {
    NearbyConnection::Factory::SetFactoryForTesting(nullptr);
    SecureChannel::Factory::SetFactoryForTesting(nullptr);
    AuthenticatedChannelImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void AttemptNearbyInitiatorConnection(const DeviceIdPair& device_id_pair,
                                        bool expected_to_add_request,
                                        bool should_cancel_attempt_on_failure) {
    SetInRemoteDeviceIdToMetadataMap(device_id_pair);

    manager_->AttemptNearbyInitiatorConnection(
        device_id_pair,
        base::BindRepeating(&SecureChannelNearbyConnectionManagerImplTest::
                                OnBleDiscoveryStateChanged,
                            base::Unretained(this), device_id_pair),
        base::BindRepeating(&SecureChannelNearbyConnectionManagerImplTest::
                                OnNearbyConnectionStateChanged,
                            base::Unretained(this), device_id_pair),
        base::BindRepeating(&SecureChannelNearbyConnectionManagerImplTest::
                                OnSecureChannelAuthenticationStateChanged,
                            base::Unretained(this), device_id_pair),
        base::BindOnce(
            &SecureChannelNearbyConnectionManagerImplTest::OnConnectionSuccess,
            base::Unretained(this), device_id_pair),
        base::BindRepeating(&SecureChannelNearbyConnectionManagerImplTest::
                                OnNearbyInitiatorFailure,
                            base::Unretained(this), device_id_pair,
                            should_cancel_attempt_on_failure));

    bool has_request =
        fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
            device_id_pair, ConnectionMedium::kNearbyConnections,
            ConnectionRole::kInitiatorRole));
    EXPECT_EQ(expected_to_add_request, has_request);
  }

  void CancelNearbyInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) {
    RemoveFromRemoteDeviceIdToMetadataMap(device_id_pair);
    manager_->CancelNearbyInitiatorConnectionAttempt(device_id_pair);
    EXPECT_FALSE(fake_ble_scanner_->HasScanRequest(ConnectionAttemptDetails(
        device_id_pair, ConnectionMedium::kNearbyConnections,
        ConnectionRole::kInitiatorRole)));
  }

  void SimulateBleDisvoceryFailed(const DeviceIdPair& device_id_pair) {
    fake_ble_scanner_->NotifyBleDiscoverySessionFailed(
        device_id_pair, mojom::DiscoveryResult::kFailure,
        mojom::DiscoveryErrorCode::kTimeout);

    // As a result of the connection, all ongoing connection attmepts should
    // have been canceled, since a connection is in progress.
    EXPECT_EQ(device_discovery_results_[device_id_pair],
              mojom::DiscoveryResult::kFailure);
  }

  // Returns the SecureChannel created by this call.
  FakeSecureChannelConnection* SimulateConnectionEstablished(
      multidevice::RemoteDeviceRef remote_device) {
    fake_ble_scanner_->NotifyReceivedAdvertisementFromDevice(
        remote_device, /*bluetooth_device=*/nullptr,
        ConnectionMedium::kNearbyConnections, ConnectionRole::kInitiatorRole,
        {0, 0} /* eid */);

    // As a result of the connection, all ongoing connection attmepts should
    // have been canceled, since a connection is in progress.
    EXPECT_TRUE(
        fake_ble_scanner_
            ->GetAllScanRequestsForRemoteDevice(remote_device.GetDeviceId())
            .empty());

    FakeSecureChannelConnection* last_created_secure_channel =
        fake_secure_channel_factory_->last_created_instance();
    EXPECT_TRUE(last_created_secure_channel->was_initialized());
    return last_created_secure_channel;
  }

  void SimulateSecureChannelDisconnection(
      const std::string& remote_device_id,
      bool fail_during_authentication,
      FakeSecureChannelConnection* fake_secure_channel,
      size_t num_initiator_attempts_canceled_from_disconnection = 0u) {
    size_t num_nearby_initiator_failures_before_call =
        nearby_initiator_failures_.size();

    // Connect, then disconnect. If needed, start authenticating before
    // disconnecting.
    fake_secure_channel->ChangeStatus(SecureChannel::Status::CONNECTED);
    if (fail_during_authentication) {
      fake_secure_channel->ChangeStatus(SecureChannel::Status::AUTHENTICATING);
    }
    fake_secure_channel->ChangeStatus(SecureChannel::Status::DISCONNECTED);

    // Iterate through all pending requests to |remote_device_id|, ensuring that
    // all expected failures have been communicated back to the client.
    size_t initiator_failures_index =
        num_nearby_initiator_failures_before_call +
        num_initiator_attempts_canceled_from_disconnection;
    for (const auto& pair :
         remote_device_id_to_id_pairs_map_[remote_device_id]) {
      EXPECT_EQ(pair,
                nearby_initiator_failures_[initiator_failures_index].first);
      EXPECT_EQ(fail_during_authentication
                    ? NearbyInitiatorFailureType::kAuthenticationError
                    : NearbyInitiatorFailureType::kConnectivityError,
                nearby_initiator_failures_[initiator_failures_index].second);
      ++initiator_failures_index;
    }
    EXPECT_EQ(initiator_failures_index, nearby_initiator_failures_.size());

    // All requests which were paused during the connection should have started
    // back up again, since the connection became disconnected.
    for (const auto& pair :
         remote_device_id_to_id_pairs_map_[remote_device_id]) {
      EXPECT_TRUE(fake_ble_scanner_->HasScanRequest(
          ConnectionAttemptDetails(pair, ConnectionMedium::kNearbyConnections,
                                   ConnectionRole::kInitiatorRole)));
    }
  }

  void SimulateSecureChannelAuthentication(
      const std::string& remote_device_id,
      FakeSecureChannelConnection* fake_secure_channel) {
    fake_authenticated_channel_factory_->SetExpectationsForNextCall(
        fake_secure_channel);

    size_t num_success_callbacks_before_call = successful_connections_.size();

    fake_secure_channel->ChangeNearbyConnectionState(
        mojom::NearbyConnectionStep::
            kWaitingForConnectionToBeAcceptedByRemoteDeviceStarted,
        mojom::NearbyConnectionStepResult::kSuccess);
    fake_secure_channel->ChangeStatus(SecureChannel::Status::CONNECTED);
    fake_secure_channel->ChangeStatus(SecureChannel::Status::AUTHENTICATING);
    fake_secure_channel->ChangeSecureChannelAuthenticationState(
        mojom::SecureChannelState::kValidatedResponderAuth);
    fake_secure_channel->ChangeStatus(SecureChannel::Status::AUTHENTICATED);

    // Verify that the callback was made. Verification that the provided
    // DeviceIdPair was correct occurs in OnConnectionSuccess().
    EXPECT_EQ(num_success_callbacks_before_call + 1u,
              successful_connections_.size());

    // For all remaining requests, verify that they were added back.
    for (const auto& pair :
         remote_device_id_to_id_pairs_map_[remote_device_id]) {
      EXPECT_TRUE(fake_ble_scanner_->HasScanRequest(
          ConnectionAttemptDetails(pair, ConnectionMedium::kNearbyConnections,
                                   ConnectionRole::kInitiatorRole)));
    }
  }

  bool WasChannelHandledByDisconnector(
      FakeSecureChannelConnection* fake_secure_channel) {
    return fake_secure_channel_disconnector_->WasChannelHandled(
        fake_secure_channel);
  }

  base::test::TaskEnvironment task_environment_;
  const multidevice::RemoteDeviceRefList& test_devices() {
    return test_devices_;
  }

 private:
  void OnConnectionSuccess(
      const DeviceIdPair& device_id_pair,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    successful_connections_.push_back(
        std::make_pair(device_id_pair, std::move(authenticated_channel)));

    // The request which received the success callback is automatically removed
    // by NearbyConnectionManager, so it no longer needs to be tracked.
    remote_device_id_to_id_pairs_map_[device_id_pair.remote_device_id()].erase(
        device_id_pair);

    // Make a copy of the entries which should be canceled. This is required
    // because the Cancel*() calls above end up removing entries from
    // |remote_device_id_to_id_pairs_map_|, which can cause access to deleted
    // memory.
    base::flat_set<DeviceIdPair> to_cancel =
        remote_device_id_to_id_pairs_map_[device_id_pair.remote_device_id()];

    for (const auto& pair : to_cancel)
      CancelNearbyInitiatorConnectionAttempt(pair);
  }

  void OnNearbyInitiatorFailure(const DeviceIdPair& device_id_pair,
                                bool should_cancel_attempt_on_failure,
                                NearbyInitiatorFailureType failure_type) {
    nearby_initiator_failures_.push_back(
        std::make_pair(device_id_pair, failure_type));
    if (!should_cancel_attempt_on_failure)
      return;

    // Make a copy of the pair before canceling the attempt, since the reference
    // points to memory owned by |manager_| which will be deleted.
    DeviceIdPair device_id_pair_copy = device_id_pair;
    CancelNearbyInitiatorConnectionAttempt(device_id_pair_copy);
  }

  void OnBleDiscoveryStateChanged(
      const DeviceIdPair& device_id_pair,
      mojom::DiscoveryResult result,
      std::optional<mojom::DiscoveryErrorCode> error_code) {
    device_discovery_results_[device_id_pair] = result;
  }

  void OnNearbyConnectionStateChanged(
      const DeviceIdPair& device_id_pair,
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result) {
    device_nearby_connection_states_[device_id_pair] = nearby_connection_step;
  }

  void OnSecureChannelAuthenticationStateChanged(
      const DeviceIdPair& device_id_pair,
      mojom::SecureChannelState secure_channel_state) {
    device_secure_channel_states_[device_id_pair] = secure_channel_state;
  }

  void SetInRemoteDeviceIdToMetadataMap(const DeviceIdPair& device_id_pair) {
    remote_device_id_to_id_pairs_map_[device_id_pair.remote_device_id()].insert(
        device_id_pair);
  }

  void RemoveFromRemoteDeviceIdToMetadataMap(
      const DeviceIdPair& device_id_pair) {
    base::flat_set<DeviceIdPair>& set_for_remote_device =
        remote_device_id_to_id_pairs_map_[device_id_pair.remote_device_id()];

    for (auto it = set_for_remote_device.begin();
         it != set_for_remote_device.end(); ++it) {
      if (*it == device_id_pair) {
        set_for_remote_device.erase(it);
        return;
      }
    }

    NOTREACHED_IN_MIGRATION();
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  base::flat_map<std::string, base::flat_set<DeviceIdPair>>
      remote_device_id_to_id_pairs_map_;
  base::flat_map<DeviceIdPair, mojom::DiscoveryResult>
      device_discovery_results_;
  base::flat_map<DeviceIdPair, mojom::NearbyConnectionStep>
      device_nearby_connection_states_;
  base::flat_map<DeviceIdPair, mojom::SecureChannelState>
      device_secure_channel_states_;
  std::vector<std::pair<DeviceIdPair, std::unique_ptr<AuthenticatedChannel>>>
      successful_connections_;
  std::vector<std::pair<DeviceIdPair, NearbyInitiatorFailureType>>
      nearby_initiator_failures_;

  std::unique_ptr<FakeNearbyConnectionFactory> fake_nearby_connection_factory_;
  std::unique_ptr<FakeSecureChannelFactory> fake_secure_channel_factory_;
  std::unique_ptr<FakeAuthenticatedChannelFactory>
      fake_authenticated_channel_factory_;

  std::unique_ptr<FakeBleScanner> fake_ble_scanner_;
  std::unique_ptr<FakeSecureChannelDisconnector>
      fake_secure_channel_disconnector_;
  std::unique_ptr<FakeNearbyConnector> fake_nearby_connector_;

  std::unique_ptr<NearbyConnectionManager> manager_;
};

TEST_F(SecureChannelNearbyConnectionManagerImplTest,
       AttemptAndCancelWithoutConnection) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptNearbyInitiatorConnection(pair,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/false);
  CancelNearbyInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelNearbyConnectionManagerImplTest,
       AttemptAndDiscoveryFailed) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptNearbyInitiatorConnection(pair,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/false);
  SimulateBleDisvoceryFailed(pair);
}

TEST_F(SecureChannelNearbyConnectionManagerImplTest,
       StartConnectionThenDisconnect_CancelAfter) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptNearbyInitiatorConnection(pair,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/false);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1]);
  SimulateSecureChannelDisconnection(pair.remote_device_id(),
                                     /*fail_during_authentication=*/true,
                                     fake_secure_channel);

  CancelNearbyInitiatorConnectionAttempt(pair);
}

TEST_F(SecureChannelNearbyConnectionManagerImplTest,
       StartConnectionThenDisconnect_CancelInCallback) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptNearbyInitiatorConnection(pair,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/true);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1]);
  SimulateSecureChannelDisconnection(
      pair.remote_device_id(),
      /*fail_during_authentication=*/true, fake_secure_channel,
      /*num_initiator_attempts_canceled_from_disconnection=*/1u);
}

TEST_F(SecureChannelNearbyConnectionManagerImplTest, SuccessfulConnection) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptNearbyInitiatorConnection(pair,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/true);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1]);
  SimulateSecureChannelAuthentication(pair.remote_device_id(),
                                      fake_secure_channel);
}

TEST_F(SecureChannelNearbyConnectionManagerImplTest, TwoSimultaneousAttempts) {
  DeviceIdPair pair_1(test_devices()[1].GetDeviceId(),
                      test_devices()[0].GetDeviceId());
  DeviceIdPair pair_2(test_devices()[2].GetDeviceId(),
                      test_devices()[0].GetDeviceId());

  AttemptNearbyInitiatorConnection(pair_1,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/true);
  AttemptNearbyInitiatorConnection(pair_2,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/true);

  FakeSecureChannelConnection* fake_secure_channel_1 =
      SimulateConnectionEstablished(test_devices()[1]);
  SimulateSecureChannelAuthentication(pair_1.remote_device_id(),
                                      fake_secure_channel_1);
  FakeSecureChannelConnection* fake_secure_channel_2 =
      SimulateConnectionEstablished(test_devices()[2]);
  SimulateSecureChannelAuthentication(pair_2.remote_device_id(),
                                      fake_secure_channel_2);
}

TEST_F(SecureChannelNearbyConnectionManagerImplTest,
       CancelWhileAuthenticating) {
  DeviceIdPair pair(test_devices()[1].GetDeviceId(),
                    test_devices()[0].GetDeviceId());

  AttemptNearbyInitiatorConnection(pair,
                                   /*expected_to_add_request=*/true,
                                   /*should_cancel_attempt_on_failure=*/true);

  FakeSecureChannelConnection* fake_secure_channel =
      SimulateConnectionEstablished(test_devices()[1]);
  CancelNearbyInitiatorConnectionAttempt(pair);
  EXPECT_TRUE(WasChannelHandledByDisconnector(fake_secure_channel));
}

}  // namespace ash::secure_channel
