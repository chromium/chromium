// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/service_provider_config.h"

#include <array>
#include <iterator>
#include <string_view>

#include "base/json/json_reader.h"

namespace enterprise_connectors {

namespace {

constexpr std::array<SupportedTag, 2> kGoogleDlpSupportedTags = {{
    {
        .name = "malware",
        .display_name = "Threat protection",
        .max_file_size = 52428800,
    },
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr std::array<const char*, 3> kGoogleDlpRegionalizedUrls = {
    // LINT.IfChange(DlpRegionEndpoints)
    {"https://safebrowsing.google.com/safebrowsing/uploads/scan",
     "https://scan.webprotect-us.goog/uploads",
     "https://scan.webprotect-eu.goog/uploads"}
    // LINT.ThenChange(/components/enterprise/connectors/core/common.h:DataRegion)
};

constexpr AnalysisConfig kGoogleAnalysisConfig = {
    .url = "https://safebrowsing.google.com/safebrowsing/uploads/scan",
    .supported_tags = base::span<const SupportedTag>(kGoogleDlpSupportedTags),
    .region_urls = base::span<const char* const>(kGoogleDlpRegionalizedUrls),
};

constexpr std::array<SupportedTag, 1> kLocalTestSupportedTags = {{
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr std::array<SupportedTag, 1> kBrcmChrmCasSupportedTags = {{
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr std::array<SupportedTag, 1> kTrellixSupportedTags = {{
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr AnalysisConfig kLocalTestUserAnalysisConfig = {
    .local_path = "path_user",
    .supported_tags = base::span<const SupportedTag>(kLocalTestSupportedTags),
    .user_specific = true,
};

constexpr AnalysisConfig kBrcmChrmCasAnalysisConfig = {
    .local_path = "brcm_chrm_cas",
    .supported_tags = base::span<const SupportedTag>(kBrcmChrmCasSupportedTags),
    .user_specific = false,
};

constexpr std::array<const char*, 1> kTrellixSubjectNames = {
    {"MUSARUBRA US LLC"}};

constexpr AnalysisConfig kTrellixAnalysisConfig = {
    .local_path = "Trellix_DLP",
    .supported_tags = base::span<const SupportedTag>(kTrellixSupportedTags),
    .user_specific = true,
    .subject_names = base::span<const char* const>(kTrellixSubjectNames),
};

constexpr ReportingConfig kGoogleReportingConfig = {
    .url = "https://chromereporting-pa.googleapis.com/v1/events",
};

}  // namespace

const ServiceProviderConfig* GetServiceProviderConfig() {
  // The policy schema validates that the provider name is an expected value, so
  // when one is added to this dictionary it also needs to be added to the
  // corresponding policy definitions.
  // LINT.IfChange
  static constexpr ServiceProviderConfig kServiceProviderConfig =
      base::MakeFixedFlatMap<std::string_view, ServiceProvider>({
          {
              "google",
              {
                  .display_name = "Google Cloud",
                  .analysis = &kGoogleAnalysisConfig,
                  .reporting = &kGoogleReportingConfig,
              },
          },
          // TODO(b/226560946): Add the actual local content analysis service
          // providers to this config.
          {
              "local_user_agent",
              {
                  .display_name = "Test user agent",
                  .analysis = &kLocalTestUserAnalysisConfig,
              },
          },
          // Temporary code(b/268532118): Once DM server no longer sends
          // this value as a service_provider name, this block can be
          // removed.
          {
              "local_system_agent",
              {
                  .display_name = "Test system agent",
                  .analysis = &kBrcmChrmCasAnalysisConfig,
              },
          },
          {
              "brcm_chrm_cas",
              {
                  .display_name = "Broadcom Inc",
                  .analysis = &kBrcmChrmCasAnalysisConfig,
              },
          },
          {
              "trellix",
              {
                  .display_name = "Trellix DLP Endpoint",
                  .analysis = &kTrellixAnalysisConfig,
              },
          },
      });
  // LINT.ThenChange(//components/policy/resources/templates/policy_definitions/Miscellaneous)
  // The following policies should have their service_provider entries updated:
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnBulkDataEntryEnterpriseConnector.yaml,
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnFileAttachedEnterpriseConnector.yaml,
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnFileDownloadedEnterpriseConnector.yaml,
  //   //components/policy/resources/templates/policy_definitions/Miscellaneous/OnPrintEnterpriseConnector.yaml
  // )
  return &kServiceProviderConfig;
}

}  // namespace enterprise_connectors
