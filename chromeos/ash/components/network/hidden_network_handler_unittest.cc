// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hidden_network_handler.h"

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

// kTwoWeeks set to 15 days due to edge case where creating network while
// internal timer is running results in network creation timestamp not being
// initialized until the next time the timer fires, eg. the next day.
constexpr base::TimeDelta kTwoWeeks = base::Days(15);
constexpr base::TimeDelta kArbitraryTime = base::Days(11686);
const char* kWiFiGuid1 = "wifi_guid1";
const char* kWiFiGuid2 = "wifi_guid2";
const char* kWiFiGuid3 = "wifi_guid3";
constexpr char kServicePattern[] =
    R"({"GUID": "%s", "Type": "wifi", "State": "idle", "SSID": "wifi",
    "Strength": 100, "WiFi.HiddenSSID": %s})";

class FakeNetworkConfigurationObserver : public NetworkConfigurationObserver {
 public:
  void OnBeforeConfigurationRemoved(const std::string& service_path,
                                    const std::string& guid) override {
    total_removed_count_++;
    service_path_ = service_path;
    guid_ = guid;
  }

  size_t total_removed_count() const { return total_removed_count_; }
  const std::string& service_path() { return service_path_; }
  const std::string& guid() { return guid_; }

 private:
  size_t total_removed_count_ = 0;
  std::string service_path_;
  std::string guid_;
};

}  // namespace

class HiddenNetworkHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHiddenNetworkMigration);
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    hidden_network_handler_ = NetworkHandler::Get()->hidden_network_handler();
    network_configuration_handler_ =
        NetworkHandler::Get()->network_configuration_handler();
    // Explicitly need to advance time for tests because the default
    // time is UnixEpoch, which would signal for a network to be
    // removed when created with default configurations.
    task_environment_.AdvanceClock(kArbitraryTime);

    base::RunLoop().RunUntilIdle();
    network_handler_test_helper_->ClearServices();
    base::RunLoop().RunUntilIdle();

    network_configuration_observer_ =
        std::make_unique<FakeNetworkConfigurationObserver>();
    network_configuration_handler_->AddObserver(
        network_configuration_observer_.get());
  }

  void TearDown() override {
    network_configuration_handler_->RemoveObserver(
        network_configuration_observer_.get());
    network_handler_test_helper_.reset();
  }

  void MaybeRegisterAndInitializePrefs(bool should_register = true) {
    if (should_register) {
      network_handler_test_helper_->RegisterPrefs(profile_prefs_.registry(),
                                                  local_state_.registry());
    }
    network_handler_test_helper_->InitializePrefs(&profile_prefs_,
                                                  &local_state_);
  }

  std::string CreateDefaultHiddenWiFiNetwork() {
    return CreateWiFiNetwork(/*hidden=*/true, /*add_to_profile=*/true,
                             kWiFiGuid1);
  }

  std::string CreateWiFiNetwork(bool hidden,
                                bool add_to_profile,
                                const char* guid) {
    std::string wifi_path;
    if (hidden) {
      wifi_path = network_handler_test_helper_->ConfigureService(
          base::StringPrintf(kServicePattern, guid, "true"));
    } else {
      wifi_path = network_handler_test_helper_->ConfigureService(
          base::StringPrintf(kServicePattern, guid, "false"));
    }

    if (add_to_profile) {
      network_handler_test_helper_->profile_test()->AddService(
          NetworkProfileHandler::GetSharedProfilePath(), wifi_path);
    }

    base::RunLoop().RunUntilIdle();
    return wifi_path;
  }

  void ConnectToNetwork(const std::string& wifi_path) {
    NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
        wifi_path, base::DoNothing(), base::DoNothing(),
        /*check_error_state=*/false, ConnectCallbackMode::ON_COMPLETED);
    base::RunLoop().RunUntilIdle();
  }

  void MakeNetworkManaged(const std::string& wifi_path) {
    network_handler_test_helper_->SetServiceProperty(
        wifi_path, shill::kONCSourceProperty,
        base::Value(shill::kONCSourceDevicePolicy));

    std::unique_ptr<NetworkUIData> ui_data = NetworkUIData::CreateFromONC(
        ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
    network_handler_test_helper_->SetServiceProperty(
        wifi_path, shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));

    base::RunLoop().RunUntilIdle();
  }

  void ExpectNetworksRemoved(const std::string service_path,
                             size_t total_removed_count) {
    EXPECT_EQ(service_path,
              network_configuration_observer_.get()->service_path());
    EXPECT_EQ(total_removed_count,
              network_configuration_observer_.get()->total_removed_count());
  }

  void ExpectHistogramCount(int bucket, int frequency, int total, int sum) {
    histogram_tester.ExpectBucketCount("Network.Ash.WiFi.Hidden.RemovalAttempt",
                                       bucket, frequency);
    histogram_tester.ExpectTotalCount("Network.Ash.WiFi.Hidden.RemovalAttempt",
                                      total);
    EXPECT_EQ(
        histogram_tester.GetTotalSum("Network.Ash.WiFi.Hidden.RemovalAttempt"),
        sum);
  }

  void ErrorCallback(const std::string& error_name) {
    ADD_FAILURE() << "Unexpected error: " << error_name;
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 protected:
  base::HistogramTester histogram_tester;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  HiddenNetworkHandler* hidden_network_handler_;
  NetworkConfigurationHandler* network_configuration_handler_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<FakeNetworkConfigurationObserver>
      network_configuration_observer_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(HiddenNetworkHandlerTest, MeetsAllCriteriaToRemove) {
  MaybeRegisterAndInitializePrefs();

  const std::string path = CreateDefaultHiddenWiFiNetwork();
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  task_environment()->FastForwardBy(kTwoWeeks);
  base::RunLoop().RunUntilIdle();
  ExpectNetworksRemoved(/*service_path=*/path,
                        /*total_removed_count=*/1);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/1,
                       /*total=*/16,
                       /*sum=*/1);
}

