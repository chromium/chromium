// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_SERVICE_PROVIDER_CONFIG_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_SERVICE_PROVIDER_CONFIG_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/values.h"

namespace enterprise_connectors {

struct SupportedTag {
  const char* name = nullptr;
  const char* display_name = nullptr;
  size_t max_file_size = -1;
};

struct AnalysisConfig {
  // Only 1 of `url` and `local_path` should be populated to differentiate
  // between cloud analysis providers and local analysis providers.
  const char* url = nullptr;
  const char* local_path = nullptr;

  const base::span<const SupportedTag> supported_tags;
  const bool user_specific = false;
  const base::span<const char* const> subject_names;
  const base::span<const char* const> region_urls;
};

struct ReportingConfig {
  const char* url = nullptr;
};

struct ServiceProvider {
  const char* display_name;
  // The fields below are not a raw_ptr<...> because they are initialized with
  // a non-nullptr value in constexpr.
  RAW_PTR_EXCLUSION const AnalysisConfig* analysis = nullptr;
  RAW_PTR_EXCLUSION const ReportingConfig* reporting = nullptr;
};

using ServiceProviderConfig =
    base::fixed_flat_map<std::string_view, ServiceProvider, 5>;

// Returns the global service provider configuration, containing every service
// provider and each of their supported Connector configs.
const ServiceProviderConfig* GetServiceProviderConfig();

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_SERVICE_PROVIDER_CONFIG_H_
