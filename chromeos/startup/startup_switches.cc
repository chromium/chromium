// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/startup_switches.h"

namespace chromeos {
namespace switches {

// FD pointing a (memory backed) file containing the startup data.
const char kCrosStartupDataFD[] = "cros-startup-data-fd";

// FD pointing to an anonymous pipe containing the post-login data.
const char kCrosPostLoginDataFD[] = "cros-postlogin-data-fd";

// The path of the log file that Lacros should use post-login.
const char kCrosPostLoginLogFile[] = "cros-postlogin-log-file";

// Whether to enable the zygote hugepage remap feature.
const char kZygoteHugepageRemap[] = "zygote-hugepage-remap";

}  // namespace switches
}  // namespace chromeos
