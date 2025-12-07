// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/stub_cellular_networks_provider.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";
const char kTestEuiccBasePath[] = "/org/chromium/Hermes/Euicc/";
const char kTestBaseEid[] = "12345678901234567890123456789012";
const char kTestPSimIccid[] = "1234567890";
const char kTestCellularServicePath[] = "/service/cellular";

std::string CreateTestEuiccPath(int euicc_num) {
  return base::StringPrintf("%s%d", kTestEuiccBasePath, euicc_num);
}

std::string CreateTestEid(int euicc_num) {
  return base::StringPrintf("%s%d", kTestBaseEid, euicc_num);
}

}  // namespace

class StubCellularNetworksProviderTest : public testing::Test {
 protected:
  StubCellularNetworksProviderTest()
      : helper_(/*use_default_devices_and_services=*/false) {}

  ~StubCellularNetworksProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    helper_.device_test()->AddDevice(kDefaultCellularDevicePath,
                                     shill::kTypeCellular, "cellular1");
  }

  void Init() {
    cellular_esim_profile_handler_.Init(helper_.network_state_handler(),
                                        /*cellular_inhibitor=*/nullptr);
    managed_cellular_pref_handler_.Init(helper_.network_state_handler());
    ManagedCellularPrefHandler::RegisterLocalStatePrefs(
        device_prefs_.registry());
    managed_cellular_pref_handler_.SetDevicePrefs(&device_prefs_);

    provider_ = std::make_unique<StubCellularNetworksProvider>();
    provider_->Init(helper_.network_state_handler(),
                    &cellular_esim_profile_handler_,
                    &managed_cellular_pref_handler_);
  }

  void AddEuicc(int euicc_num) {
    std::string euicc_path = CreateTestEuiccPath(euicc_num);

    helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(euicc_path), CreateTestEid(euicc_num),
        /*is_active=*/true, /*physical_slot=*/0);
    base::RunLoop().RunUntilIdle();
  }

  dbus::ObjectPath AddProfile(int euicc_num,
                              hermes::profile::State state,
                              const std::string& activation_code) {
    dbus::ObjectPath path = helper_.hermes_euicc_test()->AddFakeCarrierProfile(
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)), state,
        activation_code,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();
    return path;
  }

  bool AddOrRemoveStubCellularNetworks(
      NetworkStateHandler::ManagedStateList& network_list,
      NetworkStateHandler::ManagedStateList& new_stub_networks) {
    const DeviceState* device_state =
        helper_.network_state_handler()->GetDeviceStateByType(
            NetworkTypePattern::Cellular());
    return provider_->AddOrRemoveStubCellularNetworks(
        network_list, new_stub_networks, device_state);
  }

  void SetPSimSlotInfo(const std::string& iccid) {
    auto sim_slot_infos = base::Value::List().Append(
        base::Value::Dict()
            .Set(shill::kSIMSlotInfoEID, std::string())
            .Set(shill::kSIMSlotInfoICCID, iccid)
            .Set(shill::kSIMSlotInfoPrimary, true));

    helper_.device_test()->SetDeviceProperty(
        kDefaultCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(std::move(sim_slot_infos)),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void CallGetStubNetworkMetadata(const std::string& iccid, bool should_exist) {
    std::string service_path, guid;
    bool exists = provider_->GetStubNetworkMetadata(
        iccid,
        helper_.network_state_handler()->GetDeviceStateByType(
            NetworkTypePattern::Cellular()),
        &service_path, &guid);

    if (!should_exist) {
      EXPECT_FALSE(exists);
      return;
    }

    EXPECT_TRUE(exists);
    EXPECT_EQ(cellular_utils::GenerateStubCellularServicePath(iccid),
              service_path);
  }

  void DisableCellularTechnology() {
    helper_.technology_state_controller()->SetTechnologiesEnabled(
        NetworkTypePattern::Cellular(), /*enabled=*/false,
        /*error_callback=*/base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  TestingPrefServiceSimple* device_prefs() { return &device_prefs_; }

  ManagedCellularPrefHandler* managed_cellular_pref_handler() {
    return &managed_cellular_pref_handler_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_;
  TestCellularESimProfileHandler cellular_esim_profile_handler_;
  ManagedCellularPrefHandler managed_cellular_pref_handler_;
  std::unique_ptr<StubCellularNetworksProvider> provider_;
  TestingPrefServiceSimple device_prefs_;
};

TEST_F(StubCellularNetworksProviderTest, AddOrRemoveStubCellularNetworks) {
  SetPSimSlotInfo(kTestPSimIccid);
  AddEuicc(/*euicc_num=*/1);
  Init();
  dbus::ObjectPath profile1_path =
      AddProfile(/*euicc_num=*/1, hermes::profile::State::kPending,
                 /*activation_code=*/"code1");
  dbus::ObjectPath profile2_path =
      AddProfile(/*euicc_num=*/1, hermes::profile::State::kInactive,
                 /*activation_code=*/"code1");
  dbus::ObjectPath profile3_path =
      AddProfile(/*euicc_num=*/1, hermes::profile::State::kInactive,
                 /*activation_code=*/"code1");
  HermesProfileClient::Properties* profile1_properties =
      HermesProfileClient::Get()->GetProperties(profile1_path);
  HermesProfileClient::Properties* profile2_properties =
      HermesProfileClient::Get()->GetProperties(profile2_path);
  HermesProfileClient::Properties* profile3_properties =
      HermesProfileClient::Get()->GetProperties(profile3_path);

  CallGetStubNetworkMetadata(profile1_properties->iccid().value(),
                             /*should_exist=*/false);
  CallGetStubNetworkMetadata(profile2_properties->iccid().value(),
                             /*should_exist=*/true);
  CallGetStubNetworkMetadata(profile3_properties->iccid().value(),
                             /*should_exist=*/true);
  CallGetStubNetworkMetadata(kTestPSimIccid, /*should_exist=*/true);
  CallGetStubNetworkMetadata("nonexistent_iccid", /*should_exist=*/false);

  NetworkStateHandler::ManagedStateList network_list, new_stub_networks;

  // Verify that stub services are created for eSIM profiles and pSIM iccids
  // on sim slot info.
  managed_cellular_pref_handler()->AddESimMetadata(
      profile3_properties->iccid().value(), "name",
      policy_util::SmdxActivationCode(
          policy_util::SmdxActivationCode::Type::SMDP,
          "activation_code_value"));
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(3u, new_stub_networks.size());
  NetworkState* network1 = new_stub_networks[0]->AsNetworkState();
  NetworkState* network2 = new_stub_networks[1]->AsNetworkState();
  NetworkState* network3 = new_stub_networks[2]->AsNetworkState();
  EXPECT_TRUE(network1->IsNonShillCellularNetwork());
  EXPECT_TRUE(network2->IsNonShillCellularNetwork());
  EXPECT_TRUE(network3->IsNonShillCellularNetwork());
  EXPECT_EQ(network1->iccid(), profile2_properties->iccid().value());
  EXPECT_FALSE(network1->IsManagedByPolicy());
  EXPECT_EQ(network2->iccid(), profile3_properties->iccid().value());
  EXPECT_TRUE(network2->IsManagedByPolicy());
  EXPECT_EQ(network3->iccid(), kTestPSimIccid);

  // Verify the stub networks becomes unmanaged once the iccid and smdp address
  // pair is removed from pref.
  network_list = std::move(new_stub_networks);
  new_stub_networks.clear();

  // Manually reach into the device prefs and mark the eSIM profile as no longer
  // being actively managed. We modify the prefs directly so that we can test
  // `AddOrRemoveStubCellularNetworks()` directly and not rely on
  // `ManagedCellularPrefHandler` notifying pref changes.
  const std::string& iccid = profile3_properties->iccid().value();
  base::Value::Dict prefs =
      device_prefs()->GetDict(prefs::kManagedCellularESimMetadata).Clone();
  ASSERT_TRUE(prefs.contains(iccid));
  base::Value::Dict* esim_metadata = prefs.FindDict(iccid);
  esim_metadata->Set("PolicyMissing", true);
  device_prefs()->SetDict(prefs::kManagedCellularESimMetadata,
                          std::move(prefs));

  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(3u, network_list.size());
  EXPECT_EQ(0u, new_stub_networks.size());
  network2 = network_list[1]->AsNetworkState();
  EXPECT_EQ(network2->iccid(), profile3_properties->iccid().value());
  EXPECT_FALSE(network2->IsManagedByPolicy());

  // Verify the stub networks are removed when corresponding slot is no longer
  // present. e.g. SIM removed.
  SetPSimSlotInfo(/*iccid=*/std::string());
  base::RunLoop().RunUntilIdle();
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(2u, network_list.size());

  // Verify that stub networks are removed when real networks are added to the
  // list.
  std::unique_ptr<NetworkState> test_network =
      std::make_unique<NetworkState>(kTestCellularServicePath);
  test_network->PropertyChanged(shill::kTypeProperty,
                                base::Value(shill::kTypeCellular));
  test_network->PropertyChanged(
      shill::kIccidProperty, base::Value(profile2_properties->iccid().value()));
  test_network->set_update_received();
  network_list.push_back(std::move(test_network));
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(2u, network_list.size());
}

TEST_F(StubCellularNetworksProviderTest, RemoveStubWhenCellularDisabled) {
  SetPSimSlotInfo(kTestPSimIccid);
  AddEuicc(/*euicc_num=*/1);
  Init();
  dbus::ObjectPath profile_path =
      AddProfile(/*euicc_num=*/1, hermes::profile::State::kInactive,
                 /*activation_code=*/"code1");

  NetworkStateHandler::ManagedStateList network_list, new_stub_networks;

  // Verify that stub services are created for eSIM profiles and pSIM iccids
  // on sim slot info.
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(2u, new_stub_networks.size());

  // Verify the stub networks are removed when cellular technology is disabled.
  network_list = std::move(new_stub_networks);
  new_stub_networks.clear();
  DisableCellularTechnology();
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(0u, network_list.size());
  EXPECT_EQ(0u, new_stub_networks.size());
}

}  // namespace ash
