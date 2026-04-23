// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/vrp_flags/vrp_flags.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "sandbox/policy/switches.h"

namespace vrp_flags {

namespace switches {
const char kVrpFlags[] = "vrp-flags";
}

bool IsEnabled() {
  static bool enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kVrpFlags);
  if (enabled) {
    CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
        sandbox::policy::switches::kNoSandbox))
        << "flag not permitted when --vrp-flags is running";
  }
  return enabled;
}

}  // namespace vrp_flags
