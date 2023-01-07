// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";
const char kTestEuiccBasePath[] = "/org/chromium/Hermes/Euicc/";
const char kTestProfileBasePath[] = "/org/chromium/Hermes/Profile/";
const char kTestBaseEid[] = "12345678901234567890123456789012";
const char kDisableProfileResultHistogram[] =
    "Network.Cellular.ESim.DisableProfile.Result";

std::string CreateTestEuiccPath(int euicc_num) {
  return base::StringPrintf("%s%d", kTestEuiccBasePath, euicc_num);
}

std::string CreateTestEid(int euicc_num) {
  return base::StringPrintf("%s%d", kTestBaseEid, euicc_num);
}

class FakeObserver : public CellularESimProfileHandler::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_updates() const { return num_updates_; }

  // CellularESimProfileHandler::Observer:
  void OnESimProfileListUpdated() override { ++num_updates_; }

 private:
  size_t num_updates_ = 0u;
};

}  // namespace

class CellularESimProfileHandlerImplTest : public testing::Test {
 protected:
  CellularESimProfileHandlerImplTest()
      : helper_(/*use_default_devices_and_services=*/false) {}

  ~CellularESimProfileHandlerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
        device_prefs_.registry());

    cellular_inhibitor_.Init(helper_.network_state_handler(),
                             helper_.network_device_handler());
  }

  void TearDown() override {
    handler_->RemoveObserver(&observer_);
    handler_.reset();
  }

  void Init() {
    if (handler_)
      handler_->RemoveObserver(&observer_);

    handler_ = std::make_unique<CellularESimProfileHandlerImpl>();
    handler_->AddObserver(&observer_);

    handler_->Init(helper_.network_state_handler(), &cellular_inhibitor_);
  }

  void SetDevicePrefs(bool set_to_null = false) {
    handler_->SetDevicePrefs(set_to_null ? nullptr : &device_prefs_);
  }

  void AddEuicc(int euicc_num, bool also_add_to_prefs = true) {
    std::string euicc_path = CreateTestEuiccPath(euicc_num);

    helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(euicc_path), CreateTestEid(euicc_num),
        /*is_active=*/true, /*physical_slot=*/0);
    base::RunLoop().RunUntilIdle();

    if (also_add_to_prefs) {
      base::Value::List euicc_paths_from_prefs = GetEuiccListFromPrefs();
      euicc_paths_from_prefs.Append(euicc_path);
      device_prefs_.Set(prefs::kESimRefreshedEuiccs,
                        base::Value(std::move(euicc_paths_from_prefs)));
    }
  }

  void AddCellularDevice() {
    helper_.device_test()->AddDevice(kDefaultCellularDevicePath,
                                     shill::kTypeCellular, "cellular1");
    // Allow device state changes to propagate to network state handler.
    base::RunLoop().RunUntilIdle();
  }

  dbus::ObjectPath AddProfile(int euicc_num,
                              hermes::profile::State state,
                              const std::string& activation_code,
                              hermes::profile::ProfileClass profile_class =
                                  hermes::profile::ProfileClass::kOperational,
                              bool blank_iccid = false) {
    dbus::ObjectPath path(base::StringPrintf("%s%02d", kTestProfileBasePath,
                                             num_profiles_created_));

    std::string iccid;
    if (!blank_iccid) {
      iccid = base::StringPrintf("%s%02d", "iccid_", num_profiles_created_);
    }
    helper_.hermes_euicc_test()->AddCarrierProfile(
        path, dbus::ObjectPath(CreateTestEuiccPath(euicc_num)), iccid,
        base::StringPrintf("%s%02d", "name_", num_profiles_created_),
        base::StringPrintf("%s%02d", "nickname_", num_profiles_created_),
        base::StringPrintf("%s%02d", "service_provider_",
                           num_profiles_created_),
        activation_code,
        base::StringPrintf("%s%02d", "network_service_path_",
                           num_profiles_created_),
        state, profile_class,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);

    base::RunLoop().RunUntilIdle();

    ++num_profiles_created_;
    return path;
  }

  std::vector<CellularESimProfile> GetESimProfiles() {
    return handler_->GetESimProfiles();
  }

  bool HasAutoRefreshedEuicc(int euicc_num) {
    // Check both variants of HasRefreshedProfilesForEuicc using EID and EUICC
    // Path.
    return handler_->HasRefreshedProfilesForEuicc(CreateTestEid(euicc_num)) &&
           handler_->HasRefreshedProfilesForEuicc(
               dbus::ObjectPath(CreateTestEuiccPath(euicc_num)));
  }

  void DisableActiveESimProfile() { handler_->DisableActiveESimProfile(); }

  size_t NumObserverEvents() const { return observer_.num_updates(); }

  std::unique_ptr<CellularInhibitor::InhibitLock> InhibitCellularScanning() {
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    base::RunLoop inhibit_loop;

    cellular_inhibitor_.InhibitCellularScanning(
        CellularInhibitor::InhibitReason::kRefreshingProfileList,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<CellularInhibitor::InhibitLock> lock) {
              inhibit_lock = std::move(lock);
              inhibit_loop.Quit();
            }));
    inhibit_loop.Run();

    EXPECT_TRUE(inhibit_lock);
    return inhibit_lock;
  }

  void QueueEuiccErrorStatus() {
    helper_.hermes_euicc_test()->QueueHermesErrorStatus(
        HermesResponseStatus::kErrorUnknown);
  }

  void RefreshProfileList(
      int euicc_num,
      CellularESimProfileHandler::RefreshProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock = nullptr) {
    handler_->RefreshProfileList(
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)), std::move(callback),
        std::move(inhibit_lock));
  }

  bool GetLastRefreshProfilesRestoreSlotArg() {
    return helper_.hermes_euicc_test()->GetLastRefreshProfilesRestoreSlotArg();
  }

  base::Value::List GetEuiccListFromPrefs() {
    return device_prefs_.GetList(prefs::kESimRefreshedEuiccs).Clone();
  }

  void SetPSimSlotInfo(const std::string& iccid) {
    base::Value::List sim_slot_infos;
    base::Value::Dict slot_info_item;
    slot_info_item.Set(shill::kSIMSlotInfoEID, std::string());
    slot_info_item.Set(shill::kSIMSlotInfoICCID, iccid);
    slot_info_item.Set(shill::kSIMSlotInfoPrimary, true);
    sim_slot_infos.Append(std::move(slot_info_item));

    helper_.device_test()->SetDeviceProperty(
        kDefaultCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(std::move(sim_slot_infos)),
        /*notify_changed=*/true);
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);
    task_environment_.FastForwardBy(kProfileRefreshCallbackDelay);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NetworkStateTestHelper helper_;
  TestingPrefServiceSimple device_prefs_;
  FakeObserver observer_;

  int num_profiles_created_ = 0;

  CellularInhibitor cellular_inhibitor_;
  std::unique_ptr<CellularESimProfileHandlerImpl> handler_;
};

