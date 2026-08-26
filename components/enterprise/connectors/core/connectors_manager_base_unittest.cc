// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_manager_base.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

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

constexpr char kMultipleNetworkRequestSettingsPref[] = R"([
  {
    "audit": {
      "tab_domain": ["foo.com"],
      "request_domain": ["bar.org"]
    },
    "tags": ["dlp"]
  },
  {
    "audit": {
      "tab_domain": ["foo.com"],
      "request_domain": ["bar.org"]
    },
    "tags": ["malware"]
  }
])";

constexpr char kPartialMatchingNetworkRequestSettingsPref[] = R"([
  {
    "audit": {
      "tab_domain": ["foo.com"],
      "request_domain": ["bar.org"]
    },
    "tags": ["dlp"]
  },
  {
    "audit": {
      "tab_domain": ["other.com"],
      "request_domain": ["bar.org"]
    },
    "tags": ["malware"]
  }
])";

constexpr char kNonAuditNetworkRequestSettingsPref[] = R"([
  {
    "block": {
      "tab_domain": ["foo.com"],
      "request_domain": ["bar.org"]
    },
    "tags": ["dlp"]
  }
])";
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace

class TestConnectorsManagerBase : public ConnectorsManagerBase {
 public:
  using ConnectorsManagerBase::ConnectorsManagerBase;

  DataRegion GetDataRegion(AnalysisConnector connector) const override {
    return DataRegion::NO_PREFERENCE;
  }
};

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

class ConnectorsManagerBaseReportingTest : public ConnectorsManagerBaseTest {
 public:
  const char* pref() const { return kOnSecurityEventPref; }
};

TEST_F(ConnectorsManagerBaseReportingTest, DynamicPolicies) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  // The cache is initially empty.
  ASSERT_TRUE(manager.GetReportingConnectorsSettingsForTesting().empty());

  {
    ScopedConnectorPref scoped_pref(pref_service(), pref(),
                                    kNormalReportingSettingsPref);

    const auto& cached_settings =
        manager.GetReportingConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.size());

    auto settings = cached_settings.at(0).GetReportingSettings();
    ASSERT_TRUE(settings.has_value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager.GetReportingConnectorsSettingsForTesting().empty());
}

#if !BUILDFLAG(IS_IOS)
class ConnectorsManagerBaseNetworkRequestFeatureDisabledTest
    : public ConnectorsManagerBaseTest {
 public:
  ConnectorsManagerBaseNetworkRequestFeatureDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kEnableAuditOnlyNetworkRequestConnector);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ConnectorsManagerBaseNetworkRequestFeatureDisabledTest,
       GetNetworkRequestAnalysisSettings) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  ScopedConnectorPref scoped_pref(pref_service(), kOnNetworkRequestPref,
                                  kNormalNetworkRequestSettingsPref);

  auto settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org"));
  EXPECT_FALSE(settings.has_value());
}

class ConnectorsManagerBaseNetworkRequestTest
    : public ConnectorsManagerBaseTest {
 public:
  ConnectorsManagerBaseNetworkRequestTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnableAuditOnlyNetworkRequestConnector);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ConnectorsManagerBaseNetworkRequestTest, DynamicPolicies) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());

  {
    ScopedConnectorPref scoped_pref(pref_service(), kOnNetworkRequestPref,
                                    kNormalNetworkRequestSettingsPref);

    const auto& cached_settings =
        manager.GetAnalysisConnectorsSettingsForTesting();
    ASSERT_EQ(1u, cached_settings.count(AnalysisConnector::NETWORK_REQUEST));

    auto settings = manager.GetNetworkRequestAnalysisSettings(
        GURL("https://foo.com"), GURL("https://bar.org"));
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(1u, settings->tags.size());
    EXPECT_EQ(1u, settings->tags.count("dlp"));
    EXPECT_EQ(BlockUntilVerdict::kNoBlock, settings->block_until_verdict);
  }

  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
}

TEST_F(ConnectorsManagerBaseNetworkRequestTest, NoMatchingRule) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  ScopedConnectorPref scoped_pref(pref_service(), kOnNetworkRequestPref,
                                  kNormalNetworkRequestSettingsPref);

  // Tab URL mismatch.
  auto settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://nonmatching.com"), GURL("https://bar.org"));
  EXPECT_FALSE(settings.has_value());

  // Request URL mismatch.
  settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://nonmatching.org"));
  EXPECT_FALSE(settings.has_value());
}

TEST_F(ConnectorsManagerBaseNetworkRequestTest, MultipleMatchingRules) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  ScopedConnectorPref scoped_pref(pref_service(), kOnNetworkRequestPref,
                                  kMultipleNetworkRequestSettingsPref);

  auto settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org"));
  ASSERT_TRUE(settings.has_value());
  EXPECT_EQ(2u, settings->tags.size());
  EXPECT_EQ(1u, settings->tags.count("dlp"));
  EXPECT_EQ(1u, settings->tags.count("malware"));
  EXPECT_EQ(BlockUntilVerdict::kNoBlock, settings->block_until_verdict);
}

TEST_F(ConnectorsManagerBaseNetworkRequestTest, PartialMatchingRules) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  ScopedConnectorPref scoped_pref(pref_service(), kOnNetworkRequestPref,
                                  kPartialMatchingNetworkRequestSettingsPref);

  auto settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org"));
  ASSERT_TRUE(settings.has_value());
  EXPECT_EQ(1u, settings->tags.size());
  EXPECT_EQ(1u, settings->tags.count("dlp"));
  EXPECT_EQ(0u, settings->tags.count("malware"));

  settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://other.com"), GURL("https://bar.org"));
  ASSERT_TRUE(settings.has_value());
  EXPECT_EQ(1u, settings->tags.size());
  EXPECT_EQ(0u, settings->tags.count("dlp"));
  EXPECT_EQ(1u, settings->tags.count("malware"));
}

TEST_F(ConnectorsManagerBaseNetworkRequestTest, NonAuditRule) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  ScopedConnectorPref scoped_pref(pref_service(), kOnNetworkRequestPref,
                                  kNonAuditNetworkRequestSettingsPref);

  auto settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org"));
  EXPECT_FALSE(settings.has_value());
}

TEST_F(ConnectorsManagerBaseNetworkRequestTest, EmptyPolicy) {
  TestConnectorsManagerBase manager(pref_service(), GetServiceProviderConfig());
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());

  ScopedConnectorPref scoped_pref(pref_service(), kOnNetworkRequestPref, "[]");
  auto settings = manager.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL("https://bar.org"));
  EXPECT_FALSE(settings.has_value());
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace enterprise_connectors
