// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_service_base.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/connectors_manager_base.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/test_connectors_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

#if !BUILDFLAG(IS_IOS)
constexpr char kNormalNetworkRequestSettingsPref[] = R"([
  {
    "audit": {
      "tab_domain": ["foo.com"],
      "request_domain": ["bar.org"]
    },
    "tags": ["dlp"]
  }
])";

// Only has a request domain, so any tab URL matches it.
constexpr char kRequestDomainOnlyNetworkRequestSettingsPref[] = R"([
  {
    "audit": {
      "request_domain": ["bar.org"]
    },
    "tags": ["dlp"]
  }
])";
#endif  // !BUILDFLAG(IS_IOS)

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

  service.set_machine_dm_token(kMachineDMToken);

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

  service.set_profile_dm_token(kProfileDMToken);

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
  service.set_profile_dm_token(kProfileDMToken);
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
  service.set_machine_dm_token(kMachineDMToken);
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

#if !BUILDFLAG(IS_IOS)
template <bool enable_feature>
class ConnectorsServiceBaseNetworkRequestTest : public testing::Test {
 public:
  ConnectorsServiceBaseNetworkRequestTest() {
    service_.set_connectors_enabled(true);

    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(
          kEnableAuditOnlyNetworkRequestConnector);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kEnableAuditOnlyNetworkRequestConnector);
    }
  }

  void SetNetworkRequestPolicy(const char* pref_value,
                               policy::PolicyScope scope) {
    service_.GetPrefs()->Set(
        kOnNetworkRequestPref,
        *base::JSONReader::Read(pref_value,
                                base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    service_.GetPrefs()->SetInteger(kOnNetworkRequestScopePref, scope);
  }

  TestConnectorsService& service() { return service_; }

 private:
  // The feature needs to be enabled before `service_` is created so that its
  // manager observes the network request pref.
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple prefs_;
  TestConnectorsService service_{&prefs_};
};

using ConnectorsServiceBaseNetworkRequestDisabledTest =
    ConnectorsServiceBaseNetworkRequestTest<false>;

TEST_F(ConnectorsServiceBaseNetworkRequestDisabledTest, FeatureDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      kEnableAuditOnlyNetworkRequestConnector);

  service().set_machine_dm_token(kMachineDMToken);
  SetNetworkRequestPolicy(kNormalNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_MACHINE);

  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org")));
}

using ConnectorsServiceBaseNetworkRequestEnabledTest =
    ConnectorsServiceBaseNetworkRequestTest<true>;

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest, ConnectorsDisabled) {
  service().set_connectors_enabled(false);
  service().set_machine_dm_token(kMachineDMToken);
  SetNetworkRequestPolicy(kNormalNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_MACHINE);

  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org")));
}

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest, NoPolicy) {
  service().set_machine_dm_token(kMachineDMToken);

  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org")));
}

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest,
       NoDmTokenForPolicyScope) {
  // A profile DM token can't be used for a machine policy.
  service().set_profile_dm_token(kProfileDMToken);
  SetNetworkRequestPolicy(kNormalNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_MACHINE);

  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org")));
}

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest, MachinePolicy) {
  service().set_machine_dm_token(kMachineDMToken);
  SetNetworkRequestPolicy(kNormalNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_MACHINE);

  auto settings = service().GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com/page"), GURL("https://bar.org/upload"));
  ASSERT_TRUE(settings.has_value());
  ASSERT_TRUE(settings->cloud_or_local_settings.is_cloud_analysis());
  EXPECT_EQ(kMachineDMToken, settings->cloud_or_local_settings.dm_token());
  EXPECT_FALSE(settings->per_profile);
  EXPECT_EQ(1u, settings->tags.size());
  EXPECT_EQ(1u, settings->tags.count("dlp"));
  EXPECT_EQ(BlockUntilVerdict::kNoBlock, settings->block_until_verdict);
}

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest, ProfilePolicy) {
  service().set_profile_dm_token(kProfileDMToken);
  SetNetworkRequestPolicy(kNormalNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_USER);

  auto settings = service().GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com/page"), GURL("https://bar.org/upload"));
  ASSERT_TRUE(settings.has_value());
  ASSERT_TRUE(settings->cloud_or_local_settings.is_cloud_analysis());
  EXPECT_EQ(kProfileDMToken, settings->cloud_or_local_settings.dm_token());
  EXPECT_TRUE(settings->per_profile);
  EXPECT_EQ(1u, settings->tags.size());
  EXPECT_EQ(1u, settings->tags.count("dlp"));
  EXPECT_EQ(BlockUntilVerdict::kNoBlock, settings->block_until_verdict);
}

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest, NoMatchingRule) {
  service().set_machine_dm_token(kMachineDMToken);
  SetNetworkRequestPolicy(kNormalNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_MACHINE);

  // Tab URL mismatch.
  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://nonmatching.com"), GURL("https://bar.org")));

  // Request URL mismatch.
  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://nonmatching.org")));

  // The tab and request URLs are not interchangeable.
  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://bar.org"), GURL("https://foo.com")));
}

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest, ExemptTabUrl) {
  service().set_machine_dm_token(kMachineDMToken);
  SetNetworkRequestPolicy(kRequestDomainOnlyNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_MACHINE);

  EXPECT_TRUE(service().GetNetworkRequestAnalysisSettings(
      GURL("https://any.com"), GURL("https://bar.org")));
  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("chrome://settings"), GURL("https://bar.org")));
}

TEST_F(ConnectorsServiceBaseNetworkRequestEnabledTest, InnerTabUrl) {
  service().set_machine_dm_token(kMachineDMToken);
  SetNetworkRequestPolicy(kNormalNetworkRequestSettingsPref,
                          policy::POLICY_SCOPE_MACHINE);

  // Blob and filesystem tab URLs are matched against their inner URL.
  EXPECT_TRUE(service().GetNetworkRequestAnalysisSettings(
      GURL("blob:https://foo.com/0123-4567"), GURL("https://bar.org")));
  EXPECT_TRUE(service().GetNetworkRequestAnalysisSettings(
      GURL("filesystem:https://foo.com/temporary/file.txt"),
      GURL("https://bar.org")));

  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("blob:https://nonmatching.com/0123-4567"), GURL("https://bar.org")));
  EXPECT_FALSE(service().GetNetworkRequestAnalysisSettings(
      GURL("filesystem:https://nonmatching.com/temporary/file.txt"),
      GURL("https://bar.org")));
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace enterprise_connectors
