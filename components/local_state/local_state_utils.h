// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOCAL_STATE_LOCAL_STATE_UTILS_H_
#define COMPONENTS_LOCAL_STATE_LOCAL_STATE_UTILS_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Namespace for exposing the method for unit tests.
namespace internal {

// Removes elements from |prefs| where the key does not match any of the
// prefixes in |valid_prefixes|.
void FilterPrefs(const std::vector<std::string>& valid_prefixes,
                 base::Value::Dict& prefs);

}  // namespace internal

// Gets a pretty-printed string representation of the input |pref_service|.
// If the return value is true, the result will have been written to
// |json_string|. On ChromeOS, the local state file contains some information
// about other user accounts which we don't want to expose to other users. In
// that case, this will filter out the prefs to only include variations and UMA
// related fields, which don't contain PII.
absl::optional<std::string> GetPrefsAsJson(PrefService* pref_service);

#endif  // COMPONENTS_LOCAL_STATE_LOCAL_STATE_UTILS_H_
