// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_manager_base.h"

#include "base/json/json_reader.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr ReportingConnector kAllReportingConnectors[] = {
    ReportingConnector::SECURITY_EVENT};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";
#endif
}  // namespace

class ConnectorsManagerBaseTest : public testing::Test {
 public:
  ConnectorsManagerBaseTest() { RegisterProfilePrefs(prefs_.registry()); }

  PrefService* pref_service() { return &prefs_; }

  class ScopedConnectorPref {
   public:
    ScopedConnectorPref(PrefService* pref_service,
                        const char* pref,
                        const char* pref_value)
        : pref_service_(pref_service), pref_(pref) {
      auto maybe_pref_value =
          base::JSONReader::Read(pref_value, base::JSON_ALLOW_TRAILING_COMMAS);
      EXPECT_TRUE(maybe_pref_value.has_value());
      if (maybe_pref_value.has_value()) {
        pref_service_->Set(pref, maybe_pref_value.value());
      }
    }

    void UpdateScopedConnectorPref(const char* pref_value) {
      auto maybe_pref_value =
          base::JSONReader::Read(pref_value, base::JSON_ALLOW_TRAILING_COMMAS);
      EXPECT_TRUE(maybe_pref_value.has_value());
      ASSERT_NE(pref_service_, nullptr);
      ASSERT_NE(pref_, nullptr);
      pref_service_->Set(pref_, maybe_pref_value.value());
    }

    ~ScopedConnectorPref() { pref_service_->ClearPref(pref_); }

   private:
    raw_ptr<PrefService> pref_service_;
    const char* pref_;
  };

 private:
  TestingPrefServiceSimple prefs_;
};

class ConnectorsManagerBaseReportingTest
    : public ConnectorsManagerBaseTest,
      public testing::WithParamInterface<ReportingConnector> {
 public:
  ReportingConnector connector() const { return GetParam(); }

  const char* pref() const { return kOnSecurityEventPref; }
};

TEST_P(ConnectorsManagerBaseReportingTest, DynamicPolicies) {
  // TODO(b/344593927): Re-enable this test for Android and iOS
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  ASSERT_FALSE(pref_service()->FindPreference(
      "enterprise_connectors.on_security_event"));
#else
  ConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  // The cache is initially empty.
  ASSERT_TRUE(manager.GetReportingConnectorsSettingsForTesting().empty());

  {
    ScopedConnectorPref scoped_pref(pref_service(), pref(),
                                    kNormalReportingSettingsPref);

    const auto& cached_settings =
        manager.GetReportingConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.count(connector()));
    ASSERT_EQ(1u, cached_settings.at(connector()).size());

    auto settings =
        cached_settings.at(connector()).at(0).GetReportingSettings();
    ASSERT_TRUE(settings.has_value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager.GetReportingConnectorsSettingsForTesting().empty());
#endif
}

INSTANTIATE_TEST_SUITE_P(ConnectorsManagerBaseReportingTest,
                         ConnectorsManagerBaseReportingTest,
                         testing::ValuesIn(kAllReportingConnectors));

}  // namespace enterprise_connectors
