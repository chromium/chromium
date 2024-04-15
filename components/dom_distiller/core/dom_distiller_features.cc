// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_features.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/pref_names.h"

namespace dom_distiller {

bool IsDomDistillerEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDomDistiller);
}

bool ShouldStartDistillabilityService() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDistillabilityService);
}

}  // namespace dom_distiller