TEST_F(CellularESimProfileHandlerImplTest, NoEuicc) {
  AddCellularDevice();
  // No EUICCs exist, so no profiles should exist.
  Init();
  EXPECT_TRUE(GetESimProfiles().empty());

  // Set prefs; no profiles should exist.
  SetDevicePrefs();
  EXPECT_TRUE(GetESimProfiles().empty());

  // Unset prefs; no profiles should exist.
  SetDevicePrefs(/*set_to_null=*/true);
  EXPECT_TRUE(GetESimProfiles().empty());

  EXPECT_EQ(0u, NumObserverEvents());
}

TEST_F(CellularESimProfileHandlerImplTest, EuiccWithNoProfiles) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);

  // No profiles were added to the EUICC.
  Init();
  EXPECT_TRUE(GetESimProfiles().empty());

  // Set prefs; no profiles should exist.
  SetDevicePrefs();
  EXPECT_TRUE(GetESimProfiles().empty());

  // Unset prefs; no profiles should exist.
  SetDevicePrefs(/*set_to_null=*/true);
  EXPECT_TRUE(GetESimProfiles().empty());

  EXPECT_EQ(0u, NumObserverEvents());
}

TEST_F(CellularESimProfileHandlerImplTest, EuiccWithProfiles) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);

  // Add two normal (i.e., kOperational) profiles.
  dbus::ObjectPath path1 = AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kPending,
      /*activation_code=*/"code1");
  dbus::ObjectPath path2 = AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kActive,
      /*activation_code=*/"code2");

  // Add one kTesting and one kProvisioning profile. These profiles should not
  // be ignored if they are returned from Hermes.
  AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kInactive,
      /*activation_code=*/"code3", hermes::profile::ProfileClass::kTesting);
  AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kInactive,
      /*activation_code=*/"code4",
      hermes::profile::ProfileClass::kProvisioning);

  // Prefs not yet set.
  Init();
  EXPECT_TRUE(GetESimProfiles().empty());

  // Set prefs; the profiles added should be available.
  SetDevicePrefs();
  EXPECT_EQ(1u, NumObserverEvents());

  std::vector<CellularESimProfile> profiles = GetESimProfiles();
  EXPECT_EQ(4u, profiles.size());
  EXPECT_EQ(CellularESimProfile::State::kPending, profiles[0].state());
  EXPECT_EQ("code1", profiles[0].activation_code());
  EXPECT_EQ(CellularESimProfile::State::kActive, profiles[1].state());
  EXPECT_EQ("code2", profiles[1].activation_code());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[2].state());
  EXPECT_EQ("code3", profiles[2].activation_code());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[3].state());
  EXPECT_EQ("code4", profiles[3].activation_code());

  // Update profile properties; GetESimProfiles() should return the new values.
  HermesProfileClient::Properties* profile_properties1 =
      HermesProfileClient::Get()->GetProperties(dbus::ObjectPath(path1));
  profile_properties1->state().ReplaceValue(hermes::profile::kInactive);
  HermesProfileClient::Properties* profile_properties2 =
      HermesProfileClient::Get()->GetProperties(dbus::ObjectPath(path2));
  profile_properties2->state().ReplaceValue(hermes::profile::kPending);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, NumObserverEvents());

  profiles = GetESimProfiles();
  EXPECT_EQ(4u, profiles.size());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[0].state());
  EXPECT_EQ(CellularESimProfile::State::kPending, profiles[1].state());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[2].state());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[3].state());

  // Unset prefs; no profiles should exist.
  SetDevicePrefs(/*set_to_null=*/true);
  EXPECT_TRUE(GetESimProfiles().empty());
}

