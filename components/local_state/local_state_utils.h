// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOCAL_STATE_LOCAL_STATE_UTILS_H_
#define COMPONENTS_LOCAL_STATE_LOCAL_STATE_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/prefs/pref_service.h"

namespace local_state_utils {

// Gets a pretty-printed string representation of the input `pref_service`.
// `accepted_prefixes` is a list of prefixes that prefs must have to be
// returned in the result.
std::optional<std::string> GetPrefsAsJson(
    PrefService* pref_service,
    const std::vector<std::string>& accepted_prefixes = {});

}  // namespace local_state_utils

#endif  // COMPONENTS_LOCAL_STATE_LOCAL_STATE_UTILS_H_
