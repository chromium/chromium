// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/local_network_collector_impl.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/sync_wifi/local_network_collector.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "chromeos/ash/components/sync_wifi/network_type_conversions.h"
#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/ash/components/sync_wifi/test_data_generator.h"
#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::sync_wifi {

namespace {

const char kFredSsid[] = "Fred";
const char kMangoSsid[] = "Mango";
const char kAnnieSsid[] = "Annie";
const char kOzzySsid[] = "Ozzy";
const char kHopperSsid[] = "Hopper";
const char kByteSsid[] = "Byte";
const char kWalterSsid[] = "Walter";

}  // namespace

class LocalNetworkCollectorImplTest : public testing::Test {
 public:
  LocalNetworkCollectorImplTest() {
    local_test_helper_ = std::make_unique<NetworkTestHelper>();
    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());
  }

  LocalNetworkCollectorImplTest(const LocalNetworkCollectorImplTest&) = delete;
  LocalNetworkCollectorImplTest& operator=(
      const LocalNetworkCollectorImplTest&) = delete;

  ~LocalNetworkCollectorImplTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    helper()->SetUp();
    metrics_logger_ = std::make_unique<SyncedNetworkMetricsLogger>(
        /*network_state_handler=*/nullptr,
        /*network_connection_handler=*/nullptr);

    local_network_collector_ = std::make_unique<LocalNetworkCollectorImpl>(
        remote_cros_network_config_.get(), metrics_logger_.get());
    local_network_collector_->SetNetworkMetadataStore(
        NetworkHandler::Get()->network_metadata_store()->GetWeakPtr());
    on_get_all_syncable_networks_count_ = 0;
  }

  void OnGetAllSyncableNetworks(
      std::vector<std::string> expected,
      std::vector<sync_pb::WifiConfigurationSpecifics> result) {
    EXPECT_EQ(expected.size(), result.size());
    for (int i = 0; i < (int)result.size(); i++) {
      EXPECT_EQ(expected[i], DecodeHexString(result[i].hex_ssid()));
    }
    on_get_all_syncable_networks_count_++;
  }

  void OnGetSyncableNetwork(
      std::string expected_ssid,
      std::optional<sync_pb::WifiConfigurationSpecifics> result) {
    if (expected_ssid.empty()) {
      ASSERT_EQ(std::nullopt, result);
      return;
    }
    EXPECT_EQ(expected_ssid, DecodeHexString(result->hex_ssid()));
  }

  void OnGetManagedPropertiesResult(
      bool expected_autoconnect,
      chromeos::network_config::mojom::ManagedPropertiesPtr properties) {
    EXPECT_EQ(
        expected_autoconnect,
        properties->type_properties->get_wifi()->auto_connect->active_value);
  }

  LocalNetworkCollector* local_network_collector() {
    return local_network_collector_.get();
  }

  void TestGetSyncableNetwork(const std::string& guid,
                              const std::string& expected_ssid) {
    local_network_collector()->GetSyncableNetwork(
        guid,
        base::BindOnce(&LocalNetworkCollectorImplTest::OnGetSyncableNetwork,
                       base::Unretained(this), expected_ssid));
    base::RunLoop().RunUntilIdle();
  }

  void VerifyAutoconnect(const std::string& guid, bool auto_connect) {
    remote_cros_network_config_->GetManagedProperties(
        guid, base::BindOnce(
                  &LocalNetworkCollectorImplTest::OnGetManagedPropertiesResult,
                  base::Unretained(this), auto_connect));
    base::RunLoop().RunUntilIdle();
  }

  NetworkTestHelper* helper() { return local_test_helper_.get(); }
  size_t on_get_all_syncable_networks_count() {
    return on_get_all_syncable_networks_count_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<LocalNetworkCollector> local_network_collector_;
  std::unique_ptr<SyncedNetworkMetricsLogger> metrics_logger_;
  std::unique_ptr<NetworkTestHelper> local_test_helper_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  size_t on_get_all_syncable_networks_count_;
};

TEST_F(LocalNetworkCollectorImplTest, TestGetAllSyncableNetworks) {
  helper()->ConfigureWiFiNetwork(kFredSsid, /*is_secured=*/true,
                                 helper()->primary_user(),
                                 /*has_connected=*/true);

  std::vector<std::string> expected;
  expected.push_back(kFredSsid);

  local_network_collector()->GetAllSyncableNetworks(
      base::BindOnce(&LocalNetworkCollectorImplTest::OnGetAllSyncableNetworks,
                     base::Unretained(this), expected));

  base::RunLoop().RunUntilIdle();
}