TEST_F(HiddenNetworkHandlerTest, RemoveTwoNetworks) {
  MaybeRegisterAndInitializePrefs();

  const std::string path1 = CreateWiFiNetwork(
      /*hidden=*/true, /*add_to_profile=*/true, /*guid=*/kWiFiGuid1);
  const std::string path2 = CreateWiFiNetwork(
      /*hidden=*/true, /*add_to_profile=*/true, /*guid=*/kWiFiGuid2);
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);

  task_environment()->FastForwardBy(kTwoWeeks);
  base::RunLoop().RunUntilIdle();
  ExpectNetworksRemoved(/*service_path=*/path2,
                        /*total_removed_count=*/2);
  ExpectHistogramCount(/*bucket=*/2,
                       /*frequency=*/1,
                       /*total=*/16,
                       /*sum=*/2);
}

TEST_F(HiddenNetworkHandlerTest, ChecksForNetworksToRemoveDaily) {
  MaybeRegisterAndInitializePrefs();

  // Networks created one day apart, as otherwise they would all be removed on
  // the same day 2 weeks later.
  const std::string path1 = CreateWiFiNetwork(
      /*hidden=*/true, /*add_to_profile=*/true, /*guid=*/kWiFiGuid1);
  task_environment()->FastForwardBy(base::Days(1));

  const std::string path2 = CreateWiFiNetwork(
      /*hidden=*/true, /*add_to_profile=*/true, /*guid=*/kWiFiGuid2);
  task_environment()->FastForwardBy(base::Days(1));

  const std::string path3 = CreateWiFiNetwork(
      /*hidden=*/true, /*add_to_profile=*/true, /*guid=*/kWiFiGuid3);

  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);

  const base::TimeDelta kTimeSinceFirstNetworkWasCreated =
      kTwoWeeks - base::Days(2);

  task_environment()->FastForwardBy(kTimeSinceFirstNetworkWasCreated);
  ExpectNetworksRemoved(/*service_path=*/path1, /*total_removed_count=*/1);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/1,
                       /*total=*/16,
                       /*sum=*/1);

  task_environment()->FastForwardBy(base::Days(1));
  ExpectNetworksRemoved(/*service_path=*/path2, /*total_removed_count=*/2);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/2,
                       /*total=*/17,
                       /*sum=*/2);

  task_environment()->FastForwardBy(base::Days(1));
  ExpectNetworksRemoved(/*service_path=*/path3, /*total_removed_count=*/3);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/3,
                       /*total=*/18,
                       /*sum=*/3);
}

