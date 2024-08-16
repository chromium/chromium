// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_service_base.h"

#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
namespace {

constexpr char kMachineDMToken[] = "machine_dm_token";
constexpr char kProfileDMToken[] = "profile_dm_token";

class TestConnectorsService : public ConnectorsServiceBase {
 public:
  TestConnectorsService() { RegisterProfilePrefs(prefs_.registry()); }

  void set_machine_dm_token() {
    machine_dm_token_ = ConnectorsServiceBase::DmToken(
        kMachineDMToken, policy::POLICY_SCOPE_MACHINE);
  }

  void set_profile_dm_token() {
    profile_dm_token_ = ConnectorsServiceBase::DmToken(
        kProfileDMToken, policy::POLICY_SCOPE_USER);
  }

  void set_connectors_enabled(bool enabled) { connectors_enabled_ = enabled; }

  std::optional<DmToken> GetDmToken(const char* scope_pref) const override {
    switch (prefs_.GetInteger(kEnterpriseRealTimeUrlCheckScope)) {
      case policy::POLICY_SCOPE_MACHINE:
        return machine_dm_token_;
      case policy::POLICY_SCOPE_USER:
        return profile_dm_token_;
      default:
        NOTREACHED();
    }
  }

  bool ConnectorsEnabled() const override { return connectors_enabled_; }

  PrefService* GetPrefs() override { return &prefs_; }
  const PrefService* GetPrefs() const override { return &prefs_; }

 private:
  bool connectors_enabled_ = false;
  std::optional<DmToken> machine_dm_token_;
  std::optional<DmToken> profile_dm_token_;
  TestingPrefServiceSimple prefs_;
};

}  // namespace

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_NoTokenOrPolicies) {
  TestConnectorsService service;

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_connectors_enabled(true);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);
}

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_InvalidProfilePolicy) {
  TestConnectorsService service;
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                 REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                 policy::POLICY_SCOPE_USER);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_connectors_enabled(true);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_machine_dm_token();

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);
}

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_InvalidMachinePolicy) {
  TestConnectorsService service;
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                 REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                 policy::POLICY_SCOPE_MACHINE);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_connectors_enabled(true);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_profile_dm_token();

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);
}

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_ValidProfilePolicy) {
  TestConnectorsService service;
  service.set_connectors_enabled(true);
  service.set_profile_dm_token();
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                 REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                 policy::POLICY_SCOPE_USER);

  ASSERT_TRUE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(*service.GetDMTokenForRealTimeUrlCheck(), kProfileDMToken);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
}

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_ValidMachinePolicy) {
  TestConnectorsService service;
  service.set_connectors_enabled(true);
  service.set_machine_dm_token();
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                 REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                 policy::POLICY_SCOPE_MACHINE);

  ASSERT_TRUE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(*service.GetDMTokenForRealTimeUrlCheck(), kMachineDMToken);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
}

}  // namespace enterprise_connectors
