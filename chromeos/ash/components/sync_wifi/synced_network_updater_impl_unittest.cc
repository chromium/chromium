// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/synced_network_updater_impl.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/fake_pending_network_configuration_tracker.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "chromeos/ash/components/sync_wifi/pending_network_configuration_tracker_impl.h"
#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/ash/components/sync_wifi/test_data_generator.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_config {
namespace mojom = ::chromeos::network_config::mojom;
}

namespace ash::sync_wifi {

namespace {

const char kFredSsid[] = "Fred";
const char kMangoSsid[] = "Mango";

// Because the identifier is created with `GenerateInvalidPskNetworkId`, this is
// invalid because it is an invalid hex string.
const char kInvalidSsid[] = "-423";

const NetworkState* FindLocalNetworkById(const NetworkIdentifier& id) {
  NetworkStateHandler::NetworkStateList network_list;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::WiFi(), /* configured_only= */ true,
      /* visible_only= */ false, /* limit= */ 0, &network_list);
  for (const NetworkState* network : network_list) {
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
    local_test_helper_ = std::make_unique<NetworkTestHelper>();
    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());
  }

  SyncedNetworkUpdaterImplTest(const SyncedNetworkUpdaterImplTest&) = delete;
  SyncedNetworkUpdaterImplTest& operator=(const SyncedNetworkUpdaterImplTest&) =
      delete;

  ~SyncedNetworkUpdaterImplTest() override { local_test_helper_.reset(); }

  void SetUp() override {
    testing::Test::SetUp();
    local_test_helper_->SetUp();
    network_state_helper()->ResetDevicesAndServices();
    base::RunLoop().RunUntilIdle();

    auto tracker_unique_ptr =
        std::make_unique<FakePendingNetworkConfigurationTracker>();
    tracker_ = tracker_unique_ptr.get();
    timer_factory_ = std::make_unique<ash::timer_factory::FakeTimerFactory>();
    metrics_logger_ = std::make_unique<SyncedNetworkMetricsLogger>(
        /*network_state_handler=*/nullptr,
        /*network_connection_handler=*/nullptr);
    updater_ = std::make_unique<SyncedNetworkUpdaterImpl>(
        std::move(tracker_unique_ptr), remote_cros_network_config_.get(),
        timer_factory_.get(), metrics_logger_.get());
  }

