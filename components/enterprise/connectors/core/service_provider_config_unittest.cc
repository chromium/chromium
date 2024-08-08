// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/service_provider_config.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kMaxFileSize = 50 * 1024 * 1024;

}  // namespace

TEST(ServiceProviderConfigTest, Google) {
  const ServiceProviderConfig* config = GetServiceProviderConfig();
  ASSERT_TRUE(config->count("google"));
  ServiceProvider service_provider = config->at("google");

  ASSERT_TRUE(service_provider.analysis);
  ASSERT_TRUE(service_provider.reporting);

  ASSERT_EQ("https://safebrowsing.google.com/safebrowsing/uploads/scan",
            std::string(service_provider.analysis->url));
  ASSERT_TRUE(GURL(service_provider.analysis->url).is_valid());
  ASSERT_EQ("https://chromereporting-pa.googleapis.com/v1/events",
            std::string(service_provider.reporting->url));
  ASSERT_TRUE(GURL(service_provider.reporting->url).is_valid());

  // The Google service provider has 2 tags: malware and dlp.
  ASSERT_EQ(service_provider.analysis->supported_tags.size(), 2u);
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags[0].name),
            "malware");
  ASSERT_EQ(service_provider.analysis->supported_tags[0].max_file_size,
            kMaxFileSize);
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags[1].name),
            "dlp");
  ASSERT_EQ(service_provider.analysis->supported_tags[1].max_file_size,
            kMaxFileSize);

  // The Google service provider has three regionalized urls.
  ASSERT_EQ(service_provider.analysis->region_urls.size(), 3u);
  ASSERT_EQ("https://safebrowsing.google.com/safebrowsing/uploads/scan",
            std::string(service_provider.analysis->region_urls[0]));
  ASSERT_TRUE(GURL(service_provider.analysis->region_urls[0]).is_valid());
  ASSERT_EQ("https://scan.webprotect-us.goog/uploads",
            std::string(service_provider.analysis->region_urls[1]));
  ASSERT_TRUE(GURL(service_provider.analysis->region_urls[1]).is_valid());
  ASSERT_EQ("https://scan.webprotect-eu.goog/uploads",
            std::string(service_provider.analysis->region_urls[2]));
  ASSERT_TRUE(GURL(service_provider.analysis->region_urls[2]).is_valid());
}

TEST(ServiceProviderConfigTest, LocalTest1) {
  const ServiceProviderConfig* config = GetServiceProviderConfig();
  ASSERT_TRUE(config->count("local_user_agent"));
  ServiceProvider service_provider = config->at("local_user_agent");

  ASSERT_TRUE(service_provider.analysis);
  ASSERT_FALSE(service_provider.reporting);

  ASSERT_FALSE(service_provider.analysis->url);
  ASSERT_TRUE(service_provider.analysis->local_path);
  ASSERT_EQ("path_user", std::string(service_provider.analysis->local_path));
  ASSERT_TRUE(service_provider.analysis->user_specific);

  // The test local service provider has 1 tag: dlp.
  ASSERT_EQ(service_provider.analysis->supported_tags.size(), 1u);
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags[0].name),
            "dlp");
  ASSERT_EQ(service_provider.analysis->supported_tags[0].max_file_size,
            kMaxFileSize);
}

// Temporary code(b/268532118): Once DM server no longer sends
// "local_system_agent" as a service_provider name, this test can be
// removed.
TEST(ServiceProviderConfigTest, LocalTest2) {
  const ServiceProviderConfig* config = GetServiceProviderConfig();
  ASSERT_TRUE(config->count("local_system_agent"));
  ServiceProvider service_provider = config->at("local_system_agent");

  ASSERT_TRUE(service_provider.analysis);
  ASSERT_FALSE(service_provider.reporting);

  ASSERT_FALSE(service_provider.analysis->url);
  ASSERT_TRUE(service_provider.analysis->local_path);
  ASSERT_EQ("brcm_chrm_cas",
            std::string(service_provider.analysis->local_path));
  ASSERT_FALSE(service_provider.analysis->user_specific);

  // The test local service provider has 1 tag: dlp.
  ASSERT_EQ(service_provider.analysis->supported_tags.size(), 1u);
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags[0].name),
            "dlp");
  ASSERT_EQ(service_provider.analysis->supported_tags[0].max_file_size,
            kMaxFileSize);
}

TEST(ServiceProviderConfigTest, BrcmChrmCas) {
  const ServiceProviderConfig* config = GetServiceProviderConfig();
  ASSERT_TRUE(config->count("brcm_chrm_cas"));
  ServiceProvider service_provider = config->at("brcm_chrm_cas");

  ASSERT_TRUE(service_provider.analysis);
  ASSERT_FALSE(service_provider.reporting);

  ASSERT_FALSE(service_provider.analysis->url);
  ASSERT_TRUE(service_provider.analysis->local_path);
  ASSERT_EQ("brcm_chrm_cas",
            std::string(service_provider.analysis->local_path));
  ASSERT_FALSE(service_provider.analysis->user_specific);
  ASSERT_EQ(service_provider.analysis->subject_names.size(), 0u);

  // The BrcmChrmCas local service provider has 1 tag: dlp.
  ASSERT_EQ(service_provider.analysis->supported_tags.size(), 1u);
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags[0].name),
            "dlp");
  ASSERT_EQ(service_provider.analysis->supported_tags[0].max_file_size,
            kMaxFileSize);
}

TEST(ServiceProviderConfigTest, Trellix) {
  const ServiceProviderConfig* config = GetServiceProviderConfig();
  ASSERT_TRUE(config->count("trellix"));
  ServiceProvider service_provider = config->at("trellix");

  ASSERT_TRUE(service_provider.analysis);
  ASSERT_FALSE(service_provider.reporting);

  ASSERT_FALSE(service_provider.analysis->url);
  ASSERT_TRUE(service_provider.analysis->local_path);
  ASSERT_EQ("Trellix_DLP", std::string(service_provider.analysis->local_path));
  ASSERT_TRUE(service_provider.analysis->user_specific);
  ASSERT_EQ(service_provider.analysis->subject_names.size(), 1u);

  // The trellix local service provider has 1 tag: dlp.
  ASSERT_EQ(service_provider.analysis->supported_tags.size(), 1u);
  ASSERT_EQ(std::string(service_provider.analysis->supported_tags[0].name),
            "dlp");
  ASSERT_EQ(service_provider.analysis->supported_tags[0].max_file_size,
            kMaxFileSize);
}

}  // namespace enterprise_connectors