TEST_F(LocalNetworkCollectorImplTest,
       TestGetAllSyncableNetworks_MojoNetworksUnitializedThenInitialized) {
  const std::string kSharedUserDirectory =
      NetworkProfileHandler::GetSharedProfilePath();
  helper()->network_state_test_helper()->ClearProfiles();
  // Add back shared profile path to simulate user on the login screen.
  helper()->network_state_test_helper()->profile_test()->AddProfile(
      /*profile_path=*/kSharedUserDirectory, /*userhash=*/std::string());
  base::RunLoop().RunUntilIdle();

  size_t on_get_all_syncable_networks_count_before =
      on_get_all_syncable_networks_count();
  local_network_collector()->GetAllSyncableNetworks(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(on_get_all_syncable_networks_count_before,
            on_get_all_syncable_networks_count());

  // Log users back in.
  const char* kProfilePathUser =
      helper()->network_state_test_helper()->ProfilePathUser();
  const char* kUserHash = helper()->network_state_test_helper()->UserHash();
  helper()->network_state_test_helper()->profile_test()->AddProfile(
      /*profile_path=*/kProfilePathUser, /*userhash=*/kUserHash);
  helper()->network_state_test_helper()->profile_test()->AddProfile(
      /*profile_path=*/kUserHash, /*userhash=*/std::string());

  std::vector<std::string> expected;
  local_network_collector()->GetAllSyncableNetworks(
      base::BindOnce(&LocalNetworkCollectorImplTest::OnGetAllSyncableNetworks,
                     base::Unretained(this), expected));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(on_get_all_syncable_networks_count_before + 1,
            on_get_all_syncable_networks_count());
}

TEST_F(LocalNetworkCollectorImplTest,
       TestGetAllSyncableNetworks_WithFiltering) {
  helper()->ConfigureWiFiNetwork(kFredSsid, /*is_secured=*/true,
                                 helper()->primary_user(),
                                 /*has_connected=*/true);
  helper()->ConfigureWiFiNetwork(kMangoSsid, /*is_secured=*/true,
                                 /*user=*/nullptr, /*has_connected=*/true,
                                 /*owned_by_user=*/false);
  helper()->ConfigureWiFiNetwork(kAnnieSsid, /*is_secured=*/false,
                                 helper()->primary_user(),
                                 /*has_connected=*/true);
  helper()->ConfigureWiFiNetwork(kOzzySsid, /*is_secured=*/true,
                                 helper()->primary_user(),
                                 /*has_connected=*/true);
  helper()->ConfigureWiFiNetwork(kHopperSsid, /*is_secured=*/true,
                                 helper()->primary_user(),
                                 /*has_connected=*/false);
  helper()->ConfigureWiFiNetwork(kByteSsid, /*is_secured=*/true,
                                 helper()->primary_user(),
                                 /*has_connected=*/true,
                                 /*owned_by_user=*/true);
  helper()->ConfigureWiFiNetwork(kWalterSsid, /*is_secured=*/true,
                                 /*user=*/nullptr,
                                 /*has_connected=*/true,
                                 /*owned_by_user=*/true,
                                 /*configured_by_sync=*/false,
                                 /*is_from_policy=*/true);

  std::vector<std::string> expected;
  expected.push_back(kByteSsid);
  expected.push_back(kFredSsid);
  expected.push_back(kHopperSsid);
  expected.push_back(kOzzySsid);

  local_network_collector()->GetAllSyncableNetworks(
      base::BindOnce(&LocalNetworkCollectorImplTest::OnGetAllSyncableNetworks,
                     base::Unretained(this), expected));

  base::RunLoop().RunUntilIdle();
}

TEST_F(LocalNetworkCollectorImplTest, TestRecordZeroNetworksEligibleForSync) {
  base::HistogramTester histogram_tester;
  helper()->ConfigureWiFiNetwork(kMangoSsid, /*is_secured=*/true,
                                 /*user=*/nullptr, /*has_connected=*/true,
                                 /*owned_by_user=*/false);
  helper()->ConfigureWiFiNetwork(kAnnieSsid, /*is_secured=*/false,
                                 helper()->primary_user(),
                                 /*has_connected=*/true);
  helper()->ConfigureWiFiNetwork(kWalterSsid, /*is_secured=*/true,
                                 /*user=*/nullptr, /*has_connected=*/true,
                                 /*owned_by_user=*/true,
                                 /*configured_by_sync=*/false,
                                 /*is_from_policy=*/true);
  helper()->ConfigureWiFiNetwork(kHopperSsid, /*is_secured=*/true,
                                 /*user=*/nullptr, /*has_connected=*/true,
                                 /*owned_by_user=*/true,
                                 /*configured_by_sync=*/false,
                                 /*is_from_policy=*/false,
                                 /*is_hidden=*/true);

  local_network_collector()->RecordZeroNetworksEligibleForSync();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kZeroNetworksSyncedReasonHistogram, 4);
  histogram_tester.ExpectBucketCount(
      kZeroNetworksSyncedReasonHistogram,
      NetworkEligibilityStatus::kUnsupportedSecurityType, 1);
  histogram_tester.ExpectBucketCount(
      kZeroNetworksSyncedReasonHistogram,
      NetworkEligibilityStatus::kNotConfiguredByUser, 1);
  histogram_tester.ExpectBucketCount(
      kZeroNetworksSyncedReasonHistogram,
      NetworkEligibilityStatus::kProhibitedByPolicy, 1);
  histogram_tester.ExpectBucketCount(kZeroNetworksSyncedReasonHistogram,
                                     NetworkEligibilityStatus::kHiddenSsid, 1);
}

