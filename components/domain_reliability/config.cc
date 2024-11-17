// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/config.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "url/url_constants.h"

namespace {

bool ConvertURL(const base::Value* value, GURL* url) {
  if (!value->is_string())
    return false;
  *url = GURL(value->GetString());
  return url->is_valid();
}

bool ConvertOrigin(const base::Value* value, url::Origin* origin) {
  GURL url;
  if (ConvertURL(value, &url) && !url.has_username() && !url.has_password() &&
      url.SchemeIs(url::kHttpsScheme) && url.path_piece() == "/" &&
      !url.has_query() && !url.has_ref()) {
    *origin = url::Origin::Create(url);
    return true;
  }
  return false;
}

bool IsValidSampleRate(double p) {
  return p >= 0.0 && p <= 1.0;
}

}  // namespace

namespace domain_reliability {

DomainReliabilityConfig::DomainReliabilityConfig()
    : include_subdomains(false),
      success_sample_rate(-1.0),
      failure_sample_rate(-1.0) {
}
DomainReliabilityConfig::~DomainReliabilityConfig() = default;

// static
std::unique_ptr<const DomainReliabilityConfig>
DomainReliabilityConfig::FromJSON(std::string_view json) {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value)
    return nullptr;

  base::JSONValueConverter<DomainReliabilityConfig> converter;
  auto config = std::make_unique<DomainReliabilityConfig>();

  // If we can parse and convert the JSON into a valid config, return that.
  if (converter.Convert(*value, config.get()) && config->IsValid())
    return config;

  return nullptr;
}

bool DomainReliabilityConfig::IsValid() const {
  if (origin.opaque() || collectors.empty() ||
      !IsValidSampleRate(success_sample_rate) ||
      !IsValidSampleRate(failure_sample_rate)) {
    return false;
  }

  for (const auto& url : collectors) {
    if (!url->SchemeIs(url::kHttpsScheme) || !url->is_valid())
      return false;
  }

  return true;
}

double DomainReliabilityConfig::GetSampleRate(bool request_successful) const {
  return request_successful ? success_sample_rate : failure_sample_rate;
}

// static
void DomainReliabilityConfig::RegisterJSONConverter(
    base::JSONValueConverter<DomainReliabilityConfig>* converter) {
  converter->RegisterCustomValueField<url::Origin>(
      "origin", &DomainReliabilityConfig::origin, &ConvertOrigin);
  converter->RegisterBoolField("include_subdomains",
                               &DomainReliabilityConfig::include_subdomains);
  converter->RegisterRepeatedCustomValue(
      "collectors", &DomainReliabilityConfig::collectors, &ConvertURL);
  converter->RegisterRepeatedString("path_prefixes",
                                    &DomainReliabilityConfig::path_prefixes);
  converter->RegisterDoubleField("success_sample_rate",
                                 &DomainReliabilityConfig::success_sample_rate);
  converter->RegisterDoubleField("failure_sample_rate",
                                 &DomainReliabilityConfig::failure_sample_rate);
}

}  // namespace domain_reliability
