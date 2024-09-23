// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_test_helper.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ash::cellular_setup::mojom::ESimOperationResult;

namespace ash {
namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";
const char kTestEuiccBasePath[] = "/org/chromium/Hermes/Euicc/";
const char kTestProfileBasePath[] = "/org/chromium/Hermes/Profile/";
const char kTestBaseEid[] = "12345678901234567890123456789012";
const char kDisableProfileResultHistogram[] =
    "Network.Cellular.ESim.DisableProfile.Result";

constexpr base::TimeDelta kInteractiveDelay = base::Seconds(30);

std::string CreateTestProfilePath(int profile_num) {
  return base::StringPrintf("%s%02d", kTestProfileBasePath, profile_num);
}

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
 public:
  CellularESimProfileHandlerImplTest(
      const CellularESimProfileHandlerImplTest&) = delete;
  CellularESimProfileHandlerImplTest& operator=(
      const CellularESimProfileHandlerImplTest&) = delete;

 protected:
  explicit CellularESimProfileHandlerImplTest()
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
    dbus::ObjectPath path(CreateTestProfilePath(num_profiles_created_));

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

  void SetErrorForNextSetPropertyAttempt(const std::string& error_name) {
    helper_.device_test()->SetErrorForNextSetPropertyAttempt(error_name);
    base::RunLoop().RunUntilIdle();
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

  std::optional<CellularInhibitor::InhibitReason> GetInhibitReason() {
    return cellular_inhibitor_.GetInhibitReason();
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

  void RequestAvailableProfiles(
      int euicc_num,
      CellularESimProfileHandler::RequestAvailableProfilesCallback callback) {
    handler_->RequestAvailableProfiles(
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)), std::move(callback));
  }

  bool GetLastRefreshProfilesRestoreSlotArg() {
    return helper_.hermes_euicc_test()->GetLastRefreshProfilesRestoreSlotArg();
  }

  base::Value::List GetEuiccListFromPrefs() {
    return device_prefs_.GetList(prefs::kESimRefreshedEuiccs).Clone();
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
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);
    task_environment_.FastForwardBy(kProfileRefreshCallbackDelay);
  }

  void ExpectScanDurationMetricsCount(size_t other_count,
                                      size_t android_count,
                                      size_t gsma_count,
                                      bool success) {
    if (success) {
      histogram_tester()->ExpectTotalCount(
          CellularNetworkMetricsLogger::kSmdsScanOtherDurationSuccess,
          other_count);
      histogram_tester()->ExpectTotalCount(
          CellularNetworkMetricsLogger::kSmdsScanAndroidDurationSuccess,
          android_count);
      histogram_tester()->ExpectTotalCount(
          CellularNetworkMetricsLogger::kSmdsScanGsmaDurationSuccess,
          gsma_count);
      return;
    }

    histogram_tester()->ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanOtherDurationFailure,
        other_count);
    histogram_tester()->ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanAndroidDurationFailure,
        android_count);
    histogram_tester()->ExpectTotalCount(
        CellularNetworkMetricsLogger::kSmdsScanGsmaDurationFailure, gsma_count);
  }

  const base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::HistogramTester histogram_tester_;
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
  dbus::ObjectPath path3 = AddProfile(
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
  EXPECT_EQ(3u, profiles.size());
  EXPECT_EQ(CellularESimProfile::State::kActive, profiles[0].state());
  EXPECT_EQ("code2", profiles[0].activation_code());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[1].state());
  EXPECT_EQ("code3", profiles[1].activation_code());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[2].state());
  EXPECT_EQ("code4", profiles[2].activation_code());

  // Update profile properties; GetESimProfiles() should return the new values.
  HermesProfileClient::Properties* profile_properties1 =
      HermesProfileClient::Get()->GetProperties(dbus::ObjectPath(path1));
  profile_properties1->state().ReplaceValue(hermes::profile::kInactive);
  HermesProfileClient::Properties* profile_properties2 =
      HermesProfileClient::Get()->GetProperties(dbus::ObjectPath(path2));
  profile_properties2->state().ReplaceValue(hermes::profile::kPending);
  HermesProfileClient::Properties* profile_properties3 =
      HermesProfileClient::Get()->GetProperties(dbus::ObjectPath(path3));
  profile_properties3->state().ReplaceValue(hermes::profile::kActive);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, NumObserverEvents());

  profiles = GetESimProfiles();
  EXPECT_EQ(3u, profiles.size());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[0].state());
  EXPECT_EQ("code1", profiles[0].activation_code());
  EXPECT_EQ(CellularESimProfile::State::kActive, profiles[1].state());
  EXPECT_EQ("code3", profiles[1].activation_code());
  EXPECT_EQ(CellularESimProfile::State::kInactive, profiles[2].state());
  EXPECT_EQ("code4", profiles[2].activation_code());

  // Unset prefs; no profiles should exist.
  SetDevicePrefs(/*set_to_null=*/true);
  EXPECT_TRUE(GetESimProfiles().empty());

  // Set prefs again; the profiles fetched should match the ones we have already
  // cached and should not trigger an update.
  SetDevicePrefs();
  EXPECT_EQ(2u, NumObserverEvents());
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
  AddCellularDevice();
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
  histogram_tester()->ExpectBucketCount(kDisableProfileResultHistogram,
                                        HermesResponseStatus::kSuccess,
                                        /*expected_count=*/1);
}