TEST_F(LocalNetworkCollectorImplTest, TestGetSyncableNetwork) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true, helper()->primary_user(),
      /*has_connected=*/true);
  TestGetSyncableNetwork(guid, kFredSsid);
}

TEST_F(LocalNetworkCollectorImplTest,
       TestGetSyncableNetwork_Shared_OwnedByUser) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true,
      /*user=*/nullptr, /*has_connected=*/true, /*owned_by_user=*/true);
  TestGetSyncableNetwork(guid, kFredSsid);
}

TEST_F(LocalNetworkCollectorImplTest,
       TestGetSyncableNetwork_Shared_OwnedByOther) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true,
      /*user=*/nullptr, /*has_connected=*/true, /*owned_by_user=*/false);
  TestGetSyncableNetwork(guid, /*expected_ssid=*/std::string());
}

TEST_F(LocalNetworkCollectorImplTest, TestGetSyncableNetwork_DoesntExist) {
  TestGetSyncableNetwork("test_guid", /*expected_ssid=*/std::string());
}

TEST_F(LocalNetworkCollectorImplTest, TestGetSyncableNetwork_NeverConnected) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true, helper()->primary_user(),
      /*has_connected=*/false);
  TestGetSyncableNetwork(guid, kFredSsid);
}

TEST_F(LocalNetworkCollectorImplTest, TestGetSyncableNetwork_FromPolicy) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true, helper()->primary_user(),
      /*has_connected=*/true, /*owned_by_user=*/true,
      /*configured_by_sync=*/false, /*is_from_policy=*/true);
  TestGetSyncableNetwork(guid, /*expected_ssid=*/std::string());
}

TEST_F(LocalNetworkCollectorImplTest, TestFixAutoconnect) {
  std::vector<sync_pb::WifiConfigurationSpecifics> protos;
  sync_pb::WifiConfigurationSpecifics fred_network =
      GenerateTestWifiSpecifics(GeneratePskNetworkId(kFredSsid));
  sync_pb::WifiConfigurationSpecifics walt_network =
      GenerateTestWifiSpecifics(GeneratePskNetworkId(kWalterSsid));
  sync_pb::WifiConfigurationSpecifics mango_network =
      GenerateTestWifiSpecifics(GeneratePskNetworkId(kMangoSsid));
  fred_network.set_automatically_connect(
      sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_UNSPECIFIED);
  walt_network.set_automatically_connect(
      sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_UNSPECIFIED);
  mango_network.set_automatically_connect(
      sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_DISABLED);
  protos.push_back(fred_network);
  protos.push_back(walt_network);
  protos.push_back(mango_network);

  std::string fred_guid = helper()->ConfigureWiFiNetwork(
      kFredSsid,
      /*is_secured=*/true, helper()->primary_user(),
      /*has_connected=*/true,
      /*owned_by_user=*/true,
      /*configured_by_sync=*/false,
      /*is_from_policy=*/false,
      /*is_hidden=*/false,
      /*auto_connect=*/false);
  std::string walt_guid = helper()->ConfigureWiFiNetwork(
      kWalterSsid,
      /*is_secured=*/true, helper()->primary_user(),
      /*has_connected=*/true,
      /*owned_by_user=*/true,
      /*configured_by_sync=*/true,
      /*is_from_policy=*/false,
      /*is_hidden=*/false,
      /*auto_connect=*/false);
  std::string mango_guid = helper()->ConfigureWiFiNetwork(
      kMangoSsid,
      /*is_secured=*/true, helper()->primary_user(),
      /*has_connected=*/true,
      /*owned_by_user=*/true,
      /*configured_by_sync=*/true,
      /*is_from_policy=*/false,
      /*is_hidden=*/false,
      /*auto_connect=*/false);
  base::RunLoop().RunUntilIdle();
  VerifyAutoconnect(fred_guid, false);
  VerifyAutoconnect(walt_guid, false);
  VerifyAutoconnect(mango_guid, false);

  base::RunLoop run_loop;
  local_network_collector()->FixAutoconnect(
      protos,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure) { quit_closure.Run(); },
          run_loop.QuitClosure()));
  run_loop.Run();

  VerifyAutoconnect(fred_guid, true);
  VerifyAutoconnect(walt_guid, true);
  VerifyAutoconnect(mango_guid, false);
}

}  // namespace ash::sync_wifi
