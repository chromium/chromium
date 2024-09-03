// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kName0[] = "cellular0";
constexpr char kName1[] = "cellular1";
constexpr char kIccid0[] = "0000000000000000000";
constexpr char kIccid1[] = "1111111111111111111";
constexpr char kActivationCode0[] = "LPA:1$ActivationCode0$MatchingId";

class FakeObserver : public ManagedCellularPrefHandler::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  int change_count() const { return change_count_; }

  // ManagedCellularPref::Observer:
  void OnManagedCellularPrefChanged() override { ++change_count_; }

 private:
  int change_count_ = 0u;
};

}  // namespace

class ManagedCellularPrefHandlerTest : public testing::Test {
 protected:
  ManagedCellularPrefHandlerTest() = default;
  ~ManagedCellularPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ManagedCellularPrefHandler::RegisterLocalStatePrefs(
        device_prefs_.registry());
  }

  void TearDown() override {
    managed_cellular_pref_handler_->RemoveObserver(&observer_);
    managed_cellular_pref_handler_.reset();
  }

  void Init() {
    if (managed_cellular_pref_handler_ &&
        managed_cellular_pref_handler_->HasObserver(&observer_)) {
      managed_cellular_pref_handler_->RemoveObserver(&observer_);
    }
    managed_cellular_pref_handler_ =
        std::make_unique<ManagedCellularPrefHandler>();
    managed_cellular_pref_handler_->AddObserver(&observer_);
    managed_cellular_pref_handler_->Init(helper_.network_state_handler());
  }

  void SetDevicePrefs(bool set_to_null = false) {
    managed_cellular_pref_handler_->SetDevicePrefs(
        set_to_null ? nullptr : &device_prefs_);
  }

  void AddApnMigratedIccid(const std::string& iccid) {
    managed_cellular_pref_handler_->AddApnMigratedIccid(iccid);
  }

  bool ContainsApnMigratedIccid(const std::string& iccid) {
    return managed_cellular_pref_handler_->ContainsApnMigratedIccid(iccid);
  }

  void ExpectESimMetadata(
      const char* expected_iccid,
      const char* expected_name,
      const policy_util::SmdxActivationCode& expected_activation_code) {
    const base::Value::Dict* esim_metadata =
        managed_cellular_pref_handler_->GetESimMetadata(expected_iccid);
    ASSERT_TRUE(esim_metadata);

    const std::string* name =
        esim_metadata->FindString(::onc::network_config::kName);
    ASSERT_TRUE(name);
    EXPECT_EQ(expected_name, *name);

    const std::string* activation_code = esim_metadata->FindString(
        expected_activation_code.type() ==
                policy_util::SmdxActivationCode::Type::SMDP
            ? ::onc::cellular::kSMDPAddress
            : ::onc::cellular::kSMDSAddress);
    ASSERT_TRUE(activation_code);
    EXPECT_EQ(expected_activation_code.value(), *activation_code);
  }

  int NumObserverEvents() { return observer_.change_count(); }

  ManagedCellularPrefHandler* managed_cellular_pref_handler() {
    return managed_cellular_pref_handler_.get();
  }

  TestingPrefServiceSimple* device_prefs() { return &device_prefs_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/false};
  TestingPrefServiceSimple device_prefs_;
  FakeObserver observer_;

  std::unique_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_;
};

