// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/json_generation.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

const char kChromeMetadataVersionKey[] = "version";
const char kChromeMetadataOSKey[] = "OS";
const char kChromeMetadataPlatformKey[] = "platform";
const char kChromeMetadataRevisionKey[] = "revision";

JsonGenerationParams::JsonGenerationParams() = default;
JsonGenerationParams::~JsonGenerationParams() = default;
JsonGenerationParams::JsonGenerationParams(JsonGenerationParams&&) = default;

std::string GenerateJson(base::Value::Dict policy_values,
                         base::Value::Dict status,
                         const JsonGenerationParams& params) {
  base::Value::Dict dict = std::move(policy_values);
  dict.Set("chromeMetadata", GetChromeMetadataValue(params));
  dict.Set("status", std::move(status));

  std::string json_policies;
  base::JSONWriter::WriteWithOptions(
      dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_policies);

  return json_policies;
}

base::Value::Dict GetChromeMetadataValue(const JsonGenerationParams& params) {
  base::Value::Dict chrome_metadata;
  chrome_metadata.Set("application", params.application_name);

  std::string version = base::StrCat(
      {version_info::GetVersionNumber(), " (",
       l10n_util::GetStringUTF8(version_info::IsOfficialBuild()
                                    ? IDS_VERSION_UI_OFFICIAL
                                    : IDS_VERSION_UI_UNOFFICIAL),
       ") ", params.channel_name, params.channel_name.empty() ? "" : " ",
       params.processor_variation, params.cohort_name.value_or(std::string())});

  chrome_metadata.Set(kChromeMetadataVersionKey, version);

  if (params.os_name && !params.os_name->empty()) {
    chrome_metadata.Set(kChromeMetadataOSKey, params.os_name.value());
  }

  if (params.platform_name && !params.platform_name->empty()) {
    chrome_metadata.Set(kChromeMetadataPlatformKey,
                        params.platform_name.value());
  }

  chrome_metadata.Set(kChromeMetadataRevisionKey,
                      version_info::GetLastChange());

  return chrome_metadata;
}

}  // namespace policy
