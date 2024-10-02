// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_service_base.h"

#include "base/json/json_reader.h"
#include "components/enterprise/connectors/core/connectors_manager_base.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
namespace {

constexpr char kMachineDMToken[] = "machine_dm_token";
constexpr char kProfileDMToken[] = "profile_dm_token";

constexpr char kEmptySettingsPref[] = "[]";

constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";

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

  void set_connectors_manager_base() {
    connectors_manager_base_ = std::make_unique<ConnectorsManagerBase>(
        &prefs_, GetServiceProviderConfig());
  }

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

  bool IsConnectorEnabled(AnalysisConnector connector) const override {
    return false;
  }

  ConnectorsManagerBase* GetConnectorsManagerBase() override {
    return connectors_manager_base_.get();
  }
  const ConnectorsManagerBase* GetConnectorsManagerBase() const override {
    return connectors_manager_base_.get();
  }

  PrefService* GetPrefs() override { return &prefs_; }
  const PrefService* GetPrefs() const override { return &prefs_; }

  policy::CloudPolicyManager* GetManagedUserCloudPolicyManager()
      const override {
    NOTREACHED();
    // return nullptr;
  }

 private:
  bool connectors_enabled_ = false;
  std::optional<DmToken> machine_dm_token_;
  std::optional<DmToken> profile_dm_token_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<ConnectorsManagerBase> connectors_manager_base_;
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

class ConnectorsServiceBaseReportingSettingsTest
    : public TestConnectorsService,
      public testing::Test,
      public testing::WithParamInterface<
          std::tuple<ReportingConnector, const char*>> {
 public:
  ReportingConnector connector() const { return std::get<0>(GetParam()); }
  const char* pref_value() const { return std::get<1>(GetParam()); }

  const char* pref() const { return kOnSecurityEventPref; }

  const char* scope_pref() const { return kOnSecurityEventScopePref; }

  bool reporting_enabled() const {
    return pref_value() == kNormalReportingSettingsPref;
  }
};

TEST_P(ConnectorsServiceBaseReportingSettingsTest, Test) {
  TestConnectorsService service;
  // TODO(b/344593927): Re-enable this test for Android and iOS
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  ASSERT_FALSE(service.GetPrefs()->FindPreference(
      "enterprise_connectors.on_security_event"));
#else
  service.set_connectors_manager_base();
  if (pref_value()) {
    service.GetPrefs()->Set(pref(), *base::JSONReader::Read(pref_value()));
    service.GetPrefs()->SetInteger(scope_pref(), policy::POLICY_SCOPE_MACHINE);
  }

  auto settings =
      service.GetConnectorsManagerBase()->GetReportingSettings(connector());
  EXPECT_EQ(reporting_enabled(), settings.has_value());
  EXPECT_EQ(pref_value() == kNormalReportingSettingsPref,
            !service.GetConnectorsManagerBase()
                 ->GetReportingConnectorsSettingsForTesting()
                 .empty());
#endif
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceBaseReportingSettingsTest,
    testing::Combine(testing::Values(ReportingConnector::SECURITY_EVENT),
                     testing::Values(nullptr,
                                     kNormalReportingSettingsPref,
                                     kEmptySettingsPref)));

}  // namespace enterprise_connectors
