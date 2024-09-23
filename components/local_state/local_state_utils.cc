// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/local_state/local_state_utils.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"

namespace local_state_utils {
namespace {

// Returns true if |pref_name| starts with one of the |valid_prefixes|.
bool HasValidPrefix(const std::string& pref_name,
                    const std::vector<std::string> valid_prefixes) {
  for (const std::string& prefix : valid_prefixes) {
    if (base::StartsWith(pref_name, prefix, base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}

base::Value::List GetPrefsMetadata(
    PrefValueStore::PrefStoreType pref_value_store_type) {
  base::Value::List metadata;
  switch (pref_value_store_type) {
    case PrefValueStore::PrefStoreType::MANAGED_STORE:
      metadata.Append("managed");
      break;
    case PrefValueStore::PrefStoreType::SUPERVISED_USER_STORE:
      metadata.Append("managed_by_custodian");
      break;
    case PrefValueStore::PrefStoreType::EXTENSION_STORE:
#if BUILDFLAG(ENABLE_EXTENSIONS)
      metadata.Append("extension_controlled");
      metadata.Append("extension_modifiable");
#else
      NOTREACHED_IN_MIGRATION();
#endif
      break;
    case PrefValueStore::PrefStoreType::STANDALONE_BROWSER_STORE:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      metadata.Append("standalone_browser_controlled");
      metadata.Append("standalone_browser_modifiable");
#endif
      metadata.Append("extension_modifiable");
      break;
    case PrefValueStore::PrefStoreType::COMMAND_LINE_STORE:
      metadata.Append("command_line_controlled");
#if BUILDFLAG(ENABLE_EXTENSIONS)
      metadata.Append("extension_modifiable");
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      metadata.Append("standalone_browser_modifiable");
#endif
      break;
    case PrefValueStore::PrefStoreType::USER_STORE:
      metadata.Append("user_controlled");
      metadata.Append("user_modifiable");
#if BUILDFLAG(ENABLE_EXTENSIONS)
      metadata.Append("extension_modifiable");
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      metadata.Append("standalone_browser_modifiable");
#endif
      break;
    case PrefValueStore::PrefStoreType::RECOMMENDED_STORE:
      metadata.Append("recommended");
      metadata.Append("user_modifiable");
#if BUILDFLAG(ENABLE_EXTENSIONS)
      metadata.Append("extension_modifiable");
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      metadata.Append("standalone_browser_modifiable");
#endif
      break;
    case PrefValueStore::PrefStoreType::DEFAULT_STORE:
      metadata.Append("default");
      metadata.Append("user_modifiable");
#if BUILDFLAG(ENABLE_EXTENSIONS)
      metadata.Append("extension_modifiable");
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      metadata.Append("standalone_browser_modifiable");
#endif
      break;
    case PrefValueStore::PrefStoreType::INVALID_STORE:
      metadata.Append("user_modifiable");
#if BUILDFLAG(ENABLE_EXTENSIONS)
      metadata.Append("extension_modifiable");
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      metadata.Append("standalone_browser_modifiable");
#endif
      break;
  }
  return metadata;
}

}  // namespace

std::optional<std::string> GetPrefsAsJson(
    PrefService* pref_service,
    const std::vector<std::string>& accepted_prefixes) {
  std::vector<PrefService::PreferenceValueAndStore> values =
      pref_service->GetPreferencesValueAndStore();

  base::Value::Dict local_state_values;
  for (const auto& [name, value, pref_value_store_type] : values) {
    // Filter out the prefs to only include variations and UMA related fields,
    // which don't contain PII.
    if (!accepted_prefixes.empty() &&
        !HasValidPrefix(name, accepted_prefixes)) {
      continue;
    }

    base::Value::Dict pref_details;
    pref_details.Set("value", value.Clone());
    pref_details.Set("metadata", GetPrefsMetadata(pref_value_store_type));
    local_state_values.SetByDottedPath(name, std::move(pref_details));
  }

  std::string result;
  if (!base::JSONWriter::WriteWithOptions(
          local_state_values, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &result)) {
    return std::nullopt;
  }
  return result;
}

}  // namespace local_state_utils
