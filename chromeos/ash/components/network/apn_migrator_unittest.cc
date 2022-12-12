// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/apn_migrator.h"

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/mock_managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/mock_managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/mock_network_metadata_store.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/onc/onc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Truly;
using ::testing::WithArg;

namespace {
constexpr char kCellularName1[] = "cellular_device_1";
constexpr char kTestCellularPath1[] = "/device/cellular_device_1";
constexpr char kTestCellularIccid1[] = "test_iccid_1";
constexpr char kTestCellularGuid1[] = "test_guid_1";

constexpr char kCellularName2[] = "cellular_device_2";
constexpr char kTestCellularPath2[] = "/device/cellular_device_2";
constexpr char kTestCellularIccid2[] = "test_iccid_2";
constexpr char kTestCellularGuid2[] = "test_guid_2";

constexpr char kCellularName3[] = "cellular_device_3";
constexpr char kTestCellularPath3[] = "/device/cellular_device_3";
constexpr char kTestCellularIccid3[] = "test_iccid_3";
constexpr char kTestCellularGuid3[] = "test_guid_3";

constexpr char kCellularServicePattern[] =
    R"({"GUID": "%s", "Type": "cellular",  "State": "idle",
            "Strength": 0, "Cellular.NetworkTechnology": "LTE",
            "Cellular.ActivationState": "activated", "Cellular.ICCID": "%s",
            "Profile": "%s"})";
}  // namespace

class ApnMigratorTest : public testing::Test {
 public:
  ApnMigratorTest() = default;

  ApnMigratorTest(const ApnMigratorTest&) = delete;
  ApnMigratorTest& operator=(const ApnMigratorTest&) = delete;
  ~ApnMigratorTest() override = default;

  // testing::Test
  void SetUp() override {
    managed_cellular_pref_handler_ =
        base::WrapUnique(new testing::NiceMock<MockManagedCellularPrefHandler>);
    managed_network_configuration_handler_ = base::WrapUnique(
        new testing::NiceMock<MockManagedNetworkConfigurationHandler>);
    network_metadata_store_ =
        base::WrapUnique(new testing::NiceMock<MockNetworkMetadataStore>());

    apn_migrator_ = std::make_unique<ApnMigrator>(
        managed_cellular_pref_handler_.get(),
        managed_network_configuration_handler_.get(),
        network_state_helper_.network_state_handler(),
        network_metadata_store_.get());
    SetupNetworks();
  }

  void TearDown() override {
    apn_migrator_.reset();
    managed_network_configuration_handler_.reset();
    managed_cellular_pref_handler_.reset();
  }

  void TriggerNetworkListChanged() {
    static_cast<NetworkStateHandlerObserver*>(apn_migrator_.get())
        ->NetworkListChanged();
  }

 protected:
  MockManagedCellularPrefHandler* managed_cellular_pref_handler() const {
    return managed_cellular_pref_handler_.get();
  }
  MockManagedNetworkConfigurationHandler*
  managed_network_configuration_handler() const {
    return managed_network_configuration_handler_.get();
  }
  MockNetworkMetadataStore* network_metadata_store() const {
    return network_metadata_store_.get();
  }

  const std::string& cellular_service_path_1() const {
    return cellular_service_path_1_;
  }
  const std::string& cellular_service_path_2() const {
    return cellular_service_path_2_;
  }
  const std::string& cellular_service_path_3() const {
    return cellular_service_path_3_;
  }

  void AddStub(const std::string& stub_iccid, const std::string& eid) {
    stub_cellular_networks_provider_.AddStub(stub_iccid, eid);
    network_state_helper_.network_state_handler()->SyncStubCellularNetworks();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper network_state_helper_{
      /*use_default_devices_and_services=*/true};
  NetworkHandlerTestHelper handler_test_helper_;
  FakeStubCellularNetworksProvider stub_cellular_networks_provider_;

  std::unique_ptr<MockManagedCellularPrefHandler>
      managed_cellular_pref_handler_;
  std::unique_ptr<MockManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<MockNetworkMetadataStore> network_metadata_store_;

  std::string cellular_service_path_1_;
  std::string cellular_service_path_2_;
  std::string cellular_service_path_3_;

  // Class under test
  std::unique_ptr<ApnMigrator> apn_migrator_;

  void AddTestCellularDevice(const std::string& device_name,
                             const std::string& device_path,
                             const std::string& device_iccid) {
    network_state_helper_.device_test()->AddDevice(
        device_path, shill::kTypeCellular, device_name);
    network_state_helper_.device_test()->SetDeviceProperty(
        device_path, shill::kIccidProperty, base::Value(device_iccid),
        /*notify_changed=*/false);
  }

  void SetupNetworks() {
    network_state_helper_.manager_test()->AddTechnology(shill::kTypeCellular,
                                                        /*enabled=*/true);
    network_state_helper_.network_state_handler()
        ->set_stub_cellular_networks_provider(
            &stub_cellular_networks_provider_);

    AddTestCellularDevice(kCellularName1, kTestCellularPath1,
                          kTestCellularIccid1);
    cellular_service_path_1_ =
        network_state_helper_.ConfigureService(base::StringPrintf(
            kCellularServicePattern, kTestCellularGuid1, kTestCellularIccid1,
            NetworkProfileHandler::GetSharedProfilePath().c_str()));

    AddTestCellularDevice(kCellularName2, kTestCellularPath2,
                          kTestCellularIccid2);
    cellular_service_path_2_ =
        network_state_helper_.ConfigureService(base::StringPrintf(
            kCellularServicePattern, kTestCellularGuid2, kTestCellularIccid2,
            NetworkProfileHandler::GetSharedProfilePath().c_str()));

    AddTestCellularDevice(kCellularName3, kTestCellularPath3,
                          kTestCellularIccid3);
    cellular_service_path_3_ =
        network_state_helper_.ConfigureService(base::StringPrintf(
            kCellularServicePattern, kTestCellularGuid3, kTestCellularIccid3,
            NetworkProfileHandler::GetSharedProfilePath().c_str()));
  }
};

TEST_F(ApnMigratorTest, ApnRevampFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(ash::features::kApnRevamp);

  // Every network should be evaluated, pretend that two networks were already
  // migrated.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid2)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid3)))
      .Times(1)
      .WillOnce(Return(true));

  // For migrated networks, the routine should not check for the current custom
  // APN list, but rather just resets the UserApnList.
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid1))
      .Times(0);
  base::Value::Dict expected_onc1 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, /*user_apn_list=*/nullptr);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_1(),
                            Truly([&expected_onc1](const base::Value& value) {
                              return expected_onc1 == value.GetDict();
                            }),
                            _, _))
      .Times(1);
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid2))
      .Times(0);
  base::Value::Dict expected_onc2 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid3, /*user_apn_list=*/nullptr);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_3(),
                            Truly([&expected_onc2](const base::Value& value) {
                              return expected_onc2 == value.GetDict();
                            }),
                            _, _))
      .Times(1);

  // Ensure that the function does not modify the non-migrated network.
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_2(), _, _, _))
      .Times(0);

  // Function under test
  TriggerNetworkListChanged();
}

