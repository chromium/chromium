// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_switches.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/command_line.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::switches {

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// The name for the command-line switch used to provide the key which gates
// preinstallation of the container app.
const char kContainerAppPreinstallKey[] = "container-app-preinstall-key";

// Returns the value from the command-line switch for the key which gates
// preinstallation of the container app. Returns an empty string if the
// command-line switch isn't present.
std::string GetContainerAppPreinstallKey() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      kContainerAppPreinstallKey);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace chromeos::switches
