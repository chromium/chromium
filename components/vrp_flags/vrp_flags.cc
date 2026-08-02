// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/vrp_flags/vrp_flags.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/network_switches.h"

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

void PostEarlyInitialization() {
  if (!IsEnabled()) {
    return;
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string existing_rules =
      command_line->GetSwitchValueASCII(network::switches::kHostResolverRules);
  if (!existing_rules.empty()) {
    LOG(WARNING)
        << "Existing --host-resolver-rules found: \"" << existing_rules
        << "\". Skipping automatic default mapping for victim.test. "
           "If running CTF manually with custom rules, ensure your host "
           "resolver rules include \"MAP victim.test 127.0.0.1:8000\".";
  } else {
    VLOG(1) << "VRP flags enabled: mapping victim.test to 127.0.0.1:8000";
    command_line->AppendSwitchASCII(network::switches::kHostResolverRules,
                                    "MAP victim.test 127.0.0.1:8000");
  }
}

}  // namespace vrp_flags