TEST_F(CellularESimProfileHandlerImplTest, RequestAvailableProfiles) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  Init();
  SetDevicePrefs();

  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      kInteractiveDelay);

  std::optional<ESimOperationResult> result;
  std::optional<std::vector<CellularESimProfile>> profile_list;

  cellular_metrics::ESimSmdsScanHistogramState histogram_state;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/true);

  base::RunLoop run_loop;
  RequestAvailableProfiles(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](ESimOperationResult returned_result,
              std::vector<CellularESimProfile> returned_profile_list) {
            result = returned_result;
            profile_list = returned_profile_list;
            run_loop.Quit();
          }));

  task_environment()->FastForwardBy(kInteractiveDelay);

  const std::optional<CellularInhibitor::InhibitReason> inhibit_reason =
      GetInhibitReason();
  ASSERT_TRUE(inhibit_reason);
  EXPECT_EQ(inhibit_reason,
            CellularInhibitor::InhibitReason::kRequestingAvailableProfiles);

  EXPECT_FALSE(profile_list.has_value());

  task_environment()->FastForwardBy(kInteractiveDelay);
  run_loop.Run();

  EXPECT_FALSE(GetInhibitReason());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, cellular_setup::mojom::ESimOperationResult::kSuccess);

  histogram_state.smds_scan_android_user_errors_filtered.success_count++;
  histogram_state.smds_scan_android_user_errors_included.success_count++;
  histogram_state.smds_scan_gsma_user_errors_filtered.success_count++;
  histogram_state.smds_scan_gsma_user_errors_included.success_count++;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/1,
                                 /*gsma_counts=*/1, /*success=*/true);

  const std::vector<std::string> smds_activation_codes =
      cellular_utils::GetSmdsActivationCodes();

  ASSERT_TRUE(profile_list.has_value());
  EXPECT_EQ(smds_activation_codes.size(), profile_list->size());

  for (const auto& profile : *profile_list) {
    EXPECT_NE(std::find(smds_activation_codes.begin(),
                        smds_activation_codes.end(), profile.activation_code()),
              smds_activation_codes.end());
  }

  histogram_tester()->ExpectTotalCount(
      CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount,
      /*expected_count=*/1);
  EXPECT_EQ(static_cast<int64_t>(smds_activation_codes.size()),
            histogram_tester()->GetTotalSum(
                CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount));
}

TEST_F(CellularESimProfileHandlerImplTest,
       RequestAvailableProfiles_SuccessfulDespiteHermesErrors) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  Init();
  SetDevicePrefs();

  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      kInteractiveDelay);

  std::optional<ESimOperationResult> result;
  std::optional<std::vector<CellularESimProfile>> profile_list;

  cellular_metrics::ESimSmdsScanHistogramState histogram_state;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/false);

  // Queue errors for each of the expected SM-DS scans.
  const std::vector<std::string> smds_activation_codes =
      cellular_utils::GetSmdsActivationCodes();
  for (size_t i = 0; i < smds_activation_codes.size(); ++i) {
    HermesEuiccClient::Get()->GetTestInterface()->QueueHermesErrorStatus(
        HermesResponseStatus::kErrorUnknown);
  }

  base::RunLoop run_loop;
  RequestAvailableProfiles(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](ESimOperationResult returned_result,
              std::vector<CellularESimProfile> returned_profile_list) {
            result = returned_result;
            profile_list = returned_profile_list;
            run_loop.Quit();
          }));

  // Skip forward until all SM-DS scans are completed.
  for (size_t i = 0; i < smds_activation_codes.size(); ++i) {
    task_environment()->FastForwardBy(kInteractiveDelay);
    base::RunLoop().RunUntilIdle();
  }
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, cellular_setup::mojom::ESimOperationResult::kSuccess);

  histogram_state.smds_scan_android_user_errors_filtered.hermes_failed_count++;
  histogram_state.smds_scan_android_user_errors_included.hermes_failed_count++;
  histogram_state.smds_scan_gsma_user_errors_filtered.hermes_failed_count++;
  histogram_state.smds_scan_gsma_user_errors_included.hermes_failed_count++;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/1,
                                 /*gsma_counts=*/1, /*success=*/false);

  ASSERT_TRUE(profile_list.has_value());
  EXPECT_EQ(0u, profile_list->size());
}