  network_config::mojom::ManagedPropertiesPtr GetManagedProperties(
      const std::string& guid) {
    network_config::mojom::ManagedPropertiesPtr result;
    base::RunLoop run_loop;
    remote_cros_network_config_->GetManagedProperties(
        guid, base::BindOnce(
                  [](network_config::mojom::ManagedPropertiesPtr* result,
                     base::OnceClosure quit_closure,
                     network_config::mojom::ManagedPropertiesPtr network) {
                    *result = std::move(network);
                    std::move(quit_closure).Run();
                  },
                  &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  FakePendingNetworkConfigurationTracker* tracker() { return tracker_; }
  ash::timer_factory::FakeTimerFactory* timer_factory() {
    return timer_factory_.get();
  }
  SyncedNetworkUpdaterImpl* updater() { return updater_.get(); }
  NetworkStateTestHelper* network_state_helper() {
    return local_test_helper_->network_state_test_helper();
  }
  NetworkIdentifier fred_network_id() { return fred_network_id_; }
  NetworkIdentifier mango_network_id() { return mango_network_id_; }
  NetworkIdentifier invalid_network_id() { return invalid_network_id_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkTestHelper> local_test_helper_;
  std::unique_ptr<ash::timer_factory::FakeTimerFactory> timer_factory_;
  raw_ptr<FakePendingNetworkConfigurationTracker, DanglingUntriaged> tracker_;
  std::unique_ptr<SyncedNetworkMetricsLogger> metrics_logger_;
  std::unique_ptr<SyncedNetworkUpdaterImpl> updater_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  NetworkIdentifier fred_network_id_ = GeneratePskNetworkId(kFredSsid);
  NetworkIdentifier mango_network_id_ = GeneratePskNetworkId(kMangoSsid);
  NetworkIdentifier invalid_network_id_ =
      GenerateInvalidPskNetworkId(kInvalidSsid);
};

TEST_F(SyncedNetworkUpdaterImplTest, TestAdd_OneNetwork) {
  base::HistogramTester histogram_tester;
  sync_pb::WifiConfigurationSpecifics specifics =
      GenerateTestWifiSpecifics(fred_network_id());
  NetworkIdentifier id = NetworkIdentifier::FromProto(specifics);
  updater()->AddOrUpdateNetwork(specifics);
  EXPECT_TRUE(tracker()->GetPendingUpdateById(id));
  base::RunLoop().RunUntilIdle();
  const NetworkState* network = FindLocalNetworkById(id);
  ASSERT_TRUE(network);
  EXPECT_FALSE(network->hidden_ssid());
  EXPECT_FALSE(tracker()->GetPendingUpdateById(id));
  EXPECT_TRUE(
      NetworkHandler::Get()->network_metadata_store()->GetIsConfiguredBySync(
          network->guid()));
  histogram_tester.ExpectBucketCount(kApplyResultHistogram, true, 1);
}

TEST_F(SyncedNetworkUpdaterImplTest, TestUpdate_UnsetFields) {
  base::HistogramTester histogram_tester;
  sync_pb::WifiConfigurationSpecifics specifics =
      GenerateTestWifiSpecifics(fred_network_id());
  NetworkIdentifier id = NetworkIdentifier::FromProto(specifics);
  updater()->AddOrUpdateNetwork(specifics);
  EXPECT_TRUE(tracker()->GetPendingUpdateById(id));
  base::RunLoop().RunUntilIdle();
  network_config::mojom::ManagedPropertiesPtr network =
      GetManagedProperties(FindLocalNetworkById(id)->guid());
  EXPECT_TRUE(network->priority->active_value);
  EXPECT_TRUE(network->type_properties->get_wifi()->auto_connect->active_value);

  specifics.set_automatically_connect(
      sync_pb::WifiConfigurationSpecifics_AutomaticallyConnectOption::
          WifiConfigurationSpecifics_AutomaticallyConnectOption_AUTOMATICALLY_CONNECT_UNSPECIFIED);
  specifics.set_is_preferred(
      sync_pb::WifiConfigurationSpecifics_IsPreferredOption::
          WifiConfigurationSpecifics_IsPreferredOption_IS_PREFERRED_UNSPECIFIED);
  updater()->AddOrUpdateNetwork(specifics);
  EXPECT_TRUE(tracker()->GetPendingUpdateById(id));
  base::RunLoop().RunUntilIdle();
  network = GetManagedProperties(FindLocalNetworkById(id)->guid());
  // Verify that the previous values for priority and autoconnect were not
  // overwritten.
  EXPECT_TRUE(network->priority->active_value);
  EXPECT_TRUE(network->type_properties->get_wifi()->auto_connect->active_value);
}

TEST_F(SyncedNetworkUpdaterImplTest, TestAdd_ThenRemove) {
  base::HistogramTester histogram_tester;
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
  histogram_tester.ExpectBucketCount(kApplyResultHistogram, true, 2);
}

TEST_F(SyncedNetworkUpdaterImplTest, TestAdd_TwoNetworks) {
  base::HistogramTester histogram_tester;
  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(mango_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(mango_network_id()));
  base::RunLoop().RunUntilIdle();

  const NetworkState* fred_network = FindLocalNetworkById(fred_network_id());
  ASSERT_TRUE(fred_network);
  const NetworkState* mango_network = FindLocalNetworkById(mango_network_id());
  ASSERT_TRUE(mango_network);
  EXPECT_TRUE(
      NetworkHandler::Get()->network_metadata_store()->GetIsConfiguredBySync(
          fred_network->guid()));
  EXPECT_TRUE(
      NetworkHandler::Get()->network_metadata_store()->GetIsConfiguredBySync(
          mango_network->guid()));

  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(mango_network_id()));
  histogram_tester.ExpectBucketCount(kApplyResultHistogram, true, 2);
}

TEST_F(SyncedNetworkUpdaterImplTest, TestFailToAdd_Error) {
  base::HistogramTester histogram_tester;
  network_state_helper()->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);

  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(FindLocalNetworkById(fred_network_id()));

  // The tracker should no longer be tracking the update because it reached the
  // max failed number of attempts.
  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));
  // Our test tracker holds on to the number of completed attempts after an
  // update has been removed, and that should be equal to kMaxRetries (3).
  EXPECT_EQ(3, tracker()->GetCompletedAttempts(fred_network_id()));

