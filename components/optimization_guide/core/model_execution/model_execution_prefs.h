// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_

#include "components/optimization_guide/proto/model_execution.pb.h"

class PrefRegistrySimple;

namespace optimization_guide::model_execution::prefs {

// The possible values for the model execution enterprise policy.
enum class ModelExecutionEnterprisePolicyValue {
  kAllow = 0,
  kAllowWithoutLogging = 1,
  kDisable = 2,
};

extern const char kTabOrganizationEnterprisePolicyAllowed[];
extern const char kComposeEnterprisePolicyAllowed[];
extern const char kWallpaperSearchEnterprisePolicyAllowed[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the name of the pref to check for enterprise policy for `feature`.
// Null is returned when no enterprise policy is defined for the `feature`.
const char* GetEnterprisePolicyPrefName(proto::ModelExecutionFeature feature);

}  // namespace optimization_guide::model_execution::prefs

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