TEST_F(ApnMigratorTest, ApnRevampFlagEnabled_MigratedNetworks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);

  const char kTestStubIccid[] = "test_stub_iccid";
  const char kTestStubEid[] = "test_stub_eid";
  AddStub(kTestStubIccid, kTestStubEid);

  // The migrator routine will iterate through cellular networks. Stub networks
  // must be ignored. For this test, pretend that all non-stub cellular network
  // have been migrated.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid2)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid3)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestStubIccid)))
      .Times(0);

  // Return nullptr and empty list for the first two networks.
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(nullptr));
  base::Value::List empty_apn_list;
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid2))
      .Times(1)
      .WillOnce(Return(&empty_apn_list));

  // For the third network, simulate a populated custom APN list.
  base::Value::Dict custom_apn_1;
  custom_apn_1.Set(::onc::cellular_apn::kAccessPointName, "apn_1");
  base::Value::Dict custom_apn_2;
  custom_apn_2.Set(::onc::cellular_apn::kAccessPointName, "apn_2");
  base::Value::List populated_apn_list;
  populated_apn_list.Append(std::move(custom_apn_1));
  populated_apn_list.Append(std::move(custom_apn_2));
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid3))
      .Times(1)
      .WillOnce(Return(&populated_apn_list));

  // For the first and second networks, the function should update Shill with
  // empty user APN lists.
  base::Value::Dict expected_onc_1 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, &empty_apn_list);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_1(),
                            Truly([&expected_onc_1](const base::Value& value) {
                              return expected_onc_1 == value.GetDict();
                            }),
                            _, _))
      .Times(1);
  base::Value::Dict expected_onc_2 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid2, &empty_apn_list);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_2(),
                            Truly([&expected_onc_2](const base::Value& value) {
                              return expected_onc_2 == value.GetDict();
                            }),
                            _, _))
      .Times(1);

  // Verify that Shill receives the user APNs for the third list.
  base::Value::Dict expected_onc_3 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid3, &populated_apn_list);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_3(),
                            Truly([&expected_onc_3](const base::Value& value) {
                              return expected_onc_3 == value.GetDict();
                            }),
                            _, _))
      .Times(1);

  // Function under test.
  TriggerNetworkListChanged();
}

TEST_F(ApnMigratorTest, ApnRevampFlagEnabled_MigrateNetworks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);

  // Every network should be evaluated, pretend that all network need to be
  // migrated.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid2)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid3)))
      .Times(1)
      .WillOnce(Return(false));

  // Simulate that all networks do not have custom APNs
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(nullptr));
  base::Value::List empty_apn_list;
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid2))
      .Times(1)
      .WillOnce(Return(&empty_apn_list));
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid3))
      .Times(1)
      .WillOnce(Return(&empty_apn_list));

  // The function should only update Shill with empty user APN lists.
  base::Value::Dict expected_onc_1 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, &empty_apn_list);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_1(),
                            Truly([&expected_onc_1](const base::Value& value) {
                              return expected_onc_1 == value.GetDict();
                            }),
                            _, _))
      .Times(1);
  base::Value::Dict expected_onc_2 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid2, &empty_apn_list);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_2(),
                            Truly([&expected_onc_2](const base::Value& value) {
                              return expected_onc_2 == value.GetDict();
                            }),
                            _, _))
      .Times(1);
  base::Value::Dict expected_onc_3 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid3, &empty_apn_list);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_3(),
                            Truly([&expected_onc_3](const base::Value& value) {
                              return expected_onc_3 == value.GetDict();
                            }),
                            _, _))
      .Times(1);

  // All network should be marked as migrated
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1);
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid2)))
      .Times(1);
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid3)))
      .Times(1);

  // Function under test.
  TriggerNetworkListChanged();
}

}  // namespace ash
