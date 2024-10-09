// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_switches.h"

#include "base/command_line.h"

namespace chromeos::switches {

// The name for the command-line switch used to provide the activation time
// threshold for the container app. Note that this switch will only be used for
// testing purposes.
const char kContainerAppPreinstallActivationTimeThreshold[] =
    "container-app-preinstall-activation-time-threshold";

// Use in test to override mahi age and country restriction.
const char kMahiRestrictionsOverride[] = "mahi-restrictions-override";

// Returns the value from the command-line switch for the activation time
// threshold for the container app. Returns an absent value if the command-line
// switch isn't present or cannot be parsed. Note that this switch will only be
// used for testing purposes.
std::optional<base::Time> GetContainerAppPreinstallActivationTimeThreshold() {
  base::Time activation_time_threshold;
  return base::Time::FromUTCString(
             base::CommandLine::ForCurrentProcess()
                 ->GetSwitchValueASCII(
                     kContainerAppPreinstallActivationTimeThreshold)
                 .c_str(),
             &activation_time_threshold)
             ? std::make_optional(activation_time_threshold)
             : std::nullopt;
}

}  // namespace chromeos::switches