  histogram_tester.ExpectBucketCount(kApplyResultHistogram, false, 1);
  histogram_tester.ExpectBucketCount(
      kApplyFailureReasonHistogram, ApplyNetworkFailureReason::kFailedToAdd, 3);
}

TEST_F(SyncedNetworkUpdaterImplTest, TestFailToAdd_Timeout) {
  base::HistogramTester histogram_tester;
  network_state_helper()->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kTimeout);

  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_EQ(0, tracker()->GetCompletedAttempts(fred_network_id()));

  timer_factory()->FireAll();

  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_EQ(1, tracker()->GetCompletedAttempts(fred_network_id()));

  timer_factory()->FireAll();

  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_EQ(2, tracker()->GetCompletedAttempts(fred_network_id()));

  timer_factory()->FireAll();

  EXPECT_EQ(3, tracker()->GetCompletedAttempts(fred_network_id()));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));

  histogram_tester.ExpectBucketCount(kApplyResultHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kApplyFailureReasonHistogram,
                                     ApplyNetworkFailureReason::kTimedout, 3);
}

TEST_F(SyncedNetworkUpdaterImplTest, TestFailToAdd_Timeout_ThenSucceed) {
  base::HistogramTester histogram_tester;
  network_state_helper()->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kTimeout);

  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_EQ(0, tracker()->GetCompletedAttempts(fred_network_id()));

  timer_factory()->FireAll();

  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_EQ(1, tracker()->GetCompletedAttempts(fred_network_id()));

  network_state_helper()->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kSuccess);

  timer_factory()->FireAll();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_TRUE(FindLocalNetworkById(fred_network_id()));

  histogram_tester.ExpectBucketCount(kApplyResultHistogram, false, 0);
  histogram_tester.ExpectBucketCount(kApplyResultHistogram, true, 1);
  histogram_tester.ExpectBucketCount(kApplyFailureReasonHistogram,
                                     ApplyNetworkFailureReason::kTimedout, 2);
}

TEST_F(SyncedNetworkUpdaterImplTest, TestFailToRemove) {
  base::HistogramTester histogram_tester;
  network_state_helper()->profile_test()->SetSimulateDeleteResult(
      FakeShillSimulatedResult::kFailure);
  updater()->AddOrUpdateNetwork(GenerateTestWifiSpecifics(fred_network_id()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FindLocalNetworkById(fred_network_id()));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));

  updater()->RemoveNetwork(fred_network_id());
  EXPECT_TRUE(tracker()->GetPendingUpdateById(fred_network_id()));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(FindLocalNetworkById(fred_network_id()));
  EXPECT_FALSE(tracker()->GetPendingUpdateById(fred_network_id()));
  EXPECT_EQ(3, tracker()->GetCompletedAttempts(fred_network_id()));

  histogram_tester.ExpectBucketCount(kApplyResultHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kApplyFailureReasonHistogram,
                                     ApplyNetworkFailureReason::kFailedToRemove,
                                     3);
}

// Regression test for b/268408833
TEST_F(SyncedNetworkUpdaterImplTest, InvalidSsid) {
  base::HistogramTester histogram_tester;
  updater()->AddOrUpdateNetwork(
      GenerateTestWifiSpecifics(invalid_network_id()));

  EXPECT_EQ(0, tracker()->GetCompletedAttempts(invalid_network_id()));
  EXPECT_TRUE(tracker()->GetPendingUpdateById(invalid_network_id()));
  histogram_tester.ExpectBucketCount(kApplyGenerateLocalNetworkConfigHistogram,
                                     false, 1);
}

}  // namespace ash::sync_wifi
