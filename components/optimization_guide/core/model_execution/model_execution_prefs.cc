// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"

#include "base/notreached.h"
#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide::model_execution::prefs {

const char kTabOrganizationEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.tab_organization_enterprise_policy_"
    "allowed";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kTabOrganizationEnterprisePolicyAllowed,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kAllow),
      PrefRegistry::LOSSY_PREF);
}

const char* GetEnterprisePolicyPrefName(proto::ModelExecutionFeature feature) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      return nullptr;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      return kTabOrganizationEnterprisePolicyAllowed;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      return nullptr;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace optimization_guide::model_execution::prefs
