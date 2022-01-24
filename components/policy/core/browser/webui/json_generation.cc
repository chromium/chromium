// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/json_generation.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

JsonGenerationParams::JsonGenerationParams() = default;
JsonGenerationParams::~JsonGenerationParams() = default;

std::string GenerateJson(std::unique_ptr<PolicyConversionsClient> client,
                         base::Value status,
                         const JsonGenerationParams& params) {
  base::Value chrome_metadata(base::Value::Type::DICTIONARY);
  chrome_metadata.SetKey("application", base::Value(params.application_name));

  std::string version = base::StringPrintf(
      "%s (%s)%s %s%s", version_info::GetVersionNumber().c_str(),
      l10n_util::GetStringUTF8(version_info::IsOfficialBuild()
                                   ? IDS_VERSION_UI_OFFICIAL
                                   : IDS_VERSION_UI_UNOFFICIAL)
          .c_str(),
      (params.channel_name.empty() ? "" : " " + params.channel_name).c_str(),
      params.processor_variation.c_str(),
      params.cohort_name ? params.cohort_name->c_str() : "");

  chrome_metadata.SetKey("version", base::Value(version));

  if (params.os_name && !params.os_name->empty()) {
    chrome_metadata.SetKey("OS", base::Value(params.os_name.value()));
  }

  if (params.platform_name && !params.platform_name->empty()) {
    chrome_metadata.SetKey("platform",
                           base::Value(params.platform_name.value()));
  }

  chrome_metadata.SetKey("revision",
                         base::Value(version_info::GetLastChange()));

  base::Value dict =
      policy::DictionaryPolicyConversions(std::move(client)).ToValue();

  dict.SetKey("chromeMetadata", std::move(chrome_metadata));
  dict.SetKey("status", std::move(status));

  std::string json_policies;
  base::JSONWriter::WriteWithOptions(
      dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_policies);

  return json_policies;
}

}  // namespace policy