TEST_F(ManagedCellularPrefHandlerTest, AddAndRemoveESimMetadata) {
  Init();
  SetDevicePrefs();

  const policy_util::SmdxActivationCode smdp_activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());
  const policy_util::SmdxActivationCode smds_activation_code(
      policy_util::SmdxActivationCode::Type::SMDS,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  EXPECT_EQ(0, NumObserverEvents());
  EXPECT_FALSE(managed_cellular_pref_handler()->GetESimMetadata(kIccid0));

  managed_cellular_pref_handler()->AddESimMetadata(kIccid0, kName0,
                                                   smdp_activation_code);
  EXPECT_EQ(1, NumObserverEvents());

  ExpectESimMetadata(kIccid0, kName0, smdp_activation_code);

  // When there are not any differences between the existing metadata and the
  // metadata we are trying to add we don't notify observers.
  managed_cellular_pref_handler()->AddESimMetadata(kIccid0, kName0,
                                                   smdp_activation_code);
  EXPECT_EQ(1, NumObserverEvents());

  managed_cellular_pref_handler()->AddESimMetadata(kIccid0, kName1,
                                                   smdp_activation_code);
  EXPECT_EQ(2, NumObserverEvents());

  ExpectESimMetadata(kIccid0, kName1, smdp_activation_code);

  managed_cellular_pref_handler()->AddESimMetadata(kIccid0, kName1,
                                                   smds_activation_code);
  EXPECT_EQ(3, NumObserverEvents());

  ExpectESimMetadata(kIccid0, kName1, smds_activation_code);

  managed_cellular_pref_handler()->RemoveESimMetadata(kIccid0);
  EXPECT_EQ(4, NumObserverEvents());

  // When the metadata does not exist we should not notify observers.
  managed_cellular_pref_handler()->RemoveESimMetadata(kIccid0);
  EXPECT_EQ(4, NumObserverEvents());
}

TEST_F(ManagedCellularPrefHandlerTest, AddAndRemovePolicyESimMetadata) {
  Init();
  SetDevicePrefs();

  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  EXPECT_EQ(0, NumObserverEvents());
  EXPECT_FALSE(managed_cellular_pref_handler()->GetESimMetadata(kIccid0));

  managed_cellular_pref_handler()->AddESimMetadata(kIccid0, kName0,
                                                   activation_code);
  EXPECT_EQ(1, NumObserverEvents());

  ExpectESimMetadata(kIccid0, kName0, activation_code);
  EXPECT_TRUE(managed_cellular_pref_handler()->IsESimManaged(kIccid0));

  // Reach into the prefs and manually erase the field used to communicate that
  // an eSIM was installed by policy but is no longer managed.
  base::Value::Dict prefs =
      device_prefs()->GetDict(prefs::kManagedCellularESimMetadata).Clone();
  ASSERT_TRUE(prefs.contains(kIccid0));
  base::Value::Dict* esim_metadata = prefs.FindDict(kIccid0);
  esim_metadata->Remove("PolicyMissing");
  device_prefs()->SetDict(prefs::kManagedCellularESimMetadata,
                          std::move(prefs));

  // The eSIM metadata should still be considered managed even if the "policy
  // missing" key is not found.
  EXPECT_TRUE(managed_cellular_pref_handler()->IsESimManaged(kIccid0));

  managed_cellular_pref_handler()->SetPolicyMissing(kIccid0);
  EXPECT_EQ(2, NumObserverEvents());

  ExpectESimMetadata(kIccid0, kName0, activation_code);
  EXPECT_FALSE(managed_cellular_pref_handler()->IsESimManaged(kIccid0));

  managed_cellular_pref_handler()->AddESimMetadata(kIccid0, kName0,
                                                   activation_code);
  EXPECT_EQ(3, NumObserverEvents());

  // Whenever metadata is added it should always flag the eSIM as being managed.
  ExpectESimMetadata(kIccid0, kName0, activation_code);
  EXPECT_TRUE(managed_cellular_pref_handler()->IsESimManaged(kIccid0));
}

TEST_F(ManagedCellularPrefHandlerTest, AddApnMigratedIccid) {
  Init();
  SetDevicePrefs();

  EXPECT_FALSE(ContainsApnMigratedIccid(kIccid0));

  // Add APN migrated ICCIDs to pref and verify that the prefs store these
  // values.
  AddApnMigratedIccid(kIccid0);
  EXPECT_EQ(0, NumObserverEvents());
  EXPECT_TRUE(ContainsApnMigratedIccid(kIccid0));
  EXPECT_FALSE(ContainsApnMigratedIccid(kIccid1));

  AddApnMigratedIccid(kIccid1);
  EXPECT_EQ(0, NumObserverEvents());
  EXPECT_TRUE(ContainsApnMigratedIccid(kIccid0));
  EXPECT_TRUE(ContainsApnMigratedIccid(kIccid1));
}

