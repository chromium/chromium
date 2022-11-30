// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/esim_manager.h"

#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/services/cellular_setup/esim_test_base.h"
#include "chromeos/ash/services/cellular_setup/esim_test_utils.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash::cellular_setup {

class ESimManagerTest : public ESimTestBase {
 public:
  ESimManagerTest() = default;
  ESimManagerTest(const ESimManagerTest&) = delete;
  ESimManagerTest& operator=(const ESimManagerTest&) = delete;
};

TEST_F(ESimManagerTest, GetAvailableEuiccs) {
  ASSERT_EQ(0u, GetAvailableEuiccs().size());
  SetupEuicc();
  // Verify that GetAvailableEuiccs call returns list of euiccs.
  std::vector<mojo::PendingRemote<mojom::Euicc>> available_euiccs =
      GetAvailableEuiccs();
  ASSERT_EQ(1u, available_euiccs.size());
  mojo::Remote<mojom::Euicc> euicc(std::move(available_euiccs.front()));
  mojom::EuiccPropertiesPtr properties = GetEuiccProperties(euicc);
  EXPECT_EQ(kTestEid, properties->eid);
}

TEST_F(ESimManagerTest, ListChangeNotification) {
  SetupEuicc();
  // Verify that available euicc list change is notified.
  ASSERT_EQ(1, observer()->available_euicc_list_change_count());

  // Add an installed profile and verify the profile list change is notified to
  // observer.
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  // Wait for events to propagate.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, observer()->profile_list_change_calls().size());

  // Add a pending profile and verify the profile list change is notified to
  // observer.
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  // Wait for events to propagate.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
}

TEST_F(ESimManagerTest, EuiccChangeNotification) {
  SetupEuicc();
  HermesEuiccClient::Properties* dbus_properties =
      HermesEuiccClient::Get()->GetProperties(dbus::ObjectPath(kTestEuiccPath));
  dbus_properties->is_active().ReplaceValue(false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, observer()->euicc_change_calls().size());
  mojo::Remote<mojom::Euicc> euicc(observer()->PopLastChangedEuicc());
  mojom::EuiccPropertiesPtr mojo_properties = GetEuiccProperties(euicc);
  EXPECT_EQ(kTestEid, mojo_properties->eid);
}

TEST_F(ESimManagerTest, ESimProfileChangeNotification) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::kActive, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(dbus::ObjectPath(profile_path));
  dbus_properties->state().ReplaceValue(hermes::profile::kInactive);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, observer()->profile_change_calls().size());
  mojo::Remote<mojom::ESimProfile> esim_profile(
      observer()->PopLastChangedESimProfile());
  mojom::ESimProfilePropertiesPtr mojo_properties =
      GetESimProfileProperties(esim_profile);
  EXPECT_EQ(dbus_properties->iccid().value(), mojo_properties->iccid);
}

}  // namespace ash::cellular_setup