TEST_F(CellularESimProfileHandlerImplTest, Persistent) {
  AddCellularDevice();
  Init();
  SetDevicePrefs();
  EXPECT_TRUE(GetESimProfiles().empty());

  // Add a EUICC and profile; should be available.
  AddEuicc(/*euicc_num=*/1);
  AddProfile(/*euicc_num=*/1, hermes::profile::State::kInactive,
             /*activation_code=*/"code1");
  EXPECT_EQ(1u, GetESimProfiles().size());
  EXPECT_EQ(1u, NumObserverEvents());

  // Delete the old handler and create a new one; the new one will end up using
  // the same PrefService as the old one.
  Init();

  // Remove EUICC; this simulates a temporary state at startup when Hermes would
  // not yet have provided EUICC information.
  HermesEuiccClient::Get()->GetTestInterface()->ClearEuicc(
      dbus::ObjectPath(CreateTestEuiccPath(/*euicc_num=*/1)));

  // Set prefs; the handler should read from the old prefs and should still have
  // a profile available.
  SetDevicePrefs();
  EXPECT_EQ(1u, GetESimProfiles().size());

  // Now, refresh the list.
  base::RunLoop run_loop;
  RefreshProfileList(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            EXPECT_TRUE(inhibit_lock);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_FALSE(GetLastRefreshProfilesRestoreSlotArg());

  // Because the list was refreshed, we now expect GetESimProfiles() to return
  // an empty list.
  EXPECT_TRUE(GetESimProfiles().empty());
}

TEST_F(CellularESimProfileHandlerImplTest,
       RefreshProfileList_AcquireLockInterally) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);

  Init();
  SetDevicePrefs();

  base::RunLoop run_loop;
  RefreshProfileList(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            EXPECT_TRUE(inhibit_lock);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_FALSE(GetLastRefreshProfilesRestoreSlotArg());
}

TEST_F(CellularESimProfileHandlerImplTest,
       RefreshProfileList_ProvideAlreadyAcquiredLock) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);

  Init();
  SetDevicePrefs();

  std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock =
      InhibitCellularScanning();

  base::RunLoop run_loop;
  RefreshProfileList(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            EXPECT_TRUE(inhibit_lock);
            run_loop.Quit();
          }),
      std::move(inhibit_lock));
  run_loop.Run();
  EXPECT_FALSE(GetLastRefreshProfilesRestoreSlotArg());
}

TEST_F(CellularESimProfileHandlerImplTest, RefreshProfileList_Failure) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);

  Init();
  SetDevicePrefs();

  QueueEuiccErrorStatus();

  base::RunLoop run_loop;
  RefreshProfileList(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            // Failures are indicated via a null return value.
            EXPECT_FALSE(inhibit_lock);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_FALSE(GetLastRefreshProfilesRestoreSlotArg());
}

TEST_F(CellularESimProfileHandlerImplTest,
       RefreshProfileList_MultipleSimultaneousRequests) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);

  Init();
  SetDevicePrefs();

  base::RunLoop run_loop1;
  RefreshProfileList(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            EXPECT_TRUE(inhibit_lock);
            run_loop1.Quit();
          }));

  base::RunLoop run_loop2;
  RefreshProfileList(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            EXPECT_TRUE(inhibit_lock);
            run_loop2.Quit();
          }));

  run_loop1.Run();
  EXPECT_FALSE(GetLastRefreshProfilesRestoreSlotArg());
  run_loop2.Run();
  EXPECT_FALSE(GetLastRefreshProfilesRestoreSlotArg());
}