TEST_F(CellularESimProfileHandlerImplTest,
       RequestAvailableProfiles_WaitForProfileProperties) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  Init();
  SetDevicePrefs();

  base::HistogramTester histogram_tester;

  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      kInteractiveDelay);

  const dbus::ObjectPath euicc_path(CreateTestEuiccPath(1));
  const std::vector<std::string> smds_activation_codes =
      cellular_utils::GetSmdsActivationCodes();

  // Create profiles with activation codes that match all of the activation
  // codes that will be used to perform SM-DS scans. These profiles will have an
  // activation code, but are missing a name and have an incorrect state. These
  // properties should be set before RequestAvailableProfiles() returns.
  std::vector<dbus::ObjectPath> profile_paths;
  for (const auto& smds_activation_code : smds_activation_codes) {
    const dbus::ObjectPath profile_path(
        CreateTestProfilePath(profile_paths.size()));
    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        profile_path, euicc_path,
        /*iccid=*/"",
        /*name=*/"",
        /*nickname=*/"",
        /*service_provider=*/"",
        /*activation_code=*/smds_activation_code,
        /*network_service_path=*/"",
        /*state=*/hermes::profile::State::kPending,
        /*profile_class=*/hermes::profile::ProfileClass::kOperational,
        /*add_carrier_profile_behavior=*/
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();

    profile_paths.push_back(profile_path);
  }

  std::optional<ESimOperationResult> result;
  std::optional<std::vector<CellularESimProfile>> profile_list;

  base::RunLoop run_loop;
  RequestAvailableProfiles(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](ESimOperationResult returned_result,
              std::vector<CellularESimProfile> returned_profile_list) {
            result = returned_result;
            profile_list = returned_profile_list;
            run_loop.Quit();
          }));
  // Skip forward the amount of time needed to complete all of the SM-DS scans.
  for (size_t i = 0; i < smds_activation_codes.size(); ++i) {
    task_environment()->FastForwardBy(kInteractiveDelay);
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(profile_list.has_value());

  for (const auto& profile_path : profile_paths) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    ASSERT_TRUE(profile_properties);
    profile_properties->state().ReplaceValue(
        /*value=*/hermes::profile::State::kPending);
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(profile_list.has_value());

  for (const auto& profile_path : profile_paths) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    ASSERT_TRUE(profile_properties);
    profile_properties->name().ReplaceValue(
        /*value=*/"name");
  }
  run_loop.Run();
  ASSERT_TRUE(profile_list.has_value());
  EXPECT_EQ(smds_activation_codes.size(), profile_list->size());

  histogram_tester.ExpectTotalCount(
      CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount,
      /*expected_count=*/1);
  EXPECT_EQ(static_cast<int64_t>(smds_activation_codes.size()),
            histogram_tester.GetTotalSum(
                CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount));
}

