// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/actor_util.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_switches.h"

namespace actor {

bool IsActorSafetyCheckDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableActorSafetyChecks);
}

bool IsNavigationGatingEnabled() {
  return !IsActorSafetyCheckDisabled() &&
         base::FeatureList::IsEnabled(kGlicCrossOriginNavigationGating);
}

}  // namespace actor