TEST_F(CellularESimProfileHandlerImplTest,
       RefreshesAutomaticallyWhenNotSeenBefore) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1, /*also_add_to_prefs=*/false);

  Init();
  base::Value::List euicc_paths_from_prefs = GetEuiccListFromPrefs();
  EXPECT_TRUE(euicc_paths_from_prefs.empty());

  // Set device prefs; a new auto-refresh should have started but not yet
  // completed.
  SetDevicePrefs();
  euicc_paths_from_prefs = GetEuiccListFromPrefs();
  EXPECT_TRUE(euicc_paths_from_prefs.empty());
  EXPECT_FALSE(HasAutoRefreshedEuicc(/*euicc_num=*/1));

  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  euicc_paths_from_prefs = GetEuiccListFromPrefs();
  EXPECT_EQ(1u, euicc_paths_from_prefs.size());
  EXPECT_EQ(CreateTestEuiccPath(/*euicc_num=*/1),
            euicc_paths_from_prefs[0].GetString());
  EXPECT_TRUE(HasAutoRefreshedEuicc(/*euicc_num=*/1));
  EXPECT_TRUE(GetLastRefreshProfilesRestoreSlotArg());
}

TEST_F(CellularESimProfileHandlerImplTest, IgnoresESimProfilesWithNoIccid) {
  const char kTestIccid[] = "1245671234567";
  AddEuicc(/*euicc_num=*/1, /*also_add_to_prefs=*/false);
  Init();
  SetDevicePrefs();

  // Verify that no profiles are added if there are some profiles that have
  // not received iccid updates yet.
  dbus::ObjectPath profile_path1 = AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kInactive,
      /*activation_code=*/std::string(),
      hermes::profile::ProfileClass::kOperational,
      /*blank_iccid=*/true);
  dbus::ObjectPath profile_path2 = AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kInactive,
      /*activation_code=*/std::string(),
      hermes::profile::ProfileClass::kOperational,
      /*blank_iccid=*/false);
  EXPECT_TRUE(GetESimProfiles().empty());

  // Verify that profile object is created after iccid property is set.
  HermesProfileClient::Properties* properties1 =
      HermesProfileClient::Get()->GetProperties(profile_path1);
  properties1->iccid().ReplaceValue(kTestIccid);
  base::RunLoop().RunUntilIdle();

  std::vector<CellularESimProfile> esim_profiles = GetESimProfiles();
  EXPECT_EQ(2u, esim_profiles.size());
  EXPECT_EQ(kTestIccid, esim_profiles[0].iccid());
}

TEST_F(CellularESimProfileHandlerImplTest,
       SkipsAutomaticRefreshIfNoCellularDevice) {
  Init();
  AddEuicc(/*euicc_num=*/1, /*also_add_to_prefs=*/false);
  SetDevicePrefs();

  // Verify that no EUICCs exist in pref.
  base::Value::List euicc_paths_from_prefs = GetEuiccListFromPrefs();
  EXPECT_TRUE(euicc_paths_from_prefs.empty());

  // Verify that EUICCs are refreshed after the cellular device is added.
  AddCellularDevice();
  FastForwardProfileRefreshDelay();
  euicc_paths_from_prefs = GetEuiccListFromPrefs();
  EXPECT_EQ(1u, euicc_paths_from_prefs.size());
  EXPECT_EQ(CreateTestEuiccPath(/*euicc_num=*/1),
            euicc_paths_from_prefs[0].GetString());
}

TEST_F(CellularESimProfileHandlerImplTest, DisableActiveESimProfile) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  Init();
  SetDevicePrefs();
  base::HistogramTester histogram_tester;
  // Add one active profile and another inactive profiles.
  AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kActive,
      /*activation_code=*/std::string());
  AddProfile(
      /*euicc_num=*/1, hermes::profile::State::kInactive,
      /*activation_code=*/std::string());
  std::vector<CellularESimProfile> profiles = GetESimProfiles();
  EXPECT_EQ(2u, profiles.size());
  EXPECT_EQ(CellularESimProfile::State::kActive, profiles[0].state());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[1].state());
  DisableActiveESimProfile();

  // Now, refresh the list.
  base::RunLoop run_loop;
  RefreshProfileList(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            EXPECT_TRUE(inhibit_lock);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_FALSE(GetLastRefreshProfilesRestoreSlotArg());

  profiles = GetESimProfiles();
  EXPECT_EQ(2u, profiles.size());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[0].state());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[1].state());
  histogram_tester.ExpectBucketCount(kDisableProfileResultHistogram,
                                     HermesResponseStatus::kSuccess,
                                     /*expected_count=*/1);
}

}  // namespace ash
