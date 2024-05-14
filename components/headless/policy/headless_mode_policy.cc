// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/policy/headless_mode_policy.h"

#include "components/headless/policy/headless_mode_prefs.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace headless {

// static
HeadlessModePolicy::HeadlessMode HeadlessModePolicy::GetPolicy(
    const PrefService* pref_service) {
  if (!pref_service) {
    return HeadlessMode::kDefaultValue;
  }

  int value = pref_service->GetInteger(headless::prefs::kHeadlessMode);
  if (value < static_cast<int>(HeadlessMode::kMinValue) ||
      value > static_cast<int>(HeadlessMode::kMaxValue)) {
    NOTREACHED_IN_MIGRATION();
    return HeadlessMode::kDefaultValue;
  }

  return static_cast<HeadlessMode>(value);
}

// static
bool HeadlessModePolicy::IsHeadlessModeDisabled(
    const PrefService* pref_service) {
  return GetPolicy(pref_service) == HeadlessMode::kDisabled;
}

}  // namespace headless