TEST_F(ManagedCellularPrefHandlerTest, NoDevicePrefSet) {
  Init();
  SetDevicePrefs(/*set_to_null=*/true);

  // Verify that metadata cannot be added, removed, or accessed when there are
  // no device prefs.
  const policy_util::SmdxActivationCode activation_code(
      policy_util::SmdxActivationCode::Type::SMDP,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode());

  EXPECT_EQ(0, NumObserverEvents());

  managed_cellular_pref_handler()->AddESimMetadata(kIccid0, kName0,
                                                   activation_code);
  managed_cellular_pref_handler()->RemoveESimMetadata(kIccid0);
  EXPECT_EQ(0, NumObserverEvents());

  EXPECT_FALSE(managed_cellular_pref_handler()->GetESimMetadata(kIccid0));

  // Verify that APN migration information can be added or accessed when there
  // are no device prefs.
  EXPECT_FALSE(ContainsApnMigratedIccid(kIccid0));
  AddApnMigratedIccid(kIccid0);
  EXPECT_EQ(0, NumObserverEvents());
  EXPECT_FALSE(ContainsApnMigratedIccid(kIccid0));
}

TEST_F(ManagedCellularPrefHandlerTest,
       IccidSmdpPairMigration_MigrationHappensOnce) {
  Init();

  // The value that we will set the existing/pre-migration prefs to.
  auto existing_prefs = base::Value::Dict().Set(kIccid0, kActivationCode0);
  device_prefs()->Set(prefs::kManagedCellularIccidSmdpPair,
                      base::Value(existing_prefs.Clone()));

  // Set the pref to some arbitrary value since we just want to confirm that if
  // the pref has a value we will assume that we have already performed the
  // migration and will not attempt another migration.
  base::Value::Dict new_prefs;
  device_prefs()->Set(prefs::kManagedCellularESimMetadata,
                      base::Value(new_prefs.Clone()));

  EXPECT_TRUE(device_prefs()->HasPrefPath(prefs::kManagedCellularESimMetadata));

  SetDevicePrefs();

  const base::Value::Dict& migrated_prefs =
      device_prefs()->GetDict(prefs::kManagedCellularESimMetadata);
  EXPECT_EQ(new_prefs, migrated_prefs);
}

TEST_F(ManagedCellularPrefHandlerTest, IccidSmdpPairMigration_Migration) {
  Init();

  auto generate_esim_metadata = [](const std::string& smdp_activation_code) {
    return base::Value::Dict().Set(::onc::cellular::kSMDPAddress,
                                   smdp_activation_code);
  };

  // The value that we will set the existing/pre-migration prefs to.
  base::Value::Dict existing_prefs;

  // The value that we expect the new/post-migration prefs to be.
  base::Value::Dict new_prefs;

  // Nothing missing
  existing_prefs.Set(kIccid0, kActivationCode0);
  base::Value::Dict esim_metadata0 = generate_esim_metadata(kActivationCode0);
  new_prefs.Set(kIccid0, esim_metadata0.Clone());

  // Activation code empty
  existing_prefs.Set(kIccid1, "");

  device_prefs()->Set(prefs::kManagedCellularIccidSmdpPair,
                      base::Value(existing_prefs.Clone()));

  SetDevicePrefs();

  // The existing prefs should not have changed.
  EXPECT_EQ(device_prefs()->GetDict(prefs::kManagedCellularIccidSmdpPair),
            existing_prefs);

  const base::Value::Dict& migrated_prefs =
      device_prefs()->GetDict(prefs::kManagedCellularESimMetadata);

  const base::Value::Dict* actual_pref = migrated_prefs.FindDict(kIccid0);
  ASSERT_TRUE(actual_pref);
  EXPECT_EQ(esim_metadata0, *actual_pref);

  EXPECT_FALSE(migrated_prefs.FindDict(kIccid1));
}

}  // namespace ash
