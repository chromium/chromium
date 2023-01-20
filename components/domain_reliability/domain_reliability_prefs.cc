// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/domain_reliability_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace domain_reliability {
namespace prefs {

const char kDomainReliabilityAllowedByPolicy[] =
    "domain_reliability.allowed_by_policy";

}  // namespace prefs

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDomainReliabilityAllowedByPolicy, true);
}

}  // namespace domain_reliability