TEST_F(HiddenNetworkHandlerTest, NetworksAreCheckedWhenPrefsAreInitialized) {
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);

  const std::string path = CreateDefaultHiddenWiFiNetwork();
  MaybeRegisterAndInitializePrefs();
  base::RunLoop().RunUntilIdle();

  // We explicitly shut down and re-initialize the pref services to test that
  // whenever they are initialized the HiddenNetworkHandler class will
  // immediately check for wrongly configured networks.
  NetworkHandler::Get()->ShutdownPrefServices();
  base::RunLoop().RunUntilIdle();
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);

  task_environment()->FastForwardBy(kTwoWeeks);
  MaybeRegisterAndInitializePrefs(/*should_register=*/false);
  base::RunLoop().RunUntilIdle();
  ExpectNetworksRemoved(/*service_path=*/path,
                        /*total_removed_count=*/1);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/1,
                       /*total=*/2,
                       /*sum=*/1);
}

TEST_F(HiddenNetworkHandlerTest, LessThanTwoWeeks) {
  MaybeRegisterAndInitializePrefs();

  CreateDefaultHiddenWiFiNetwork();
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  task_environment()->FastForwardBy(kTwoWeeks - base::Hours(5));
  base::RunLoop().RunUntilIdle();
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/0,
                       /*total=*/15,
                       /*sum=*/0);
}

TEST_F(HiddenNetworkHandlerTest, OnlyRemovesNetworksInCurrentProfile) {
  MaybeRegisterAndInitializePrefs();

  CreateWiFiNetwork(true, false, kWiFiGuid1);
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  task_environment()->FastForwardBy(kTwoWeeks);
  base::RunLoop().RunUntilIdle();
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/0,
                       /*total=*/16,
                       /*sum=*/0);
}

TEST_F(HiddenNetworkHandlerTest, ConnectedNetworkNotRemoved) {
  MaybeRegisterAndInitializePrefs();

  const std::string path = CreateDefaultHiddenWiFiNetwork();
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  ConnectToNetwork(path);
  task_environment()->FastForwardBy(kTwoWeeks);
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/0,
                       /*total=*/16,
                       /*sum=*/0);
}

TEST_F(HiddenNetworkHandlerTest, ManagedNetworkNotRemoved) {
  MaybeRegisterAndInitializePrefs();
  const std::string path = CreateDefaultHiddenWiFiNetwork();
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  MakeNetworkManaged(path);
  task_environment()->FastForwardBy(kTwoWeeks);
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/0,
                       /*total=*/16,
                       /*sum=*/0);
}

TEST_F(HiddenNetworkHandlerTest, UnhiddenNetworkNotRemoved) {
  MaybeRegisterAndInitializePrefs();

  CreateWiFiNetwork(false, true, kWiFiGuid1);
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  task_environment()->FastForwardBy(kTwoWeeks);
  ExpectNetworksRemoved(/*service_path=*/"", /*total_removed_count=*/0u);
  ExpectHistogramCount(/*bucket=*/1,
                       /*frequency=*/0,
                       /*total=*/16,
                       /*sum=*/0);
}

}  // namespace ash
