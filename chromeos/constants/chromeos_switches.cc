// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_switches.h"

#include "base/command_line.h"
#include "base/hash/sha1.h"

namespace chromeos::switches {
namespace {

// Whether to ignore the key used to gate debugging of preinstallation of the
// container app. This should only be used in testing.
bool g_ignore_container_app_preinstall_debug_key_for_testing = false;

}  // namespace

// The name for the command-line switch used to provide the activation time
// threshold for the container app. Note that this switch will only be used for
// testing purposes.
const char kContainerAppPreinstallActivationTimeThreshold[] =
    "container-app-preinstall-activation-time-threshold";

// The name for the command-line switch used to provide the key which gates
// debugging preinstallation of the container app.
const char kContainerAppPreinstallDebugKey[] =
    "container-app-preinstall-debug-key";

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

// Returns whether there is a match for the value from the command-line switch
// for the key which gates debugging preinstallation of the container app.
bool IsContainerAppPreinstallDebugKeyMatched() {
  if (g_ignore_container_app_preinstall_debug_key_for_testing) {
    return true;
  }
  constexpr char kKeyHash[] =
      "\xa1\x65\xcd\x65\x2a\x94\xed\xe6\x97\x7d\xcc\x5b\xcc\x94\x66\xd4\x0a\x90"
      "\x67\x65";
  const std::string key =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kContainerAppPreinstallDebugKey);
  return base::SHA1HashString(key) == kKeyHash;
}

base::AutoReset<bool> SetIgnoreContainerAppPreinstallDebugKeyForTesting() {
  return {&g_ignore_container_app_preinstall_debug_key_for_testing, true};
}

}  // namespace chromeos::switches
