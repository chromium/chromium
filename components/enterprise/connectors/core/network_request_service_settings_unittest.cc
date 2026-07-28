// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/network_request_service_settings.h"

#include "base/json/json_reader.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

AnalysisSettings GetExpectedSettings(DataRegion data_region) {
  CloudAnalysisSettings cloud_settings;
  cloud_settings.analysis_url =
      GURL(GetServiceProviderConfig()
               ->at("google")
               .analysis->region_urls[static_cast<size_t>(data_region)]);
  cloud_settings.max_file_size = 52428800;

  AnalysisSettings expected_settings;
  expected_settings.tags = {{"dlp", TagSettings()}, {"malware", TagSettings()}};
  expected_settings.block_until_verdict = BlockUntilVerdict::kNoBlock;
  expected_settings.minimum_data_size = 0u;
  expected_settings.cloud_or_local_settings =
      CloudOrLocalAnalysisSettings(std::move(cloud_settings));

  return expected_settings;
}

}  // namespace

class NetworkRequestServiceSettingsTest
    : public testing::TestWithParam<DataRegion> {
 public:
  DataRegion data_region() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         NetworkRequestServiceSettingsTest,
                         testing::Values(DataRegion::NO_PREFERENCE,
                                         DataRegion::UNITED_STATES,
                                         DataRegion::EUROPE));

TEST_P(NetworkRequestServiceSettingsTest, RequestAndTabDomains) {
  constexpr char kSetting[] = R"({
                                   "audit": {
                                     "tab_domain": ["baz", "google.com"],
                                     "request_domain": ["foo.com", "bar.org"]
                                   },
                                   "tags": ["dlp", "malware"]
                                 })";
  auto value =
      base::JSONReader::ReadDict(kSetting, base::JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(value);

  NetworkRequestServiceSettings service_settings(base::Value(std::move(*value)),
                                                 *GetServiceProviderConfig());
  EXPECT_EQ(service_settings.service_provider_name(), "google");
  EXPECT_TRUE(service_settings.is_cloud_analysis());

  // Having a match in both URLs should always return settings.
  auto analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL("https://foo.com"), data_region());
  EXPECT_TRUE(analysis_settings);
  EXPECT_EQ(analysis_settings, GetExpectedSettings(data_region()));

  // Having either of the URLs mismatched results in no settings being
  // returned.
  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://bad.url"), GURL("https://foo.com"), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL("https://bad.url"), data_region());
  EXPECT_FALSE(analysis_settings);

  // Having either of the URLs be empty results in no settings being returned.
  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL("https://foo.com"), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);
}

TEST_P(NetworkRequestServiceSettingsTest, RequestDomainOnly) {
  constexpr char kSetting[] = R"({
                                   "audit": {
                                     "request_domain": ["foo.com", "bar.org"]
                                   },
                                   "tags": ["dlp", "malware"]
                                 })";
  auto value =
      base::JSONReader::ReadDict(kSetting, base::JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(value);

  NetworkRequestServiceSettings service_settings(base::Value(std::move(*value)),
                                                 *GetServiceProviderConfig());
  EXPECT_EQ(service_settings.service_provider_name(), "google");
  EXPECT_TRUE(service_settings.is_cloud_analysis());

  // Having a match in the request URL should always return settings.
  auto analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL("https://bar.org"), data_region());
  EXPECT_TRUE(analysis_settings);
  EXPECT_EQ(analysis_settings, GetExpectedSettings(data_region()));

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL("https://bar.org"), data_region());
  EXPECT_TRUE(analysis_settings);
  EXPECT_EQ(analysis_settings, GetExpectedSettings(data_region()));

  // Having the request URL mismatched results in no settings being returned.
  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL("https://bad.url"), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL("https://bad.url"), data_region());
  EXPECT_FALSE(analysis_settings);

  // Having the request URL empty results in no settings being returned.
  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.com"), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);
}

TEST_P(NetworkRequestServiceSettingsTest, TabDomainOnly) {
  constexpr char kSetting[] = R"({
                                   "audit": {
                                     "tab_domain": ["baz", "google.com"]
                                   },
                                   "tags": ["dlp", "malware"]
                                 })";
  auto value =
      base::JSONReader::ReadDict(kSetting, base::JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(value);

  NetworkRequestServiceSettings service_settings(base::Value(std::move(*value)),
                                                 *GetServiceProviderConfig());
  EXPECT_EQ(service_settings.service_provider_name(), "google");
  EXPECT_TRUE(service_settings.is_cloud_analysis());

  // Having a match in the tab URL should always return settings.
  auto analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL("https://bar.org"), data_region());
  EXPECT_TRUE(analysis_settings);
  EXPECT_EQ(analysis_settings, GetExpectedSettings(data_region()));

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.google.com"), GURL("https://bar.org"), data_region());
  EXPECT_TRUE(analysis_settings);
  EXPECT_EQ(analysis_settings, GetExpectedSettings(data_region()));

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://google.com"), GURL(), data_region());
  EXPECT_TRUE(analysis_settings);
  EXPECT_EQ(analysis_settings, GetExpectedSettings(data_region()));

  // Having the tab URL mismatched results in no settings being returned.
  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://bad.url"), GURL("https://google.com"), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://bad.url"), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);

  // Having the tab URL empty results in no settings being returned.
  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL("https://google.com"), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);
}

class InvalidNetworkRequestServiceSettingsTest
    : public testing::TestWithParam<std::tuple<const char*, DataRegion>> {
 public:
  const char* setting() { return std::get<0>(GetParam()); }
  DataRegion data_region() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    InvalidNetworkRequestServiceSettingsTest,
    testing::Combine(testing::Values(
                         // No "audit" field
                         R"({
                              "tags": ["dlp", "malware"]
                            })",
                         // Empty "audit" field
                         R"({
                              "audit": {
                              },
                              "tags": ["dlp", "malware"]
                            })",
                         // No "tags" field
                         R"({
                              "audit": {
                                "tab_domain": ["baz", "google.com"],
                                "request_domain": ["foo.com", "bar.org"]
                              },
                            })",
                         // Empty "tags" field
                         R"({
                              "audit": {
                                "tab_domain": ["baz", "google.com"],
                                "request_domain": ["foo.com", "bar.org"]
                              },
                              "tags": []
                            })",
                         // Empty dict
                         "{}"),
                     testing::Values(DataRegion::NO_PREFERENCE,
                                     DataRegion::UNITED_STATES,
                                     DataRegion::EUROPE)));

TEST_P(InvalidNetworkRequestServiceSettingsTest, Test) {
  auto value =
      base::JSONReader::ReadDict(setting(), base::JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(value);

  NetworkRequestServiceSettings service_settings(base::Value(std::move(*value)),
                                                 *GetServiceProviderConfig());
  EXPECT_EQ(service_settings.service_provider_name(), "google");
  EXPECT_TRUE(service_settings.is_cloud_analysis());

  auto analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL("https://bar.org"), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL("https://bar.org"), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL("https://foo.baz"), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);

  analysis_settings = service_settings.GetNetworkRequestAnalysisSettings(
      GURL(), GURL(), data_region());
  EXPECT_FALSE(analysis_settings);
}

}  // namespace enterprise_connectors
