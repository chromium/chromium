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

class ConnectorsManager : public ConnectorsManagerBase {
 public:
  using ConnectorsManagerBase::ConnectorsManagerBase;

  void CacheAnalysisConnectorPolicy(
      AnalysisConnector connector) const override {
    // do nothing
  }

  DataRegion GetDataRegion(AnalysisConnector connector) const override {
    return DataRegion::NO_PREFERENCE;
  }
};

class TestConnectorsService : public ConnectorsServiceBase {
 public:
  explicit TestConnectorsService(TestingPrefServiceSimple* prefs)
      : ConnectorsServiceBase(
            std::make_unique<ConnectorsManager>(prefs,
                                                GetServiceProviderConfig())),
        prefs_(prefs) {
    RegisterProfilePrefs(prefs_->registry());
  }

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
    switch (prefs_->GetInteger(kEnterpriseRealTimeUrlCheckScope)) {
      case policy::POLICY_SCOPE_MACHINE:
        return machine_dm_token_;
      case policy::POLICY_SCOPE_USER:
        return profile_dm_token_;
      default:
        NOTREACHED();
    }
  }

  std::optional<std::string> GetBrowserDmToken() const override {
    return std::nullopt;
  }

  std::unique_ptr<ClientMetadata> BuildClientMetadata(bool is_cloud) override {
    return nullptr;
  }

  bool ConnectorsEnabled() const override { return connectors_enabled_; }

  PrefService* GetPrefs() override { return prefs_; }
  const PrefService* GetPrefs() const override { return prefs_; }

  policy::CloudPolicyManager* GetManagedUserCloudPolicyManager()
      const override {
    NOTREACHED();
    // return nullptr;
  }

 private:
  bool connectors_enabled_ = false;
  std::optional<DmToken> machine_dm_token_;
  std::optional<DmToken> profile_dm_token_;
  raw_ptr<TestingPrefServiceSimple> prefs_;
};

}  // namespace

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_NoTokenOrPolicies) {
  TestingPrefServiceSimple prefs;
  TestConnectorsService service(&prefs);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kConnectorsDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_connectors_enabled(true);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kPolicyDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);
}

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_InvalidProfilePolicy) {
  TestingPrefServiceSimple prefs;
  TestConnectorsService service(&prefs);

  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                 REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                 policy::POLICY_SCOPE_USER);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kConnectorsDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_connectors_enabled(true);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(
      service.GetDMTokenForRealTimeUrlCheck().error(),
      ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::kNoDmToken);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_machine_dm_token();

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(
      service.GetDMTokenForRealTimeUrlCheck().error(),
      ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::kNoDmToken);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);
}

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_InvalidMachinePolicy) {
  TestingPrefServiceSimple prefs;
  TestConnectorsService service(&prefs);

  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckMode,
                                 REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  service.GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                 policy::POLICY_SCOPE_MACHINE);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kConnectorsDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_connectors_enabled(true);

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(
      service.GetDMTokenForRealTimeUrlCheck().error(),
      ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::kNoDmToken);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);

  service.set_profile_dm_token();

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(
      service.GetDMTokenForRealTimeUrlCheck().error(),
      ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::kNoDmToken);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(), REAL_TIME_CHECK_DISABLED);
}

TEST(ConnectorsServiceBaseTest, RealTimeUrlCheck_ValidProfilePolicy) {
  TestingPrefServiceSimple prefs;
  TestConnectorsService service(&prefs);

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
  TestingPrefServiceSimple prefs;
  TestConnectorsService service(&prefs);

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
    : public testing::Test,
      public testing::WithParamInterface<const char*> {
 public:
  const char* pref_value() const { return GetParam(); }

  const char* pref() const { return kOnSecurityEventPref; }

  const char* scope_pref() const { return kOnSecurityEventScopePref; }

  bool reporting_enabled() const {
    return pref_value() == kNormalReportingSettingsPref;
  }
};

TEST_P(ConnectorsServiceBaseReportingSettingsTest, Test) {
  TestingPrefServiceSimple prefs;
  TestConnectorsService service(&prefs);

  if (pref_value()) {
    service.GetPrefs()->Set(
        pref(), *base::JSONReader::Read(pref_value(),
                                        base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    service.GetPrefs()->SetInteger(scope_pref(), policy::POLICY_SCOPE_MACHINE);
  }

  auto settings =
      service.ConnectorsManagerBaseForTesting()->GetReportingSettings();
  EXPECT_EQ(reporting_enabled(), settings.has_value());
  EXPECT_EQ(pref_value() == kNormalReportingSettingsPref,
            !service.ConnectorsManagerBaseForTesting()
                 ->GetReportingConnectorsSettingsForTesting()
                 .empty());
}

INSTANTIATE_TEST_SUITE_P(,
                         ConnectorsServiceBaseReportingSettingsTest,
                         testing::Values(nullptr,
                                         kNormalReportingSettingsPref,
                                         kEmptySettingsPref));

}  // namespace enterprise_connectors