TEST_F(CellularESimProfileHandlerImplTest,
       RequestAvailableProfiles_FailToInhibit) {
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  Init();
  SetDevicePrefs();

  // The cellular device is inhibited by setting a device property. Simulate a
  // failure to inhibit by making the next attempt to set a property fail.
  SetErrorForNextSetPropertyAttempt("error_name");

  cellular_metrics::ESimSmdsScanHistogramState histogram_state;
  histogram_state.Check(histogram_tester());

  std::optional<ESimOperationResult> result;
  std::optional<std::vector<CellularESimProfile>> profile_list;

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/true);

  {
    base::RunLoop run_loop;
    RequestAvailableProfiles(
        /*euicc_num=*/1,
        base::BindLambdaForTesting(
            [&](ESimOperationResult returned_result,
                std::vector<CellularESimProfile> returned_profile_list) {
              result = returned_result;
              profile_list = returned_profile_list;
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, cellular_setup::mojom::ESimOperationResult::kFailure);

  ASSERT_TRUE(profile_list.has_value());
  EXPECT_TRUE(profile_list->empty());

  histogram_tester()->ExpectTotalCount(
      CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount,
      /*expected_count=*/0);
  EXPECT_EQ(0, histogram_tester()->GetTotalSum(
                   CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount));

  histogram_state.smds_scan_android_user_errors_filtered.inhibit_failed_count++;
  histogram_state.smds_scan_android_user_errors_included.inhibit_failed_count++;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/true);

  {
    base::RunLoop run_loop;
    RequestAvailableProfiles(
        /*euicc_num=*/1,
        base::BindLambdaForTesting(
            [&](ESimOperationResult returned_result,
                std::vector<CellularESimProfile> returned_profile_list) {
              result = returned_result;
              profile_list = returned_profile_list;
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  EXPECT_EQ(result, cellular_setup::mojom::ESimOperationResult::kSuccess);
  EXPECT_FALSE(profile_list->empty());

  histogram_state.smds_scan_android_user_errors_filtered.success_count++;
  histogram_state.smds_scan_android_user_errors_included.success_count++;
  histogram_state.smds_scan_gsma_user_errors_filtered.success_count++;
  histogram_state.smds_scan_gsma_user_errors_included.success_count++;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/1,
                                 /*gsma_counts=*/1, /*success=*/true);

  histogram_tester()->ExpectTotalCount(
      CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount,
      /*expected_count=*/1);
  EXPECT_EQ(
      static_cast<int64_t>(cellular_utils::GetSmdsActivationCodes().size()),
      histogram_tester()->GetTotalSum(
          CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount));
}

TEST_F(CellularESimProfileHandlerImplTest,
       RequestAvailableProfiles_StorkEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kUseStorkSmdsServerAddress);
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  Init();
  SetDevicePrefs();

  std::optional<ESimOperationResult> result;
  std::optional<std::vector<CellularESimProfile>> profile_list;

  cellular_metrics::ESimSmdsScanHistogramState histogram_state;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/true);

  base::RunLoop run_loop;
  RequestAvailableProfiles(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](ESimOperationResult returned_result,
              std::vector<CellularESimProfile> returned_profile_list) {
            result = returned_result;
            profile_list = returned_profile_list;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, cellular_setup::mojom::ESimOperationResult::kSuccess);

  const std::vector<std::string> smds_activation_codes =
      cellular_utils::GetSmdsActivationCodes();

  ASSERT_TRUE(profile_list.has_value());
  ASSERT_EQ(1u, smds_activation_codes.size());
  ASSERT_EQ(smds_activation_codes.size(), profile_list->size());
  EXPECT_EQ(smds_activation_codes.front(),
            profile_list->front().activation_code());

  histogram_state.smds_scan_other_user_errors_filtered.success_count++;
  histogram_state.smds_scan_other_user_errors_included.success_count++;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/1, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/true);

  histogram_tester()->ExpectTotalCount(
      CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount,
      /*expected_count=*/1);
  EXPECT_EQ(static_cast<int64_t>(smds_activation_codes.size()),
            histogram_tester()->GetTotalSum(
                CellularNetworkMetricsLogger::kSmdsScanViaUserProfileCount));
}

TEST_F(CellularESimProfileHandlerImplTest,
       RequestAvailableProfiles_SuccessfulDespiteHermesErrors_StorkEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kUseStorkSmdsServerAddress);
  AddCellularDevice();
  AddEuicc(/*euicc_num=*/1);
  Init();
  SetDevicePrefs();

  std::optional<ESimOperationResult> result;
  std::optional<std::vector<CellularESimProfile>> profile_list;

  cellular_metrics::ESimSmdsScanHistogramState histogram_state;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/0, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/false);

  // Queue errors for each of the expected SM-DS scans.
  const std::vector<std::string> smds_activation_codes =
      cellular_utils::GetSmdsActivationCodes();
  for (size_t i = 0; i < smds_activation_codes.size(); ++i) {
    HermesEuiccClient::Get()->GetTestInterface()->QueueHermesErrorStatus(
        HermesResponseStatus::kErrorUnknown);
  }

  base::RunLoop run_loop;
  RequestAvailableProfiles(
      /*euicc_num=*/1,
      base::BindLambdaForTesting(
          [&](ESimOperationResult returned_result,
              std::vector<CellularESimProfile> returned_profile_list) {
            result = returned_result;
            profile_list = returned_profile_list;
            run_loop.Quit();
          }));

  // Skip forward until all SM-DS scans are completed.
  for (size_t i = 0; i < smds_activation_codes.size(); ++i) {
    task_environment()->FastForwardBy(kInteractiveDelay);
    base::RunLoop().RunUntilIdle();
  }
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, cellular_setup::mojom::ESimOperationResult::kSuccess);

  histogram_state.smds_scan_other_user_errors_filtered.hermes_failed_count++;
  histogram_state.smds_scan_other_user_errors_included.hermes_failed_count++;
  histogram_state.Check(histogram_tester());

  ExpectScanDurationMetricsCount(/*other_count=*/1, /*android_count=*/0,
                                 /*gsma_counts=*/0, /*success=*/false);

  ASSERT_TRUE(profile_list.has_value());
  EXPECT_EQ(0u, profile_list->size());
}

}  // namespace ash
