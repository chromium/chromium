// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/web_distiller_features.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/web_distiller/core/pref_names.h"
#include "components/web_distiller/core/web_distiller_switches.h"

namespace web_distiller {

bool IsWebDistillerEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebDistiller);
}

bool ShouldStartDistillabilityService() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDistillabilityService);
}

BASE_FEATURE(kSyncReaderForAccessibility, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace web_distiller
