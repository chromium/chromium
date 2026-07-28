// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/network_request_service_settings.h"

#include "base/notreached.h"
#include "base/values.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"

namespace enterprise_connectors {

namespace {

inline constexpr char kGoogleServiceProvider[] = "google";

inline constexpr char kKeyAudit[] = "audit";
inline constexpr char kKeyRequestDomain[] = "request_domain";
inline constexpr char kKeyTabDomain[] = "tab_domain";

std::vector<std::string> GetStringList(const base::DictValue& dict,
                                       const std::string& key) {
  std::vector<std::string> list;
  const base::ListValue* list_value = dict.FindList(key);
  if (list_value) {
    for (const base::Value& entry : *list_value) {
      if (!entry.is_string()) {
        continue;
      }
      list.push_back(entry.GetString());
    }
  }
  return list;
}

bool ListContainsDomain(const GURL& url,
                        const std::vector<std::string>& domains) {
  for (const std::string& domain : domains) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

}  // namespace

NetworkRequestServiceSettings::NetworkRequestServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict()) {
    return;
  }

  CHECK(SetServiceProvider(kGoogleServiceProvider, service_provider_config));
  const base::DictValue& settings_dict = settings_value.GetDict();

  // Entries of the "OnNetworkRequestEnterpriseConnector" list policy have the
  // following schema:
  // {
  //   "audit": {
  //     "tab_domain": [<string>],
  //     "request_domain": [<string>]
  //   },
  //   "tags": [<string>]
  // }
  const base::DictValue* audit = settings_dict.FindDict(kKeyAudit);
  if (audit) {
    audit_request_domains_ = GetStringList(*audit, kKeyRequestDomain);
    audit_tab_domains_ = GetStringList(*audit, kKeyTabDomain);
  }

  const base::ListValue* tags = settings_dict.FindList(kKeyTags);
  if (tags) {
    for (const base::Value& tag : *tags) {
      if (!tag.is_string()) {
        continue;
      }
      tags_[tag.GetString()] = TagSettings();
    }
  }
}

NetworkRequestServiceSettings::NetworkRequestServiceSettings(
    NetworkRequestServiceSettings&&) = default;
NetworkRequestServiceSettings& NetworkRequestServiceSettings::operator=(
    NetworkRequestServiceSettings&&) = default;
NetworkRequestServiceSettings::~NetworkRequestServiceSettings() = default;

std::optional<AnalysisSettings>
NetworkRequestServiceSettings::GetAnalysisSettings(
    const GURL& url,
    DataRegion data_region) const {
  NOTREACHED();
}

std::optional<AnalysisSettings>
NetworkRequestServiceSettings::GetNetworkRequestAnalysisSettings(
    const GURL& tab_url,
    const GURL& request_url,
    DataRegion data_region) const {
  if (audit_tab_domains_.empty() && audit_request_domains_.empty()) {
    return std::nullopt;
  }

  // If only one of the two domain lists is populated, the other one is
  // considered as matching.
  bool tab_url_matches = audit_tab_domains_.empty() ||
                         ListContainsDomain(tab_url, audit_tab_domains_);
  bool request_url_matches =
      audit_request_domains_.empty() ||
      ListContainsDomain(request_url, audit_request_domains_);
  if (!tab_url_matches || !request_url_matches || tags_.empty()) {
    return std::nullopt;
  }

  AnalysisSettings settings;
  settings.cloud_or_local_settings =
      CloudOrLocalAnalysisSettings(GetCloudAnalysisSettings(data_region));
  settings.minimum_data_size = 0;
  settings.tags = tags_;
  return settings;
}

}  // namespace enterprise_connectors
