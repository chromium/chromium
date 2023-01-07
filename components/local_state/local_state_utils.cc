// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/local_state/local_state_utils.h"

#include <string>
#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#define ENABLE_FILTERING true
#else
#define ENABLE_FILTERING false
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// Returns true if |pref_name| starts with one of the |valid_prefixes|.
bool HasValidPrefix(const std::string& pref_name,
                    const std::vector<std::string> valid_prefixes) {
  for (const std::string& prefix : valid_prefixes) {
    if (base::StartsWith(pref_name, prefix, base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

}  // namespace

namespace internal {

void FilterPrefs(const std::vector<std::string>& valid_prefixes,
                 base::Value& prefs) {
  std::vector<std::string> prefs_to_remove;
  for (auto it : prefs.DictItems()) {
    if (!HasValidPrefix(it.first, valid_prefixes))
      prefs_to_remove.push_back(it.first);
  }
  for (const std::string& pref_to_remove : prefs_to_remove) {
    bool successfully_removed = prefs.RemovePath(pref_to_remove);
    DCHECK(successfully_removed);
  }
}

}  // namespace internal

bool GetPrefsAsJson(PrefService* pref_service, std::string* json_string) {
  base::Value local_state_values =
      pref_service->GetPreferenceValues(PrefService::EXCLUDE_DEFAULTS);
  if (ENABLE_FILTERING) {
    // Filter out the prefs to only include variations and UMA related fields,
    // which don't contain PII.
    std::vector<std::string> allowlisted_prefixes = {"variations",
                                                     "user_experience_metrics"};
    internal::FilterPrefs(allowlisted_prefixes, local_state_values);
  }

  JSONStringValueSerializer serializer(json_string);
  serializer.set_pretty_print(true);
  return serializer.Serialize(local_state_values);
}