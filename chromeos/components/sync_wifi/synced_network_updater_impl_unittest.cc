// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sync_wifi/fake_pending_network_configuration_tracker.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/pending_network_configuration_tracker_impl.h"
#include "chromeos/components/sync_wifi/synced_network_updater_impl.h"
#include "chromeos/components/sync_wifi/test_data_generator.h"
#include "chromeos/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

class NetworkDeviceHandler;
class NetworkProfileHandler;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class ManagedNetworkConfigurationHandler;

namespace sync_wifi {

namespace {

const char kFredSsid[] = "Fred";
const char kMangoSsid[] = "Mango";

const chromeos::NetworkState* FindLocalNetworkById(
    const NetworkIdentifier& id) {
  chromeos::NetworkStateHandler::NetworkStateList network_list;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkListByType(
          chromeos::NetworkTypePattern::WiFi(), /* configured_only= */ true,
          /* visible_only= */ false, /* limit= */ 0, &network_list);
  for (const chromeos::NetworkState* network : network_list) {
    if (network->GetHexSsid() == id.hex_ssid() &&
        network->security_class() == id.security_type()) {
      return network;
    }
  }
  return nullptr;
}

}  // namespace

class SyncedNetworkUpdaterImplTest : public testing::Test {
 public:
  SyncedNetworkUpdaterImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::DEFAULT,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    LoginState::Initialize();
    network_state_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    network_device_handler_ =
        chromeos::NetworkDeviceHandler::InitializeForTesting(
            network_state_helper_->network_state_handler());
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_helper_->network_state_handler());
    network_configuration_handler_ =
        base::WrapUnique<NetworkConfigurationHandler>(
            NetworkConfigurationHandler::InitializeForTest(
                network_state_helper_->network_state_handler(),
                network_device_handler_.get()));
    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            network_state_helper_->network_state_handler(),
            network_profile_handler_.get(), network_device_handler_.get(),
            network_configuration_handler_.get());
    managed_network_configuration_handler_->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());
    cros_network_config_impl_ =
        std::make_unique<chromeos::network_config::CrosNetworkConfig>(
            network_state_helper_->network_state_handler(),
            network_device_handler_.get(),
            managed_network_configuration_handler_.get(),
            network_connection_handler_.get(),
            /*network_certificate_handler=*/nullptr);
    OverrideInProcessInstanceForTesting(cros_network_config_impl_.get());

    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  ~SyncedNetworkUpdaterImplTest() override {
    LoginState::Shutdown();
    network_config::OverrideInProcessInstanceForTesting(nullptr);
  }

  void SetUp() override {
    testing::Test::SetUp();
    NetworkHandler::Initialize();
    network_state_helper()->ResetDevicesAndServices();
    base::RunLoop().RunUntilIdle();

    auto tracker_unique_ptr =
        std::make_unique<FakePendingNetworkConfigurationTracker>();
    tracker_ = tracker_unique_ptr.get();
    updater_ = std::make_unique<SyncedNetworkUpdaterImpl>(
        std::move(tracker_unique_ptr), remote_cros_network_config_.get());
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    testing::Test::TearDown();
  }

  FakePendingNetworkConfigurationTracker* tracker() { return tracker_; }
  SyncedNetworkUpdaterImpl* updater() { return updater_.get(); }
  chromeos::NetworkStateTestHelper* network_state_helper() {
    return network_state_helper_.get();
  }
  NetworkIdentifier fred_network_id() { return fred_network_id_; }
  NetworkIdentifier mango_network_id() { return mango_network_id_; }

 private:
  base::test::TaskEnvironment task_environment_;
  FakePendingNetworkConfigurationTracker* tracker_;
  std::unique_ptr<NetworkStateTestHelper> network_state_helper_;
  std::unique_ptr<SyncedNetworkUpdaterImpl> updater_;
  std::unique_ptr<network_config::CrosNetworkConfig> cros_network_config_impl_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  NetworkIdentifier fred_network_id_ = GeneratePskNetworkId(kFredSsid);
  NetworkIdentifier mango_network_id_ = GeneratePskNetworkId(kMangoSsid);

  DISALLOW_COPY_AND_ASSIGN(SyncedNetworkUpdaterImplTest);
};

TEST_F(SyncedNetworkUpdaterImplTest, TestAdd_OneNetwork) {
  sync_pb::WifiConfigurationSpecificsData specifics =
      GenerateTestWifiSpecifics(fred_network_id());
  NetworkIdentifier id = NetworkIdentifier::FromProto(specifics);
  updater()->AddOrUpdateNetwork(specifics);
  EXPECT_TRUE(tracker()->GetPendingUpdateById(id));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FindLocalNetworkById(id));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(id));
}

TEST_F(SyncedNetworkUpdaterImplTest, TestAdd_ThenRemove) {
  EXPECT_FALSE(FindLocalNetworkById(fred_network_id()));
  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FindLocalNetworkById(fred_network_id()));
  updater()->RemoveNetwork(fred_network_id());
  PendingNetworkConfigurationUpdate* update =
      tracker()->GetPendingUpdateById(fred_network_id());
  EXPECT_TRUE(update);
  EXPECT_FALSE(update->specifics());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_FALSE(FindLocalNetworkById(fred_network_id()));
}

TEST_F(SyncedNetworkUpdaterImplTest, TestAdd_TwoNetworks) {
  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(mango_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(mango_network_id()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FindLocalNetworkById(fred_network_id()));
  EXPECT_TRUE(FindLocalNetworkById(mango_network_id()));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(mango_network_id()));
}

TEST_F(SyncedNetworkUpdaterImplTest, TestFailToAdd_Error) {
  network_state_helper()->manager_test()->SetSimulateConfigurationResult(
      chromeos::FakeShillSimulatedResult::kFailure);

  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(FindLocalNetworkById(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
}

TEST_F(SyncedNetworkUpdaterImplTest, TestFailToRemove) {
  network_state_helper()->profile_test()->SetSimulateDeleteResult(
      chromeos::FakeShillSimulatedResult::kFailure);
  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FindLocalNetworkById(fred_network_id()));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));

  updater()->RemoveNetwork(fred_network_id());
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(FindLocalNetworkById(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
}

}  // namespace sync_wifi

}  // namespace chromeos
